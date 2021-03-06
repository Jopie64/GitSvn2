#include "StdAfx.h"
#include "SvnToGit.h"
#include "gitcpp/jstd/CmdLine.h"

#include "GitSvnAux.h"
#include "svncpp/client.hpp"
#include <sstream>
using namespace std;

using JStd::CmdLine::CmdLine;


void onClone(CmdLine& cmdLine);
void onFetch(CmdLine& cmdLine);
static bool registered1 = JStd::CmdLine::Register(L"clone", &onClone);
static bool registered2 = JStd::CmdLine::Register(L"fetch", &onFetch);

void SvnToGitSync(const wchar_t* gitRepoPath, const char* svnRepoUrl, const char* refBaseName);
void onClone(CmdLine& cmdLine)
{
	std::wstring gitPath = cmdLine.next();
	std::wstring svnPath = cmdLine.next();
	if(gitPath.empty() || svnPath.empty())
		JStd::CmdLine::throwUsage(L"<git repo path> <svn repo path> [remote name]");

	std::string remoteName = JStd::String::ToMult(cmdLine.next(), CP_UTF8);
	if(remoteName.empty())
		remoteName = "svn";
	SvnToGitSync(gitPath.c_str(), JStd::String::ToMult(svnPath.c_str(), CP_UTF8).c_str(), remoteName.c_str());
}


extern svn::Context* G_svnCtxt;

using namespace svn::replay;

using namespace GitSvn;

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

	GitOids& SetOids(const std::string& path, svn_revnum_t rev = SVN_INVALID_REVNUM)
	{
		return m_mapRev.Get(path, rev, false);
	}

	GitOids GetOids(const std::string& path, svn_revnum_t rev = SVN_INVALID_REVNUM)
	{
		if(rev != SVN_INVALID_REVNUM)
			return m_mapRev.Get(path, rev, true);

		GitOids oids;
		Git::CTreeNode* contentNode	= m_Tree_Content->GetByPath(path.c_str(), false);
		Git::CTreeNode* metaNode	= m_Tree_Meta->GetByPath(path.c_str(), false);
		if(!metaNode)
			throw std::logic_error(JStd::String::Format("Meta node should always exist. Path \"%s\"@r%d doesn't.", path.c_str(), rev));

		if(metaNode->IsFile())
		{
			if(!contentNode)
				throw std::logic_error("Content node of file should have existed...");
			oids.m_oidContent			= contentNode->m_oid;
			oids.m_oidMeta				= metaNode->m_oid;
		}
		else
		{
			if(contentNode)
				oids.m_oidContentTree	= contentNode->m_oid;
			oids.m_oidMetaTree			= metaNode->m_oid;
			oids.m_oidMeta				= metaNode->GetByPath(".svnDirectoryProps")->m_oid;
		}
		return oids;
	}

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
		return GitSvn::toRef(m_csBaseRefName, GitSvn::eRT_meta);
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
			ReplayDeltaHandler(ReplayFile* file):m_file(file), m_iWindow(0){}
			ReplayFile* m_file;
			int m_iWindow;
			virtual void handleWindow(svn_txdelta_window_t *window)
			{
				++m_iWindow;
				std::string& content = m_file->Content(window->sview_len != 0);
				std::string out;
				out.resize(window->tview_len);
				apr_size_t size = out.size();
				if(window->sview_len != 0 && window->sview_len == content.size())
				{
					svn_txdelta_apply_instructions(window, content.c_str(), &*out.begin(), &size);
					if(out.size() != size)
						out.resize(size);
					content.swap(out);
				}
				else
				{
					if(window->sview_len > content.size())
						throw svn::Exception(JStd::String::Format("Expected current content size of file %s was too small. Expected minimal %Id, got %Id.",
												m_file->m_name.c_str(), window->sview_len, content.size()).c_str());
					if(!content.empty())
						cout << "Window " << m_iWindow << "\r";
					svn_txdelta_apply_instructions(window, window->sview_len == 0 ? NULL : &*(content.end() - window->sview_len), &*out.begin(), &size);
					if(out.size() != size)
						out.resize(size);
					content += out;
				}

			}
		};

		ReplayFile(RevSyncCtxt* ctxt, svn_revnum_t rev=0):m_ctxt(ctxt), m_props(ctxt), m_rev(rev), m_bHasBeenRead(false), m_bModified(false), m_iWindowCount(0){}
		ReplayFile(RevSyncCtxt* ctxt, const char* copyfrom_path, svn_revnum_t copyfrom_rev):m_ctxt(ctxt), m_props(ctxt),m_bHasBeenRead(false), m_bModified(false), m_rev(SVN_INVALID_REVNUM), m_iWindowCount(0)
		{
			if(copyfrom_path)
				m_blobs = m_ctxt->GetOids(copyfrom_path, copyfrom_rev);
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
				m_blobs = m_ctxt->GetOids(m_name, m_rev);
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
			m_ctxt->SetOids(m_name, m_editor->m_TargetRevision) = m_blobs;
			m_ctxt->SetOids(m_name, SVN_INVALID_REVNUM)			= m_blobs;
		}

	};

	class ReplayDir : public Directory
	{
	public:
		ReplayDir(RevSyncCtxt* ctxt, svn_revnum_t rev, const char* path, const char* copyfrom_path = NULL, svn_revnum_t copyfrom_revision = SVN_INVALID_REVNUM):m_ctxt(ctxt), m_rev(rev), m_props(ctxt)
		{
			if(copyfrom_path)
			{
				GitOids& trees = ctxt->GetOids(copyfrom_path, copyfrom_revision);
				if(!trees.m_oidContentTree.isNull()) //Can be NULL because GIT does not support empty directories.
					m_ctxt->m_gitRepo.BuildTreeNode(*m_ctxt->m_Tree_Content->GetByPath(path),	trees.m_oidContentTree);
				if(trees.m_oidMetaTree.isNull())
					throw std::logic_error("Meta data should never be null for an existing path.");
				m_ctxt->m_gitRepo.BuildTreeNode(*m_ctxt->m_Tree_Meta->GetByPath(path),			trees.m_oidMetaTree);
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
					m_trees = m_ctxt->GetOids(m_name, m_rev);
			}
			m_props.m_Oid = m_trees.m_oidMeta; //.m_fileName = m_name + "/.svnDirectoryProps";
		}

		void onClose()
		{
			if(m_name.empty())
				return;
			m_trees.m_oidMeta = m_props.Write();
			m_ctxt->m_Tree_Meta->Insert((m_name + "/.svnDirectoryProps").c_str(), m_trees.m_oidMeta);
			m_ctxt->SetOids(m_name, m_rev)				= m_trees;
			m_ctxt->SetOids(m_name, SVN_INVALID_REVNUM)	= m_trees;
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
			return new ReplayDir(m_ctxt, SVN_INVALID_REVNUM, path, copyfrom_path, copyfrom_revision);
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
		ReplayEditor(RevSyncCtxt* ctxt):m_ctxt(ctxt){}

		Directory* openRoot(svn_revnum_t base_revision)
		{
			cout << " root@" << base_revision << endl;
			return new ReplayDir(m_ctxt, base_revision, "/");
		}

		virtual void onFinish(svn_revnum_t rev, Properties& props)
		{
			std::string msg;
			std::string author;
			std::string date;
			for(Properties::iterator i = props.begin(); i != props.end(); ++i)
			{
				if(i->first == "svn:log")
					msg = i->second;
				else if(i->first == "svn:author")
					author = i->second;
				else if(i->first == "svn:date")
					date = i->second;
			}
			m_ctxt->Write(rev, msg, author, date);
		}

		RevSyncCtxt* m_ctxt;
	};

	class EditorMaker : public RangeReplay
	{
	public:
		EditorMaker(RevSyncCtxt* ctxt):m_ctxt(ctxt){}
		virtual Editor* makeEditor(svn_revnum_t rev, Properties& props)
		{
			cout << "** Fetching rev " << rev << endl;
			return new ReplayEditor(m_ctxt);
		}

		RevSyncCtxt* m_ctxt;
	};

	void Write(svn_revnum_t rev, const std::string& log, std::string author, const std::string& date)
	{
		Git::CTree tree(m_gitRepo, m_rootTree.Write(m_gitRepo));

		//Cache the root tree
		GitOids& rootOids			  = SetOids("", rev);
		GitOids& rootOidsLast		  = SetOids("", SVN_INVALID_REVNUM);
		rootOidsLast.m_oidContentTree = rootOids.m_oidContentTree = m_Tree_Content->m_oid;
		rootOidsLast.m_oidMetaTree	  = rootOids.m_oidMetaTree	  = m_Tree_Meta->m_oid;

		if(author.empty())
			author = "nobody";
		Git::CSignature sig(author.c_str(), (author + "@svn").c_str());


		std::ostringstream msg;
		msg << "Rev: " << rev << endl
			<< "Time: " << date << endl
			<< "Msg:" << endl << log << endl;

		Git::COids oids;
		if(!m_lastCommit.isNull())
			oids << m_lastCommit;
		//m_lastCommit = m_gitRepo.Commit((m_csBaseRefName + "/_svnmeta").c_str(), sig, sig, msg.str().c_str(), tree, oids);
		m_lastCommit = m_gitRepo.Commit(m_lastCommit.isNull() ? "HEAD" : SvnMetaRefName().c_str(), sig, sig, msg.str().c_str(), tree, m_gitRepo.ToCommits( oids));

		cout << "r" << rev << "=" << m_lastCommit << endl;

		if(oids.m_oids.empty())
		{
			//First commit done. Lets make a ref.
			CheckExistingRefs();
		}
	}
};

void SvnToGitSync(const wchar_t* gitRepoPath, const char* svnRepoUrl, const char* refBaseName)
{
	svn::Repo svnRepo(G_svnCtxt, svnRepoUrl);

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

	RevSyncCtxt ctxt(gitRepo, svnRepo, svnRepoUrl);
	//ctxt.CheckExistingRefs();

	ctxt.m_csBaseRefName = refBaseName;

	RevSyncCtxt::EditorMaker editorMaker(&ctxt);
	editorMaker.replay(&svnRepo, 1, svnRepo.head(), 1, true);


	cout << "Done." << endl;
}

void onFetch(CmdLine& cmdLine)
{
	Git::CRepo gitRepo;
	GitSvn::openCur(gitRepo);

	svn::Repo svnRepo;

	string remote = GitSvn::wtoa(cmdLine.next());
	if(remote.empty())
		remote = "svn";


	RevSyncCtxt ctxt(gitRepo, svnRepo, "");

	ctxt.m_csBaseRefName = remote;

	//Initialize
	std::string			svnPath;
	bool				bFirst = true;
	svn_revnum_t		LastRev = 0;
	Git::CCommitWalker	walker;
	walker.Init(gitRepo);
	walker.AddRev(gitRepo.GetRef(GitSvn::toRef(remote, GitSvn::eRT_meta).c_str()).Oid());

	for(;!walker.End(); ++walker)
	{
		if(ctxt.m_lastCommit.isNull())
			ctxt.m_lastCommit = walker.Curr();

		Git::CCommit commit(gitRepo, walker.Curr());
		istringstream msg(commit.Message());
		string line;
		string svnMsg;
		bool bSvnMsg = false;
		int rev = -1;
		while(getline(msg, line))
		{
			if(bSvnMsg)
				svnMsg += line;
			else if(strncmp(line.c_str(), "SvnPath: ", 9) == 0)
			{
				if(svnPath.empty())
					svnPath = line.c_str() + 9;				
			}
			else if(strncmp(line.c_str(), "Rev: ", 5) == 0)
			{
				rev = atoi(line.c_str() + 4);
				if(LastRev <= 0 && rev > 0)
				{
					LastRev = rev;
					cout << "Head is at r" << LastRev << ", " << ctxt.m_lastCommit << endl;
				}
				cout << "Initializing... " << rev << "  \r" << flush;
			}
			else if(strncmp(line.c_str(), "Msg:", 4) == 0)
				bSvnMsg = true;
		}
		if(rev >= 0)
		{
			GitOids& rootOids			= ctxt.SetOids("", rev);
			Git::CTree tree(gitRepo, commit.Tree());
			
			rootOids.m_oidContentTree	= tree.Entry("content").Oid();
			rootOids.m_oidMetaTree		= tree.Entry("meta").Oid();
			if(bFirst)
			{
				bFirst = false;
				ctxt.SetOids("", SVN_INVALID_REVNUM) = rootOids;
			}
		}
	}

	if(svnPath.empty())
		throw std::runtime_error("SVN path not set. Please specify with 'gitsvn setsvnpath <path>' command.");

	svnRepo.Open(G_svnCtxt, svnPath.c_str());

	gitRepo.BuildTreeNode(ctxt.m_rootTree, Git::CTree(gitRepo, Git::CCommit(gitRepo, ctxt.m_lastCommit).Tree()));
	ctxt.m_svnRepoUrl = svnPath;

	cout << "Initializing... " << "Done. Continuing from rev " << LastRev << "..." << endl;

	RevSyncCtxt::EditorMaker editorMaker(&ctxt);
	editorMaker.replay(&svnRepo, LastRev+1, svnRepo.head(), 1, true);

	cout << "Done." << endl;
}