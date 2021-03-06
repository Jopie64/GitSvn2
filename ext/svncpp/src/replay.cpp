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
:	m_TargetRevision(0)
{
	svn_delta_editor_t* ed = svn_delta_default_editor(pool());
	Attach(ed);
	callbacks::SetCallbacks(ed);
}

Editor::~Editor(void)
{
}


void Editor::replay(Repo* repo,
					svn_revnum_t revision,
					svn_revnum_t low_water_mark,
					svn_boolean_t send_deltas)
{
	
	ThrowIfError(svn_ra_replay(repo->GetInternalObj(), revision, low_water_mark, send_deltas, GetInternalObj(), this, pool()));
}


// *** RangeReplay
RangeReplay::RangeReplay()
{
}

RangeReplay::~RangeReplay()
{
}

namespace callbacks
{

svn_error_t *replay_revstart(
  svn_revnum_t revision,
  void *replay_baton,
  const svn_delta_editor_t **editor,
  void **edit_baton,
  apr_hash_t *rev_props,
  apr_pool_t *pool)
{
	RangeReplay* replay = (RangeReplay*)replay_baton;

	try
	{
		Properties props(rev_props);
		Editor* newEditor	= replay->makeEditor(revision, props);
		*edit_baton			= newEditor;
		*editor				= newEditor->GetInternalObj();
	}
	catch(svn::ClientException& e){ return e.detach(); }

	return NULL;
}

svn_error_t *replay_revfinish(
  svn_revnum_t revision,
  void *replay_baton,
  const svn_delta_editor_t *editor,
  void *edit_baton,
  apr_hash_t *rev_props,
  apr_pool_t *pool)
{
	RangeReplay* replay = (RangeReplay*)replay_baton;
	Editor* edit = (Editor*)edit_baton;

	try
	{
		Properties props(rev_props);
		edit->onFinish(revision, props);
	}
	catch(svn::ClientException& e){ delete edit; return e.detach(); }
	delete edit;
	return NULL;
}

}//namespace callbacks

void RangeReplay::replay(Repo* repo,
			svn_revnum_t revStart,
			svn_revnum_t revEnd,
			svn_revnum_t low_water_mark,
			svn_boolean_t send_deltas)
{
	Pool pool;
	ThrowIfError(svn_ra_replay_range(repo->GetInternalObj(), revStart, revEnd, low_water_mark, send_deltas, &callbacks::replay_revstart, &callbacks::replay_revfinish, this, pool));
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
			newDir->onInit();
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

	if(dir)
	{
		dir->onClose();
		delete dir;
	}

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
			newDir->m_new		= true;
			newDir->onInit();
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
			newDir->onInit();
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
			newFile->m_new		= true;
			newFile->onInit();
		}
		*file_baton = newFile;
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *open_file (const char *path,
						void *parent_baton,
						svn_revnum_t base_revision,
						apr_pool_t *result_pool,
						void **file_baton)
{
	Directory* parent = (Directory*)parent_baton;
	try
	{
		File* newFile = parent->openFile(path, base_revision);
		if(newFile)
		{
			newFile->m_name		= path;
			newFile->m_parent	= parent;
			newFile->m_editor	= parent->m_editor;
			newFile->onInit();
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


svn_error_t *apply_textdelta (void *file_baton,
							  const char *base_checksum,
							  apr_pool_t *result_pool,
							  svn_txdelta_window_handler_t *handler,
							  void **handler_baton)
{
	File* file = (File*)file_baton;
	try
	{
		ApplyDeltaHandler* dhandler	= file->applyDelta(base_checksum);
		if(dhandler)
		{
//			dhandler->m_file			= file;
			*handler					= &ApplyDeltaHandler::txdelta_window_handler;
		}
		*handler_baton				= dhandler;
	}
	catch(svn::ClientException& e){ return e.detach(); }

	return NULL;
}

svn_error_t *change_dir_prop (void *dir_baton,
							  const char *name,
							  const svn_string_t *value,
							  apr_pool_t *scratch_pool)
{
	Directory* dir = (Directory*)dir_baton;
	try
	{
		dir->changeProp(name, value);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

svn_error_t *change_file_prop (void *file_baton,
							   const char *name,
							   const svn_string_t *value,
							   apr_pool_t *scratch_pool)
{
	File* file = (File*)file_baton;
	try
	{
		file->changeProp(name, value);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}



#if 0

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
	ed->open_file			= &open_file;
	ed->close_file			= &close_file;
	ed->apply_textdelta		= &apply_textdelta;
	ed->change_dir_prop		= &change_dir_prop;
	ed->change_file_prop	= &change_file_prop;
}

}//callbacks
}//replay
}//svn