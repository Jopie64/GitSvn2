========================================================================
    GitSvn2 Project
========================================================================

 Git SVN on Windows is very slow because it is written in Perl.
 Perl forks extensively, and forking a process on Windows is slow.
 
 This project aims to make a quicker git-svn, build on c++ using
 libgit2 and the svn c++ api. It also uses the GitCpp project
 which is a c++ wrapper for the libgit2 library.
 
 GitSvn2 will also store the svn meta data in the repository.
 When a GitSvn2 repository is cloned by the 'git clone' command,
 the svn meta data should also be cloned with it. This is currently
 not done in git-svn, because the meta data is stored in different
 files which are not transfered by the 'git clone' command.

/////////////////////////////////////////////////////////////////////////////


Command line examples:
init D:\test\gitsvn\gitpartcmd svn://jdmstorage.jdm1.maassluis/main

setsvnpath svn://jdmstorage.jdm1.maassluis/main
