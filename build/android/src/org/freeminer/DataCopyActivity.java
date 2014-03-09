package org.freeminer;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.widget.ProgressBar;

import java.io.IOException;


public class DataCopyActivity extends Activity {
    private static final String TAG = "DataCopyActivity";

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.data_copy);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        ProgressBar progress = (ProgressBar)findViewById(R.id.progressBar);
        try {
            progress.setMax(Util.getNumberOfFilesToCopy(getAssets()));
        } catch (IOException e) {
            // can't even get number of files to copy
            return;
        }
        new CopyFilesTask(getApplicationContext(), this).execute();
    }
}