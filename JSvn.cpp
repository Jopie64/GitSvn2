#include "StdAfx.h"
#include "JSvn.h"
#include "svncpp/url.hpp"

namespace svn{

void ThrowIfError(svn_error_t* error)
{
	if(error == NULL)
		return;
	throw svn::ClientException(error);
}

} //namespace svn