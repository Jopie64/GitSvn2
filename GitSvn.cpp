// GitSvn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "GitCpp\Git.h"
#include <iostream>
#include "GitCpp\jstd\JStd.h"

#include <windows.h>
using namespace std;

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
}