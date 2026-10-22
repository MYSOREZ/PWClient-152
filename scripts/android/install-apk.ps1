# Installs the newest built APK (or -Apk <path>) and starts streaming logcat.
#
#   .\scripts\android\install-apk.ps1
#   .\scripts\android\install-apk.ps1 -Apk .\apk\PWX-20261023-120000-debug.apk

param(
    [string]$Apk,
    [switch]$NoLogcat
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'

if (-not $Apk) {
    $Apk = Get-ChildItem (Join-Path $repo 'apk') -Filter 'PWX-*.apk' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Apk -or -not (Test-Path $Apk)) {
    throw 'No APK found. Run .\scripts\android\build-apk.ps1 first.'
}

& $adb devices
& $adb install -r $Apk
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed ($LASTEXITCODE). Enable USB debugging and accept the fingerprint prompt."
}

# Let the app write into Download/PWX/logs.
& $adb shell appops set com.pwx.client MANAGE_EXTERNAL_STORAGE allow 2>$null

& $adb shell am start -n com.pwx.client/.MainActivity

if (-not $NoLogcat) {
    Write-Host "Streaming logcat (Ctrl+C to stop)..."
    & $adb logcat -v time -s PWX:V PWX-native:V AndroidRuntime:E
}
