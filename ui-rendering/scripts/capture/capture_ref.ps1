param(
    [string]$Name = "frame",
    [string]$Size = "1920x1080",
    [int]$Fps = 30
)

$ErrorActionPreference = "Stop"

$ff = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter ffmpeg.exe -Recurse -ErrorAction SilentlyContinue |
      Select-Object -First 1 -ExpandProperty FullName
if (-not $ff) { throw "ffmpeg not found (winget install Gyan.FFmpeg)" }

$dir = Join-Path $PSScriptRoot "..\..\artifacts\reference"
New-Item -ItemType Directory -Force $dir | Out-Null
$out = Join-Path $dir ("real-" + $Name + "-" + $Size + ".png")

& $ff -hide_banner -loglevel error -f dshow -video_size $Size -framerate $Fps `
      -i video="USB3 PLUS Video" -frames:v 1 -update 1 -y $out

if (Test-Path $out) {
    Get-Item $out | Select-Object FullName, Length, LastWriteTime
} else {
    throw "capture failed"
}
