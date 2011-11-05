#include "StdAfx.h"
#include "SvnToGit.h"

#include "SvnToGitAux.h"
#include "svncpp/client.hpp"
using namespace std;


extern svn::Context* G_svnCtxt;

using namespace svn::replay;

using namespace SvnToGit;

struct RevSyncCtxt : RunCtxt
{
	typedef std::tr1::shared_ptr<Git::CTree> sharedTree;
	RevSyncCtxt(Git::CRepo& gitRepo, svn::Repo& svnRepo, const std::string& svnRepoUrl):RunCtxt(gitRepo), m_svnRepo(svnRepo), m_svnRepoUrl(svnRepoUrl)
	{
		m_Tree_Meta		= m_rootTree.GetByPath("meta");
		m_Tree_Content	= m_rootTree.GetByPath("content");
	}

	svn::Repo&	m_svnRepo;
	std::string m_svnRepoUrl;

	Git::COid	m_lastCommit;
	//sharedTree	m_lastTree;
	Git::CTreeNode	m_rootTree;
	Git::CTreeNode*	m_Tree_Content;
	Git::CTreeNode*	m_Tree_Meta;


	std::string m_csBaseRefName;

	void CheckExistingRef(const std::string refName)
	{
		try
		{
			m_gitRepo.GetRef(refName.c_str());
		}
		catch(std::exception&)
		{
			cout << "Creating new ref: " << refName << endl;
			m_gitRepo.MakeRef(refName.c_str(), m_lastCommit);
		}
	}

	void CheckExistingRefs()
	{
		CheckExistingRef(SvnMetaRefName());
	}

	std::string SvnMetaRefName()
	{
		return m_csBaseRefName + "/_svnmeta";
	}

	static std::string MakeGitPathName(std::string src)
	{
		size_t pos = src.find_first_not_of('/');
		if(pos != std::string::npos)
			src = src.substr(pos);
		return src;
	}

	class ReplayFile : public File
	{
	public:
		class ReplayDeltaHandler : public ApplyDeltaHandler
		{
		public:
			ReplayDeltaHandler(ReplayFile* file):m_file(file){}
			ReplayFile* m_file;
			virtual void handleWindow(svn_txdelta_window_t *window)
			{
				std::string& content = m_file->Content(window->sview_len != 0);
				std::string out;
				out.resize(window->tview_len);
				apr_size_t size = out.size();
				if(window->sview_len != 0)
				{
					if(window->sview_len != content.size())
						throw svn::Exception(JStd::String::Format("Content size mismatch of file %s. Expected %Id, got %Id.",
												m_file->m_name.c_str(), window->sview_len, content.size()).c_str());
					svn_txdelta_apply_instructions(window, content.c_str(), &*out.begin(), &size);
					if(out.size() != size)
						out.resize(size);
					content.swap(out);
				}
				else
				{
					if(!content.empty())
						cout << "Test" << endl;
					svn_txdelta_apply_instructions(window, NULL, &*out.begin(), &size);
					if(out.size() != size)
						out.resize(size);
					content += out;
				}

			}
		};

		ReplayFile(RevSyncCtxt* ctxt, svn_revnum_t rev=0):m_ctxt(ctxt), m_props(ctxt), m_rev(rev), m_bHasBeenRead(false), m_bModified(false), m_iWindowCount(0){}
		ReplayFile(RevSyncCtxt* ctxt, const char* copyfrom_path, svn_revnum_t copyfrom_rev):m_ctxt(ctxt), m_props(ctxt),m_bHasBeenRead(false), m_bModified(false), m_rev(-1), m_iWindowCount(0)
		{
			if(copyfrom_path)
				m_blobs = m_ctxt->m_mapRev.Get(copyfrom_path, copyfrom_rev);
			else
				m_bHasBeenRead = true; //Does not have to be read
			m_props.setCopyFrom(copyfrom_path, copyfrom_rev);
		}

		RevSyncCtxt*	m_ctxt;
		std::string		m_content;
		svn_revnum_t	m_rev;
		bool			m_bHasBeenRead;
		bool			m_bModified;
		GitOids			m_blobs;
		PropertyFile	m_props;

		int				m_iWindowCount;

		void onInit()
		{
			if(!m_new)
				m_blobs = m_ctxt->m_mapRev.Get(m_name, m_rev);
			m_props.m_Oid = m_blobs.m_oidMeta;
		}

		std::string& Content(bool P_bRead)
		{
			if(P_bRead && !m_bHasBeenRead)
			{
				//Lazy read
				Git::CBlob blob;
				m_ctxt->m_gitRepo.Read(blob, m_blobs.m_oidContent);
				m_content.assign((const char*)blob.Content(), blob.Size());
			}
			m_bHasBeenRead = true;
			m_bModified = true; //Window is going to be applied. So it is going to be modified.
			++m_iWindowCount;
			return m_content;
		}

		virtual ApplyDeltaHandler* applyDelta(const char* baseChecksum)
		{
			return new ReplayDeltaHandler(this);
		}

		void changeProp(const char *name, const svn_string_t *value)
		{
			m_props.changeProp(name, value);
		}

		void onClose(const char* checksum)
		{
			//TODO: Check content with checksum
			//Write content
			if(m_bModified)
				m_blobs.m_oidContent = m_ctxt->m_gitRepo.WriteBlob(m_content.data(), m_content.size());
			m_ctxt->m_Tree_Content->Insert(m_name.c_str(), m_blobs.m_oidContent);
			//Write meta data
			m_blobs.m_oidMeta = m_props.Write();
			if(!m_blobs.m_oidMeta.isNull())
				m_ctxt->m_Tree_Meta->Insert(m_name.c_str(), m_blobs.m_oidMeta);
			m_ctxt->m_mapRev.Get(m_name, m_editor->m_TargetRevision, false) = m_blobs;
			m_ctxt->m_mapRev.Get(m_name, -1, false)							= m_blobs;
		}

	};

	class ReplayDir : public Directory
	{
	public:
		ReplayDir(RevSyncCtxt* ctxt, svn_revnum_t rev, const char* path, const char* copyfrom_path = NULL, svn_revnum_t copyfrom_revision = -1):m_ctxt(ctxt), m_rev(rev), m_props(ctxt)
		{
			if(copyfrom_path)
			{
				GitOids& trees = ctxt->m_mapRev.Get(copyfrom_path, copyfrom_revision);
				m_ctxt->m_gitRepo.BuildTreeNode(*m_ctxt->m_Tree_Content->GetByPath(path), trees.m_oidContentTree);
				m_ctxt->m_gitRepo.BuildTreeNode(*m_ctxt->m_Tree_Meta->GetByPath(path),	  trees.m_oidMetaTree);
				//TODO: should we remove the copy from data from the copied tree?
			}
			m_props.setCopyFrom(copyfrom_path, copyfrom_revision);
		}
		RevSyncCtxt* m_ctxt;
		PropertyFile m_props;

		GitOids			m_trees;
		svn_revnum_t	m_rev;

		void onInit()
		{
			if(!m_new)
			{
				if(!m_name.empty())
					m_trees = m_ctxt->m_mapRev.Get(m_name + "/.svnDirectoryProps", m_rev);
			}
			m_props.m_Oid = m_trees.m_oidMeta; //.m_fileName = m_name + "/.svnDirectoryProps";
		}

		void onClose()
		{
			if(m_name.empty())
				return;
			m_trees.m_oidMeta = m_props.Write();
			m_ctxt->m_Tree_Meta->Insert((m_name + "/.svnDirectoryProps").c_str(), m_trees.m_oidMeta);
			m_ctxt->m_mapRev.Get(m_name + "/.svnDirectoryProps", m_rev, false) = m_trees;
			m_ctxt->m_mapRev.Get(m_name + "/.svnDirectoryProps", -1, false) = m_trees;
			//TODO: also update current revision
		}

		virtual File* addFile(const char* path, const char* copyfrom_path, svn_revnum_t copyfrom_revision)
		{
			cout << "A " << path;
			if(copyfrom_path)
				cout << " -" << copyfrom_path << "@" << copyfrom_revision;
			cout << endl;
			return new ReplayFile(m_ctxt, copyfrom_path, copyfrom_revision);
		}

		virtual File* openFile(const char* path, svn_revnum_t base_revision)
		{
			cout << "M " << path << "@" << base_revision << endl;
			return new ReplayFile(m_ctxt, base_revision);
		}


		virtual Directory* add(const char* path, const char* copyfrom_path, svn_revnum_t copyfrom_revision)
		{
			cout << "A " << path;
			if(copyfrom_path)
				cout << " -" << copyfrom_path << "@" << copyfrom_revision;
			cout << endl;
			return new ReplayDir(m_ctxt, -1, path, copyfrom_path, copyfrom_revision);
		}

		virtual Directory* open(const char* path, svn_revnum_t base_revision)
		{
			cout << "M " << path;
			if(base_revision >= 0)
				cout << "@" << base_revision;
			cout << endl;
			return new ReplayDir(m_ctxt, base_revision, path);
		}

		virtual void deleteEntry(const char* path, svn_revnum_t revision)
		{
			cout << "D " << path;
			if(revision >= 0)
				cout << "@" << revision;
			cout << endl;
			m_ctxt->m_Tree_Content->Delete(path);
			m_ctxt->m_Tree_Meta->Delete(path);
		}

		void changeProp(const char *name, const svn_string_t *value)
		{
			m_props.changeProp(name, value);
		}
	};

	class ReplayEditor : public Editor
	{
	public:
		Directory* openRoot(svn_revnum_t base_revision)
		{
			cout << " root@" << base_revision << endl;
			return new ReplayDir(m_ctxt, base_revision, "/");
		}

		RevSyncCtxt* m_ctxt;
	};


	void OnSvnLogEntry(const svn::LogEntry& entry)
	{
		if(entry.revision == 0)
			return; //Skip rev 0..

		std::ostringstream text;

		text << "*** Fetching rev " << entry.revision;

		cout << text.str() << " " << flush;

		ReplayEditor editor;
		editor.m_ctxt = this;
		editor.replay(&m_svnRepo, entry.revision, 0, true);


		Git::CTree tree(m_gitRepo, m_rootTree.Write(m_gitRepo));

		//Cache the root tree
		GitOids& rootOids = m_mapRev.Get("", entry.revision, false);
		rootOids.m_oidContentTree = m_Tree_Content->m_oid;
		rootOids.m_oidMetaTree = m_Tree_Meta->m_oid;

		std::string author = entry.author;
		if(author.empty())
			author = "nobody";
		Git::CSignature sig(author.c_str(), (author + "@svn").c_str());


		std::ostringstream msg;
		msg << "Rev: " << entry.revision << endl
			<< "Time: " << entry.date << endl
			<< "Msg:" << endl << entry.message << endl;

		Git::COids oids;
		if(!m_lastCommit.isNull())
			oids << m_lastCommit;
		//m_lastCommit = m_gitRepo.Commit((m_csBaseRefName + "/_svnmeta").c_str(), sig, sig, msg.str().c_str(), tree, oids);
		m_lastCommit = m_gitRepo.Commit(m_lastCommit.isNull() ? "HEAD" : SvnMetaRefName().c_str(), sig, sig, msg.str().c_str(), tree, m_gitRepo.ToCommits( oids));

		if(oids.m_oids.empty())
		{
			//First commit done. Lets make a ref.
			CheckExistingRefs();
		}

	}
};

void SvnToGitSync(const wchar_t* gitRepoPath, const char* svnRepoUrl, const char* refBaseName)
{
//	Git::CSignature sig("Johan", "johan@test.nl");
//	svn::Context* W_Context2Ptr = new svn::Context();
	//Need a separate repository for the log and for the replay.
	//Or else the replay function will throw when using the svn protocol.
	svn::Repo svnRepoLog(G_svnCtxt, svnRepoUrl);
	svn::Repo svnRepoReplay(G_svnCtxt, svnRepoUrl); 

	//svn::Repo svnRepo(G_svnCtxt, svnRepoUrl);

	Git::CRepo gitRepo;
	try
	{
		gitRepo.Open((wstring(gitRepoPath) + L".git/").c_str());
		cout << "Opened git repository on " << JStd::String::ToMult(gitRepoPath, CP_OEMCP) << endl;
	}
	catch(std::exception&)
	{
		cout << "Creating git repository on " << JStd::String::ToMult(gitRepoPath, CP_OEMCP) << "..." << endl;

		gitRepo.Create(gitRepoPath, false);
	}


	cout << "Fetching subversion log from " << svnRepoUrl << " ..." << endl;

	RevSyncCtxt ctxt(gitRepo, svnRepoReplay, svnRepoUrl);
	//ctxt.CheckExistingRefs();

	ctxt.m_csBaseRefName = refBaseName;
	//G_svnClient->log(std::tr1::bind(&RevSyncCtxt::OnSvnLogEntry, &ctxt, std::tr1::placeholders::_1),
	svnRepoLog.log(std::tr1::bind(&RevSyncCtxt::OnSvnLogEntry, &ctxt, std::tr1::placeholders::_1),
					 svnRepoUrl,
					 0,
					 SVN_INVALID_REVNUM,
					 true);
	cout << "Done." << endl;
}