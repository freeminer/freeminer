call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
SET EnableNuGetPackageRestore=true
python build.py > build.log

::SET PATH=%PATH%;"C:\Program Files (x86)\CMake\bin";"C:\Program Files (x86)\MSBuild\14.0\Bin";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools";"C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin" 
::::SET INCLUDE="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\include";"C:\Program Files (x86)\Windows Kits\10\Include\10.0.10150.0\ucrt\"; "C:\Program Files (x86)\Windows Kits\8.1\Include\um\";"C:\Program Files\Microsoft SDKs\Windows\v7.1\Include";
::SET INCLUDE="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\include";"C:\Program Files (x86)\Windows Kits\10\Include\10.0.10150.0\ucrt\";"C:\Program Files\Microsoft SDKs\Windows\v7.1\Include";
::::SET LIB="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\lib\";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\lib\"; "C:\Program Files (x86)\Windows Kits\8.1\Include\um\";"C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib";
::SET LIB="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\lib\";"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\lib\"; "C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib";
