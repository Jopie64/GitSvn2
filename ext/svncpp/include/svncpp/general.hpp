#pragma once

#include "svncpp/exception.hpp"
#include "svncpp/pool.hpp"
#include <iostream>
#include <memory>

struct svn_stream_t;

namespace svn{

typedef std::tr1::shared_ptr<Pool> shared_pool;

void ThrowIfError(svn_error_t* error);


template<class T_GitObj >
class CLibSvnObjWrapper
{
public:
	CLibSvnObjWrapper():m_obj(NULL)								{}
	CLibSvnObjWrapper(T_GitObj* obj):m_obj(NULL)				{ Attach(obj); }
	virtual ~CLibSvnObjWrapper()								{ }

	void		Attach(T_GitObj* obj)							{ m_obj = obj; if(!m_pool) m_pool = shared_pool(new Pool); }
	bool		IsValid() const									{ return m_obj != NULL; }
	void		CheckValid() const								{ if(!IsValid()) throw ClientException("LibSvn object not valid"); }
	T_GitObj*	GetInternalObj() const							{ CheckValid(); return m_obj; }
	apr_pool_t*	pool(shared_pool pool = shared_pool())const		{ if(pool) m_pool = pool; if(!m_pool) m_pool = shared_pool(new Pool()); return m_pool->pool(); }

	typedef bool (CLibSvnObjWrapper::*T_IsValidFuncPtr)() const;
	operator T_IsValidFuncPtr () const {return IsValid() ? &CLibSvnObjWrapper::IsValid : NULL;}


protected:
	T_GitObj*			m_obj;
	mutable shared_pool	m_pool;
};

} //namespace svn
