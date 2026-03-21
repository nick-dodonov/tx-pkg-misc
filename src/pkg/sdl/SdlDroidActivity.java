package com.tx;

import android.content.pm.PackageManager;
import android.view.View;
// import android.view.WindowManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

public class SdlDroidActivity extends SDLActivity {
    private final String TAG = "sdl.A";

    // Using the same <meta-data> notation as NativeActivity for convenience
    public static final String META_DATA_LIB_NAME = "android.app.lib_name";

    private String _libname = "app";

    @Override // SDLActivity method
    protected String[] getLibraries() {
        return new String[] { 
            _libname  // SDL3 is statically linked, so we don't need to specify "SDL3" here, just the main library that contains SDL_main
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "-> onCreate");

        // Get the library name from the manifest meta-data, defaulting to "app" if not specified
        try {
            var activityInfo = getPackageManager().getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
            var metaData = activityInfo.metaData;
            if (metaData != null) {
                String ln = metaData.getString(META_DATA_LIB_NAME);
                if (ln != null) {
                    _libname = ln;
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Error getting activity info", e);
        }

        super.onCreate(savedInstanceState);

        setImmersiveSticky();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "<- onDestroy");
        super.onDestroy();

        //TODO: share w/ configurable impl in DroidActivity.java and exitCode support
        //  required for current droid.py runner implementation to detect finish now
        System.exit(0);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)  {
            setImmersiveSticky();
        }
    }

    void setImmersiveSticky() {
        // SdlActivity do itself using settings
        // View decorView = getWindow().getDecorView();
        // decorView.setSystemUiVisibility(
        //     View.SYSTEM_UI_FLAG_FULLSCREEN
        //     | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        //     | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        //     | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        //     | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        //     | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        // );
    }
}
