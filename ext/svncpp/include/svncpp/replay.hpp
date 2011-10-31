#pragma once
#include "svncpp/ra.hpp"

namespace svn
{
namespace replay
{

class Editor;
class Directory;
class File;

class DirEntry
{
public:
	DirEntry():m_parent(NULL), m_editor(NULL){}
	virtual ~DirEntry(){}

	DirEntry(const char* name, Directory* parent, Editor* editor):m_parent(parent), m_name(name), m_editor(editor){}

	std::string	m_name;
	Directory*	m_parent;
	Editor*		m_editor;
};

class Directory : public DirEntry
{
public:

	virtual Directory* add(const char* path, const char* copyfrom_path, const Revision& copyfrom_revision) =0;
	virtual File*	   addFile(const char* path, const char* copyfrom_path, const Revision& copyfrom_revision) =0;
	virtual Directory* open(const char* path, const Revision& base_revision) =0;
	virtual File*	   openFile(const char* path, const Revision& base_revision) =0;
	virtual void	   deleteEntry(const char* path, const Revision& revision) =0;
};

class ApplyDeltaHandler
{
public:
	ApplyDeltaHandler();
	void handleWindow(svn_txdelta_window_t *window){}

	File* m_file;
};

class File : public DirEntry
{
public:
	virtual ApplyDeltaHandler*	applyDelta(const char* baseChecksum){ return NULL; }
	virtual void				onClose(const char* checksum){}
};

class Editor : public CLibSvnObjWrapper<svn_delta_editor_t>, Uncopyable
{
public:
	Editor(void);
	virtual ~Editor(void);

	virtual void		onTargetRevision(const Revision& target_revision){}
	virtual Directory*	openRoot(const Revision& base_revision) =0;

	void replay(  Repo* repo,
				  svn_revnum_t revision,
				  svn_revnum_t low_water_mark,
				  svn_boolean_t send_deltas);

	Revision m_TargetRevision;
};




}
}