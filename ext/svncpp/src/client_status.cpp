/*
 * ====================================================================
 * Copyright (c) 2002-2009 The RapidSvn Group.  All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the file GPL.txt.  
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://rapidsvn.tigris.org/.
 * ====================================================================
 */
#if defined( _MSC_VER) && _MSC_VER <= 1200
#pragma warning( disable: 4786 )// debug symbol truncated
#endif

// Stdlib (for strcmp)
#include "string.h"

// Subversion api
#include "svn_client.h"
#include "svn_sorts.h"
//#include "svn_utf.h"

// svncpp
#include "svncpp/client.hpp"
#include "svncpp/dirent.hpp"
#include "svncpp/exception.hpp"
#include "svncpp/info.hpp"
#include "svncpp/pool.hpp"
#include "svncpp/status.hpp"
#include "svncpp/targets.hpp"
#include "svncpp/url.hpp"
#include "log_receiver.hpp"

namespace svn
{

  static void
  statusEntriesFunc(void *baton,
                    const char *path,
                    svn_wc_status2_t *status)
  {
    StatusEntries * entries = static_cast<StatusEntries *>(baton);

    entries->push_back(Status(path, status));
  }

  static StatusEntries
  localStatus(const char * path,
              const bool descend,
              const bool get_all,
              const bool update,
              const bool no_ignore,
              Context * context,
              const bool ignore_externals)
  {
    svn_error_t *error;
    StatusEntries entries;
    svn_revnum_t revnum;
    Revision rev(Revision::HEAD);
    Pool pool;

    error = svn_client_status2(
              &revnum,    // revnum
              path,       // path
              rev,        // revision
              statusEntriesFunc, // status func
              &entries,   // status baton
              descend,    // recurse
              get_all,
              update,     // need 'update' to be true to get repository lock info
              no_ignore,
              ignore_externals, // ignore_externals
              *context,   // client ctx
              pool);

    if (error!=NULL)
      throw ClientException(error);

    return entries;
  }

  static Status
  dirEntryToStatus(const char * path, const DirEntry & dirEntry)
  {
    Pool pool;

    svn_wc_entry_t * e =
      static_cast<svn_wc_entry_t *>(
        apr_pcalloc(pool, sizeof(svn_wc_entry_t)));

    std::string url(path);
    url += "/";
    url += dirEntry.name();

    e->name = dirEntry.name();
    e->revision = dirEntry.createdRev();
    e->url = url.c_str();
    e->kind = dirEntry.kind();
    e->schedule = svn_wc_schedule_normal;
    e->text_time = dirEntry.time();
    e->prop_time = dirEntry.time();
    e->cmt_rev = dirEntry.createdRev();
    e->cmt_date = dirEntry.time();
    e->cmt_author = dirEntry.lastAuthor();

    svn_wc_status2_t * s =
      static_cast<svn_wc_status2_t *>(
        apr_pcalloc(pool, sizeof(svn_wc_status2_t)));
    s->entry = e;
    s->text_status = svn_wc_status_normal;
    s->prop_status = svn_wc_status_normal;
    s->locked = 0;
    s->switched = 0;
    s->repos_text_status = svn_wc_status_normal;
    s->repos_prop_status = svn_wc_status_normal;

    return Status(url.c_str(), s);
  }


  static svn_revnum_t
  remoteStatus(Client * client,
               const char * path,
               const bool descend,
               StatusEntries & entries,
               Context * /*context*/)
  {
    Revision rev(Revision::HEAD);
    DirEntries dirEntries = client->list(path, rev, descend);
    DirEntries::const_iterator it;
    svn_revnum_t revnum = 0;

    for (it = dirEntries.begin(); it != dirEntries.end(); it++)
    {
      const DirEntry & dirEntry = *it;

      entries.push_back(dirEntryToStatus(path, dirEntry));
    }

    if (dirEntries.size() > 0)
      revnum = dirEntries[0].createdRev();


    return revnum;
  }

  StatusEntries
  Client::status(const char * path,
                 const bool descend,
                 const bool get_all,
                 const bool update,
                 const bool no_ignore,
                 const bool ignore_externals) throw(ClientException)
  {
    if (Url::isValid(path))
    {
      StatusEntries entries;
      remoteStatus(this, path, descend, entries, m_context);
      return entries;
    }
    else
      return localStatus(path, descend, get_all, update,
                         no_ignore, m_context, ignore_externals);
  }

  struct StatusFilter;

  struct StatusBaton
  {
public:
    const StatusFilter & filter;
    StatusEntries & entries;

    StatusBaton(const StatusFilter & filter_, StatusEntries & entries_)
        : filter(filter_), entries(entries_)
    {
    }
  };


  static void
  filteredStatusFunc(void *baton_,
                     const char *path,
                     svn_wc_status2_t *status)
  {
    StatusBaton * baton = static_cast<StatusBaton *>(baton_);

    // now we have to decide whether to return the entry or not
    if (0 == status)
      return;

    bool useStatus = false;

    bool isUnversioned = 0 == status->entry;
    if (isUnversioned)
    { // unversioned
      if (baton->filter.showUnversioned)
        useStatus = true;
    }
    else
    {
      bool isUnmodified =
        ((svn_wc_status_normal == status->text_status) &&
         (svn_wc_status_normal == status->prop_status));

      if (isUnmodified)
      {
        if (baton->filter.showUnmodified)
          useStatus = true;
      }
      else
      {
        // so here we know its modified.
        // what are we interested in?
        if (baton->filter.showModified)
          useStatus = true;
        else if (baton->filter.showConflicted)
        {
          if (svn_wc_status_conflicted == status->text_status)
            useStatus = true;
        }
      }
    }

    if (useStatus)
      baton->entries.push_back(Status(path, status));
  }

  static svn_revnum_t
  localFilteredStatus(const char * path,
                      const StatusFilter & filter,
                      const bool descend,
                      const bool update,
                      StatusEntries & entries,
                      Context * context)
  {
    svn_error_t *error;
    svn_revnum_t revnum;
    Revision rev(Revision::HEAD);
    Pool pool;
    StatusBaton baton(filter, entries);

    error = svn_client_status2(
              &revnum,    // revnum
              path,       // path
              rev,        // revision
              filteredStatusFunc, // status func
              &baton,   // status baton
              descend,    // recurse
              filter.showUnmodified,
              update,     // need 'update' to be true to get repository lock info
              filter.showIgnored, // no_ignores
              !filter.showExternals, // ignore_externals
              *context,   // client ctx
              pool);

    if (error!=NULL)
      throw ClientException(error);

    return revnum;
  }


  svn_revnum_t
  Client::status(const char * path,
                 const StatusFilter & filter,
                 const bool descend,
                 const bool update,
                 StatusEntries & entries) throw(ClientException)
  {
    entries.clear();

    if (Url::isValid(path))
      return remoteStatus(this, path, descend,
                          entries, m_context);
    else
    {
      // remote URLs only need a subset of the filters:
      // we dont expect any modified, conflicting, unknown,
      // ignored entries. And externals arent visible there anyhow
      return localFilteredStatus(
               path, filter, descend, update, entries, m_context);
    }
  }

  void addLogEntry(LogEntries* entries, const LogEntry& entry)
  {
	  entries->insert(entries->begin(), entry);
  }

  const LogEntries *
  Client::log(const char * path, const Revision & revisionStart,
              const Revision & revisionEnd, bool discoverChangedPaths,
              bool strictNodeHistory) throw(ClientException)
  {
    LogEntries * entries = new LogEntries();
	try
	{
		log(std::tr1::bind(&addLogEntry, entries, std::tr1::placeholders::_1),
			path, revisionStart, revisionEnd, discoverChangedPaths, strictNodeHistory);
	}
	catch(...)
	{
		delete entries;
		throw;
	}
	return entries;
  }


  void
  Client::log(const LogEntryCb& cb,
			  const char * path,
			  const Revision & revisionStart,
			  const Revision & revisionEnd,
			  bool discoverChangedPaths,
			  bool strictNodeHistory) throw(ClientException)
  {
    Pool pool;
    Targets target(path);
	LogReceiver recv;
	recv.m_cb = cb;
    int limit = 0;

    svn_error_t * error = svn_client_log2(
              target.array(pool),
              revisionStart.revision(),
              revisionEnd.revision(),
              limit,
              discoverChangedPaths ? 1 : 0,
              strictNodeHistory ? 1 : 0,
			  &LogReceiver::receive_s,
              &recv,
              *m_context, // client ctx
              pool);
	if(error)
		throw ClientException(error);
  }


  /**
   * callback function for Client::info, will be
   * called for every entry svn_client_info wants to
   * return
   */
  static svn_error_t *
  infoReceiverFunc(void * baton, const char * path,
                   const svn_info_t * info,
                   apr_pool_t * /*pool*/)
  {
    InfoVector * infoVector = static_cast<InfoVector *>(baton);

    infoVector->push_back(Info(path, info));

    return 0;
  }


  InfoVector
  Client::info(const Path & pathOrUrl,
               bool recurse,
               const Revision & revision,
               const Revision & pegRevision) throw(ClientException)
  {
    Pool pool;
    InfoVector infoVector;

    svn_error_t * error =
      svn_client_info(pathOrUrl.c_str(),
                      pegRevision.revision(),
                      revision.revision(),
                      infoReceiverFunc,
                      &infoVector,
                      recurse,
                      *m_context,
                      pool);

    if (error != 0)
      throw ClientException(error);

    return infoVector;
  }

}

/* -----------------------------------------------------------------
 * local variables:
 * eval: (load-file "../../rapidsvn-dev.el")
 * end:
 */
