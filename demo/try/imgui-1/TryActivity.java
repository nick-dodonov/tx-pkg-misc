package com.tx;

import org.libsdl.app.SDLActivity;

public class TryActivity extends SDLActivity {
    protected String[] getLibraries() {
        return new String[] { 
            //"SDL3",
            "imgui-1-droid-apk" // SDL3 is statically linked
        };
    }
}
