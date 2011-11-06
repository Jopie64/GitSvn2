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
	typedef key												key_type;
	typedef value											right_type;
	typedef std::pair<const key_type*, right_type*>			value_type;

	class iterator : public std::iterator<std::forward_iterator_tag, value_type>
	{
	friend AprHash;
	public:
		iterator():m_ix(NULL){}

		bool operator==(const iterator& that) const { return m_ix == that.m_ix; }
		bool operator!=(const iterator& that) const { return m_ix != that.m_ix; }

		iterator& operator++(){ ensureValid(); m_ix = apr_hash_next(m_ix); init(); return *this; }
		iterator  operator++(int){ iterator next(*this); ++next; return next; }

		value_type& operator*(){ ensureValid(); return m_val; }
		value_type* operator->(){ ensureValid(); return &m_val; }

		void ensureValid()const { if(!isValid()) throw std::logic_error("Dont do this with an invalid iterator!"); }

		bool isValid() const { return !!m_ix; }

	//Only public because AprHashCpp::iterator (which is not derived from this) needs it
	//protected:
		virtual void		init()
		{
			if(!m_ix) return;
			const void* key=0;
			void* right=0;

			apr_hash_this(m_ix, &key, NULL, &right);
			m_val.first = (key_type*)key;
			m_val.second = (right_type*)right;
		}

		iterator(apr_hash_index_t* ix):m_ix(ix) { init(); }

		apr_hash_index_t*	m_ix;
		value_type			m_val;

	};

	AprHash(apr_hash_t* hash):m_hash(hash){}
	virtual ~AprHash(void){}

	iterator begin() { return apr_hash_first(m_pool, m_hash); }
	iterator end() { return iterator(); }
//	iterator find(key_type* key) { apr_hash_get(

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
	typedef typename AprHash::iterator		base_iterator;

	class iterator : public std::iterator<std::forward_iterator_tag, value_type>
	{
	friend AprHashCpp;
	public:
		iterator():m_cont(NULL){}
		iterator(base_iterator i, AprHashCpp* cont):m_baseIterator(i), m_cont(cont){ init(); }

		bool operator==(const iterator& that) const { return m_baseIterator == that.m_baseIterator; }
		bool operator!=(const iterator& that) const { return m_baseIterator != that.m_baseIterator; }

		iterator& operator++(){ ++m_baseIterator; init(); return *this; }
		iterator  operator++(int){ iterator next(*this); ++next; return next; }

		value_type& operator*(){ m_baseIterator.ensureValid(); return m_valcpp; }
		value_type* operator->(){ m_baseIterator.ensureValid(); return &m_valcpp; }

	private:
		iterator(AprHashCpp* cont):m_cont(cont){}
		virtual void init()
		{
			if(!m_baseIterator.isValid()) return;
			m_cont->toKeyCpp(m_valcpp.first, m_baseIterator.m_val.first);
			m_cont->toRightCpp(m_valcpp.second, m_baseIterator.m_val.second);
		}

		base_iterator m_baseIterator;
		AprHashCpp* m_cont;
		value_type m_valcpp;
	};

	AprHashCpp(apr_hash_t* hash):AprHash(hash){}

	iterator begin() { return iterator(AprHash::begin(), this); }
	iterator end() { return iterator(this); }
//	iterator find(const key_type& key) { orig_key_type* okey = 0; toOrigKey(okey, key); return iterator(AprHash::find(okey), this); }

//	virtual void toOrigKey(orig_key_type*& dest, const key_type& src) =0; //Destination needs to remain while src remains
	virtual void toKeyCpp(key_type& dest, const orig_key_type* src) =0;
	virtual void toRightCpp(right_type& dest, const orig_right_type* src) =0;

};

}
