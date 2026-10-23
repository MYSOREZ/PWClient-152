package com.pwx.client;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import org.libsdl.app.SDLActivity;

/**
 * Game activity. SDL owns the window, the GLES 3.0 context, input and the render thread;
 * this class only prepares logging before SDL starts and hands the log path to native code.
 *
 * The demo clears the screen and draws a spinning triangle, which proves the shader
 * pipeline end to end. The Perfect World renderer replaces it later.
 */
public class MainActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        PwxLog.init(this, BuildConfig.BUILD_STAMP);
        PwxLog.installCrashHandler();
        PwxLog.i("Main", "activity onCreate");

        ensurePublicLogAccess();

        // Debug-only self-test: proves the crash handler reaches the log file.
        // adb shell touch /sdcard/Download/PWX/crash-test, then relaunch.
        if (BuildConfig.DEBUG && new java.io.File(
                Environment.getExternalStorageDirectory(), "Download/PWX/crash-test").exists()) {
            PwxLog.i("Main", "crash self-test triggered by Download/PWX/crash-test");
            throw new RuntimeException("PWX crash self-test");
        }

        super.onCreate(savedInstanceState);

        try {
            PwxLog.i("Main", "native: " + NativeBridge.buildInfo());
            NativeBridge.setLogFile(PwxLog.path());
            PwxLog.i("Main", "native log attached to " + PwxLog.path());
        } catch (Throwable t) {
            PwxLog.e("Main", "native bridge failed", t);
        }
    }

    @Override
    protected void onResume() {
        PwxLog.i("Main", "onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        PwxLog.i("Main", "onPause");
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        PwxLog.i("Main", "onDestroy");
        super.onDestroy();
    }

    /**
     * Logs land in Download/PWX/logs when All-files access is granted.
     * Otherwise Android keeps them in the app-specific external dir, which is harder to reach.
     */
    private void ensurePublicLogAccess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        if (Environment.isExternalStorageManager()) {
            return;
        }
        PwxLog.w("Main", "no All-files access; asking once (log currently at " + PwxLog.path() + ")");
        try {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            intent.setData(Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        } catch (Exception e) {
            PwxLog.w("Main", "cannot open All-files settings: " + e);
        }
    }
}
