#include "StdAfx.h"
#include "SvnToGit.h"
#include "GitCpp\Git.h"
#include <iostream>
#include <sstream>
#include "GitCpp\jstd\JStd.h"

#undef strcasecmp
#undef strncasecmp

#include "svncpp\stream.hpp"
#include "svncpp\pool.hpp"
#include "svncpp\client.hpp"
using namespace std;

extern svn::Client* G_svnClient;

namespace Git
{

class CTreeNode
{
public:
	typedef std::list<CTreeNode> list_t;

	CTreeNode();
	CTreeNode(const std::string& name);
	virtual ~CTreeNode();

	CTreeNode*	GetByPath(const char* name);
	void		Insert(const char* name, COid oid, int attributes = 0100644);

	COid		Write(CRepo& repo);
	int			GetAttributes()const;

	std::string m_name;
	COid		m_oid;
	list_t		m_subTree; //when this one is not empty, its not a leaf
	int			m_attributes;
};

CTreeNode::CTreeNode()
:	m_attributes(-1)
{
}

CTreeNode::CTreeNode(const std::string& name)
:	m_attributes(-1), m_name(name)
{
}

CTreeNode::~CTreeNode()
{
}

CTreeNode* CTreeNode::GetByPath(const char* name)
{
	const char* nameStart = name;
	if(*nameStart == '/')
		++nameStart;
	const char* nameEnd = strchr(nameStart, '/');
	if(nameEnd == NULL)
		nameEnd = nameStart + strlen(nameStart);
	if(nameEnd == nameStart)
		//Empty name. Its this treenode.
		return this;
	std::string namePart = std::string(nameStart, nameEnd);
	list_t::iterator i;
	for(i = m_subTree.begin(); i != m_subTree.end(); ++i)
		if(i->m_name == namePart)
			break;
	if(i == m_subTree.end())
		i = m_subTree.insert(m_subTree.end(), namePart);
	return i->GetByPath(nameEnd);
}

void CTreeNode::Insert(const char* name, COid oid, int attributes)
{
	CTreeNode* node = GetByPath(name);
	node->m_oid			= oid;
	node->m_attributes	= attributes;
}

int CTreeNode::GetAttributes()const
{
	if(m_attributes >= 0)
		return m_attributes;
	if(m_subTree.empty())
		return 0100644;
	return 040000;
}

COid CTreeNode::Write(CRepo& repo)
{
	if(m_subTree.empty())
		return m_oid;
	CTreeBuilder treeB;
	for(list_t::iterator i = m_subTree.begin(); i != m_subTree.end(); ++i)
	{
		COid oid = i->Write(repo);
		if(oid.isNull())
			continue;
		treeB.Insert(JStd::String::ToWide(i->m_name, CP_UTF8).c_str(), oid, i->GetAttributes());
	}
	return repo.Write(treeB);
}



}

struct RevSyncCtxt
{
	typedef std::tr1::shared_ptr<Git::CTree> sharedTree;
	RevSyncCtxt(Git::CRepo& gitRepo, const std::string& svnRepoUrl):m_gitRepo(gitRepo), m_svnRepoUrl(svnRepoUrl)
	{
		m_Tree_Meta		= m_rootTree.GetByPath("meta");
		m_Tree_Content	= m_rootTree.GetByPath("content");
	}

	Git::CRepo& m_gitRepo;
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

	void OnSvnLogEntry(const svn::LogEntry& entry)
	{
		if(entry.revision == 0)
			return; //Skip rev 0..
		
		cout << "\rFetching rev " << entry.revision << "..." << flush;


		//Git::CTreeBuilder treeB(&*m_lastTree);
		for(std::list<svn::LogChangePathEntry>::const_iterator i = entry.changedPaths.begin(); i != entry.changedPaths.end(); ++i)
		{
			try
			{
				std::ostringstream os;
				G_svnClient->get(svn::Stream(os), m_svnRepoUrl + i->path, entry.revision);
				m_Tree_Content->Insert(i->path.c_str(), m_gitRepo.WriteBlob(os.str()));
			}
			catch(svn::ClientException& e)
			{
				if(e.apr_err() == SVN_ERR_CLIENT_IS_DIRECTORY)
					cout << "jay!" << endl;
			}
		}

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

	RevSyncCtxt ctxt(gitRepo,svnRepoUrl);
	//ctxt.CheckExistingRefs();

	ctxt.m_csBaseRefName = refBaseName;
	G_svnClient->log(std::tr1::bind(&RevSyncCtxt::OnSvnLogEntry, &ctxt, std::tr1::placeholders::_1),
					 svnRepoUrl,
					 svn::Revision::START,
					 svn::Revision::HEAD,
					 true);
	cout << "Done." << endl;
}