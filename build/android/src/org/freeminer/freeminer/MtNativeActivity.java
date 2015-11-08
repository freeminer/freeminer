package org.freeminer.freeminer;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

public class MtNativeActivity extends NativeActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		m_MessagReturnCode = -1;
		m_MessageReturnValue = "";

	}

	@Override
	public void onDestroy() {
		super.onDestroy();
	}


	public void copyAssets() {
		Intent intent = new Intent(this, freeminerAssetCopy.class);
		startActivity(intent);
	}

	public void showDialog(String acceptButton, String hint, String current,
			int editType) {

		Intent intent = new Intent(this, freeminerTextEntry.class);
		Bundle params = new Bundle();
		params.putString("acceptButton", acceptButton);
		params.putString("hint", hint);
		params.putString("current", current);
		params.putInt("editType", editType);
		intent.putExtras(params);
		startActivityForResult(intent, 101);
		m_MessageReturnValue = "";
		m_MessagReturnCode   = -1;
	}

	public static native void putMessageBoxResult(String text);

	/* ugly code to workaround putMessageBoxResult not beeing found */
	public int getDialogState() {
		return m_MessagReturnCode;
	}

	public String getDialogValue() {
		m_MessagReturnCode = -1;
		return m_MessageReturnValue;
	}

	public float getDensity() {
		return getResources().getDisplayMetrics().density;
	}

	public float get_scaledDensity() {
		return getResources().getDisplayMetrics().scaledDensity;
	}

	public float get_xdpi() {
		return getResources().getDisplayMetrics().xdpi;
	}

	public float get_ydpi() {
		return getResources().getDisplayMetrics().ydpi;
	}

	public int get_densityDpi() {
		return getResources().getDisplayMetrics().densityDpi;
	}

	public int getDisplayWidth() {
		return getResources().getDisplayMetrics().widthPixels;
	}

	public int getDisplayHeight() {
		return getResources().getDisplayMetrics().heightPixels;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode,
			Intent data) {
		if (requestCode == 101) {
			if (resultCode == RESULT_OK) {
				String text = data.getStringExtra("text");
				m_MessagReturnCode = 0;
				m_MessageReturnValue = text;
			}
			else {
				m_MessagReturnCode = 1;
			}
		}
	}

	static {
		System.loadLibrary("openal");
		System.loadLibrary("ogg");
		System.loadLibrary("vorbis");
		System.loadLibrary("ssl");
		System.loadLibrary("crypto");
		System.loadLibrary("gmp");
		//System.loadLibrary("iconv");

		// We don't have to load libminetest.so ourselves,
		// but if we do, we get nicer logcat errors when
		// loading fails.
		System.loadLibrary("freeminer");
	}

	private int m_MessagReturnCode;
	private String m_MessageReturnValue;
}
