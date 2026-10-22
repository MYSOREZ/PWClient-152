# Pulls game logs from a connected device into .\logs\<timestamp>\.
# Works for both log locations:
#   /sdcard/Download/PWX/logs           (All-files access granted)
#   /sdcard/Android/data/com.pwx.client/files/logs   (fallback)

param(
    [string]$Dest
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'

if (-not $Dest) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $Dest = Join-Path $repo "logs\$stamp"
}
New-Item -ItemType Directory -Force $Dest | Out-Null

$pulled = 0
foreach ($remote in @('/sdcard/Download/PWX/logs/', '/sdcard/Android/data/com.pwx.client/files/logs/')) {
    $target = Join-Path $Dest ((($remote -replace '/sdcard/Download', 'download') -replace '/sdcard/Android/data/com.pwx.client/files', 'appfiles') -replace '[/:]', '_')
    & $adb pull $remote $target 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $pulled++
        Write-Host "pulled: $remote -> $target"
    }
}

if ($pulled -eq 0) {
    Write-Warning 'Nothing pulled. Check the device is connected (adb devices) and the app has run at least once.'
} else {
    Get-ChildItem $Dest -Recurse -Filter '*.log' | ForEach-Object {
        Write-Host ""
        Write-Host "--- $($_.FullName) ($([math]::Round($_.Length / 1KB, 1)) KB)"
        Get-Content $_.FullName -Tail 40
    }
}
