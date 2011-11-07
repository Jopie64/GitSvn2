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

bool Register(const wchar_t* cmd, const FuncCmd& cmdFunc)
{
	Registrar::I().m_mapCmd[cmd] = cmdFunc;
	return true;
}

void Call(const wchar_t* cmd, int argc, wchar_t* argv[])
{
	if (FuncCmd& func = Registrar::I().m_mapCmd[cmd])
		func(argc, argv);
	else
		throw std::runtime_error(String::Format("Command '%s' not understood.", String::ToMult(cmd, 0).c_str()));

}


}
