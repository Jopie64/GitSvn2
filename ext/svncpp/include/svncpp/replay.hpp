#pragma once
#include "svncpp/ra.hpp"

namespace svn
{
namespace replay
{

class Editor;

class Directory
{
public:
	Directory(const char* name, Directory* parent, Editor* editor):m_parent(parent), m_name(name), m_editor(editor){}

	std::string	m_name;
	Directory*	m_parent;
	Editor*		m_editor;
};

class Editor : public CLibSvnObjWrapper<svn_delta_editor_t>, Uncopyable
{
public:
	Editor(void);
	virtual ~Editor(void);

private:

	static svn_error_t *set_target_revision(void *edit_baton,
											svn_revnum_t target_revision,
											apr_pool_t *scratch_pool);

	static svn_error_t *open_root(void *edit_baton,
								  svn_revnum_t base_revision,
								  apr_pool_t *result_pool,
								  void **root_baton);

	static svn_error_t *close_directory(void *dir_baton,
										apr_pool_t *scratch_pool);


#if 0
	/** Remove the directory entry named @a path, a child of the directory
	* represented by @a parent_baton.  If @a revision is a valid
	* revision number, it is used as a sanity check to ensure that you
	* are really removing the revision of @a path that you think you are.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*
	* @note The @a revision parameter is typically used only for
	* client->server commit-type operations, allowing the server to
	* verify that it is deleting what the client thinks it should be
	* deleting.  It only really makes sense in the opposite direction
	* (during server->client update-type operations) when the trees
	* whose delta is being described are ancestrally related (that is,
	* one tree is an ancestor of the other).
	*/
	svn_error_t *(*delete_entry)(const char *path,
							   svn_revnum_t revision,
							   void *parent_baton,
							   apr_pool_t *scratch_pool);


	/** We are going to add a new subdirectory named @a path.  We will use
	* the value this callback stores in @a *child_baton as the
	* @a parent_baton for further changes in the new subdirectory.
	*
	* If @a copyfrom_path is non-@c NULL, this add has history (i.e., is a
	* copy), and the origin of the copy may be recorded as
	* @a copyfrom_path under @a copyfrom_revision.
	*
	* Allocations for the returned @a child_baton should be performed in
	* @a result_pool. It is also typical to (possibly) save this pool for
	* later usage by @c close_directory.
	*/
	svn_error_t *(*add_directory)(const char *path,
								void *parent_baton,
								const char *copyfrom_path,
								svn_revnum_t copyfrom_revision,
								apr_pool_t *result_pool,
								void **child_baton);

	/** We are going to make changes in a subdirectory (of the directory
	* identified by @a parent_baton). The subdirectory is specified by
	* @a path. The callback must store a value in @a *child_baton that
	* should be used as the @a parent_baton for subsequent changes in this
	* subdirectory.  If a valid revnum, @a base_revision is the current
	* revision of the subdirectory.
	*
	* Allocations for the returned @a child_baton should be performed in
	* @a result_pool. It is also typical to (possibly) save this pool for
	* later usage by @c close_directory.
	*/
	svn_error_t *(*open_directory)(const char *path,
								 void *parent_baton,
								 svn_revnum_t base_revision,
								 apr_pool_t *result_pool,
								 void **child_baton);

	/** Change the value of a directory's property.
	* - @a dir_baton specifies the directory whose property should change.
	* - @a name is the name of the property to change.
	* - @a value is the new (final) value of the property, or @c NULL if the
	*   property should be removed altogether.
	*
	* The callback is guaranteed to be called exactly once for each property
	* whose value differs between the start and the end of the edit.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*change_dir_prop)(void *dir_baton,
								  const char *name,
								  const svn_string_t *value,
								  apr_pool_t *scratch_pool);


	/** In the directory represented by @a parent_baton, indicate that
	* @a path is present as a subdirectory in the edit source, but
	* cannot be conveyed to the edit consumer (perhaps because of
	* authorization restrictions).
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*absent_directory)(const char *path,
								   void *parent_baton,
								   apr_pool_t *scratch_pool);

	/** We are going to add a new file named @a path.  The callback can
	* store a baton for this new file in @a **file_baton; whatever value
	* it stores there should be passed through to @c apply_textdelta.
	*
	* If @a copyfrom_path is non-@c NULL, this add has history (i.e., is a
	* copy), and the origin of the copy may be recorded as
	* @a copyfrom_path under @a copyfrom_revision.
	*
	* Allocations for the returned @a file_baton should be performed in
	* @a result_pool. It is also typical to save this pool for later usage
	* by @c apply_textdelta and possibly @c close_file.
	*
	* @note Because the editor driver could be employing the "postfix
	* deltas" paradigm, @a result_pool could potentially be relatively
	* long-lived.  Every file baton created by the editor for a given
	* editor drive might be resident in memory similtaneously.  Editor
	* implementations should ideally keep their file batons as
	* conservative (memory-usage-wise) as possible, and use @a result_pool
	* only for those batons.  (Consider using a subpool of @a result_pool
	* for scratch work, destroying the subpool before exiting this
	* function's implementation.)
	*/
	svn_error_t *(*add_file)(const char *path,
						   void *parent_baton,
						   const char *copyfrom_path,
						   svn_revnum_t copyfrom_revision,
						   apr_pool_t *result_pool,
						   void **file_baton);

	/** We are going to make change to a file named @a path, which resides
	* in the directory identified by @a parent_baton.
	*
	* The callback can store a baton for this new file in @a **file_baton;
	* whatever value it stores there should be passed through to
	* @c apply_textdelta.  If a valid revnum, @a base_revision is the
	* current revision of the file.
	*
	* Allocations for the returned @a file_baton should be performed in
	* @a result_pool. It is also typical to save this pool for later usage
	* by @c apply_textdelta and possibly @c close_file.
	*
	* @note See note about memory usage on @a add_file, which also
	* applies here.
	*/
	svn_error_t *(*open_file)(const char *path,
							void *parent_baton,
							svn_revnum_t base_revision,
							apr_pool_t *result_pool,
							void **file_baton);

	/** Apply a text delta, yielding the new revision of a file.
	*
	* @a file_baton indicates the file we're creating or updating, and the
	* ancestor file on which it is based; it is the baton set by some
	* prior @c add_file or @c open_file callback.
	*
	* The callback should set @a *handler to a text delta window
	* handler; we will then call @a *handler on successive text
	* delta windows as we receive them.  The callback should set
	* @a *handler_baton to the value we should pass as the @a baton
	* argument to @a *handler. These values should be allocated within
	* @a result_pool.
	*
	* @a base_checksum is the hex MD5 digest for the base text against
	* which the delta is being applied; it is ignored if NULL, and may
	* be ignored even if not NULL.  If it is not ignored, it must match
	* the checksum of the base text against which svndiff data is being
	* applied; if it does not, @c apply_textdelta or the @a *handler call
	* which detects the mismatch will return the error
	* SVN_ERR_CHECKSUM_MISMATCH (if there is no base text, there may
	* still be an error if @a base_checksum is neither NULL nor the hex
	* MD5 checksum of the empty string).
	*/
	svn_error_t *(*apply_textdelta)(void *file_baton,
								  const char *base_checksum,
								  apr_pool_t *result_pool,
								  svn_txdelta_window_handler_t *handler,
								  void **handler_baton);

	/** Change the value of a file's property.
	* - @a file_baton specifies the file whose property should change.
	* - @a name is the name of the property to change.
	* - @a value is the new (final) value of the property, or @c NULL if the
	*   property should be removed altogether.
	*
	* The callback is guaranteed to be called exactly once for each property
	* whose value differs between the start and the end of the edit.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*change_file_prop)(void *file_baton,
								   const char *name,
								   const svn_string_t *value,
								   apr_pool_t *scratch_pool);

	/** We are done processing a file, whose baton is @a file_baton (set by
	* @c add_file or @c open_file).  We won't be using the baton any
	* more, so whatever resources it refers to may now be freed.
	*
	* @a text_checksum is the hex MD5 digest for the fulltext that
	* resulted from a delta application, see @c apply_textdelta.  The
	* checksum is ignored if NULL.  If not null, it is compared to the
	* checksum of the new fulltext, and the error
	* SVN_ERR_CHECKSUM_MISMATCH is returned if they do not match.  If
	* there is no new fulltext, @a text_checksum is ignored.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*close_file)(void *file_baton,
							 const char *text_checksum,
							 apr_pool_t *scratch_pool);

	/** In the directory represented by @a parent_baton, indicate that
	* @a path is present as a file in the edit source, but cannot be
	* conveyed to the edit consumer (perhaps because of authorization
	* restrictions).
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*absent_file)(const char *path,
							  void *parent_baton,
							  apr_pool_t *scratch_pool);

	/** All delta processing is done.  Call this, with the @a edit_baton for
	* the entire edit.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*close_edit)(void *edit_baton,
							 apr_pool_t *scratch_pool);

	/** The editor-driver has decided to bail out.  Allow the editor to
	* gracefully clean up things if it needs to.
	*
	* Any temporary allocations may be performed in @a scratch_pool.
	*/
	svn_error_t *(*abort_edit)(void *edit_baton,
							 apr_pool_t *scratch_pool);

#endif

	Revision m_TargetRevision;
};



}
}