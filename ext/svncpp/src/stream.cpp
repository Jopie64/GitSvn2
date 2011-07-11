#include "svncpp/stream.hpp"
#include "svncpp/url.hpp"
#include "svn_io.h"

namespace svn{

Stream::Stream()
: m_is(NULL), m_os(NULL)
{
	CreateSvnStream();
}

Stream::Stream(std::istream& stream)
: m_is(NULL), m_os(NULL)
{
	CreateSvnStream();
	Attach(stream);
}

Stream::Stream(std::ostream& stream)
: m_is(NULL), m_os(NULL)
{
	CreateSvnStream();
	Attach(stream);
}

Stream::Stream(std::iostream& stream)
: m_is(NULL), m_os(NULL)
{
	CreateSvnStream();
	Attach(stream);
}

Stream::~Stream()
{
	if(IsValid())
		svn_error_clear(svn_stream_close(GetInternalObj())); //Ignore errors because destructor may not throw.
}

void Stream::Attach(std::iostream& stream)
{
	Attach(static_cast<std::istream&>(stream));
	Attach(static_cast<std::ostream&>(stream));
}

void Stream::Attach(std::istream& stream)
{
	m_is = &stream;
	svn_stream_set_read(GetInternalObj(), &Stream::OnSvnRead);
}

void Stream::Attach(std::ostream& stream)
{
	m_os = &stream;
	svn_stream_set_write(GetInternalObj(), &Stream::OnSvnWrite);
}

void Stream::CreateSvnStream()
{
	CLibSvnObjWrapper::Attach(svn_stream_create(this, pool()));
}

svn_error_t* Stream::OnSvnRead(void *baton,
							  char *buffer,
							  apr_size_t *len)
{
	Stream* thisP = (Stream*)baton;
	if(thisP->m_is == NULL)
		return svn_error_createf(SVN_ERR_IO_WRITE_ERROR, NULL, "Input stream not defined.");
	try
	{
		*len = thisP->m_is->read(buffer, *len).gcount();
	}
	catch(ClientException& e)
	{
		return e.detach();
	}
	catch(std::exception& e)
	{
		return svn_error_createf(SVN_ERR_IO_WRITE_ERROR, NULL, "%s", e.what());
	}
	return NULL;
}

svn_error_t* Stream::OnSvnWrite(void *baton,
							   const char *data,
							   apr_size_t *len)
{
	Stream* thisP = (Stream*)baton;
	if(thisP->m_os == NULL)
		return svn_error_createf(SVN_ERR_IO_WRITE_ERROR, NULL, "Output stream not defined.");
	try
	{
		thisP->m_os->write(data, *len);
	}
	catch(ClientException& e)
	{
		return e.detach();
	}
	catch(std::exception& e)
	{
		return svn_error_createf(SVN_ERR_IO_WRITE_ERROR, NULL, "%s", e.what());
	}
	return NULL;
}


} //namespace svn