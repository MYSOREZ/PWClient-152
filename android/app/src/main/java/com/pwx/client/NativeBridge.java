package com.pwx.client;

public final class NativeBridge {
    static {
        System.loadLibrary("pwx");
    }

    private NativeBridge() {
    }

    public static native String buildInfo();
}
