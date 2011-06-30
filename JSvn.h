#pragma once

#include "svncpp/exception.hpp"

namespace svn{

void ThrowIfError(svn_error_t* error);


} //namespace svn
