package org.freeminer;

import android.content.res.AssetManager;

import java.io.*;

public class Util {
    public static int readNumber(InputStream is) {
        BufferedReader r = new BufferedReader(new InputStreamReader(is));
        String s = null;
        try {
            s = r.readLine();
        } catch (IOException e) {
            return 0;
        }
        return Integer.parseInt(s);
    }

    public static int getInstalledVersion(File shareRoot) {
        int version = 0;

        File version_txt = new File(shareRoot, "version.txt");
        try {
            FileInputStream fis = new FileInputStream(version_txt);
            version = readNumber(fis);
        } catch (IOException e) {
            version = 0;
        }

        return version;
    }

    public static int getAssetsVersion(AssetManager am) {
        int version = 0;

        try {
            InputStream is = am.open("share/version.txt");
            version = readNumber(is);
        } catch (IOException e) {
            e.printStackTrace();
        }

        return version;
    }

    public static int getNumberOfFilesToCopy(AssetManager am) throws IOException {
        InputStream is = am.open("share/count.txt");
        return readNumber(is);
    }
}
