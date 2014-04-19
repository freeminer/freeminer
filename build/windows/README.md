To build Freeminer using Visual Studio for Windows you'll need:
- [CMake](http://www.cmake.org/cmake/resources/software.html) and [Git](http://msysgit.github.io/), both should be in your PATH
- [Python 3.x](https://www.python.org/downloads/)
- [Visual Studio 2013](http://www.visualstudio.com/downloads/download-visual-studio-vs) (if you decide to use the Express edition make sure to get Microsoft Visual Studio Express 2013 for Windows **Desktop**)
- [DirectX SDK](http://www.microsoft.com/en-us/download/details.aspx?id=6812)

To start the build, launch "Developer Command Prompt for VS2013", navigate to `freeminer\build\windows` and run `python build.py` (assuming Python is available in PATH). This script will download all dependencies, compile them, and then build Freeminer.

After the build is finished you'll find a ZIP file suitable for redistribution in the `project` directory.

You can also build "Debug" configuration (for development purposes) by executing `python build.py debug`. You can then use the generated `freeminer.sln` VS project file, found in the `project` directory. Note that while the generated project has both "Debug" and "Release" configurations (and more) you should only use the one that was generated ("Release" when running `python build.py`, and "Debug" for `python build.py debug`).

Switching between "Debug" and "Release" configurations will require all dependencies to be rebuilt, the easiest way to achieve that is just to delete the `deps` directory.

If you are going to develop FM using Visual Studio you'll have to change startup project. Right click "Solution 'freeminer' (10 projects)" → Properties → Common Properties → Startup Project. Select "Single startup project", then select "freeminer" in the dropdown. Then you'll be able to start Freeminer from VS and use the debugger.

A word of warning: gettext and libintl will install themselves to `C:\usr\`. If you don't want this behavior don't build Freeminer for Windows.