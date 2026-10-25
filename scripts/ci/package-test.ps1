# TT Central — build the Windows packages and prove they are self-contained.
#
# 1. scripts/package.py assembles dist/TTCentral-<ver>-win64 and the portable zip.
# 2. The packaged TTCentral.exe runs its self-test with Qt and Visual Studio
#    removed from PATH, so a missing DLL or plugin fails here, not on a
#    user's computer.
# 3. The packaged media bot starts and finds ffmpeg, yt-dlp and Deno.
# 4. The Inno Setup installer is built, installed silently, and the installed
#    copy passes the self-test too, then uninstalls cleanly.
# Nothing is uploaded: the packages contain the proprietary TeamTalk SDK.

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $root

python -m pip install --quiet markdown
python scripts/package.py
if ($LASTEXITCODE -ne 0) { throw "packaging failed" }

$pkg = Get-ChildItem dist -Directory -Filter 'TTCentral-*-win64' | Select-Object -First 1
$zip = Get-ChildItem dist -File -Filter 'TTCentral-*-portable.zip' | Select-Object -First 1
if (-not $pkg -or -not $zip) { throw "package folder or zip missing" }
$version = ($pkg.Name -replace '^TTCentral-', '' -replace '-win64$', '')
Write-Host "Package: $($pkg.FullName) ($version)"

# A clean environment: only Windows itself on PATH, no Qt variables.
function Invoke-Clean([string]$exe, [string[]]$arguments, [hashtable]$extraEnv, [int]$timeoutSec = 120) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    foreach ($a in $arguments) { [void]$psi.ArgumentList.Add($a) }
    $psi.WorkingDirectory = Split-Path $exe
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    foreach ($k in @('QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QT_ROOT_DIR', 'Qt6_DIR', 'QTDIR')) { [void]$psi.Environment.Remove($k) }
    $psi.Environment['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
    if ($extraEnv) { foreach ($k in $extraEnv.Keys) { $psi.Environment[$k] = $extraEnv[$k] } }
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEndAsync()
    $err = $p.StandardError.ReadToEndAsync()
    if (-not $p.WaitForExit($timeoutSec * 1000)) { $p.Kill(); throw "$exe did not finish within $timeoutSec s" }
    return @{ Code = $p.ExitCode; Out = $out.Result; Err = $err.Result }
}

function Test-SelfTest([string]$dir, [string]$label) {
    $report = Join-Path $env:RUNNER_TEMP "selftest-$label.txt"
    Remove-Item -ErrorAction SilentlyContinue $report
    $r = Invoke-Clean (Join-Path $dir 'TTCentral.exe') @() @{ TTC_SELFTEST = $report } 120
    if (Test-Path $report) { Get-Content $report | ForEach-Object { Write-Host "  $_" } }
    else { Write-Host $r.Err; throw ("{0}: no self-test report (exit code 0x{1:X8}); a DLL or plugin is probably missing" -f $label, $r.Code) }
    if ($r.Code -ne 0) { throw "${label}: self-test failed" }
    Write-Host "${label}: self-test passed with a clean PATH."
}

# --- the portable folder ---------------------------------------------------------
foreach ($f in @('TTCentral.exe', 'ttc-mediabot.exe', 'TeamTalk5.dll', 'server\tt5srv.exe', 'Qt6Core.dll',
                 'platforms\qwindows.dll', 'vcruntime140.dll', 'Sounds\logged_on.wav', 'Sounds\Majorly-G',
                 'tools\ffmpeg\ffmpeg.exe', 'tools\yt-dlp\yt-dlp.exe', 'tools\deno\deno.exe', 'LICENSE.txt',
                 'NOTICE.txt', 'TTCentral.ini.default')) {
    if (-not (Test-Path (Join-Path $pkg.FullName $f))) { throw "package is missing $f" }
}
Test-SelfTest $pkg.FullName 'portable'

$bot = Join-Path $pkg.FullName 'ttc-mediabot.exe'
$r = Invoke-Clean $bot @('--version') @{} 30
if ($r.Code -ne 0) { throw "ttc-mediabot --version failed: $($r.Err)" }
Write-Host "media bot: $($r.Out.Trim())"
$ini = Join-Path $env:RUNNER_TEMP 'bot-check.ini'
"[mediabot]`nhost=127.0.0.1`nusername=bot`n" | Set-Content -Encoding ascii $ini
$r = Invoke-Clean $bot @('--config', $ini, '--check') @{} 60
Write-Host $r.Err
if ($r.Code -ne 0) { throw "ttc-mediabot --check did not find its tools (exit $($r.Code))" }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$entries = [System.IO.Compression.ZipFile]::OpenRead($zip.FullName).Entries | ForEach-Object { $_.FullName }
if (-not ($entries -contains "$($pkg.Name)/TTCentral.ini")) { throw "the zip has no TTCentral.ini (portable mode)" }
Write-Host ("portable zip: {0:N1} MB, {1} entries" -f ($zip.Length / 1MB), $entries.Count)

# --- the installer ------------------------------------------------------------------
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) {
    choco install innosetup -y --no-progress | Out-Null
}
if (-not (Test-Path $iscc)) { throw "Inno Setup is not available" }
& $iscc /Q "/DSourceDir=$($pkg.FullName)" "/DAppVersion=$version" "/DOutputDir=$root\dist" packaging\windows\ttcentral.iss
if ($LASTEXITCODE -ne 0) { throw "the installer build failed" }
$setup = Join-Path $root "dist\TTCentral-$version-setup.exe"
if (-not (Test-Path $setup)) { throw "installer not produced" }
Write-Host ("installer: {0:N1} MB" -f ((Get-Item $setup).Length / 1MB))

$target = Join-Path $env:RUNNER_TEMP 'TTCentral-installed'
$p = Start-Process $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=$target" -PassThru -Wait
if ($p.ExitCode -ne 0) { throw "silent install failed ($($p.ExitCode))" }
if (Test-Path (Join-Path $target 'TTCentral.ini')) { throw "the installer must not install TTCentral.ini (portable mode)" }
Test-SelfTest $target 'installed'
$p = Start-Process (Join-Path $target 'unins000.exe') -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -PassThru -Wait
if ($p.ExitCode -ne 0) { throw "silent uninstall failed ($($p.ExitCode))" }
Start-Sleep -Seconds 2
if (Test-Path (Join-Path $target 'TTCentral.exe')) { throw "uninstall left TTCentral.exe behind" }
Write-Host "Installer: install, self-test and uninstall all passed."
