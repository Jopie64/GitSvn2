#include "stdafx.h"
#include "CmdLine.h"
#include "GitSvnAux.h"

void onSetSvnPath(int argc, wchar_t* argv[]);
static bool registered = CmdLine::Register(L"setsvnpath", &onSetSvnPath);

using namespace std;

void onSetSvnPath(int argc, wchar_t* argv[])
{
	if(argc < 3)
		CmdLine::throwUsage(L"<new svn path> [remote name]");

	wstring path = argv[2];
	string remote;
	if(argc > 3)
		remote = GitSvn::wtoa(argv[3]);
	else
		remote = "svn";

	remote = GitSvn::toRef(remote, GitSvn::eRT_meta);

	//Git::CRepo gitRepo(L".\");
	Git::CRepo gitRepo;
	GitSvn::openCur(gitRepo);

	Git::VectorCommit parents;
	parents.push_back(std::tr1::shared_ptr<Git::CCommit>(new Git::CCommit(gitRepo, gitRepo.GetRef(remote.c_str()).Oid())));
	Git::CTree tree(gitRepo, parents.front()->Tree());


	std::string commitMsg = GitSvn::wtoa(L"SvnPath: " + path);

	Git::CSignature sig(gitRepo);
	gitRepo.Commit(remote.c_str(), sig, sig, commitMsg.c_str(), tree, parents);
	cout << "SVN path set to " << GitSvn::wtoa(path) << endl;
}
