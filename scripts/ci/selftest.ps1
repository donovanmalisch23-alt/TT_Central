# TT Central — in-app self-test of the built client (CI).
#
# Runs TTCentral.exe with TTC_SELFTEST set. Inside the real client, TT Central
# then checks that its menu is in place before Help, that its keyboard
# shortcuts do not clash with any of the client's, that its windows open, and
# runs an accessibility audit (every focusable control must have a name a
# screen reader can announce). The client writes a report and exits with 0 on
# success or 1 on failure; anything else (or no report) is a crash.

param(
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'

$root   = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$exe    = Join-Path $root 'external\TeamTalk5\Client\qtTeamTalk\TTCentral.exe'
$dllDir = Join-Path $root 'external\TeamTalk5\Library\TeamTalk_DLL'
$tmp    = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$report = Join-Path $tmp 'ttc-selftest.txt'

if (-not (Test-Path $exe)) {
    Write-Host "::error::Client executable not found: $exe"
    exit 1
}
Remove-Item -ErrorAction SilentlyContinue $report

# The TeamTalk SDK DLL must be resolvable (Qt is already on PATH in CI).
$env:PATH = "$dllDir;$env:PATH"
$env:TTC_SELFTEST = $report

Write-Host "Running TT Central self-test: $exe"
$proc = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
$null = $proc.Handle   # keep a handle so ExitCode is available afterwards

if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $proc.Id -Force
    Write-Host "::error::TT Central self-test did not finish within $TimeoutSeconds s"
    exit 1
}
$code = $proc.ExitCode

if (Test-Path $report) {
    Get-Content $report | ForEach-Object { Write-Host $_ }
} else {
    Write-Host ("::error::No self-test report; the client exited with code {0} (0x{0:X8})" -f $code)
    exit 1
}
if ($code -ne 0) {
    Write-Host "::error::TT Central self-test failed (exit code $code)"
    exit 1
}
Write-Host "TT Central self-test passed."
exit 0
