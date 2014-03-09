package org.freeminer;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class MainActivity extends Activity {
    private static final String TAG = "MainActivity";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = new Intent(this, DataCopyActivity.class);
        startActivityForResult(intent, 0);
	}

    protected void onActivityResult (int requestCode, int resultCode, Intent data) {
        Intent intent = new Intent(this, MyNativeActivity.class);
        startActivity(intent);
    }
}
