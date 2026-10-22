package com.pwx.client;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.graphics.Color;
import android.graphics.Typeface;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Bring-up shell: creates a GLES 3.0 surface, logs everything to a file the tester can reach,
 * and proves the NDK toolchain works. The real client replaces the renderer later.
 */
public class MainActivity extends Activity {

    private GLSurfaceView surface;
    private DemoRenderer renderer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        hideSystemUi();

        PwxLog.init(this, BuildConfig.BUILD_STAMP);
        PwxLog.i("Main", "native: " + NativeBridge.buildInfo());

        DisplayMetrics dm = getResources().getDisplayMetrics();
        PwxLog.i("Main", "screen: " + dm.widthPixels + "x" + dm.heightPixels
                + " density=" + dm.density + " dpi=" + dm.densityDpi);

        ensurePublicLogAccess();

        FrameLayout root = new FrameLayout(this);

        renderer = new DemoRenderer();
        surface = new GLSurfaceView(this);
        surface.setEGLContextClientVersion(3);
        surface.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
        surface.setRenderer(renderer);
        surface.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        root.addView(surface, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        TextView overlay = new TextView(this);
        overlay.setText("PWX " + BuildConfig.BUILD_STAMP + "\nlog: " + PwxLog.path());
        overlay.setTextColor(Color.argb(160, 255, 255, 255));
        overlay.setTextSize(10f);
        overlay.setTypeface(Typeface.MONOSPACE);
        overlay.setPadding(16, 16, 16, 16);
        root.addView(overlay, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        setContentView(root);
        PwxLog.i("Main", "activity created, waiting for first frame");
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (surface != null) {
            surface.onResume();
        }
        PwxLog.i("Main", "onResume");
    }

    @Override
    protected void onPause() {
        PwxLog.i("Main", "onPause");
        if (surface != null) {
            surface.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        PwxLog.i("Main", "onDestroy");
        super.onDestroy();
    }

    private void hideSystemUi() {
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
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

    private static final class DemoRenderer implements GLSurfaceView.Renderer {
        private int frames;
        private long startNs;
        private boolean firstFrameLogged;
        private boolean glInfoLogged;

        @Override
        public void onSurfaceCreated(GL10 unused, EGLConfig config) {
            startNs = System.nanoTime();
            PwxLog.i("GL", "surface created, EGL config=" + config);
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            android.opengl.GLES30.glViewport(0, 0, width, height);
            if (!glInfoLogged) {
                glInfoLogged = true;
                PwxLog.i("GL", "GL_VENDOR: " + android.opengl.GLES30.glGetString(android.opengl.GLES30.GL_VENDOR));
                PwxLog.i("GL", "GL_RENDERER: " + android.opengl.GLES30.glGetString(android.opengl.GLES30.GL_RENDERER));
                PwxLog.i("GL", "GL_VERSION: " + android.opengl.GLES30.glGetString(android.opengl.GLES30.GL_VERSION));
                PwxLog.i("GL", "GLSL: " + android.opengl.GLES30.glGetString(android.opengl.GLES30.GL_SHADING_LANGUAGE_VERSION));
            }
            PwxLog.i("GL", "surface changed: " + width + "x" + height);
        }

        @Override
        public void onDrawFrame(GL10 unused) {
            float t = (float) ((System.nanoTime() - startNs) / 1e9);
            android.opengl.GLES30.glClearColor(
                    0.10f + 0.05f * (float) Math.sin(t),
                    0.12f,
                    0.22f + 0.05f * (float) Math.cos(t),
                    1.0f);
            android.opengl.GLES30.glClear(android.opengl.GLES30.GL_COLOR_BUFFER_BIT);
            frames++;
            if (!firstFrameLogged) {
                firstFrameLogged = true;
                PwxLog.i("GL", "first frame drawn");
            }
            if (frames % 120 == 0) {
                double secs = (System.nanoTime() - startNs) / 1e9;
                PwxLog.i("GL", "frames=" + frames + " avg_fps=" + String.format(java.util.Locale.US, "%.1f", frames / secs));
            }
        }
    }
}
