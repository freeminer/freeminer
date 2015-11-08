SET PATH=%PATH%;"C:\Program Files (x86)\CMake\bin";"C:\Program Files (x86)\MSBuild\12.0\Bin";"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\";"C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\IDE\";"C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools";"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin" 
SET INCLUDE="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include";"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc\include";"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include";
SET LIB="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib\";"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\atlmfc\lib\";"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib";
::SET LIBPATH="C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib";
::SET LIBPATH="C:\Windows\Microsoft.NET\Framework\v4.0.30319";"C:\Windows\Microsoft.NET\Framework\v3.5;"C:\Program Files\Microsoft Visual Studio 10.0\VC\LIB";"C:\Program Files\Microsoft Visual Studio 10.0\VC\ATLMFC\LIB";
::SET VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio 10.0\VC\"
::SET VSINSTALLDIR="C:\Program Files\Microsoft Visual Studio 10.0\"
::SET WINDOWSSDKDIR="C:\Program Files\Microsoft SDKs\Windows\v7.0A\"

SET EnableNuGetPackageRestore=true

python build.py > build.log


