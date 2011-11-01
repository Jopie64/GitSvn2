#include "StdAfx.h"
#include "SvnToGit.h"
#include "GitCpp\Git.h"
#include <iostream>
#include <sstream>
#include "GitCpp\jstd\JStd.h"

#undef strcasecmp
#undef strncasecmp

#include "svncpp\stream.hpp"
#include "svncpp\client.hpp"
#include "svncpp\replay.hpp"
using namespace std;

extern svn::Client* G_svnClient;
extern svn::Context* G_svnCtxt;

using namespace svn::replay;

class PathRev
{
public:
	PathRev(const std::string& path, svn_revnum_t rev = -1):m_path(path),m_rev(rev)
	{
		while(*m_path.c_str() == '/')
			m_path = m_path.substr(1);
	}

	bool operator<(const PathRev& that)const
	{
		if(m_rev < that.m_rev) return true;
		if(m_rev > that.m_rev) return false;
		return m_path < that.m_path;
	}

	std::string m_path;
	svn_revnum_t m_rev;
};
typedef std::map<PathRev, Git::COid> MapPathRev_Oid;

struct RevSyncCtxt
{
	typedef std::tr1::shared_ptr<Git::CTree> sharedTree;
	RevSyncCtxt(Git::CRepo& gitRepo, svn::Repo& svnRepo, svn::Repo& svnRepo2, const std::string& svnRepoUrl):m_gitRepo(gitRepo), m_svnRepo(svnRepo), m_svnRepo2(svnRepo2), m_svnRepoUrl(svnRepoUrl)
	{
		m_Tree_Meta		= m_rootTree.GetByPath("meta");
		m_Tree_Content	= m_rootTree.GetByPath("content");
	}

	Git::CRepo& m_gitRepo;
	svn::Repo&	m_svnRepo;
	svn::Repo&	m_svnRepo2;
	std::string m_svnRepoUrl;

	Git::COid	m_lastCommit;
	//sharedTree	m_lastTree;
	Git::CTreeNode	m_rootTree;
	Git::CTreeNode*	m_Tree_Content;
	Git::CTreeNode*	m_Tree_Meta;

	MapPathRev_Oid	m_mapBlob;

	std::string m_csBaseRefName;

	Git::COid FindBlob(const std::string& path, svn_revnum_t rev)
	{
		MapPathRev_Oid::iterator i = m_mapBlob.find(PathRev(path, rev));
		if(i == m_mapBlob.end())
			throw svn::Exception(JStd::String::Format("Cannot find GIT blob for %s@%d", path.c_str(), rev).c_str());
		return i->second;
	}

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

		ReplayFile(RevSyncCtxt* ctxt, svn_revnum_t rev=0):m_ctxt(ctxt), m_rev(rev), m_bHasBeenRead(false), m_bModified(false), m_iWindowCount(0){}
		ReplayFile(RevSyncCtxt* ctxt, const char* copyfrom_path, svn_revnum_t copyfrom_rev):m_ctxt(ctxt), m_bHasBeenRead(false), m_bModified(false), m_rev(-1), m_iWindowCount(0)
		{
			if(copyfrom_path)
				m_blob = m_ctxt->FindBlob(copyfrom_path, copyfrom_rev);
			else
				m_bHasBeenRead = true; //Does not have to be read
		}

		RevSyncCtxt*	m_ctxt;
		std::string		m_content;
		svn_revnum_t	m_rev;
		bool			m_bHasBeenRead;
		bool			m_bModified;
		Git::COid		m_blob;

		int				m_iWindowCount;

		void onInit()
		{
			if(!m_new)
				m_blob = m_ctxt->FindBlob(m_name, m_rev);
		}

		std::string& Content(bool P_bRead)
		{
			if(P_bRead && !m_bHasBeenRead && !m_new)
			{
				//Lazy read
				Git::CBlob blob;
				m_ctxt->m_gitRepo.Read(blob, m_blob);
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

		void onClose(const char* checksum)
		{
			//TODO: Check content with checksum
			if(m_bModified)
				m_blob = m_ctxt->m_gitRepo.WriteBlob(m_content.data(), m_content.size());
			m_ctxt->m_Tree_Content->Insert(m_name.c_str(), m_blob);
			m_ctxt->m_mapBlob[PathRev(m_name, m_editor->m_TargetRevision)] = m_blob;
			m_ctxt->m_mapBlob[m_name] = m_blob;
		}

	};

	class ReplayDir : public Directory
	{
	public:
		ReplayDir(RevSyncCtxt* ctxt):m_ctxt(ctxt){}
		RevSyncCtxt* m_ctxt;

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
			return new ReplayDir(m_ctxt);
		}

		virtual Directory* open(const char* path, svn_revnum_t base_revision)
		{
			cout << "M " << path << "@" << base_revision << endl;
			return new ReplayDir(m_ctxt);
		}

		virtual void	   deleteEntry(const char* path, svn_revnum_t revision)
		{
		}

	};

	class ReplayEditor : public Editor
	{
	public:
		Directory* openRoot(svn_revnum_t base_revision)
		{
			cout << " root@" << base_revision << endl;
			return new ReplayDir(m_ctxt);
		}

		RevSyncCtxt* m_ctxt;
	};


	void OnSvnLogEntry(const svn::LogEntry& entry)
	{
		if(entry.revision == 0)
			return; //Skip rev 0..

		std::ostringstream text;

		text << "\r*** Fetching rev " << entry.revision;
		
		cout << text.str() << " " << flush;

		ReplayEditor editor;
		editor.m_ctxt = this;
		editor.replay(&m_svnRepo, entry.revision, 0, true);
		//svn_ra_replay(m_svnRepo.GetInternalObj(), entry.revision, 0, true, editor.GetInternalObj(), (void*)&editor, editor.pool());

		//Git::CTreeBuilder treeB(&*m_lastTree);
/*		for(std::list<svn::LogChangePathEntry>::const_iterator i = entry.changedPaths.begin(); i != entry.changedPaths.end(); ++i)
		{
			try
			{
				cout << text.str() << ": " << i->path << " ..." << flush;
				std::ostringstream os;
				//G_svnClient->get(svn::Stream(os), m_svnRepoUrl + i->path, entry.revision);
				m_svnRepo.getFile(svn::Stream(os), i->path.substr(1), entry.revision);
				m_Tree_Content->Insert(i->path.c_str(), m_gitRepo.WriteBlob(os.str()));
			}
			catch(svn::ClientException& e)
			{
				if(e.apr_err() == SVN_ERR_CLIENT_IS_DIRECTORY)
					cout << "jay!" << endl;
				else if(e.apr_err() == SVN_ERR_FS_NOT_FILE)
					cout << "jay2!" << endl;
				else if(e.apr_err() == SVN_ERR_FS_NOT_FOUND)
					cout << "Huh? " << e.message() << endl;
				else
					cout << "Oops: " << e.message() << endl;
			}
		}
*/

		Git::CTree tree(m_gitRepo, m_rootTree.Write(m_gitRepo));

		Git::CSignature sig(entry.author.c_str(), (entry.author + "@svn").c_str());


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
	svn::Context* W_Context2Ptr = new svn::Context();
	svn::Repo svnRepo(W_Context2Ptr, svnRepoUrl);
	svn::Repo svnRepo2(new svn::Context(), svnRepoUrl);

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

	RevSyncCtxt ctxt(gitRepo, svnRepo, svnRepo2, svnRepoUrl);
	//ctxt.CheckExistingRefs();

	ctxt.m_csBaseRefName = refBaseName;
	//G_svnClient->log(std::tr1::bind(&RevSyncCtxt::OnSvnLogEntry, &ctxt, std::tr1::placeholders::_1),
	svnRepo.log(std::tr1::bind(&RevSyncCtxt::OnSvnLogEntry, &ctxt, std::tr1::placeholders::_1),
					 svnRepoUrl,
					 svn::Revision::START,
					 svn::Revision::HEAD,
					 true);
	cout << "Done." << endl;
}