#pragma once

#include "apr/apr_hash.h"
#include "general.hpp"
#include <iterator>

namespace svn
{

template<class key, class value>
class AprHash
{
public:
	typedef key									key_type;
	typedef value								right_type;
	typedef std::pair<key_type*, right_type*>	value_type;

	class iterator : std::iterator<std::forward_iterator_tag, value_type>
	{
	friend AprHash;
	public:
		iterator():ix(NULL){}

		bool operator==(const iterator& that) const { return m_ix == that.m_ix; }
		bool operator!=(const iterator& that) const { return m_ix != that.m_ix; }

		iterator& operator++(){ ensureValid(); apr_hash_next(m_ix); init(); return *this; }
		iterator  operator++(int){ iterator next(*this); ++next; return next; }

		value_type& operator*(){ ensureValid(); return m_val; }
		value_type* operator->(){ ensureValid(); return &m_val; }

		void ensureValid()const { if(!m_ix) throw std::logic_error("Dont do this with an invalid iterator!"); }


	protected:
		iterator(apr_hash_index_t* ix):m_ix(ix) {}

		virtual void		init() { if(!m_ix) return; apr_hash_this(m_ix, &m_val.first, NULL, &m_val.second); }
		apr_hash_index_t*	m_ix;
		value_type			m_val;

	};

	AprHash(apr_hash_t* hash):m_hash(hash){}
	virtual ~AprHash(void){}

	iterator begin() { return apr_hash_first(m_pool, m_hash); }
	iterator end() { return iterator(); }

	apr_hash_t* m_hash;
	Pool		m_pool;
	
};

//AprHashCpp gets key and value as CPP type
template<class key, class value, class keycpp, class valuecpp>
class AprHashCpp: public AprHash<key, value>
{
public:
	typedef key								orig_key_type;
	typedef value							orig_right_type;
	typedef keycpp							key_type;
	typedef valuecpp						right_type;
	typedef std::pair<keycpp, right_type>	value_type;

	class iterator : public AprHash::iterator
	{
	public:
		iterator(apr_hash_index_t* ix, AprHashCpp* cont):AprHash::iterator(ix), m_cont(cont){}

		value_type& operator*(){ ensureValid(); return m_valcpp; }
		value_type* operator->(){ ensureValid(); return &m_valcpp; }

	private:
		virtual void init()
		{
			if(!m_ix) return;
			AprHash::iterator::init();
			m_cont->toKeyCpp(m_valcpp.first, m_val.first);
			m_cont->ToValueCpp(m_valcpp.second, m_val.second);
		}

		AprHashCpp* m_cont;
		value_type m_valcpp;
	};

	AprHashCpp(apr_hash_t* hash):AprHash(hash){}

	virtual void toKeyCpp(key_type& dest, const orig_key_type* src) =0;
	virtual void toRightCpp(right_type& dest, const orig_right_type* src) =0;
};

}
