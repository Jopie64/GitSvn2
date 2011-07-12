#pragma once
#include "svn_ra.h"
#include "svncpp/general.hpp"
#include "svncpp/stream.hpp"
#include "svncpp/path.hpp"
#include "svncpp/revision.hpp"

namespace svn
{

class Repo : public CLibSvnObjWrapper<svn_ra_session_t>
{
public:
	Repo(class Context* ctxt,
		 const char * repos_URL,
		 const char * uuid = NULL);

	std::string getRoot() const;
	std::string getUuid() const;

    void getFile(Stream & dst,
				 const Path & path,
				 const Revision & revision,
				 const Revision & peg_revision = Revision::UNSPECIFIED) throw(ClientException);

};


} //namespace svn
