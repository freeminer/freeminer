import os
import urllib.request
from zipfile import ZipFile
import tarfile
import shutil
import subprocess
import sys

# http://stackoverflow.com/a/377028/2606891
def which(program):
    import os
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None

	
def extract_zip(what, where):
	print("extracting {}...".format(what))
	zip_file = ZipFile(what)
	zip_file.extractall(path=where)
	zip_file.close()
	

def extract_tar(what, where):
	print("extracting {}...".format(what))
	tar_file = tarfile.open(what)
	tar_file.extractall(path=where)
	tar_file.close()

	
def download(url, path):
	urllib.request.urlretrieve(url, path)
	

def patch(path, source, target):
	print("PATCHING {}".format(path))
	fin = open(path, "r")
	text = fin.read()
	fin.close()
	# some BOM autism
	if ord(text[0]) == 239:
		text = text[3:]
	text = text.replace(source, target)
	fout = open(path, "w", encoding="utf-8")
	fout.write(text)
	fout.close()

	
LIBOGG_VERSION = "1.3.2"
LEVELDB_VERSION = "1.16.0.5"
CRC32C_VERSION = "1.0.4"
SNAPPY_VERSION = "1.1.1.7"
irrlicht = "irrlicht-1.8.1"
curl = "curl-7.45.0"
openal = "openal-soft-1.16.0"
libogg = "libogg-{}".format(LIBOGG_VERSION)
libvorbis = "libvorbis-1.3.5"
zlib = "zlib-1.2.8"
freetype = "freetype-2.6.1"
luajit = "LuaJIT-2.0.4"
gettext = "gettext-0.14.6"
libiconv = "libiconv-1.9.2"
#MSGPACK_VERSION = "cpp-1.1.0"
#msgpack = "msgpack-c-{}".format(MSGPACK_VERSION)
#SQLITE_VERSION="3080704"
#sqlite = "sqlite-autoconf-{}".format(SQLITE_VERSION)
SQLITE_VERSION="3.8.4.2"
sqlite = "sqlite-{}".format(SQLITE_VERSION)

#somtimes vs becomes mad
#error MSB8020: The build tools for Visual Studio 2012 (Platform Toolset = 'v110') cannot be found.
patch_toolset = 1

def main():
	build_type = "Release"
	if len(sys.argv) > 1 and sys.argv[1] == "debug":
		build_type = "Debug"
	msbuild = which("MSBuild.exe")
	cmake = which("cmake.exe")
	vcbuild = which("vcbuild.exe")
	if not msbuild:
		print("MSBuild.exe not found! Make sure you run 'Visual Studio Command Prompt', not cmd.exe")
		return
	if not cmake:
		print("cmake.exe not found! Make sure you have CMake installed and added to PATH.")
		return
	print("Found msbuild: {}\nFound cmake: {}".format(msbuild, cmake))

	print("Build type: {}".format(build_type))
	
	if not os.path.exists("deps"):
		print("Creating `deps` directory.")
		os.mkdir("deps")

	os.chdir("deps")
	if not os.path.exists(irrlicht):
		print("Irrlicht not found, downloading.")
		zip_path = "{}.zip".format(irrlicht)
		urllib.request.urlretrieve("http://downloads.sourceforge.net/irrlicht/{}.zip".format(irrlicht), zip_path)
		extract_zip(zip_path, ".")
		os.chdir(os.path.join(irrlicht, "source", "Irrlicht"))
		# sorry but this breaks the build
		patch(os.path.join("zlib", "deflate.c"), "const char deflate_copyright[] =", "static const char deflate_copyright[] =")
		os.system("devenv /upgrade Irrlicht12.0.vcxproj")
		#patch("Irrlicht12.0.vcxproj", "; _ITERATOR_DEBUG_LEVEL=0", "")
		#if patch_toolset:
		#	patch("Irrlicht12.0.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
		os.system('MSBuild Irrlicht12.0.vcxproj /p:Configuration="Static lib - {}"'.format(build_type))
		os.chdir(os.path.join("..", "..", ".."))

	if not os.path.exists(curl):
		print("curl not found, downloading.")
		os.mkdir(curl)
		tar_path = "{}.tar.gz".format(curl)
		urllib.request.urlretrieve("http://curl.haxx.se/download/{}.tar.gz".format(curl), tar_path)
		extract_tar(tar_path, ".")
		os.chdir(os.path.join(curl, "winbuild"))
		os.system("nmake /f Makefile.vc mode=static RTLIBCFG=static USE_IDN=no DEBUG={}".format("yes" if build_type=="Debug" else "no"))
		os.chdir(os.path.join("..", ".."))

	if not os.path.exists(openal):
		print("openal not found, downloading.")
		tar_path = "{}.tar.bz2".format(openal)
		download("http://kcat.strangesoft.net/openal-releases/{}.tar.bz2".format(openal), tar_path)
		extract_tar(tar_path, ".")
		print("building openal")
		os.chdir(os.path.join(openal, "build"))
		os.system("cmake .. -DFORCE_STATIC_VCRT=1 -DLIBTYPE=STATIC")
		os.system("MSBuild ALL_BUILD.vcxproj /p:Configuration={}".format(build_type))
		os.chdir(os.path.join("..", ".."))
	
	if not os.path.exists(libogg):
		print("libogg not found, downloading.")
		tar_path = "{}.tar.gz".format(libogg)
		download("http://downloads.xiph.org/releases/ogg/{}.tar.gz".format(libogg), tar_path)
		extract_tar(tar_path, ".")
		print("building libogg")
		os.chdir(os.path.join(libogg, "win32", "VS2010"))

		if patch_toolset:
			patch("libogg_static.vcxproj", '<ConfigurationType>StaticLibrary</ConfigurationType>', '<ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v140</PlatformToolset>')

		os.system("devenv /upgrade libogg_static.vcxproj")
		os.system("MSBuild libogg_static.vcxproj /p:Configuration={}".format(build_type))
		os.chdir(os.path.join("..", "..", ".."))
	
	if not os.path.exists(libvorbis):
		print("libvorbis not found, downloading.")
		tar_path = "{}.tar.gz".format(libvorbis)
		download("http://downloads.xiph.org/releases/vorbis/{}.tar.gz".format(libvorbis), tar_path)
		extract_tar(tar_path, ".")
		print("building libvorbis")
		os.chdir(os.path.join(libvorbis, "win32", "VS2010"))
		# patch libogg.props to have correct version of libogg
		patch("libogg.props", "<LIBOGG_VERSION>1.2.0</LIBOGG_VERSION>", "<LIBOGG_VERSION>{}</LIBOGG_VERSION>".format(LIBOGG_VERSION))
		# not static enough!
		patch(os.path.join("libvorbisfile", "libvorbisfile_static.vcxproj"), "<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreaded</RuntimeLibrary>")
		patch(os.path.join("libvorbisfile", "libvorbisfile_static.vcxproj"), "<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>")
		patch(os.path.join("libvorbis", "libvorbis_static.vcxproj"), "<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreaded</RuntimeLibrary>")
		patch(os.path.join("libvorbis", "libvorbis_static.vcxproj"), "<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>")

		if patch_toolset:
			patch(os.path.join("libvorbis", "libvorbis_static.vcxproj"), '<ConfigurationType>StaticLibrary</ConfigurationType>', '<ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v140</PlatformToolset>')
			patch(os.path.join("libvorbisfile", "libvorbisfile_static.vcxproj"), '<ConfigurationType>StaticLibrary</ConfigurationType>', '<ConfigurationType>StaticLibrary</ConfigurationType><PlatformToolset>v140</PlatformToolset>')

		os.system("devenv /upgrade vorbis_static.sln")
		os.system("MSBuild vorbis_static.sln /p:Configuration={}".format(build_type))
		os.chdir(os.path.join("..", "..", ".."))
		
	if not os.path.exists(zlib):
		print("zlib not found, downloading.")
		tar_path = "{}.tar.gz".format(zlib)
		download("http://prdownloads.sourceforge.net/libpng/{}.tar.gz?download".format(zlib), tar_path)
		extract_tar(tar_path, ".")
		print("building zlib")
		os.chdir(zlib)
		# enable SAFESEH, http://www.helyar.net/2010/compiling-zlib-lib-on-windows/comment-page-1/#comment-12250
		patch(os.path.join("contrib", "masmx86", "bld_ml32.bat"), "ml /coff /Zi /c /Flmatch686.lst match686.asm", "ml /safeseh /coff /Zi /c /Flmatch686.lst match686.asm")
		patch(os.path.join("contrib", "masmx86", "bld_ml32.bat"), "ml /coff /Zi /c /Flinffas32.lst inffas32.asm", "ml /safeseh /coff /Zi /c /Flinffas32.lst inffas32.asm")
		os.chdir(os.path.join("contrib", "vstudio", "vc11"))
		# http://stackoverflow.com/a/20022239
		patch("zlibvc.def", "VERSION		1.2.8", "VERSION		1.2")
		os.system("devenv /upgrade zlibvc.sln")
		patch("zlibstat.vcxproj", "MultiThreadedDebugDLL", "MultiThreadedDebug")
		if patch_toolset:
			patch("zlibstat.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
			patch("zlibvc.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
			patch("minizip.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
			patch("testzlibdll.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
			patch("testzlib.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
			patch("miniunz.vcxproj", "<PlatformToolset>v110</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")
		
		os.system("MSBuild zlibvc.sln /p:Configuration={} /p:Platform=win32".format(build_type))
		os.chdir(os.path.join("..", "..", "..", ".."))

	if not os.path.exists(freetype):
		print("freetype not found, downloading.")
		tar_path = "{}.tar.gz".format(freetype)
		download("http://www.mirrorservice.org/sites/download.savannah.gnu.org/releases/freetype/{}.tar.gz".format(freetype), tar_path)
		extract_tar(tar_path, ".")
		print("building freetype")
		os.chdir(os.path.join(freetype, "builds", "windows", "vc2010"))

		if patch_toolset:
			patch("freetype.vcxproj", "<PlatformToolset>v100</PlatformToolset>", "<PlatformToolset>v140</PlatformToolset>")

		os.system("devenv /upgrade freetype.vcxproj")
		os.system('MSBuild freetype.vcxproj /p:Configuration="{} Multithreaded"'.format(build_type))
		os.chdir(os.path.join("..", "..", "..", ".."))
	
	if not os.path.exists(luajit):
		print("LuaJIT not found, downloading.")
		tar_path = "{}.tar.gz".format(luajit)
		download("http://luajit.org/download/{}.tar.gz".format(luajit), tar_path)
		extract_tar(tar_path, ".")
		print("building LuaJIT")
		os.chdir(os.path.join(luajit, "src"))
		patch("msvcbuild.bat", "/MD", "/MT" if build_type != "Debug" else "/MTd /Zi /Og")
		os.system("msvcbuild.bat static")
		os.chdir(os.path.join("..", ".."))
		
	if not os.path.exists(gettext):
		print("gettext/libiconv not found, downloading")
		tar_path = "{}.tar.gz".format(gettext)
		download("http://ftp.gnu.org/gnu/gettext/{}.tar.gz".format(gettext), tar_path)
		extract_tar(tar_path, ".")
		tar_path = "{}.tar.gz".format(libiconv)
		download("http://ftp.gnu.org/gnu/libiconv/{}.tar.gz".format(libiconv), tar_path)
		extract_tar(tar_path, ".")
		
		os.chdir(libiconv)
		mflags = "-MT" if build_type != "Debug" else "-MTd"
		# we don't want 'typedef enum { false = 0, true = 1 } _Bool;'
		patch(os.path.join("windows", "stdbool.h"), "# if !0", "# if 0")
		os.system("nmake -f Makefile.msvc NO_NLS=1 MFLAGS={}".format(mflags))
		os.system("nmake -f Makefile.msvc NO_NLS=1 MFLAGS={} install".format(mflags))
		os.system("nmake -f Makefile.msvc NO_NLS=1 MFLAGS={} distclean".format(mflags))
		os.chdir("..")
		os.chdir(gettext)
		patch(os.path.join("gettext-runtime", "intl", "localename.c"), "case SUBLANG_PUNJABI_PAKISTAN:", "//case SUBLANG_PUNJABI_PAKISTAN:")
		patch(os.path.join("gettext-runtime", "intl", "localename.c"), "case SUBLANG_ROMANIAN_MOLDOVA:", "//case SUBLANG_ROMANIAN_MOLDOVA:")
		patch(os.path.join("gettext-tools", "windows", "stdbool.h"), "# if !0", "# if 0")
		os.system("nmake -f Makefile.msvc MFLAGS={}".format(mflags))
		os.system("nmake -f Makefile.msvc MFLAGS={} install".format(mflags))
		os.chdir("..")
		os.chdir(libiconv)
		os.system("nmake -f Makefile.msvc MFLAGS={}".format(mflags))
		# DARK MSVC MAGIC
		os.environ["CL"] = "/Za"
		os.system("nmake -f Makefile.msvc MFLAGS={}".format(mflags))
		os.environ["CL"] = ""
		os.system("nmake -f Makefile.msvc MFLAGS={}".format(mflags))
		# /magic
		os.system("nmake -f Makefile.msvc MFLAGS={} install".format(mflags))
		
		os.chdir("..")

	#if os.path.exists(msgpack):
	#	print("msgpack not found, downloading")
	#	#download("https://github.com/msgpack/msgpack-c/archive/{}.zip".format(MSGPACK_VERSION), "msgpack.zip")
	#	#extract_zip("msgpack.zip", ".")
	#	os.chdir(msgpack)
	#	#patch(os.path.join("src", "msgpack", "type.hpp"), '#include "type/tr1/unordered_map.hpp"', '// #include "type/tr1/unordered_map.hpp"')
	#	#patch(os.path.join("src", "msgpack", "type.hpp"), '#include "type/tr1/unordered_set.hpp"', '// #include "type/tr1/unordered_set.hpp"')
	#	patch("msgpack_vc8.vcproj", 'RuntimeLibrary="2"', 'RuntimeLibrary="0"')
	#	patch("msgpack_vc8.vcproj", 'RuntimeLibrary="3"', 'RuntimeLibrary="1"')
	#	# use newer compiler, won't link otherwise
	#	os.system("vcupgrade msgpack_vc8.vcproj")
	#	os.system("MSBuild msgpack_vc8.vcxproj /p:Configuration={}".format(build_type))
	#	os.chdir("..")

	if not os.path.exists("leveldb.nupkg"):
		print("Downloading LevelDB + dependencies from NuGet")
		#download("http://www.nuget.org/api/v2/package/LevelDB/{}".format(LEVELDB_VERSION), "leveldb.nupkg")
		#download("http://www.nuget.org/api/v2/package/Crc32C/{}".format(CRC32C_VERSION), "crc32c.nupkg")
		#download("http://www.nuget.org/api/v2/package/Snappy/{}".format(SNAPPY_VERSION), "snappy.nupkg")		
		download("http://www.nuget.org/api/v2/package/LevelDB/", "leveldb.nupkg")
		download("http://www.nuget.org/api/v2/package/Crc32C/", "crc32c.nupkg")
		download("http://www.nuget.org/api/v2/package/Snappy/", "snappy.nupkg")		

	#if not os.path.exists("sqlite.nupkg"):
	#	download("http://www.nuget.org/api/v2/package/sqlite/{}".format(SQLITE_VERSION), "sqlite.nupkg")		
	#if not os.path.exists("sqlite.redist.nupkg"):
	#	download("http://www.nuget.org/api/v2/package/sqlite.redist/{}".format(SQLITE_VERSION), "sqlite.redist.nupkg")		

	#if not os.path.exists(sqlite):
		#print("sqlite not found, downloading.")
		#os.mkdir(sqlite)

		#zip_path = "sqlite-amalgamation-{}.zip".format(SQLITE_VERSION)
		#urllib.request.urlretrieve("http://www.sqlite.org/snapshot/{}".format(zip_path), zip_path)
		#extract_zip(zip_path, sqlite)

		#tar_path = "{}.tar.gz".format(sqlite)
		#urllib.request.urlretrieve("http://www.sqlite.org/2014/{}".format(tar_path), tar_path)
		#extract_tar(tar_path, ".")

		#os.chdir(sqlite)
		#os.system("cmake . -DFORCE_STATIC_VCRT=1 -DLIBTYPE=STATIC")
		#os.system("MSBuild ALL_BUILD.vcxproj /p:Configuration={}".format(build_type))
		#os.chdir("..")

	
	os.chdir("..")
	
	print("=> Building Freeminer")
	# multi-process build
	os.environ["CL"] = "/MP"
	if os.path.exists("project"):
		shutil.rmtree("project")
	if os.path.exists("install_tmp"):
		shutil.rmtree("install_tmp")
	os.mkdir("project")
	os.chdir("project")
	cmake_string = r"""
		-DCMAKE_BUILD_TYPE={build_type}
		-DRUN_IN_PLACE=1
		-DCUSTOM_BINDIR=.
		-DCMAKE_INSTALL_PREFIX=..\install_tmp\
		-DSTATIC_BUILD=1
		-DIRRLICHT_SOURCE_DIR=..\deps\{irrlicht}\
		-DENABLE_SOUND=1
		-DOPENAL_INCLUDE_DIR=..\deps\{openal}\include\AL\
		-DOPENAL_LIBRARY=..\deps\{openal}\build\{build_type}\OpenAL32.lib;..\deps\{openal}\build\{build_type}\common.lib
		-DOGG_INCLUDE_DIR=..\deps\{libogg}\include\
		-DOGG_LIBRARY=..\deps\{libogg}\win32\VS2010\Win32\{build_type}\libogg_static.lib
		-DVORBIS_INCLUDE_DIR=..\deps\{libvorbis}\include\
		-DVORBIS_LIBRARY=..\deps\{libvorbis}\win32\VS2010\Win32\{build_type}\libvorbis_static.lib
		-DVORBISFILE_LIBRARY=..\deps\{libvorbis}\win32\VS2010\Win32\{build_type}\libvorbisfile_static.lib
		-DZLIB_INCLUDE_DIR=..\deps\{zlib}\
		-DZLIB_LIBRARIES=..\deps\{zlib}\contrib\vstudio\vc11\x86\ZlibStat{build_type}\zlibstat.lib
		-DFREETYPE_INCLUDE_DIR_freetype2=..\deps\{freetype}\include\
		-DFREETYPE_INCLUDE_DIR_ft2build=..\deps\{freetype}\include\
		-DFREETYPE_LIBRARY=..\deps\{freetype}\objs\vc2010\win32\{freetype_lib}
		-DLUA_LIBRARY=..\deps\{luajit}\src\lua51.lib
		-DLUA_INCLUDE_DIR=..\deps\{luajit}\src\
		-DENABLE_CURL=1
		-DCURL_LIBRARY=..\deps\{curl}\builds\libcurl-vc-x86-{build_type}-static-ipv6-sspi-winssl\lib\{curl_lib}
		-DCURL_INCLUDE_DIR=..\deps\{curl}\builds\libcurl-vc-x86-{build_type}-static-ipv6-sspi-winssl\include
		-DGETTEXT_INCLUDE_DIR=C:\usr\include\
		-DGETTEXT_LIBRARY=C:\usr\lib\intl.lib
		-DICONV_LIBRARY=C:\usr\lib\iconv.lib
		-DGETTEXT_MSGFMT=C:\usr\bin\msgfmt.exe
		-DENABLE_GETTEXT=1
		-DENABLE_LEVELDB=1
		-DFORCE_LEVELDB=1
		-DENABLE_SQLITE3=1
	""".format(
		curl_lib="libcurl_a.lib" if build_type != "Debug" else "libcurl_a_debug.lib",
		freetype_lib="freetype261MT.lib" if build_type != "Debug" else "freetype261MT_D.lib",
		build_type=build_type,
		irrlicht=irrlicht,
		zlib=zlib,
		freetype=freetype,
		luajit=luajit,
		openal=openal,
		libogg=libogg,
		libvorbis=libvorbis,
		curl=curl,
		#msgpack=msgpack,
		#msgpack_suffix="d" if build_type == "Debug" else "",
	).replace("\n", "")

# now in cmake
#		-DMSGPACK_INCLUDE_DIR=..\deps\{msgpack}\include\
#		-DMSGPACK_LIBRARY=..\deps\{msgpack}\lib\msgpack{msgpack_suffix}.lib

	
	os.system(r"cmake ..\..\.. " + cmake_string)
	patch(os.path.join("src", "freeminer.vcxproj"), "</AdditionalLibraryDirectories>", r";$(DXSDK_DIR)\Lib\x86</AdditionalLibraryDirectories>")
	#patch(os.path.join("src", "sqlite", "sqlite3.vcxproj"), "MultiThreadedDebugDLL", "MultiThreadedDebug")
	# wtf, cmake?
	patch(os.path.join("src", "enet", "enet.vcxproj"), "<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreaded</RuntimeLibrary>")
	patch(os.path.join("src", "enet", "enet.vcxproj"), "<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>", "<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>")

	# install LevelDB package
	os.system(r"..\NuGet.exe install LevelDB -source {}\..\deps".format(os.getcwd()))
	# patch project file to use these packages
	patch(os.path.join("src", "freeminer.vcxproj"), '<ItemGroup Label="ProjectConfigurations">',
		r"""<Import Project="..\LevelDB.1.16.0.5\build\native\LevelDB.props" Condition="Exists('..\LevelDB.1.16.0.5\build\native\LevelDB.props')" />
  		<Import Project="..\Snappy.1.1.1.7\build\native\Snappy.props" Condition="Exists('..\Snappy.1.1.1.7\build\native\Snappy.props')" />
  		<Import Project="..\Crc32C.1.0.4\build\native\Crc32C.props" Condition="Exists('..\Crc32C.1.0.4\build\native\Crc32C.props')" />
  		<ItemGroup Label="ProjectConfigurations">""")

	## install sqlite package
	#os.system(r"..\NuGet.exe install sqlite -source {}\..\deps".format(os.getcwd()))
	## patch project file to use these packages
	#patch(os.path.join("src", "freeminer.vcxproj"), '<ItemGroup Label="ProjectConfigurations">',
	#	r"""<Import Project="..\sqlite.{}\build\native\sqlite.targets" Condition="Exists('..\sqlite.{}\build\native\sqlite.targets')" />
	#	<ItemGroup Label="ProjectConfigurations">""".format(SQLITE_VERSION,SQLITE_VERSION))


	os.system("MSBuild ALL_BUILD.vcxproj /p:Configuration={}".format(build_type))
	os.system("MSBuild INSTALL.vcxproj /p:Configuration={}".format(build_type))
	os.system("MSBuild PACKAGE.vcxproj /p:Configuration={}".format(build_type))
	
	
	
if __name__ == "__main__":
	main()
