#include "stdafx.h"
#include "gitcpp/jstd/CmdLine.h"
#include "GitSvnAux.h"

using JStd::CmdLine::CmdLine;

void onSetSvnPath(CmdLine& cmdLine);
static bool registered = JStd::CmdLine::Register(L"setsvnpath", &onSetSvnPath);

using namespace std;

void onSetSvnPath(CmdLine& cmdLine)
{
	std::wstring path = cmdLine.next();
	if(path.empty())
		JStd::CmdLine::throwUsage(L"<new svn path> [remote name]");

	string remote = GitSvn::wtoa(cmdLine.next());
	if(remote.empty())
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
