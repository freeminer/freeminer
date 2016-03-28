call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
SET EnableNuGetPackageRestore=true
python build.py > build.log
