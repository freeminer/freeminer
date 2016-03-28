call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
SET EnableNuGetPackageRestore=true
python build.py release amd64 > build.log
