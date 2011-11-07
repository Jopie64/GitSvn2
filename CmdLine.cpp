#include "StdAfx.h"
#include "CmdLine.h"
#include "GitCpp\jstd\JStd.h"
#include <map>

namespace CmdLine
{

using namespace std;
using namespace JStd;

typedef map<wstring, FuncCmd> MapFunc;

struct Registrar: CSingleton<Registrar>
{
	MapFunc m_mapCmd;
};

void Register(const wchar_t* cmd, const FuncCmd& cmdFunc)
{
	Registrar::I().m_mapCmd[cmd] = cmdFunc;
}

void Call(const wchar_t* cmd, int argc, const wchar_t* argv[])
{
	if (FuncCmd& func = Registrar::I().m_mapCmd[cmd])
		func(argc, argv);
	else
		throw std::runtime_error(String::Format("Command '%s' not understood.", cmd));

}


}
