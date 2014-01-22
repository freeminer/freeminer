To build Freeminer using MSVC for Windows you'll need:
- cmake, git, svn, ruby; bash, sed, cp, mv, rm (those come with msys git); all should be added to your PATH
- MSVC 2010
- Windows SDK for Windows Server 2008 and .NET Framework 3.5: http://www.microsoft.com/en-us/download/details.aspx?id=11310
- `C:\Program Files\Microsoft Visual Studio 9.0\VC\vcpackages` or similar added to your PATH, you'll need `vcbuild` as well (to build msgpack)
- DirectX SDK: http://www.microsoft.com/en-us/download/details.aspx?id=6812
- Boost (for LevelDB): http://sourceforge.net/projects/boost/files/boost-binaries/1.55.0-build2/boost_1_55_0-msvc-10.0-32.exe/download

You'll have to build LevelDB manually. All other dependencies are built automatically. Read this to know how: https://code.google.com/p/leveldb/source/browse/WINDOWS?name=windows

When creative project for LevelDB in MSVC make sure to name the project `leveldb` and don't forget to switch runtime library to `/MT`. You'll also have to modify `port.h` to `#include "port/port_win.h"`. And remove `#include <unistd.h>` from `c.cc`.

After you've successfully built LevelDB copy settings.py.example to settings.py and edit it to make `BOOST_PATH` and `LEVELDB_PATH` to point to boost and LevelDB respectively.

For example,

```python
BOOST_PATH = "C:\\local\\boost_1_55_0\\"
LEVELDB_PATH = "C:\\leveldb\\"
```

After the build finishes you'll find `.zip` in `build_tmp` directory.

Note: gettext and libintl will install themselves to `C:\usr\`. If you don't want this behavior don't build Freeminer for Windows.