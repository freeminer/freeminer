call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
SET EnableNuGetPackageRestore=true
python build.py release x64 > build.log
