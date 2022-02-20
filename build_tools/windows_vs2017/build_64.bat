call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
SET EnableNuGetPackageRestore=true
python build.py release x64 > build_64.log
