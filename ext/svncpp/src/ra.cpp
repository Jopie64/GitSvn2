#include "svncpp/ra.hpp"
#include "svncpp/context.hpp"

namespace svn
{

class RaInit
{
public:
	RaInit(){ 
		ThrowIfError(svn_ra_initialize(m_pool));
	}

	Pool m_pool;
};

Repo::Repo(svn::Context* ctxt,
		   const char * repos_URL,
		   const char * uuid)
{
	static RaInit init;
	svn_ra_callbacks2_t* callbacks = NULL;
	svn_ra_session_t * ses = NULL;
	ThrowIfError(svn_ra_create_callbacks(&callbacks, pool()));
	callbacks->auth_baton = ctxt->ctx()->auth_baton;
	ThrowIfError(svn_ra_open3(&ses, repos_URL, uuid, callbacks, this, NULL, pool()));
	Attach(ses);
}

std::string Repo::getRoot() const
{
	Pool pool;
	const char* url;
	ThrowIfError(svn_ra_get_repos_root2(GetInternalObj(), &url, pool));
	std::string ret(url);
	return ret;
}

std::string Repo::getUuid() const
{
	Pool pool;
	const char* uuid;
	ThrowIfError(svn_ra_get_uuid2(GetInternalObj(), &uuid, pool));
	std::string ret(uuid);
	return ret;
}

void Repo::getFile(Stream & dst,
				   const Path & path,
				   const Revision & revision,
				   const Revision & peg_revision) throw(ClientException)
{
	Pool pool;
	svn_revnum_t fetchedRev = 0;
	apr_hash_t* props = NULL;
	ThrowIfError(svn_ra_get_file(GetInternalObj(), path.c_str(), revision, dst.GetInternalObj(), &fetchedRev, &props, pool));
}

} //namespace svn
