#pragma once
#include "svn_ra.h"
#include "svncpp/general.hpp"

namespace svn
{

class Repo : public CLibSvnObjWrapper<svn_ra_session_t>
{
public:
	Repo(const char * repos_URL,
		 const char * uuid = NULL);

};


} //namespace svn
