#include "StdAfx.h"
#include "GitSvnAux.h"

namespace GitSvn {

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
	if(i != m_Map.end())
		return i->second;

	i = m_Map.find(PathRev("", rev));
	if(i == m_Map.end())
		throw svn::Exception(JStd::String::Format("Cannot find GIT oids for %s@%d", path.c_str(), rev).c_str());

	GitOids oids;
	Git::CTreeEntry contentEntry	= m_ctxt->m_gitRepo.TreeFind(i->second.m_oidContentTree, path.c_str());
	Git::CTreeEntry metaEntry		= m_ctxt->m_gitRepo.TreeFind(i->second.m_oidMetaTree,	 path.c_str());

	if(metaEntry.IsFile())
	{
		oids.m_oidContent			= contentEntry.Oid();
		oids.m_oidMeta				= metaEntry.Oid();
	}
	else
	{
		if(contentEntry.IsValid())
			oids.m_oidContentTree	= contentEntry.Oid();
		oids.m_oidMetaTree			= metaEntry.Oid();
		oids.m_oidMeta				= m_ctxt->m_gitRepo.TreeFind(oids.m_oidMetaTree,		 ".svnDirectoryProps").Oid();
	}
	return m_Map[PathRev(path,rev)]= oids;
}


void PropertyFile::MaybeInit()
{
	if(m_bModified || m_Oid.isNull())
		return; //Already initialized or dont need to

	//Was not read yet and an original was known. So read it first.
	Git::CBlob blob;
	m_ctxt->m_gitRepo.Read(blob, m_Oid);
	const char* content = (const char*)blob.Content();
	m_Prop.Read(content, content + blob.Size());
}

void PropertyFile::setCopyFrom(const char *path, const svn_revnum_t rev)
{
	MaybeInit();
	m_bModified = true;
	m_Prop.setCopyFrom(path, rev);
}

void PropertyFile::changeProp(const char *name, const svn_string_t *value)
{
	MaybeInit();
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

std::string wtoa(const std::wstring& str)
{
	return JStd::String::ToMult(str, CP_ACP);
}

std::wstring atow(const std::string& str)
{
	return JStd::String::ToWide(str, CP_ACP);
}

std::string toRef(const std::string& remoteName, eRefType type)
{
	switch(type)
	{
	case eRT_meta:	return JStd::String::Format("refs/remotes/%s/_svnmeta", remoteName.c_str());
	}
	throw std::logic_error("toRef() unknown ref type");
}

void openCur(Git::CRepo& repo)
{
	repo.Open(Git::CRepo::DiscoverPath(L".").c_str());
}


#pragma warning(disable: 4355) //This used in member initializer list
RunCtxt::RunCtxt(Git::CRepo& gitRepo)
:	m_gitRepo(gitRepo),
	m_mapRev(this)
{
}

}