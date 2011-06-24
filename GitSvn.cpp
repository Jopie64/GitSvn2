// GitSvn.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>

int _tmain(int argc, _TCHAR* argv[])
{
	HMODULE libsvn = LoadLibrary(L"C:\\Program Files\\Subversion\\libsvn_wc-1.dll");
	return 0;
}

