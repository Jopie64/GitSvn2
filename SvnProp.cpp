#include "StdAfx.h"
#include "SvnProp.h"

CSvnProp::CSvnProp(void)
{
}

CSvnProp::~CSvnProp(void)
{
}

static bool IsEnd(char c)
{
	return c == '\04';
}

static bool IsSkip(char c)
{
	if(c == '\r' || c == '\n' || IsEnd(c))
		return true;
	return false;
}


static void Skip(const char** data, const char* dataEnd)
{
	while(*data != dataEnd && IsSkip(**data)) ++*data;
}

const char* CSvnProp::ReadSingle(const char* data, const char* dataEnd)
{
	//name
	std::string name;
	for(;data != dataEnd && !IsSkip(*data); ++data)
		name += *data;
	Skip(&data, dataEnd);
	
	std::string value;
	for(;data != dataEnd && !IsEnd(*data); ++data)
		value += *data;
	Skip(&data, dataEnd);

	m_NameVal[name].swap(value);
	return data;
}

void CSvnProp::Read(const char* data, const char* dataEnd)
{
	while(data != dataEnd)
		data = ReadSingle(data, dataEnd);
}

void CSvnProp::changeProp(const char *name, const svn_string_t *value)
{
	if(value)
		m_NameVal[name].swap(std::string(value->data, 0, value->len));
	else
		m_NameVal.erase(name);
}

void CSvnProp::Write(std::ostream& os)
{
	for(MapProp::iterator i = m_NameVal.begin(); i != m_NameVal.end(); ++i)
		os << i->first << std::endl << i->second << '\04' << std::endl;
}
