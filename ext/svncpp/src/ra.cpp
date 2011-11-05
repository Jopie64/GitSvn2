#include "svncpp/ra.hpp"
#include "svncpp/context.hpp"
#include "svncpp/targets.hpp"
#include "log_receiver.hpp"

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
	//svn_revnum_t fetchedRev = 0;
	apr_hash_t* props = NULL;
	const char* fileName = path.c_str();
	while(*fileName == '/')
		++fileName;
	ThrowIfError(svn_ra_get_file(GetInternalObj(), fileName, revision, dst.GetInternalObj(), NULL, &props, pool));
	//ThrowIfError(svn_ra_get_file(GetInternalObj(), path.c_str(), revision, NULL, NULL, &props, pool));
	//ThrowIfError(svn_ra_get_file(GetInternalObj(), "trunk/file1.txt", revision, NULL, NULL, &props, pool));
}



void Repo::log(const LogEntryCb& cb,
		  const char * path,
		  svn_revnum_t revisionStart,
		  svn_revnum_t revisionEnd,
		  bool discoverChangedPaths,
		  bool strictNodeHistory) throw(ClientException)
{
	Pool pool;
	Targets target(path);
	LogReceiver recv;
	recv.m_cb = cb;
	int limit = 0;

	ThrowIfError(svn_ra_get_log(GetInternalObj(),
								 NULL,
								 revisionStart,
								 revisionEnd,
								 limit,
								 discoverChangedPaths ? 1 : 0,
								 strictNodeHistory ? 1 : 0,
								 &LogReceiver::receive_s,
								 &recv,
								 pool));
}

} //namespace svn
