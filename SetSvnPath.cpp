#include "stdafx.h"
#include "CmdLine.h"
#include "GitCpp\Git.h"
#undef strcasecmp
#undef strncasecmp

void onSetSvnPath(int argc, wchar_t* argv[]);
static bool registered = CmdLine::Register(L"setsvnpath", &onSetSvnPath);

void onSetSvnPath(int argc, wchar_t* argv[])
{
	if(argc < 3)
		CmdLine::throwUsage(L"<new svn path> [remote name]");

	Git::CRepo gitRepo(L".");
	


}
