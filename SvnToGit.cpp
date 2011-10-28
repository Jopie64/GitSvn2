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
#include "svncpp\ra.hpp"
using namespace std;

extern svn::Client* G_svnClient;
extern svn::Context* G_svnCtxt;

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

	static svn_error_t* Replay_open_root(void *edit_baton,
                            svn_revnum_t base_revision,
                            apr_pool_t *result_pool,
                            void **root_baton)
	{
		cout << "Opening root! " << base_revision << endl;
		return NULL;
	}
	static svn_error_t *Replay_add_file
						  (const char *path,
						   void *parent_baton,
						   const char *copyfrom_path,
						   svn_revnum_t copyfrom_revision,
						   apr_pool_t *result_pool,
						   void **file_baton)
	{
		cout << "Yeah! Adding file " << path << "..." << endl;
		if(copyfrom_path)
			cout << "-- Copied from " << copyfrom_path << "@" << copyfrom_revision << endl;
		return NULL;
	}

	static svn_error_t *Replay_open_file(
							const char *path,
							void *parent_baton,
							svn_revnum_t base_revision,
							apr_pool_t *result_pool,
							void **file_baton)
	{
		cout << "Opening file " << path << "@" << base_revision << "..." << endl;
		*file_baton = (char*)path;
		return NULL;
	}

	static svn_error_t *Replay_delta_window(svn_txdelta_window_t *window, void *baton)
	{
		if(!window)
		{
			cout << "Data ended..." << endl;
			return NULL;
		}
		cout << "Received some delta!!" << endl;
		cout.write(window->new_data->data, window->new_data->len);
		return NULL;
	}

	static svn_error_t *Replay_apply_textdelta(
								  void *file_baton,
								  const char *base_checksum,
								  apr_pool_t *result_pool,
								  svn_txdelta_window_handler_t *handler,
								  void **handler_baton)
	{
		cout << "Applying text delta..." << endl;
		*handler = Replay_delta_window;
		return NULL;
	}

	void OnSvnLogEntry(const svn::LogEntry& entry)
	{
		if(entry.revision == 0)
			return; //Skip rev 0..

		std::ostringstream text;

		text << "\rFetching rev " << entry.revision;
		
		cout << text.str() << "..." << flush;

		svn::Pool pool;
		svn_delta_editor_t* editor	= svn_delta_default_editor(pool);
		editor->open_root			= Replay_open_root;
		editor->add_file			= Replay_add_file;
		editor->open_file			= Replay_open_file;
		editor->apply_textdelta		= Replay_apply_textdelta;

		svn_ra_replay(m_svnRepo.GetInternalObj(), entry.revision, 0, true, editor, (void*)10, pool);

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