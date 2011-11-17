#pragma once

#include "stream.hpp"
#include "apr_hash.hpp"

namespace svn
{

namespace dump
{

class Properties : public AprHashCpp<const char, const char, std::string, std::string>
{
public:
	Properties(apr_hash_t* hash):AprHashCpp(hash) {}

	virtual void toKeyCpp(key_type& dest, const orig_key_type* src) { dest = src; }
	virtual void toRightCpp(right_type& dest, const orig_right_type* src) { dest = src; }
};

class ObjWithProps
{
public:
	virtual void onSetProp(const char* name, const char* value){}
};

class Node : public ObjWithProps
{
public:

};

class Revision : public ObjWithProps
{
public:
	virtual Node* onNewNode(Properties& props) =0;

};

class Parser
{
public:
	void parse(Stream& str);


	virtual void		onUuid(const char* uuid){}
	virtual Revision*	onNewRevision(Properties& props){ return NULL; }

	
};

}

}