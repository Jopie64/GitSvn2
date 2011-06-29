// GitSvn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "GitCpp\Git.h"
#include <iostream>
#include "GitCpp\jstd\JStd.h"

#include <windows.h>
using namespace std;

#pragma comment(lib, "libsvncpp.lib")
void Test();

int _tmain(int argc, _TCHAR* argv[])
{

	cout << "Startup..." << endl;

	try
	{
		Test();

	}

	catch(exception& e)
	{
		cout << "Something wend wrong: " << e.what() << endl;
	}


	char c;
	cin >> c;
	return 0;
}

void Test()
{
	Git::CRepo repo;
	static const wchar_t* testRepoPath = L"D:/Test/gitsvn/gitpart/";
	
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
}