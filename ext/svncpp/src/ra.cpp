#include "svncpp/ra.hpp"

namespace svn
{
static svn_error_t* racb_get_wc_prop(void *baton,
                                          const char *relpath,
                                          const char *name,
                                          const svn_string_t **value,
                                          apr_pool_t *pool)
{
	return NULL;
}

static svn_error_t* racb_set_wc_prop(void *baton,
                                          const char *path,
                                          const char *name,
                                          const svn_string_t *value,
                                          apr_pool_t *pool)
{
	return NULL;
}


static svn_error_t* racb_push_wc_prop(void *baton,
                                           const char *path,
                                           const char *name,
                                           const svn_string_t *value,
                                           apr_pool_t *pool)
{
	return NULL;
}


static svn_error_t* racb_invalidate_wc_props(void *baton,
                                                  const char *path,
                                                  const char *name,
                                                  apr_pool_t *pool)
{
	return NULL;
}


void racb_progress_notify(apr_off_t progress,
                                      apr_off_t total,
                                      void *baton,
                                      apr_pool_t *pool)
{
}


static svn_ra_callbacks2_t initCallbacks()
{
	svn_ra_callbacks2_t cb;
	memset(&cb, 0, sizeof(cb));

	cb.get_wc_prop			= &racb_get_wc_prop;
	cb.set_wc_prop			= &racb_set_wc_prop;
	cb.push_wc_prop			= &racb_push_wc_prop;
	cb.invalidate_wc_props	= &racb_invalidate_wc_props;
	cb.progress_func		= &racb_progress_notify;

	return cb;
}

class RaInit
{
public:
	RaInit(){ 
		ThrowIfError(svn_ra_initialize(m_pool));
	}

	Pool m_pool;
};

Repo::Repo(const char * repos_URL,
		   const char * uuid)
{
	static RaInit init;
	static svn_ra_callbacks2_t callbacks = initCallbacks();
	svn_ra_session_t * ses = NULL;
	ThrowIfError(svn_ra_open3(&ses, repos_URL, uuid, &callbacks, this, NULL, pool()));
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
