/*
Minetest
Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __ANDROID__
#error This file may only be compiled for android!
#endif

#include "util/numeric.h"
#include "porting.h"
#include "porting_android.h"
#include "threading/thread.h"
#include "config.h"
#include "filesys.h"
#include "log.h"

#include <sstream>
#include <exception>
#include <stdlib.h>

#include "settings.h"

#ifdef GPROF
#include "prof.h"
#endif

extern int main(int argc, char *argv[]);

void android_main(android_app *app)
{
	int retval = 0;
	porting::app_global = app;

	Thread::setName("Main");

	try {
		app_dummy();
		char *argv[] = {strdup(PROJECT_NAME), NULL};
		main(ARRLEN(argv) - 1, argv);
		free(argv[0]);
	} catch (std::exception &e) {
		errorstream << "Uncaught exception in main thread: " << e.what() << std::endl;
		retval = -1;
	} catch (...) {
		errorstream << "Uncaught exception in main thread!" << std::endl;
		retval = -1;
	}

	porting::cleanupAndroid();
	infostream << "Shutting down." << std::endl;
	exit(retval);
}

/* handler for finished message box input */
/* Intentionally NOT in namespace porting */
/* TODO this doesn't work as expected, no idea why but there's a workaround   */
/* for it right now */
extern "C" {
	JNIEXPORT void JNICALL Java_org_freeminer_MtNativeActivity_putMessageBoxResult(
			JNIEnv * env, jclass thiz, jstring text)
	{
		errorstream << "Java_net_freeminer_MtNativeActivity_putMessageBoxResult got: "
				<< std::string((const char*)env->GetStringChars(text,0))
				<< std::endl;
	}
}

namespace porting {

std::string path_storage = DIR_DELIM "sdcard" DIR_DELIM;

android_app* app_global;
JNIEnv*      jnienv;
jclass       nativeActivity;

void handleAndroidActivityEvents(int max)
{
	int ident;
	int events;
	struct android_poll_source *source;

	while ( (ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
		if (source)
			source->process(porting::app_global, source);
		if (--max < 0)
			break;
	}
}

int android_version_sdk_int = 0;

jclass findClass(std::string classname)
{
	if (jnienv == 0) {
		return 0;
	}

	jclass nativeactivity = jnienv->FindClass("android/app/NativeActivity");
	jmethodID getClassLoader =
			jnienv->GetMethodID(nativeactivity,"getClassLoader",
					"()Ljava/lang/ClassLoader;");
	jobject cls =
			jnienv->CallObjectMethod(app_global->activity->clazz, getClassLoader);
	jclass classLoader = jnienv->FindClass("java/lang/ClassLoader");
	jmethodID findClass =
			jnienv->GetMethodID(classLoader, "loadClass",
					"(Ljava/lang/String;)Ljava/lang/Class;");
	jstring strClassName =
			jnienv->NewStringUTF(classname.c_str());
	return (jclass) jnienv->CallObjectMethod(cls, findClass, strClassName);
}

void copyAssets()
{
	jmethodID assetcopy = jnienv->GetMethodID(nativeActivity,"copyAssets","()V");

	if (assetcopy == 0) {
		assert("porting::copyAssets unable to find copy assets method" == 0);
	}

	jnienv->CallVoidMethod(app_global->activity->clazz, assetcopy);
}

void initAndroid()
{
	porting::jnienv = NULL;
	JavaVM *jvm = app_global->activity->vm;
	JavaVMAttachArgs lJavaVMAttachArgs;
	lJavaVMAttachArgs.version = JNI_VERSION_1_6;
	lJavaVMAttachArgs.name = PROJECT_NAME_C "NativeThread";
	lJavaVMAttachArgs.group = NULL;
#ifdef NDEBUG
	// This is a ugly hack as arm v7a non debuggable builds crash without this
	// printf ... if someone finds out why please fix it!
	infostream << "Attaching native thread. " << std::endl;
#endif
	if ( jvm->AttachCurrentThread(&porting::jnienv, &lJavaVMAttachArgs) == JNI_ERR) {
		errorstream << "Failed to attach native thread to jvm" << std::endl;
		exit(-1);
	}

	nativeActivity = findClass("org/freeminer/" PROJECT_NAME_C "/MtNativeActivity");
	if (nativeActivity == 0) {
		errorstream <<
			"porting::initAndroid unable to find java native activity class" <<
			std::endl;
	}

#ifdef GPROF
	/* in the start-up code */
	__android_log_print(ANDROID_LOG_ERROR, PROJECT_NAME_C,
			"Initializing GPROF profiler");
	monstartup("libfreeminer.so");
#endif

	{
		// https://code.google.com/p/android/issues/detail?id=40753
		// http://stackoverflow.com/questions/10196361/how-to-check-the-device-running-api-level-using-c-code-via-ndk
		// http://developer.android.com/reference/android/os/Build.VERSION_CODES.html#JELLY_BEAN_MR2
		jclass versionClass = porting::jnienv->FindClass("android/os/Build$VERSION");
		if (versionClass) {
			jfieldID sdkIntFieldID = porting::jnienv->GetStaticFieldID(versionClass, "SDK_INT", "I");
			if (sdkIntFieldID) {
				android_version_sdk_int = porting::jnienv->GetStaticIntField(versionClass, sdkIntFieldID);
				infostream << "Android version = "<< android_version_sdk_int << std::endl;
			}
		}
	}

}


void cleanupAndroid()
{

#ifdef GPROF
	errorstream << "Shutting down GPROF profiler" << std::endl;
	setenv("CPUPROFILE", (path_user + DIR_DELIM + "gmon.out").c_str(), 1);
	moncleanup();
#endif

	JavaVM *jvm = app_global->activity->vm;
	if (jvm)
	jvm->DetachCurrentThread();
	ANativeActivity_finish(app_global->activity);
}

void setExternalStorageDir(JNIEnv* lJNIEnv)
{
	// Android: Retrieve ablsolute path to external storage device (sdcard)
	jclass ClassEnv      = lJNIEnv->FindClass("android/os/Environment");
	jmethodID MethodDir  =
			lJNIEnv->GetStaticMethodID(ClassEnv,
					"getExternalStorageDirectory","()Ljava/io/File;");
	jobject ObjectFile   = lJNIEnv->CallStaticObjectMethod(ClassEnv, MethodDir);
	jclass ClassFile     = lJNIEnv->FindClass("java/io/File");

	jmethodID MethodPath =
			lJNIEnv->GetMethodID(ClassFile, "getAbsolutePath",
					"()Ljava/lang/String;");
	jstring StringPath   =
			(jstring) lJNIEnv->CallObjectMethod(ObjectFile, MethodPath);

	const char *externalPath = lJNIEnv->GetStringUTFChars(StringPath, NULL);
	std::string userPath(externalPath);
	lJNIEnv->ReleaseStringUTFChars(StringPath, externalPath);

	path_storage             = userPath;
	path_user                = userPath + DIR_DELIM + PROJECT_NAME;
	path_share               = userPath + DIR_DELIM + PROJECT_NAME;
	path_locale              = path_share + DIR_DELIM + "locale";
}

void showInputDialog(const std::string& acceptButton, const  std::string& hint,
		const std::string& current, int editType)
{
	jmethodID showdialog = jnienv->GetMethodID(nativeActivity,"showDialog",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

	if (showdialog == 0) {
		assert("porting::showInputDialog unable to find java show dialog method" == 0);
	}

	jstring jacceptButton = jnienv->NewStringUTF(acceptButton.c_str());
	jstring jhint         = jnienv->NewStringUTF(hint.c_str());
	jstring jcurrent      = jnienv->NewStringUTF(current.c_str());
	jint    jeditType     = editType;

	jnienv->CallVoidMethod(app_global->activity->clazz, showdialog,
			jacceptButton, jhint, jcurrent, jeditType);
}

int getInputDialogState()
{
	jmethodID dialogstate = jnienv->GetMethodID(nativeActivity,
			"getDialogState", "()I");

	if (dialogstate == 0) {
		assert("porting::getInputDialogState unable to find java dialog state method" == 0);
	}

	return jnienv->CallIntMethod(app_global->activity->clazz, dialogstate);
}

std::string getInputDialogValue()
{
	jmethodID dialogvalue = jnienv->GetMethodID(nativeActivity,
			"getDialogValue", "()Ljava/lang/String;");

	if (dialogvalue == 0) {
		assert("porting::getInputDialogValue unable to find java dialog value method" == 0);
	}

	jobject result = jnienv->CallObjectMethod(app_global->activity->clazz,
			dialogvalue);

	const char* javachars = jnienv->GetStringUTFChars((jstring) result,0);
	std::string text(javachars);
	jnienv->ReleaseStringUTFChars((jstring) result, javachars);

	return text;
}

#ifndef SERVER
float getDisplayDensity()
{
	static bool firstrun = true;
	static float value = 0;

	if (firstrun) {
		jmethodID getDensity = jnienv->GetMethodID(nativeActivity, "getDensity",
					"()F");

		if (getDensity == 0) {
			assert("porting::getDisplayDensity unable to find java getDensity method" == 0);
		}

		value = jnienv->CallFloatMethod(app_global->activity->clazz, getDensity);
		firstrun = false;
	}
	return value;
}

float get_dpi() {
	static bool firstrun = true;
	static float value = 0;

	if (firstrun) {
		auto method = jnienv->GetMethodID(nativeActivity, "get_ydpi", "()F");

		if (!method)
			return 160;

		value = jnienv->CallFloatMethod(app_global->activity->clazz, method);
		firstrun = false;
	}
	return value;
}

int get_densityDpi() {
	static bool firstrun = true;
	static int value = 0;

	if (firstrun) {
		auto method = jnienv->GetMethodID(nativeActivity, "get_densityDpi", "()I");

		if (!method)
			return 160;

		value = jnienv->CallFloatMethod(app_global->activity->clazz, method);
		firstrun = false;
	}
	return value;
}

v2u32 getDisplaySize()
{
	static bool firstrun = true;
	static v2u32 retval;

	if (firstrun) {
		jmethodID getDisplayWidth = jnienv->GetMethodID(nativeActivity,
				"getDisplayWidth", "()I");

		if (getDisplayWidth == 0) {
			assert("porting::getDisplayWidth unable to find java getDisplayWidth method" == 0);
		}

		retval.X = jnienv->CallIntMethod(app_global->activity->clazz,
				getDisplayWidth);

		jmethodID getDisplayHeight = jnienv->GetMethodID(nativeActivity,
				"getDisplayHeight", "()I");

		if (getDisplayHeight == 0) {
			assert("porting::getDisplayHeight unable to find java getDisplayHeight method" == 0);
		}

		retval.Y = jnienv->CallIntMethod(app_global->activity->clazz,
				getDisplayHeight);

		firstrun = false;
	}
	return retval;
}
#endif // ndef SERVER


int canKeyboard() {
	auto v = g_settings->getS32("android_keyboard");
	if (v)
		return v;
	// dont work on some 4.4.2
	//if (porting::android_version_sdk_int >= 18)
	//	return 1;
	return false;
}

// http://stackoverflow.com/questions/5864790/how-to-show-the-soft-keyboard-on-native-activity
void displayKeyboard(bool pShow, android_app* mApplication, JNIEnv* lJNIEnv) {
    jint lFlags = 0;

    // Retrieves NativeActivity.
    jobject lNativeActivity = mApplication->activity->clazz;
    jclass ClassNativeActivity = lJNIEnv->GetObjectClass(lNativeActivity);

    // Retrieves Context.INPUT_METHOD_SERVICE.
    jclass ClassContext = lJNIEnv->FindClass("android/content/Context");
    jfieldID FieldINPUT_METHOD_SERVICE = lJNIEnv->GetStaticFieldID(ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject INPUT_METHOD_SERVICE = lJNIEnv->GetStaticObjectField(ClassContext, FieldINPUT_METHOD_SERVICE);
    //jniCheck(INPUT_METHOD_SERVICE);

    // Runs getSystemService(Context.INPUT_METHOD_SERVICE).
    jclass ClassInputMethodManager = lJNIEnv->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID MethodGetSystemService = lJNIEnv->GetMethodID(ClassNativeActivity, "getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
    jobject lInputMethodManager = lJNIEnv->CallObjectMethod(lNativeActivity, MethodGetSystemService,INPUT_METHOD_SERVICE);

    // Runs getWindow().getDecorView().
    jmethodID MethodGetWindow = lJNIEnv->GetMethodID(ClassNativeActivity, "getWindow","()Landroid/view/Window;");
    jobject lWindow = lJNIEnv->CallObjectMethod(lNativeActivity,MethodGetWindow);
    jclass ClassWindow = lJNIEnv->FindClass("android/view/Window");
    jmethodID MethodGetDecorView = lJNIEnv->GetMethodID(ClassWindow, "getDecorView", "()Landroid/view/View;");
    jobject lDecorView = lJNIEnv->CallObjectMethod(lWindow,MethodGetDecorView);

    if (pShow) {
        // Runs lInputMethodManager.showSoftInput(...).
        jmethodID MethodShowSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "showSoftInput","(Landroid/view/View;I)Z");
        jboolean lResult = lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodShowSoftInput,lDecorView, lFlags);
    } else {
        // Runs lWindow.getViewToken()
        jclass ClassView = lJNIEnv->FindClass("android/view/View");
        jmethodID MethodGetWindowToken = lJNIEnv->GetMethodID(ClassView, "getWindowToken", "()Landroid/os/IBinder;");
        jobject lBinder = lJNIEnv->CallObjectMethod(lDecorView,MethodGetWindowToken);

        // lInputMethodManager.hideSoftInput(...).
        jmethodID MethodHideSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "hideSoftInputFromWindow","(Landroid/os/IBinder;I)Z");
        jboolean lRes = lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodHideSoftInput,lBinder, lFlags);
    }
}


}
