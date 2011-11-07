#pragma once
#include <functional>

namespace CmdLine
{

typedef std::tr1::function<void(int argc, const wchar_t* argv[])> FuncCmd;

void Register(const wchar_t* cmd, const FuncCmd& cmdFunc);

void Call(const wchar_t* cmd, int argc, const wchar_t* argv[]);


}