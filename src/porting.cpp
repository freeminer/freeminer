/*
porting.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	Random portability stuff

	See comments in porting.h
*/

#include "porting.h"

#if defined(__FreeBSD__)
	#include <sys/types.h>
	#include <sys/sysctl.h>
#elif defined(_WIN32)
	#include <algorithm>
#endif
#if !defined(_WIN32)
	#include <unistd.h>
	#include <sys/utsname.h>
#endif
#if defined(__hpux)
	#define _PSTAT64
	#include <sys/pstat.h>
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && \
	!defined(__ANDROID__) && !defined(SERVER)
	#define XORG_USED
#endif
#ifdef XORG_USED
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
#endif

#include "config.h"
#include "debug.h"
#include "filesys.h"
#include "log.h"
#include "util/string.h"
#include "settings.h"
#include <list>

namespace porting
{

/*
	Signal handler (grabs Ctrl-C on POSIX systems)
*/

bool g_killed = false;

bool * signal_handler_killstatus(void)
{
	return &g_killed;
}

std::atomic_bool g_sighup, g_siginfo;

#if !defined(_WIN32) // POSIX
	#include <signal.h>

void sigint_handler(int sig)
{
	switch(sig) {
#if defined(SIGINFO)
		case SIGINFO:
			g_siginfo = true;
		break;
#endif
		case SIGHUP:
			g_sighup = true;
		break;
		case SIGINT:
		case SIGTERM:

	if(!g_killed)
	{
		g_killed = true;

		dstream << " INFO: sigint_handler(): "
			<< "Ctrl-C pressed, shutting down." << std::endl;

		// Comment out for less clutter when testing scripts
		/*dstream << "INFO: sigint_handler(): "
				<< "Printing debug stacks" << std::endl;
		debug_stacks_print();*/
	}
		break;

		default:
		(void)signal(sig, SIG_DFL);
	}

}

void signal_handler_init(void)
{
	g_sighup = false;
	g_siginfo = false;

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGHUP, sigint_handler);
#if defined(SIGINFO)
	signal(SIGINFO, sigint_handler);
#endif
}

#else // _WIN32
	#include <signal.h>

BOOL WINAPI event_handler(DWORD sig)
{
	switch (sig) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (!g_killed) {
			dstream << "INFO: event_handler(): "
				<< "Ctrl+C, Close Event, Logoff Event or Shutdown Event,"
				" shutting down." << std::endl;
			g_killed = true;
		} else {
			(void)signal(SIGINT, SIG_DFL);
		}
		break;
	case CTRL_BREAK_EVENT:
		break;
	}

	return TRUE;
}

void signal_handler_init(void)
{
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)event_handler, TRUE);
}

#endif

/*
	Path mangler
*/

// Default to RUN_IN_PLACE style relative paths
std::string path_share = "..";
std::string path_user = "..";
std::string path_locale = path_share + DIR_DELIM + "locale";


std::string getDataPath(const char *subpath)
{
	return path_share + DIR_DELIM + subpath;
}

void pathRemoveFile(char *path, char delim)
{
	// Remove filename and path delimiter
	int i;
	for(i = strlen(path)-1; i>=0; i--)
	{
		if(path[i] == delim)
			break;
	}
	path[i] = 0;
}

bool detectMSVCBuildDir(const std::string &path)
{
	const char *ends[] = {
		"bin\\Release",
		"bin\\Debug",
		"bin\\Build",
		NULL
	};
	return (removeStringEnd(path, ends) != "");
}

std::string get_sysinfo()
{
#ifdef _WIN32
	OSVERSIONINFO osvi;
	std::ostringstream oss;
	std::string tmp;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	tmp = osvi.szCSDVersion;
	std::replace(tmp.begin(), tmp.end(), ' ', '_');

	oss << "Windows/" << osvi.dwMajorVersion << "."
		<< osvi.dwMinorVersion;
	if (osvi.szCSDVersion[0])
		oss << "-" << tmp;
	oss << " ";
	#ifdef _WIN64
	oss << "x86_64";
	#else
	BOOL is64 = FALSE;
	if (IsWow64Process(GetCurrentProcess(), &is64) && is64)
		oss << "x86_64"; // 32-bit app on 64-bit OS
	else
		oss << "x86";
	#endif

	return oss.str();
#else
	struct utsname osinfo;
	uname(&osinfo);
	return std::string(osinfo.sysname) + "/"
		+ osinfo.release + " " + osinfo.machine;
#endif
}


bool getCurrentWorkingDir(char *buf, size_t len)
{
#ifdef _WIN32
	DWORD ret = GetCurrentDirectory(len, buf);
	return (ret != 0) && (ret <= len);
#else
	return getcwd(buf, len);
#endif
}


bool getExecPathFromProcfs(char *buf, size_t buflen)
{
#ifndef _WIN32
	buflen--;

	ssize_t len;
	if ((len = readlink("/proc/self/exe",     buf, buflen)) == -1 &&
		(len = readlink("/proc/curproc/file", buf, buflen)) == -1 &&
		(len = readlink("/proc/curproc/exe",  buf, buflen)) == -1)
		return false;

	buf[len] = '\0';
	return true;
#else
	return false;
#endif
}

//// Windows
#if defined(_WIN32)

bool getCurrentExecPath(char *buf, size_t len)
{
	DWORD written = GetModuleFileNameA(NULL, buf, len);
	if (written == 0 || written == len)
		return false;

	return true;
}


//// Linux
#elif defined(linux) || defined(__linux) || defined(__linux__)

bool getCurrentExecPath(char *buf, size_t len)
{
	if (!getExecPathFromProcfs(buf, len))
		return false;

	return true;
}


//// Mac OS X, Darwin
#elif defined(__APPLE__)

bool getCurrentExecPath(char *buf, size_t len)
{
	uint32_t lenb = (uint32_t)len;
	if (_NSGetExecutablePath(buf, &lenb) == -1)
		return false;

	return true;
}


//// FreeBSD, NetBSD, DragonFlyBSD
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)

bool getCurrentExecPath(char *buf, size_t len)
{
	// Try getting path from procfs first, since valgrind
	// doesn't work with the latter
	if (getExecPathFromProcfs(buf, len))
		return true;

	int mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;

	if (sysctl(mib, 4, buf, &len, NULL, 0) == -1)
		return false;

	return true;
}


//// Solaris
#elif defined(__sun) || defined(sun)

bool getCurrentExecPath(char *buf, size_t len)
{
	const char *exec = getexecname();
	if (exec == NULL)
		return false;

	if (strlcpy(buf, exec, len) >= len)
		return false;

	return true;
}


// HP-UX
#elif defined(__hpux)

bool getCurrentExecPath(char *buf, size_t len)
{
	struct pst_status psts;

	if (pstat_getproc(&psts, sizeof(psts), 0, getpid()) == -1)
		return false;

	if (pstat_getpathname(buf, len, &psts.pst_fid_text) == -1)
		return false;

	return true;
}


#else

bool getCurrentExecPath(char *buf, size_t len)
{
	return false;
}

#endif


//// Windows
#if defined(_WIN32)

bool setSystemPaths()
{
	char buf[BUFSIZ];

	// Find path of executable and set path_share relative to it
	FATAL_ERROR_IF(!getCurrentExecPath(buf, sizeof(buf)),
		"Failed to get current executable path");
	pathRemoveFile(buf, '\\');

	// Use ".\bin\.."
	path_share = std::string(buf) + "\\..";

	// Use "C:\Documents and Settings\user\Application Data\<PROJECT_NAME>"
	DWORD len = GetEnvironmentVariable("APPDATA", buf, sizeof(buf));
	FATAL_ERROR_IF(len == 0 || len > sizeof(buf), "Failed to get APPDATA");

	path_user = std::string(buf) + DIR_DELIM + PROJECT_NAME;
	return true;
}


//// Linux
#elif defined(linux) || defined(__linux)

bool setSystemPaths()
{
	char buf[BUFSIZ];

	if (!getCurrentExecPath(buf, sizeof(buf))) {
#ifdef __ANDROID__
		errorstream << "Unable to read bindir "<< std::endl;
#else
		FATAL_ERROR("Unable to read bindir");
#endif
		return false;
	}

	pathRemoveFile(buf, '/');
	std::string bindir(buf);

	// Find share directory from these.
	// It is identified by containing the subdirectory "builtin".
	std::list<std::string> trylist;
	std::string static_sharedir = STATIC_SHAREDIR;
	if (static_sharedir != "" && static_sharedir != ".")
		trylist.push_back(static_sharedir);

	trylist.push_back(bindir + DIR_DELIM ".." DIR_DELIM "share"
		DIR_DELIM + PROJECT_NAME);
	trylist.push_back(bindir + DIR_DELIM "..");

#ifdef __ANDROID__
	trylist.push_back(path_user);
#endif

	for (std::list<std::string>::const_iterator
			i = trylist.begin(); i != trylist.end(); i++) {
		const std::string &trypath = *i;
		if (!fs::PathExists(trypath) ||
			!fs::PathExists(trypath + DIR_DELIM + "builtin")) {
			warningstream << "system-wide share not found at \""
					<< trypath << "\""<< std::endl;
			continue;
		}

		// Warn if was not the first alternative
		if (i != trylist.begin()) {
			warningstream << "system-wide share found at \""
					<< trypath << "\"" << std::endl;
		}

		path_share = trypath;
		break;
	}

#ifndef __ANDROID__
	path_user = std::string(getenv("HOME")) + DIR_DELIM "."
		+ PROJECT_NAME;
#endif

	return true;
}


//// Mac OS X
#elif defined(__APPLE__)

bool setSystemPaths()
{
	CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if (CFURLGetFileSystemRepresentation(resources_url,
			TRUE, (UInt8 *)path, PATH_MAX)) {
		path_share = std::string(path);
	} else {
		warningstream << "Could not determine bundle resource path" << std::endl;
	}
	CFRelease(resources_url);

	path_user = std::string(getenv("HOME"))
		+ "/Library/Application Support/"
		+ PROJECT_NAME;
	return true;
}


#else

bool setSystemPaths()
{
	path_share = STATIC_SHAREDIR;
	path_user  = std::string(getenv("HOME")) + DIR_DELIM "."
		+ lowercase(PROJECT_NAME);
	return true;
}


#endif


void initializePaths()
{
#if RUN_IN_PLACE
	char buf[BUFSIZ];

	infostream << "Using relative paths (RUN_IN_PLACE)" << std::endl;

	bool success =
		getCurrentExecPath(buf, sizeof(buf)) ||
		getExecPathFromProcfs(buf, sizeof(buf));

	if (success) {
		pathRemoveFile(buf, DIR_DELIM_CHAR);
		std::string execpath(buf);

		path_share = execpath + DIR_DELIM "..";
		path_user  = execpath + DIR_DELIM "..";

		if (detectMSVCBuildDir(execpath)) {
			path_share += DIR_DELIM "..";
			path_user  += DIR_DELIM "..";
		}
		else {
		#if STATIC_BUILD
			path_share = execpath + "\\.";
			path_user = execpath + "\\.";
		#endif
		}

	} else {
		errorstream << "Failed to get paths by executable location, "
			"trying cwd" << std::endl;

		if (!getCurrentWorkingDir(buf, sizeof(buf)))
			FATAL_ERROR("Ran out of methods to get paths");

		size_t cwdlen = strlen(buf);
		if (cwdlen >= 1 && buf[cwdlen - 1] == DIR_DELIM_CHAR) {
			cwdlen--;
			buf[cwdlen] = '\0';
		}

		if (cwdlen >= 4 && !strcmp(buf + cwdlen - 4, DIR_DELIM "bin"))
			pathRemoveFile(buf, DIR_DELIM_CHAR);

		std::string execpath(buf);

		path_share = execpath;
		path_user  = execpath;
	}
#else
	infostream << "Using system-wide paths (NOT RUN_IN_PLACE)" << std::endl;

	if (!setSystemPaths())
		errorstream << "Failed to get one or more system-wide path" << std::endl;

#endif

	infostream << "Detected share path: " << path_share << std::endl;
	infostream << "Detected user path: " << path_user << std::endl;

	bool found_localedir = false;
#ifdef STATIC_LOCALEDIR
	if (STATIC_LOCALEDIR[0] && fs::PathExists(STATIC_LOCALEDIR)) {
		found_localedir = true;
		path_locale = STATIC_LOCALEDIR;
		infostream << "Using locale directory " << STATIC_LOCALEDIR << std::endl;
	} else {
		path_locale = getDataPath("locale");
		if (fs::PathExists(path_locale)) {
			found_localedir = true;
			infostream << "Using in-place locale directory " << path_locale
				<< " even though a static one was provided "
				<< "(RUN_IN_PLACE or CUSTOM_LOCALEDIR)." << std::endl;
		}
	}
#else
	path_locale = getDataPath("locale");
	if (fs::PathExists(path_locale)) {
		found_localedir = true;
	}
#endif
	if (!found_localedir) {
		errorstream << "Couldn't find a locale directory!" << std::endl;
	}

}



void setXorgClassHint(const video::SExposedVideoData &video_data,
	const std::string &name)
{
#ifdef XORG_USED
	if (video_data.OpenGLLinux.X11Display == NULL)
		return;

	XClassHint *classhint = XAllocClassHint();
	classhint->res_name  = (char *)name.c_str();
	classhint->res_class = (char *)name.c_str();

	XSetClassHint((Display *)video_data.OpenGLLinux.X11Display,
		video_data.OpenGLLinux.X11Window, classhint);
	XFree(classhint);
#endif
}


////
//// Video/Display Information (Client-only)
////

#ifndef SERVER

static irr::IrrlichtDevice *device;

void initIrrlicht(irr::IrrlichtDevice *device_)
{
	device = device_;
}

v2u32 getWindowSize()
{
	return device->getVideoDriver()->getScreenSize();
}


std::vector<core::vector3d<u32> > getSupportedVideoModes()
{
	IrrlichtDevice *nulldevice = createDevice(video::EDT_NULL);
	sanity_check(nulldevice != NULL);

	std::vector<core::vector3d<u32> > mlist;
	video::IVideoModeList *modelist = nulldevice->getVideoModeList();

	u32 num_modes = modelist->getVideoModeCount();
	for (u32 i = 0; i != num_modes; i++) {
		core::dimension2d<u32> mode_res = modelist->getVideoModeResolution(i);
		s32 mode_depth = modelist->getVideoModeDepth(i);
		mlist.push_back(core::vector3d<u32>(mode_res.Width, mode_res.Height, mode_depth));
	}

	nulldevice->drop();

	return mlist;
}

std::vector<irr::video::E_DRIVER_TYPE> getSupportedVideoDrivers()
{
	std::vector<irr::video::E_DRIVER_TYPE> drivers;

	for (int i = 0; i != irr::video::EDT_COUNT; i++) {
		if (irr::IrrlichtDevice::isDriverSupported((irr::video::E_DRIVER_TYPE)i))
			drivers.push_back((irr::video::E_DRIVER_TYPE)i);
	}

	return drivers;
}

const char *getVideoDriverName(irr::video::E_DRIVER_TYPE type)
{
	static const char *driver_ids[] = {
		"null",
		"software",
		"burningsvideo",
		"direct3d8",
		"direct3d9",
		"opengl",
		"ogles1",
		"ogles2",
	};

	return driver_ids[type];
}


const char *getVideoDriverFriendlyName(irr::video::E_DRIVER_TYPE type)
{
	static const char *driver_names[] = {
		"NULL Driver",
		"Software Renderer",
		"Burning's Video",
		"Direct3D 8",
		"Direct3D 9",
		"OpenGL",
		"OpenGL ES1",
		"OpenGL ES2",
	};

	return driver_names[type];
}

#	ifndef __ANDROID__
#		if defined(WTF) && defined(XORG_USED)

static float calcDisplayDensity()
{
	const char *current_display = getenv("DISPLAY");

	if (current_display != NULL) {
		Display *x11display = XOpenDisplay(current_display);

		if (x11display != NULL) {
			/* try x direct */
			float dpi_height = floor(DisplayHeight(x11display, 0) /
							(DisplayHeightMM(x11display, 0) * 0.039370) + 0.5);
			float dpi_width = floor(DisplayWidth(x11display, 0) /
							(DisplayWidthMM(x11display, 0) * 0.039370) + 0.5);

			XCloseDisplay(x11display);

			return std::max(dpi_height,dpi_width) / 96.0;
		}
	}

	/* return manually specified dpi */
	return get_dpi()/96.0;
}


float getDisplayDensity()
{
	static float cached_display_density = calcDisplayDensity();
	return cached_display_density;
}

#		else // XORG_USED
float getDisplayDensity()
{
	return get_dpi()/96.0;
}
#		endif // XORG_USED

v2u32 getDisplaySize()
{
	IrrlichtDevice *nulldevice = createDevice(video::EDT_NULL);

	core::dimension2d<u32> deskres = nulldevice->getVideoModeList()->getDesktopResolution();
	nulldevice -> drop();

	return deskres;
}

float get_dpi() {
	return g_settings->getFloat("screen_dpi");
}

int get_densityDpi() { return 0; }

#	endif // __ANDROID__
#endif // SERVER

} //namespace porting


extern "C" unsigned int get_time_us() {
	return porting::getTimeUs();
}
