package com.pwx.client;

public final class NativeBridge {
    static {
        // SDLActivity loads these too; System.loadLibrary is reference counted, so the
        // explicit load here only makes the bridge usable before the activity starts.
        System.loadLibrary("SDL3");
        System.loadLibrary("main");
    }

    private NativeBridge() {
    }

    public static native String buildInfo();

    /** Routes the native log into the same file PwxLog writes to. */
    public static native void setLogFile(String path);
}
