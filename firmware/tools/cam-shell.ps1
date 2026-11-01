param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ToolArgs
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$tool = Join-Path $root "cam_shell.py"
$usbipd = Join-Path $env:ProgramFiles "usbipd-win\usbipd.exe"

if (-not (Test-Path $usbipd)) {
    throw "usbipd-win not found: $usbipd"
}

function Get-CameraLine {
    $list = & $usbipd list 2>$null
    return ($list | Select-String '054c:' | Select-Object -First 1)
}

$line = Get-CameraLine
if (-not $line) {
    throw "Sony camera not found. Connect it in Mass Storage mode."
}

# Keep exactly one auto-attach loop running (needed for the 0c02 -> 0336
# re-enumeration when switching to service mode).
$loops = @(Get-CimInstance Win32_Process -Filter "Name='usbipd.exe'" |
           Where-Object { $_.CommandLine -match ' attach ' })
if ($loops.Count -eq 0) {
    $busid = ($line.ToString().Trim() -split '\s+')[0]
    Write-Host "starting usbipd auto-attach for $busid ..."
    Start-Process -FilePath $usbipd `
        -ArgumentList "attach --wsl --busid $busid --auto-attach" `
        -WindowStyle Hidden
    Start-Sleep -Seconds 2
}

# Wait until the device is actually attached to WSL.
$attached = $false
for ($i = 0; $i -lt 20; $i++) {
    $line = Get-CameraLine
    if ($line -and $line.ToString() -match 'Attached') {
        $attached = $true
        break
    }
    Start-Sleep -Milliseconds 500
}
if (-not $attached) {
    throw "camera is not attached to WSL (usbipd attach failed?)"
}

$drive = $tool.Substring(0, 1).ToLower()
$wslTool = "/mnt/$drive" + ($tool.Substring(2) -replace "\\", "/")
wsl.exe -u root -e python3 $wslTool @ToolArgs
exit $LASTEXITCODE
