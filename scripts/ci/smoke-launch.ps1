# TT Central — client launch smoke test (CI).
#
# Starts the built client and checks that it is still running after a short
# wait. This catches the failures that matter most at this stage: missing
# runtime DLLs (Qt, TeamTalk5), startup crashes, and header/DLL version
# mismatches that abort before the main window appears.
#
# A modal dialog (first-run wizard, SDK trial notice) keeps the process alive,
# which counts as a successful start.

param(
    [int]$WaitSeconds = 20
)

$ErrorActionPreference = 'Stop'

$root   = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$exe    = Join-Path $root 'external\TeamTalk5\Client\qtTeamTalk\TeamTalk5.exe'
$dllDir = Join-Path $root 'external\TeamTalk5\Library\TeamTalk_DLL'

if (-not (Test-Path $exe)) {
    Write-Host "::error::Client executable not found: $exe"
    exit 1
}

# Make the TeamTalk SDK DLL resolvable (Qt is already on PATH in CI).
$env:PATH = "$dllDir;$env:PATH"

Write-Host "Launching $exe"
$proc = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru

Start-Sleep -Seconds $WaitSeconds

if ($proc.HasExited) {
    $code = $proc.ExitCode
    Write-Host ("Client exited within {0}s with code {1} (0x{2:X8})" -f $WaitSeconds, $code, $code)
    if ($code -ne 0) {
        Write-Host "::error::TT Central client failed to start (exit code $code)"
        exit 1
    }
    Write-Host "Client exited cleanly."
} else {
    Write-Host "Client is running after ${WaitSeconds}s (pid $($proc.Id)) - start-up OK"
    Stop-Process -Id $proc.Id -Force
}
exit 0
