#pragma once
#include "GitCpp\Git.h"
#undef strcasecmp
#undef strncasecmp
#include <iostream>
#include <sstream>
#include <map>
#include "GitCpp\jstd\JStd.h"
#include "SvnProp.h"


#include "svncpp\stream.hpp"
//#include "svncpp\client.hpp"
#include "svncpp\replay.hpp"

namespace GitSvn {

struct RunCtxt;

class PathRev
{
public:
	PathRev(const std::string& path, svn_revnum_t rev = -1);

	bool operator<(const PathRev& that)const;

	std::string m_path;
	svn_revnum_t m_rev;
};

class GitOids
{
public:
	//For when its a file
	Git::COid m_oidContent;
	Git::COid m_oidMeta; //This counts for as well as the file as for the directory

	//For when its a directory
	Git::COid m_oidContentTree;
	Git::COid m_oidMetaTree;
};



class CMapGitSvnRev
{
public:
	typedef std::map<PathRev, GitOids> MapPathRev_Oid;
	CMapGitSvnRev(RunCtxt* ctxt);
	~CMapGitSvnRev(void);

	GitOids& Get(const std::string& path, svn_revnum_t rev = SVN_INVALID_REVNUM, bool bMustExist = true);

	RunCtxt*		m_ctxt;
	MapPathRev_Oid	m_Map;
};


class PropertyFile
{
public:
	PropertyFile(RunCtxt* ctxt):m_ctxt(ctxt), m_bModified(false){}

	
	void changeProp(const char *name, const svn_string_t *value);
	void setCopyFrom(const char *path, svn_revnum_t rev);

	void MaybeInit();
	
	Git::COid Write();

	RunCtxt*		m_ctxt;
	bool			m_bModified;
	CSvnProp		m_Prop;
	Git::COid		m_Oid;
};

std::string  wtoa(const std::wstring& str);
std::wstring atow(const std::string& str);

enum eRefType
{
	eRT_meta
};

std::string  toRef(const std::string& remoteName, eRefType type);

void		 openCur(Git::CRepo& repo);

struct RunCtxt
{
	RunCtxt(Git::CRepo& gitRepo);

	Git::CRepo&		m_gitRepo;
	CMapGitSvnRev	m_mapRev;

};



}