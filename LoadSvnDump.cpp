#include "StdAfx.h"
#include "LoadSvnDump.h"
#include "GitSvnAux.h"
#include "gitcpp/jstd/CmdLine.h"
#include "svncpp/repos.hpp"
#include <fstream>

#pragma comment(lib, "ext\\svn\\lib\\libsvn_repos-1.lib")

using JStd::CmdLine::CmdLine;

void onLoadSvnDump(CmdLine& cmdLine);
static bool registered = JStd::CmdLine::Register(L"loadsvndump", &onLoadSvnDump);

using namespace std;
using namespace GitSvn;


struct LoadSvnDump : RunCtxt, svn::dump::Parser
{
	class MyNode : public svn::dump::Node
	{
	public:
		MyNode(const char* path):m_path(path){}

		std::string m_path;
	};

	class MyFile : public MyNode
	{
	public:
		MyFile(const char* path):MyNode(path){}
		~MyFile()
		{
		}
	};

	class MyDir : public MyNode
	{
	public:
		MyDir(const char* path):MyNode(path){}
		~MyDir()
		{
		}
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
			showProps("Node", props);
			std::string path;
			bool isDir = false;
			for(svn::dump::Properties::iterator i = props.begin(); i != props.end(); ++i)
			{
				if(i->first == "Node-path")
					path = i->second;
				else if(i->first == "Node-kind")
					isDir = i->second == "dir";
				//cout << i->first << ": " << i->second << endl;
			}
			if(isDir)
				return new MyDir(path.c_str());
			else
				return new MyFile(path.c_str());
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





void onLoadSvnDump(CmdLine& cmdLine)
{
	std::wstring fileName = cmdLine.next();
	if(fileName.empty())
		JStd::CmdLine::throwUsage(L"[path_to_dumpfile]");
	Git::CRepo repo;
	GitSvn::openCur(repo);

	
	LoadSvnDump dumpLoader(repo);
	ifstream dumpFile(fileName.c_str(), ios::binary | ios::in);
	if(!dumpFile)
		throw runtime_error(JStd::String::Format("Cannot open dumpfile: %s", fileName.c_str()));

	dumpLoader.parse(svn::Stream(dumpFile));
}