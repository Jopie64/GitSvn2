#include "svncpp/ra.hpp"

namespace svn
{

Repo::Repo(const char * repos_URL,
		   const char * uuid)
{
	svn_ra_session_t * ses = NULL;
	ThrowIfError(svn_ra_open3(&ses, repos_URL, uuid, NULL, this, NULL, pool()));
	Attach(ses);
}

} //namespace svn
