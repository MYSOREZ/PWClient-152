# Android bring-up shell

Console-only build of the PWX Android port. Android Studio is not used; the IDE is only
the source of the SDK/JDK that these scripts point at.

## Status

SDL3 + GLES 3.0 bring-up shell: `SDLActivity` owns the window/context/input, `libmain.so`
runs the render loop, draws a rotating triangle through a real GLES 3.0 shader pipeline,
and writes both Java and native lines into one log file.

Verified on the target device (Poco F8 Pro / 2510DPC44G, Android 16, Adreno 830):
APK installs and the triangle renders. The engine is not integrated yet.

## Toolchain (this machine)

| Component | Path |
|---|---|
| Android SDK | `D:\Android\Sdk` |
| NDK | `D:\Android\Sdk\ndk\30.0.16248370` |
| CMake | `D:\Android\Sdk\cmake\3.22.1` |
| JDK | `D:\Android Studio\Canary\jbr` (JDK 21) |
| Gradle | `tools/gradle/gradle-8.14.3` (downloaded on first build) |

## Build

```powershell
.\scripts\android\build-apk.ps1
```

Result: `apk\PWX-<stamp>-debug.apk`. The script keeps only the newest APK in `apk\`.

Release variant (debug-signed, for testing only):

```powershell
.\scripts\android\build-apk.ps1 -Release
```

## Install and watch

```powershell
.\scripts\android\install-apk.ps1
```

Installs over the previous build, grants All-files access via `appops`, starts the app and
streams `logcat` filtered to the `PWX` tags.

## Where the logs go

1. `/sdcard/Download/PWX/logs/pwx-<stamp>.log` — preferred, visible in any file manager.
   Requires All-files access; the app asks once on first launch, and the install script grants
   it with `adb shell appops set com.pwx.client MANAGE_EXTERNAL_STORAGE allow`.
2. `/sdcard/Android/data/com.pwx.client/files/logs/` — fallback when access is denied.
3. `/data/data/com.pwx.client/files/logs/` — last resort.

Pull them with:

```powershell
.\scripts\android\pull-logs.ps1
```

## What is logged

- build stamp, device model, Android version, ABI list;
- native build info (ABI, pointer width, `__ANDROID_API__`, C++ standard);
- `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, `GLSL` version;
- surface size changes, first frame, periodic FPS;
- activity lifecycle (`, onResume`, `onPause`, `onDestroy`) — enough to tell a crash from a
  backgrounding kill.

## Layout

```
android/
  app/src/main/java/com/pwx/client/   MainActivity, PwxLog, NativeBridge
  app/src/main/cpp/                   CMake + JNI stubs
  app/src/main/AndroidManifest.xml
scripts/android/                      build-apk, install-apk, pull-logs
apk/                                  newest APK (convention inherited from GeneralsZH port)
```
