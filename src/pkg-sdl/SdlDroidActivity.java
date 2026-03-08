package com.tx;

import android.content.pm.PackageManager;
// import android.view.View;
// import android.view.WindowManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

public class SdlDroidActivity extends SDLActivity {
    private final String TAG = "sdl.A";

    // Using the same <meta-data> notation as NativeActivity for convenience
    public static final String META_DATA_LIB_NAME = "android.app.lib_name";

    private String _libname = "app";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "->->->-> onCreate");

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
    }

    protected String[] getLibraries() {
        return new String[] { 
            _libname  // SDL3 is statically linked, so we don't need to specify "SDL3" here, just the main library that contains SDL_main
        };
    }
}
