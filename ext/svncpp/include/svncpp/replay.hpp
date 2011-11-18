#pragma once
#include "svncpp/ra.hpp"
#include "svncpp/apr_hash.hpp"
#include "svncpp/delta.hpp"

namespace svn
{
namespace replay
{

class Editor;
class Directory;
class File;

class Properties : public AprHashCpp<const char, svn_string_t, std::string, std::string>
{
public:
	Properties(apr_hash_t* hash):AprHashCpp(hash) {}

//	virtual void toOrigKey((orig_key_type*)& dest, const key_type& src);
	virtual void toKeyCpp(key_type& dest, const orig_key_type* src) { dest = src; }
	virtual void toRightCpp(right_type& dest, const orig_right_type* src) { dest = src->data; }
};

class DirEntry
{
public:
	DirEntry():m_parent(NULL), m_editor(NULL), m_new(false){}
	virtual ~DirEntry(){}

	DirEntry(const char* name, Directory* parent, Editor* editor):m_parent(parent), m_name(name), m_editor(editor), m_new(false){}

	virtual void onInit(){}

	virtual void changeProp(const char *name, const svn_string_t *value) {}


	bool		m_new;
	std::string	m_name;
	Directory*	m_parent;
	Editor*		m_editor;
};

class Directory : public DirEntry
{
public:

	virtual void	   onClose(){}
	virtual Directory* add(const char* path, const char* copyfrom_path, svn_revnum_t copyfrom_revision) =0;
	virtual File*	   addFile(const char* path, const char* copyfrom_path, svn_revnum_t copyfrom_revision) =0;
	virtual Directory* open(const char* path, svn_revnum_t base_revision) =0;
	virtual File*	   openFile(const char* path, svn_revnum_t base_revision) =0;
	virtual void	   deleteEntry(const char* path, svn_revnum_t revision) =0;
};

class ApplyDeltaHandler : public delta::ApplyDeltaHandler
{
public:
	ApplyDeltaHandler();

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

	virtual void		onTargetRevision(svn_revnum_t target_revision){}
	virtual Directory*	openRoot(svn_revnum_t base_revision) =0;

	virtual void		onFinish(svn_revnum_t rev, Properties& props){};

	void replay(  Repo* repo,
				  svn_revnum_t revision,
				  svn_revnum_t low_water_mark,
				  svn_boolean_t send_deltas);

	svn_revnum_t m_TargetRevision;
};

class RangeReplay: Uncopyable
{
public:
	RangeReplay();
	virtual ~RangeReplay();

	virtual Editor* makeEditor(svn_revnum_t rev, Properties& props) =0;

	void replay(Repo* repo,
			svn_revnum_t revStart,
			svn_revnum_t revEnd,
			svn_revnum_t low_water_mark,
			svn_boolean_t send_deltas);

};




}
}