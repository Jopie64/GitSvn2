#include "StdAfx.h"
#include "LoadSvnDump.h"
#include "GitSvnAux.h"
#include "CmdLine.h"
#include "svncpp/repos.hpp"
#include <fstream>

#pragma comment(lib, "ext\\svn\\lib\\libsvn_repos-1.lib")

void onLoadSvnDump(int argc, wchar_t* argv[]);
static bool registered = CmdLine::Register(L"loadsvndump", &onLoadSvnDump);

using namespace std;
using namespace GitSvn;


struct LoadSvnDump : RunCtxt, svn::dump::Parser
{
	class MyNode : public svn::dump::Node
	{
	public:
	};

	static void showProps(const char* name, svn::dump::Properties& props)
	{
		cout << "----" << name << " props" << endl;
		for(svn::dump::Properties::iterator i = props.begin(); i != props.end(); ++i)
		{
			cout << i->first << ": " << i->second << endl;
		}
		cout << "----" << endl;
	}

	class MyRevision : public svn::dump::Revision
	{
	public:
		virtual svn::dump::Node* onNewNode(svn::dump::Properties& props)
		{
			//showProps("Node", props);
			for(svn::dump::Properties::iterator i = props.begin(); i != props.end(); ++i)
			{
				//cout << i->first << ": " << i->second << endl;
			}
			return new MyNode;
		}
	};

	LoadSvnDump(Git::CRepo& gitRepo)
	:	RunCtxt(gitRepo)
	{
	}

	virtual ~LoadSvnDump(void)
	{
	}

	svn::dump::Revision* onNewRevision(svn::dump::Properties& props)
	{
		//showProps("Revision", props);
		for(svn::dump::Properties::iterator i = props.begin(); i != props.end(); ++i)
		{
			//cout << i->first << ": " << i->second << endl;
			if(i->first == "Revision-number")
				cout << i->second << ",";
		}
		return new MyRevision;
	}
};





void onLoadSvnDump(int argc, wchar_t* argv[])
{
	if(argc < 3)
		CmdLine::throwUsage(L"[path_to_dumpfile]");
	Git::CRepo repo;
	GitSvn::openCur(repo);
	
	LoadSvnDump dumpLoader(repo);
	ifstream dumpFile(argv[2], ios::binary | ios::in);
	if(!dumpFile)
		throw runtime_error(JStd::String::Format("Cannot open dumpfile: %s", argv[2]));

	dumpLoader.parse(svn::Stream(dumpFile));
}