Directory Contents
==================

vs2019
  Visual Studio project and solution files for the game DLLs. This is the
  only Visual Studio project set used for Windows builds.
    hldll.sln / hldll.vcxproj      server DLL (ice.dll)
    hl_cdll.sln / hl_cdll.vcxproj  client DLL (client.dll)
  New source files must be added to the .vcxproj, its .vcxproj.filters and
  the matching Linux Makefile in ../linux/.

  Utility projects (makefont, makels, qlumpy, sprgen, studiomdl) have their
  own .sln/.vcxproj files under ../utils/.


Other notes
===========

MSVC++ 6.0 .dsp files can be found in the appropriate folders. They are legacy
and not maintained.

Linux Makefiles can be found in ../linux/
