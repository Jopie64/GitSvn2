#pragma once

#include "svncpp/exception.hpp"
#include "svncpp/pool.hpp"
#include <iostream>

struct svn_stream_t;

namespace svn{

void ThrowIfError(svn_error_t* error);

} //namespace svn
