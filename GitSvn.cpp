// GitSvn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "GitCpp\Git.h"
#include <iostream>
#include "GitCpp\jstd\JStd.h"
#include "GitCpp\jstd\CmdLine.h"


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

int _tmain(int argc, _TCHAR* argv[])
{

	cout << "Startup..." << endl;

	try
	{
		G_svnCtxt = new svn::Context();

		//Test();
		JStd::CmdLine::CallDefaultWithCatch(wcout, argc, argv);
	}

	catch(JStd::CmdLine::CUsageException& e)
	{
		if(argc > 1)
			wcout << L"Usage: " << argv[0] << L" " << argv[1];
		else if(argc > 0)
			wcout << L"Usage: " << argv[0];

		wcout << L" " << e.m_usage << endl;
	}
	catch(Git::CGitException& e)
	{
		cout << "Git exception: " << e.what() << endl;
	}
	catch(svn::Exception& e)
	{
		cout << "Subversion exception: " << e.message() << endl;
	}
	catch(exception& e)
	{
		cout << "General exception: " << e.what() << endl;
	}

	delete G_svnCtxt;

#ifdef _DEBUG
	char c;
	cin >> c;
#endif
	return 0;
}

void Test()
{
	Git::CRepo repo;
	static const wchar_t* testRepoPath = L"D:/Test/gitsvn/gitpart/";
	//const char* svnrepo = "svn://jdmstorage.jdm1.maassluis/johan/";
	const char* svnrepo = "svn://jdmstorage.jdm1.maassluis/main/";
	//const char* svnrepo = "file:///d:/Develop/test/gitsvnbug/svnrepo";
	//const char* svnrepo = "http://gwt-multipage.googlecode.com/svn";
	//const char* svnrepo = "file:///H://Temp//marco";

	SvnToGitSync(testRepoPath, svnrepo, "svn");

	return;

}