#include "..\include\svncpp\replay.hpp"

namespace svn
{
namespace replay
{

Editor::Editor(void)
{
	svn_delta_editor_t* ed = svn_delta_default_editor(pool());
	Attach(ed);
	ed->set_target_revision	= &Editor::set_target_revision;
	ed->open_root			= &Editor::open_root;
	ed->close_directory		= &Editor::close_directory;
}

Editor::~Editor(void)
{
}

svn_error_t *Editor::set_target_revision(void *edit_baton,
										svn_revnum_t target_revision,
										apr_pool_t *scratch_pool)
{
	Editor* pthis = (Editor*)edit_baton;
	pthis->m_TargetRevision = target_revision;
	return NULL;
}

svn_error_t *Editor::open_root(void *edit_baton,
							  svn_revnum_t base_revision,
							  apr_pool_t *result_pool,
							  void **root_baton)
{
	Editor* pthis = (Editor*)edit_baton;

	*root_baton = new Directory("", NULL, pthis);
	return NULL;
}

svn_error_t *Editor::close_directory(void *dir_baton,
									apr_pool_t *scratch_pool)
{
	Directory* dir = (Directory*)dir_baton;

	delete dir;

	return NULL;
}


}
}