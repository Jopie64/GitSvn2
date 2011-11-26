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
		MyNode(const char* path, LoadSvnDump* ctxt):m_path(path),m_ctxt(ctxt){}

		
		LoadSvnDump* m_ctxt;
		std::string m_path;
	};

	class MyFile : public MyNode
	{
	public:
		MyFile(const char* path, LoadSvnDump* ctxt):MyNode(path,ctxt){}
		~MyFile()
		{
		}
		virtual svn::Stream*					getStream() override
		{
			return NULL;
		}
		virtual svn::delta::ApplyDeltaHandler*	applyDelta() override
		{
			return NULL;
		}
	};

	class MyDir : public MyNode
	{
	public:
		MyDir(const char* path, LoadSvnDump* ctxt):MyNode(path,ctxt){}
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
		MyRevision(LoadSvnDump* ctxt):m_ctxt(ctxt){}
		LoadSvnDump* m_ctxt;
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
				return new MyDir(path.c_str(), m_ctxt);
			else
				return new MyFile(path.c_str(), m_ctxt);
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
		return new MyRevision(this);
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