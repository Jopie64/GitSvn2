#include "svncpp\repos.hpp"
#include "svn_repos.h"

namespace svn
{

namespace dump
{

namespace callbacks
{
/** The parser has discovered a new revision record within the
* parsing session represented by @a parse_baton.  All the headers are
* placed in @a headers (allocated in @a pool), which maps <tt>const
* char *</tt> header-name ==> <tt>const char *</tt> header-value.
* The @a revision_baton received back (also allocated in @a pool)
* represents the revision.
*/
svn_error_t *new_revision_record(void **revision_baton,
                                  apr_hash_t *headers,
                                  void *parse_baton,
                                  apr_pool_t *pool)
{
	Parser* parser = (Parser*)parse_baton;
	try
	{
		*revision_baton = parser->onNewRevision(Properties(headers));
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** The parser has discovered a new uuid record within the parsing
* session represented by @a parse_baton.  The uuid's value is
* @a uuid, and it is allocated in @a pool.
*/
svn_error_t *uuid_record(const char *uuid,
                          void *parse_baton,
                          apr_pool_t *pool)
{
	Parser* parser = (Parser*)parse_baton;
	try
	{
		parser->onUuid(uuid);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** The parser has discovered a new node record within the current
* revision represented by @a revision_baton.  All the headers are
* placed in @a headers (as with @c new_revision_record), allocated in
* @a pool.  The @a node_baton received back is allocated in @a pool
* and represents the node.
*/
svn_error_t *new_node_record(void **node_baton,
                              apr_hash_t *headers,
                              void *revision_baton,
                              apr_pool_t *pool)
{
	Revision* rev = (Revision*)revision_baton;
	if(!rev) return NULL;
	try
	{
		*node_baton = rev->onNewNode(Properties(headers));
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a revision_baton, set a property @a name to @a value. */
svn_error_t *set_revision_property(void *revision_baton,
                                    const char *name,
                                    const svn_string_t *value)
{
	Revision* rev = (Revision*)revision_baton;
	if(!rev) return NULL;
	try
	{
		rev->onSetProp(name, value ? std::string(value->data, 0, value->len).c_str() : NULL);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a node_baton, set a property @a name to @a value. */
svn_error_t *set_node_property(void *node_baton,
                                const char *name,
                                const svn_string_t *value)
{
	Node* node = (Node*)node_baton;
	if(!node) return NULL;
	try
	{
		node->onSetProp(name, value ? std::string(value->data, 0, value->len).c_str() : NULL);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a node_baton, delete property @a name. */
svn_error_t *delete_node_property(void *node_baton, const char *name)
{
	Node* node = (Node*)node_baton;
	try
	{
		node->onDelProp(name);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a node_baton, remove all properties. */
svn_error_t *remove_node_props(void *node_baton)
{
	Node* node = (Node*)node_baton;
	try
	{
		node->onDelAllProps();
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a node_baton, receive a writable @a stream capable of
* receiving the node's fulltext.  After writing the fulltext, call
* the stream's close() function.
*
* If a @c NULL is returned instead of a stream, the vtable is
* indicating that no text is desired, and the parser will not
* attempt to send it.
*/
svn_error_t *set_fulltext(svn_stream_t **stream,
                           void *node_baton)
{
	Node* node = (Node*)node_baton;
	try
	{
		Stream* myStream = node->getStream();
		*stream = myStream ? myStream->GetInternalObj() : NULL;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** For a given @a node_baton, set @a handler and @a handler_baton
* to a window handler and baton capable of receiving a delta
* against the node's previous contents.  A NULL window will be
* sent to the handler after all the windows are sent.
*
* If a @c NULL is returned instead of a handler, the vtable is
* indicating that no delta is desired, and the parser will not
* attempt to send it.
*/
svn_error_t *apply_textdelta(svn_txdelta_window_handler_t *handler,
                              void **handler_baton,
                              void *node_baton)
{
	Node* node = (Node*)node_baton;
	try
	{
		delta::ApplyDeltaHandler* dhandler = node->applyDelta();
		if(dhandler)
		{
			*handler		= &delta::ApplyDeltaHandler::txdelta_window_handler;
			*handler_baton	= dhandler;
		}
		else
			*handler = NULL;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** The parser has reached the end of the current node represented by
* @a node_baton, it can be freed.
*/
svn_error_t *close_node(void *node_baton)
{
	Node* node = (Node*)node_baton;
	try
	{
		delete node;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

/** The parser has reached the end of the current revision
* represented by @a revision_baton.  In other words, there are no more
* changed nodes within the revision.  The baton can be freed.
*/
svn_error_t *close_revision(void *revision_baton)
{
	Revision* rev = (Revision*)revision_baton;
	try
	{
		delete rev;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}


}// callbacks


void Parser::parse(Stream& str)
{
	svn_repos_parse_fns2_t parse; memset(&parse, 0, sizeof(parse));
	using namespace callbacks;
	parse.new_revision_record	= &new_revision_record;
	parse.uuid_record			= &uuid_record;
	parse.new_node_record		= &new_node_record;
	parse.set_revision_property = &set_revision_property;
	parse.set_node_property		= &set_node_property;
	parse.delete_node_property	= &delete_node_property;
	parse.remove_node_props		= &remove_node_props;
	parse.set_fulltext			= &set_fulltext;
	parse.apply_textdelta		= &apply_textdelta;
	parse.close_node			= &close_node;
	parse.close_revision		= &close_revision;

	Pool pool;

	ThrowIfError(svn_repos_parse_dumpstream2(str.GetInternalObj(), &parse, this, NULL, NULL, pool));
}


}//dump
}//svn