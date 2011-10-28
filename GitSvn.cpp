// GitSvn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "GitCpp\Git.h"
#include <iostream>
#include "GitCpp\jstd\JStd.h"


#include <windows.h>

//Some libgit2 <-> svn collition
#undef strcasecmp
#undef strncasecmp

#include "svncpp\pool.hpp"
#include "svncpp\client.hpp"
using namespace std;

#include "SvnToGit.h"

#pragma comment(lib, "libsvncpp.lib")
#pragma comment(lib, "ext\\svn\\lib\\libsvn_client-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\libsvn_wc-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\libsvn_subr-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\libsvn_ra-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\libsvn_delta-1.lib")

//#pragma comment(lib, "ext\\svn\\lib\\libsvn_ra_local-1.lib")
//#pragma comment(lib, "ext\\svn\\lib\\libsvn_ra_svn-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\apr\\libapr-1.lib")
#pragma comment(lib, "ext\\svn\\lib\\apr-util\\libaprutil-1.lib")


void Test();

svn::Context* G_svnCtxt = NULL;
svn::Client* G_svnClient = NULL;

int _tmain(int argc, _TCHAR* argv[])
{

	cout << "Startup..." << endl;

	try
	{
		G_svnCtxt = new svn::Context();
		G_svnClient = new svn::Client(G_svnCtxt);

		Test();

	}

	catch(exception& e)
	{
		cout << "Something went wrong: " << e.what() << endl;
	}
	catch(svn::Exception& e)
	{
		cout << "Some subversion stuff went wrong: " << e.message() << endl;
	}

	delete G_svnClient;
	delete G_svnCtxt;

	char c;
	cin >> c;
	return 0;
}

void OnLogEntry(const svn::LogEntry& entry)
{


	cout << "\r[Rev: " << entry.revision << "]" << flush; //<< entry.message.substr(0,50) << flush;
}

void Test()
{
	Git::CRepo repo;
	static const wchar_t* testRepoPath = L"D:/Test/gitsvn/gitpart/";
	//const char* svnrepo = "svn://jdmstorage.jdm1.maassluis/johan";
	const char* svnrepo = "file:///d:/Develop/test/gitsvnbug/svnrepo";
	//const char* svnrepo = "http://gwt-multipage.googlecode.com/svn";

	SvnToGitSync(testRepoPath, svnrepo);

	return;
	
	try
	{
		repo.Open((wstring(testRepoPath) + L".git/").c_str());
		cout << "Opened git repository on " << JStd::String::ToMult(testRepoPath, CP_OEMCP) << endl;
	}
	catch(std::exception&)
	{
		cout << "Creating test git repository on " << JStd::String::ToMult(testRepoPath, CP_OEMCP) << "..." << endl;

		repo.Create(testRepoPath, false);
	}

	Git::CSignature sig("Johan", "johan@test.nl");

	Git::CTreeBuilder treeB;
	treeB.Insert(L"test1.txt", repo.WriteBlob("This is test file number 1...\n"));
	treeB.Insert(L"test2.txt", repo.WriteBlob("This is test file number 2...\n"));

	Git::COid oidTree = repo.Write(treeB);
	Git::COid oidCommit = repo.Commit("HEAD", sig, sig, "Test commit 1", oidTree, Git::COids());

	cout << "1st commit done: " << oidCommit << endl;

	Git::CTreeBuilder treeB2(Git::CTree(repo, oidTree));
	treeB2.Insert(L"test2.txt", repo.WriteBlob("This is test file number 2...\nA line has just been added to this file.\n"));
	oidTree = repo.Write(treeB2);
	oidCommit = repo.Commit("HEAD", sig, sig, "Test commit 2", oidTree, oidCommit);


	cout << "2nd commit done: " << oidCommit << endl;


	cout << "Doing svn stuff..." << endl;



	//svn::Pool svnPool;

	//svn::Client svnClient;
	//const char* svnrepo = "file:///D:/Develop/test/gitsvnbug/svnrepo";

#if 0
	const svn::LogEntries* entries = G_svnClient->log(svnrepo, svn::Revision::START, svn::Revision::HEAD, true);

	cout << "Log got " << entries->size() << " entries." << endl;
	for(svn::LogEntries::const_iterator i = entries->begin(); i != entries->end(); ++i)
	{
		cout << "Rev[" << i->revision << "] " << i->message << endl;
		for(std::list<svn::LogChangePathEntry>::const_iterator p = i->changedPaths.begin(); p != i->changedPaths.end(); ++p)
		{
			cout << " -" << p->action << ": " << p->path << endl;
		}
	}

	delete entries;
#endif

	G_svnClient->log(&OnLogEntry, svnrepo, svn::Revision::START, svn::Revision::HEAD);
	cout << endl << "Done." << endl;
}