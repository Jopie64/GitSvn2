#include "svncpp/log_entry.hpp"

namespace svn {

struct LogReceiver
{
  LogEntryCb m_cb;

  static svn_error_t *
  receive_s(void *baton,
			  apr_hash_t * changedPaths,
			  svn_revnum_t rev,
			  const char *author,
			  const char *date,
			  const char *msg,
			  apr_pool_t * pool)
  {
	  try
	  {
		  ((LogReceiver*)baton)->receive(changedPaths, rev, author, date, msg, pool);
	  }
	  catch(ClientException& e)
	  {
		  return e.detach();
	  }
	  return NULL;
  }

  static svn_error_t *
  receive_entry_s(void *baton,
                  svn_log_entry_t *e,
				  apr_pool_t *pool)
  {
	  try
	  {
		  //((LogReceiver*)baton)->receive(changedPaths, e->revision, e->revprops, date, msg, pool);
	  }
	  catch(ClientException& e)
	  {
		  return e.detach();
	  }
	  return NULL;
   }

  void
  receive(
			  apr_hash_t * changedPaths,
			  svn_revnum_t rev,
			  const char *author,
			  const char *date,
			  const char *msg,
			  apr_pool_t * pool)
  {
	LogEntry entry(rev, author, date, msg);

	if (changedPaths != NULL)
	{

	  for (apr_hash_index_t *hi = apr_hash_first(pool, changedPaths);
		   hi != NULL;
		   hi = apr_hash_next(hi))
	  {
		char *path;
		void *val;
		apr_hash_this(hi, (const void **)&path, NULL, &val);

		svn_log_changed_path_t *log_item = reinterpret_cast<svn_log_changed_path_t *>(val);

		entry.changedPaths.push_back(
		  LogChangePathEntry(path,
							 log_item->action,
							 log_item->copyfrom_path,
							 log_item->copyfrom_rev));
	  }
	}

	m_cb(entry);
  }

};

}//namespace svn