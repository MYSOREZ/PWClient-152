package com.pwx.client;

import android.content.Context;
import android.os.Build;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * File logger with an on-device location the tester can reach without adb.
 *
 * Preferred:  /sdcard/Download/PWX/logs/pwx-<stamp>.log   (needs All-files access)
 * Fallback :  /sdcard/Android/data/com.pwx.client/files/logs/pwx-<stamp>.log
 * Last     :  /data/data/com.pwx.client/files/logs/...
 *
 * Every line is mirrored to logcat under the "PWX" tag.
 */
public final class PwxLog {
    private static final String TAG = "PWX";
    private static final SimpleDateFormat TS =
            new SimpleDateFormat("HH:mm:ss.SSS", Locale.US);

    private static PrintWriter writer;
    private static File logFile;

    private PwxLog() {
    }

    public static synchronized void init(Context ctx, String buildStamp) {
        if (writer != null) {
            return;
        }
        File dir = pickDir(ctx);
        if (dir == null) {
            Log.e(TAG, "no writable log directory");
            return;
        }
        String name = "pwx-" + new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US)
                .format(new Date()) + ".log";
        logFile = new File(dir, name);
        try {
            writer = new PrintWriter(new FileWriter(logFile, true), true);
        } catch (IOException e) {
            Log.e(TAG, "cannot open log file " + logFile, e);
            logFile = null;
            return;
        }
        write("I", "PwxLog", "log file: " + logFile.getAbsolutePath());
        write("I", "PwxLog", "build: " + buildStamp);
        write("I", "PwxLog", "device: " + Build.MANUFACTURER + " " + Build.MODEL
                + " | android " + Build.VERSION.RELEASE + " (SDK " + Build.VERSION.SDK_INT + ")"
                + " | abis " + String.join(",", Build.SUPPORTED_ABIS));
        write("I", "PwxLog", "all-files-access: " + hasPublicAccess());
    }

    /**
     * Crashes must land in the log file: the tester reports "it died", and the file has to
     * explain who died and where. The previous handler still runs, so logcat keeps its FATAL.
     */
    public static void installCrashHandler() {
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            write("E", "CRASH", "uncaught in thread " + thread.getName() + ": " + error);
            synchronized (PwxLog.class) {
                if (writer != null) {
                    error.printStackTrace(writer);
                    writer.flush();
                }
            }
            if (previous != null) {
                previous.uncaughtException(thread, error);
            }
        });
    }

    public static synchronized String path() {        return logFile == null ? "(none)" : logFile.getAbsolutePath();
    }

    public static synchronized String dirPath() {
        return logFile == null ? "(none)" : logFile.getParent();
    }

    public static void i(String tag, String msg) {
        write("I", tag, msg);
    }

    public static void w(String tag, String msg) {
        write("W", tag, msg);
    }

    public static void e(String tag, String msg) {
        write("E", tag, msg);
    }

    public static void e(String tag, String msg, Throwable t) {
        write("E", tag, msg + " :: " + t);
        synchronized (PwxLog.class) {
            if (writer != null) {
                t.printStackTrace(writer);
                writer.flush();
            }
        }
    }

    private static synchronized void write(String level, String tag, String msg) {
        String line = "[" + TS.format(new Date()) + "] " + level + "/" + tag + ": " + msg;
        switch (level) {
            case "E":
                Log.e(TAG, msg);
                break;
            case "W":
                Log.w(TAG, msg);
                break;
            default:
                Log.i(TAG, msg);
                break;
        }
        if (writer != null) {
            writer.println(line);
            writer.flush();
        }
    }

    private static boolean hasPublicAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        return true;
    }

    private static File pickDir(Context ctx) {
        if (hasPublicAccess()) {
            File pub = new File(Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_DOWNLOADS), "PWX/logs");
            if (ensureWritable(pub)) {
                return pub;
            }
        }
        File ext = ctx.getExternalFilesDir("logs");
        if (ext != null && ensureWritable(ext)) {
            return ext;
        }
        File in = new File(ctx.getFilesDir(), "logs");
        return ensureWritable(in) ? in : null;
    }

    private static boolean ensureWritable(File dir) {
        if (!dir.exists() && !dir.mkdirs()) {
            return false;
        }
        File probe = new File(dir, ".probe");
        try {
            if (!probe.createNewFile() && !probe.exists()) {
                return false;
            }
            return probe.delete() || true;
        } catch (IOException e) {
            return false;
        }
    }
}
