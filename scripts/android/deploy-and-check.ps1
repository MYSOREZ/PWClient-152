# Waits for an adb device, installs the newest APK, launches it, then collects evidence:
# logcat, the on-device log file, and a screenshot.
#
#   .\scripts\android\deploy-and-check.ps1
#   .\scripts\android\deploy-and-check.ps1 -WaitSeconds 300

param(
    [int]$WaitSeconds = 120,
    [string]$Apk
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'

if (-not $Apk) {
    $Apk = Get-ChildItem (Join-Path $repo 'apk') -Filter 'PWX-*.apk' |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Apk) { throw 'No APK in apk\. Build it first.' }

& $adb start-server 2>&1 | Out-Null

Write-Host "Waiting up to $WaitSeconds s for a device..."
$deadline = (Get-Date).AddSeconds($WaitSeconds)
$serial = $null
while ((Get-Date) -lt $deadline) {
    $lines = & $adb devices 2>$null | Select-Object -Skip 1 | Where-Object { $_ -match '\S' }
    $ready = $lines | Where-Object { $_ -match '\sdevice$' }
    if ($ready) { $serial = (@($ready)[0] -split '\s+')[0]; break }
    $bad = $lines | Where-Object { $_ -match 'unauthorized|offline|no permissions' }
    if ($bad) { Write-Host "  state: $(@($bad)[0]) - check the phone screen (allow USB debugging)" }
    Start-Sleep -Seconds 2
}
if (-not $serial) { throw "No device after $WaitSeconds s. adb devices is empty or unauthorized." }

Write-Host "device: $serial"
& $adb -s $serial shell getprop ro.product.model
& $adb -s $serial shell getprop ro.build.version.release
& $adb -s $serial shell getprop ro.product.cpu.abi

Write-Host "installing $Apk"
& $adb -s $serial install -r -g $Apk | Select-Object -Last 2

& $adb -s $serial shell appops set com.pwx.client MANAGE_EXTERNAL_STORAGE allow 2>&1 | Out-Null
& $adb -s $serial logcat -c 2>&1 | Out-Null
& $adb -s $serial shell am force-stop com.pwx.client 2>&1 | Out-Null
& $adb -s $serial shell am start -W -n com.pwx.client/.MainActivity 2>&1 | Select-Object -Last 3

Start-Sleep -Seconds 8

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path $repo "logs\$stamp"
New-Item -ItemType Directory -Force $out | Out-Null

& $adb -s $serial logcat -d -v time -s PWX:V PWX-native:V AndroidRuntime:E DEBUG:V libc:V > (Join-Path $out 'logcat.txt')
& $adb -s $serial shell pidof com.pwx.client 2>&1 | ForEach-Object { "pid: $_" }
& $adb -s $serial exec-out screencap -p > (Join-Path $out 'screen.png')

foreach ($remote in @('/sdcard/Download/PWX/logs/', '/sdcard/Android/data/com.pwx.client/files/logs/')) {
    & $adb -s $serial pull $remote (Join-Path $out 'device') 2>&1 | Out-Null
}

Write-Host ""
Write-Host "=== logcat (app) ==="
Get-Content (Join-Path $out 'logcat.txt') -Tail 40
Write-Host ""
Write-Host "=== on-device log ==="
$log = Get-ChildItem $out -Recurse -Filter '*.log' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($log) {
    Write-Host $log.FullName
    Get-Content $log.FullName -Tail 40
} else {
    Write-Warning 'no .log file pulled'
}
Write-Host ""
Write-Host "artifacts: $out"
