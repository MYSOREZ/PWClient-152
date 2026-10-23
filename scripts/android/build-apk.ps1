# Builds the PWX Android APK from the console. No Android Studio involved.
#
#   .\scripts\android\build-apk.ps1            # debug APK
#   .\scripts\android\build-apk.ps1 -Release   # release APK (debug-signed)
#
# Output: apk\PWX-<stamp>-<variant>.apk

param(
    [switch]$Release
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$sdk = 'D:\Android\Sdk'
$jdk = 'D:\Android Studio\Canary\jbr'
$gradleVersion = '8.14.3'
$gradleHome = Join-Path $repo 'tools\gradle'
$gradleBat = Join-Path $gradleHome "gradle-$gradleVersion\bin\gradle.bat"
$variant = if ($Release) { 'release' } else { 'debug' }
$assembleTask = if ($Release) { 'assembleRelease' } else { 'assembleDebug' }

if (-not (Test-Path (Join-Path $sdk 'platform-tools\adb.exe'))) {
    throw "Android SDK not found at $sdk. Install it with: android sdk install platform-tools 'platforms;android-36' 'build-tools;36.0.0' 'ndk;30.0.16248370' 'cmake;3.22.1'"
}
if (-not (Test-Path (Join-Path $jdk 'bin\java.exe'))) {
    throw "JDK not found at $jdk"
}

if (-not (Test-Path $gradleBat)) {
    Write-Host "Gradle $gradleVersion not cached, downloading..."
    New-Item -ItemType Directory -Force $gradleHome | Out-Null
    $zip = Join-Path $env:TEMP "gradle-$gradleVersion-bin.zip"
    if (-not (Test-Path $zip)) {
        Invoke-WebRequest -Uri "https://services.gradle.org/distributions/gradle-$gradleVersion-bin.zip" -OutFile $zip -TimeoutSec 900
    }
    Expand-Archive -Path $zip -DestinationPath $gradleHome -Force
}

$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk
$env:JAVA_HOME = $jdk
$env:PATH = "$sdk\platform-tools;$sdk\cmake\3.22.1\bin;$jdk\bin;$env:PATH"

# local.properties is machine-specific and never committed.
"sdk.dir=$($sdk -replace '\\','/')" | Set-Content -Encoding ascii (Join-Path $repo 'android\local.properties')

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
Write-Host "Building $variant (stamp $stamp)..."

$sdlTag = 'release-3.4.18'
$sdlDir = Join-Path $repo 'tools\sdl3'
if (-not (Test-Path (Join-Path $sdlDir 'CMakeLists.txt'))) {
    Write-Host "SDL3 $sdlTag not cached, cloning..."
    New-Item -ItemType Directory -Force $sdlDir | Out-Null
    git clone --depth 1 --branch $sdlTag https://github.com/libsdl-org/SDL.git $sdlDir
    if ($LASTEXITCODE -ne 0) { throw 'SDL3 clone failed' }
}

& $gradleBat -p (Join-Path $repo 'android') $assembleTask --no-daemon --console=plain
if ($LASTEXITCODE -ne 0) {
    throw "Gradle failed with exit code $LASTEXITCODE"
}

$apk = Join-Path $repo "android\app\build\outputs\apk\$variant\app-$variant.apk"
if (-not (Test-Path $apk)) {
    throw "APK not produced at $apk"
}

$outDir = Join-Path $repo 'apk'
New-Item -ItemType Directory -Force $outDir | Out-Null
$out = Join-Path $outDir "PWX-$stamp-$variant.apk"
Copy-Item $apk $out -Force

Get-ChildItem $outDir -Filter 'PWX-*.apk' | Where-Object { $_.FullName -ne $out } | ForEach-Object {
    Remove-Item $_.FullName -Force
}

$sizeMb = [math]::Round((Get-Item $out).Length / 1MB, 1)
$sha = (Get-FileHash $out -Algorithm SHA256).Hash.Substring(0, 16)
Write-Host ""
Write-Host "APK: $out"
Write-Host "size: $sizeMb MB   sha256: $sha..."
Write-Host "install: .\scripts\android\install-apk.ps1 -Apk `"$out`""
