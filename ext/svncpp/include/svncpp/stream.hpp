#pragma once

#include "svncpp/exception.hpp"
#include "svncpp/pool.hpp"
#include "svncpp/general.hpp"
#include <iostream>

struct svn_stream_t;

namespace svn{

class Stream : public CLibSvnObjWrapper<svn_stream_t>
{
public:
	Stream();
	Stream(std::iostream& stream);
	Stream(std::istream& stream);
	Stream(std::ostream& stream);
	virtual ~Stream();

	void Attach(std::iostream& stream);
	void Attach(std::istream& stream);
	void Attach(std::ostream& stream);

private:

	void CreateSvnStream();
	static svn_error_t* OnSvnRead(void *baton,
								  char *buffer,
								  apr_size_t *len);

	static svn_error_t* OnSvnWrite(void *baton,
								   const char *data,
								   apr_size_t *len);

	std::istream* m_is;
	std::ostream* m_os;

};

} //namespace svn
