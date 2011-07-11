#include "svncpp/general.hpp"
#include "svncpp/url.hpp"
#include "svn_io.h"

namespace svn{

void ThrowIfError(svn_error_t* error)
{
	if(error == NULL)
		return;
	throw svn::ClientException(error);
}

} //namespace svn