#pragma once

#include "svn_string.h"
#include <map>
#include <string>

class CSvnProp
{
public:
	typedef std::map<std::string, std::string> MapProp;

	CSvnProp(void);
	virtual ~CSvnProp(void);

	void Read(const char* data, const char* dataEnd);
	const char* ReadSingle(const char* data, const char* dataEnd);

	void Write(std::ostream& os);

	void setCopyFrom(const char *path, svn_revnum_t rev);
	void changeProp(const char *name, const svn_string_t *value);
	

	std::string m_copyFromPath;
	svn_revnum_t m_copyFromRev;

	MapProp m_NameVal;
	
};
