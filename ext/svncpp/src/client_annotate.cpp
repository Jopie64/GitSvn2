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

// Subversion api
#include "svn_client.h"

// svncpp
#include "svncpp/client.hpp"


namespace svn
{
  static svn_error_t *
  annotateReceiver(void *baton,
                   apr_int64_t line_no,
                   svn_revnum_t revision,
                   const char *author,
                   const char *date,
                   const char *line,
                   apr_pool_t * /*pool*/)
  {
    AnnotatedFile * entries = (AnnotatedFile *) baton;
    entries->push_back(
      AnnotateLine(line_no, revision,
                   author?author:"unknown",
                   date?date:"unknown date",
                   line?line:"???"));

    return NULL;
  }

  AnnotatedFile *
  Client::annotate(const Path & path,
                   const Revision & revisionStart,
                   const Revision & revisionEnd) throw(ClientException)
  {
    Pool pool;
    AnnotatedFile * entries = new AnnotatedFile;
    svn_error_t *error;
    error = svn_client_blame(
              path.c_str(),
              revisionStart.revision(),
              revisionEnd.revision(),
              annotateReceiver,
              entries,
              *m_context, // client ctx
              pool);

    if (error != NULL)
    {
      delete entries;
      throw ClientException(error);
    }

    return entries;
  }
}
