#include "..\include\svncpp\replay.hpp"

namespace svn
{
namespace replay
{
namespace callbacks
{
void SetCallbacks(svn_delta_editor_t* ed);
}

Editor::Editor(void)
{
	svn_delta_editor_t* ed = svn_delta_default_editor(pool());
	Attach(ed);
	callbacks::SetCallbacks(ed);
}

Editor::~Editor(void)
{
}


void Editor::replay(Repo* repo,
					const Revision& revision,
					const Revision& low_water_mark,
					svn_boolean_t send_deltas)
{
	ThrowIfError(svn_ra_replay(repo->GetInternalObj(), revision, low_water_mark, send_deltas, GetInternalObj(), this, pool()));
}

namespace callbacks
{

svn_error_t *set_target_revision(void *edit_baton,
								 svn_revnum_t target_revision,
								 apr_pool_t *scratch_pool)
{
	Editor* pthis = (Editor*)edit_baton;
	try
	{
		pthis->m_TargetRevision = target_revision;
		pthis->onTargetRevision(target_revision);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *open_root(void *edit_baton,
					   svn_revnum_t base_revision,
					   apr_pool_t *result_pool,
					   void **root_baton)
{
	Editor* pthis = (Editor*)edit_baton;

	try
	{
		Directory* newDir = pthis->openRoot(base_revision);
		if(newDir)
		{
			newDir->m_editor	= pthis;
		}
		*root_baton = newDir;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *close_directory(void *dir_baton,
							 apr_pool_t *scratch_pool)
{
	Directory* dir = (Directory*)dir_baton;

	delete dir;

	return NULL;
}

svn_error_t *add_directory (const char *path,
							void *parent_baton,
							const char *copyfrom_path,
							svn_revnum_t copyfrom_revision,
							apr_pool_t *result_pool,
							void **child_baton)
{
	Directory* parent = (Directory*)parent_baton;
	try
	{
		Directory* newDir = parent->add(path, copyfrom_path, copyfrom_revision);
		if(newDir)
		{
			newDir->m_name		= path;
			newDir->m_parent	= parent;
			newDir->m_editor	= parent->m_editor;
		}
		*child_baton = newDir;
	}
	catch(svn::ClientException& e){ return e.detach(); }

	return NULL;
}

svn_error_t *open_directory (const char *path,
							 void *parent_baton,
							 svn_revnum_t base_revision,
							 apr_pool_t *result_pool,
							 void **child_baton)
{
	Directory* parent = (Directory*)parent_baton;
	try
	{
		Directory* newDir = parent->open(path, base_revision);
		if(newDir)
		{
			newDir->m_name		= path;
			newDir->m_parent	= parent;
			newDir->m_editor	= parent->m_editor;
		}
		*child_baton = newDir;
	}
	catch(svn::ClientException& e){ return e.detach(); }

	return NULL;
}

svn_error_t *delete_entry (const char *path,
						   svn_revnum_t revision,
						   void *parent_baton,
						   apr_pool_t *scratch_pool)
{
	Directory* parent = (Directory*)parent_baton;
	try
	{
		parent->deleteEntry(path, revision);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *add_file (const char *path,
					   void *parent_baton,
					   const char *copyfrom_path,
					   svn_revnum_t copyfrom_revision,
					   apr_pool_t *result_pool,
					   void **file_baton)
{
	Directory* parent = (Directory*)parent_baton;
	try
	{
		File* newFile = parent->addFile(path, copyfrom_path, copyfrom_revision);
		if(newFile)
		{
			newFile->m_name		= path;
			newFile->m_parent	= parent;
			newFile->m_editor	= parent->m_editor;
		}
		*file_baton = newFile;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *close_file (void *file_baton,
						 const char *text_checksum,
						 apr_pool_t *scratch_pool)
{
	File* file = (File*)file_baton;
	if(file)
		file->onClose(text_checksum);
	delete file;
	return NULL;
}
#if 0

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



void SetCallbacks(svn_delta_editor_t* ed)
{
	ed->set_target_revision	= &set_target_revision;
	ed->open_root			= &open_root;
	ed->close_directory		= &close_directory;
	ed->add_directory		= &add_directory;
	ed->open_directory		= &open_directory;
	ed->delete_entry		= &delete_entry;
	ed->add_file			= &add_file;
	ed->close_file			= &close_file;
}

}//callbacks
}//replay
}//svn