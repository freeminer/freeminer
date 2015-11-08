To build Freeminer using Visual Studio for Windows you'll need:
  virtualbox, ~25GB space
  - [windows 10] http://www.microsoft.com/en-us/software-download/windows10
- [CMake](http://www.cmake.org/cmake/resources/software.html) and [Git](http://msysgit.github.io/), both should be in your PATH
- [Python 3.x](https://www.python.org/downloads/)
- [Visual Studio 2015 Community](http://www.visualstudio.com/downloads/download-visual-studio-vs)
- [DirectX SDK](http://www.microsoft.com/en-us/download/details.aspx?id=6812)

Automated build:
build.bat


Manual build (with debug support):

You'll need to allow NuGet to restore packages, otherwise it'll output the following error:

> Package restore is disabled by default (or maybe enabled in vs2013 community edition). To give consent, open the Visual Studio
Options dialog, click on Package Manager node and check 'Allow NuGet to download
missing packages during build.' You can also give consent by setting the
environment variable 'EnableNuGetPackageRestore' to 'true'.

Tools → Options → Package Manager → [x] Allow NuGet to download missing packages

To start the build, launch "Developer Command Prompt for VS2013", navigate to `freeminer\build\windows` and run `python build.py` (assuming Python is available in PATH). This script will download all dependencies, compile them, and then build Freeminer.

After the build is finished you'll find a ZIP file suitable for redistribution in the `project` directory.

You can also build "Debug" configuration (for development purposes) by executing `python build.py debug`. You can then use the generated `freeminer.sln` VS project file, found in the `project` directory. Note that while the generated project has both "Debug" and "Release" configurations (and more) you should only use the one that was generated ("Release" when running `python build.py`, and "Debug" for `python build.py debug`).

Switching between "Debug" and "Release" configurations will require all dependencies to be rebuilt, the easiest way to achieve that is just to delete the `deps` directory.

If you are going to develop FM using Visual Studio you'll have to change startup project. Select Solution explorer (Ctrl-Alt-L or view → solution explorer);  Right click "Solution 'freeminer' (10 projects)" → Properties → Common Properties → Startup Project. Select "Single startup project", then select "freeminer" in the dropdown. Then you'll be able to start Freeminer from VS and use the debugger.
Also useful for debugging release build: http://msdn.microsoft.com/en-us/library/fsk896zz.aspx

A word of warning: gettext and libintl will install themselves to `C:\usr\`. If you don't want this behavior don't build Freeminer for Windows.
