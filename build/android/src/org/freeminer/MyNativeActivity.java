package org.freeminer;

import android.app.NativeActivity;
import android.os.Environment;

import java.io.File;

public class MyNativeActivity extends NativeActivity {

    public String getRootDir() {
        File rootDir = new File(Environment.getExternalStorageDirectory(), "freeminer");
        return rootDir.getAbsolutePath();
    }
}
