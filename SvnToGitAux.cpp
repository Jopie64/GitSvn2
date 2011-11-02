#include "StdAfx.h"
#include "SvnToGitAux.h"

namespace SvnToGit {

PathRev::PathRev(const std::string& path, svn_revnum_t rev)
:	m_path(path),m_rev(rev)
{
	while(*m_path.c_str() == '/')
		m_path = m_path.substr(1);
}

bool PathRev::operator<(const PathRev& that)const
{
	if(m_rev < that.m_rev) return true;
	if(m_rev > that.m_rev) return false;
	return m_path < that.m_path;
}

CMapGitSvnRev::CMapGitSvnRev(RunCtxt* ctxt)
:	m_ctxt(ctxt)
{
}

CMapGitSvnRev::~CMapGitSvnRev(void)
{
}

GitOids& CMapGitSvnRev::Get(const std::string& path, svn_revnum_t rev, bool bMustExist)
{
	if(!bMustExist)
		return m_Map[PathRev(path, rev)];

	MapPathRev_Oid::iterator i = m_Map.find(PathRev(path, rev));
	if(i == m_Map.end())
		throw svn::Exception(JStd::String::Format("Cannot find GIT blob for %s@%d", path.c_str(), rev).c_str());
	return i->second;
}

void PropertyFile::changeProp(const char *name, const svn_string_t *value)
{
	if(!m_bModified && !m_Oid.isNull())
	{
		//Was not read yet and an original was known. So read it first.
		Git::CBlob blob;
		m_ctxt->m_gitRepo.Read(blob, m_Oid);
		const char* content = (const char*)blob.Content();
		m_Prop.Read(content, content + blob.Size());
	}

	m_bModified = true;
	m_Prop.changeProp(name, value);
}

Git::COid PropertyFile::Write()
{
	if(!m_bModified && !m_Oid.isNull())
		return m_Oid;
	std::ostringstream os;
	m_Prop.Write(os);
	std::string newContent = os.str();

	m_Oid = m_ctxt->m_gitRepo.WriteBlob(newContent.data(), newContent.size());
	return m_Oid;
}

#pragma warning(disable: 4355) //This used in member initializer list
RunCtxt::RunCtxt(Git::CRepo& gitRepo)
:	m_gitRepo(gitRepo),
	m_mapRev(this)
{
}

}