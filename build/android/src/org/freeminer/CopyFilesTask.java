package org.freeminer;

import android.app.Activity;
import android.content.Context;
import android.content.res.AssetManager;
import android.os.AsyncTask;
import android.os.Environment;
import android.util.Log;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.*;


public class CopyFilesTask extends AsyncTask<Void, String, Void> {
    private static final String TAG = "CopyFilesTask";

    private Activity activity;
    private Context context;
    private AssetManager am;
    private File freeminerRoot;
    private int copiedFiles;

    /**
     * Copy the asset at the specified path to this app's data directory. If the
     * asset is a directory, its contents are also copied.
     *
     * @param path
     * Path to asset, relative to app's assets directory.
     */
    private void copyAsset(String path) {
        // If we have a directory, we make it and recurse. If a file, we copy its
        // contents.
        try {
            String[] contents = this.am.list(path);

            // The documentation suggests that list throws an IOException, but doesn't
            // say under what conditions. It'd be nice if it did so when the path was
            // to a file. That doesn't appear to be the case. If the returned array is
            // null or has 0 length, we assume the path is to a file. This means empty
            // directories will get turned into files.
            if (contents == null || contents.length == 0)
                throw new IOException();

            // Make the directory.
            File dir = new File(freeminerRoot, path);
            dir.mkdirs();

            // Recurse on the contents.
            for (String entry : contents) {
                copyAsset(path + "/" + entry);
            }
        } catch (IOException e) {
            copyFileAsset(path);
        }
    }

    /**
     * Copy the asset file specified by path to app's data directory. Assumes
     * parent directories have already been created.
     *
     * @param path
     * Path to asset, relative to app's assets directory.
     */
    private void copyFileAsset(String path) {
        if (path.equals("share/version.txt") || path.equals("share/count.txt"))
            // make sure to only copy it after everything else is copied
            return;
        Log.d(TAG, path);
        File file = new File(freeminerRoot, path);
        try {
            InputStream in = this.am.open(path);
            OutputStream out = new FileOutputStream(file);
            byte[] buffer = new byte[1024];
            int read = in.read(buffer);
            while (read != -1) {
                out.write(buffer, 0, read);
                read = in.read(buffer);
            }
            out.close();
            in.close();
        } catch (IOException e) {
            Log.e(TAG, e.toString());
        }

        publishProgress(Integer.toString(copiedFiles), path.replace("share/", ""));
        copiedFiles++;
    }

    void DeleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory())
            for (File child : fileOrDirectory.listFiles())
                DeleteRecursive(child);

        fileOrDirectory.delete();
    }

    public CopyFilesTask(Context context, Activity activity) {
        this.context = context;
        this.activity = activity;
    }

    protected Void doInBackground(Void... nothing) {
        this.am = this.context.getAssets();
        this.freeminerRoot = new File(Environment.getExternalStorageDirectory(), "freeminer");
        File shareRoot = new File(freeminerRoot, "share");

        int assetsVersion = Util.getAssetsVersion(am);
        int installedVersion = Util.getInstalledVersion(shareRoot);

        if (assetsVersion == installedVersion)
            return null;

        publishProgress("0", "Cleaning up older installation...");
        this.DeleteRecursive(shareRoot);
        shareRoot.mkdirs();

        copiedFiles = 0;
        copyAsset("share");

        // write version we just installed to version.txt
        File newTextFile = new File(shareRoot, "version.txt");
        FileWriter fw = null;
        try {
            fw = new FileWriter(newTextFile);
            fw.write(Integer.toString(assetsVersion));
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            fw.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    protected void onProgressUpdate(String... pr) {
        ProgressBar progress = (ProgressBar)activity.findViewById(R.id.progressBar);
        progress.setProgress(Integer.parseInt(pr[0]));
        TextView text = (TextView)activity.findViewById(R.id.fileName);
        text.setText(pr[1]);
    }

    protected void onPostExecute(Void result) {
        this.activity.finish();
    }
}