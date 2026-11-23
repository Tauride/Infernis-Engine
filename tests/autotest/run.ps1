<#
.SYNOPSIS
    Runs an autotest scenario in the game and reports the result.

.DESCRIPTION
    Starts <GameDir>\<Bin>\xrEngine.exe in autotest mode with the given scenario, waits
    for it to finish and prints a summary of the JSON result. Exits with the game's exit
    code: 0 passed, 1 failed, 2 bad scenario, 3 crashed, 4 timed out or hung.

    The game runs windowed with its own settings file, autotest.ltx in the game's
    application data folder (created from user.ltx on first use: windowed, 1280x720), so
    the user's settings are never changed. Only one game can run at a time: it locks the
    binaries.

    -Renderer r1|r2a|r2|r2.5|r3|r4 runs with another renderer: a temporary copy of
    autotest.ltx with that "renderer" line (autotest_<renderer>.ltx, deleted afterwards).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\autotest\run.ps1 tests\autotest\menu.cfg
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\autotest\run.ps1 tests\autotest\menu.cfg -Renderer r3
#>
param(
    [Parameter(Mandatory = $true, Position = 0)] [string] $Scenario,
    [string] $GameDir = '',     # default: engine_tests_infernis next to the sources folder
    [string] $Bin = 'bin\x64',
    [int]    $Timeout = 900,
    [string] $ExtraArgs = '',
    [ValidateSet('', 'r1', 'r2a', 'r2', 'r2.5', 'r3', 'r4')] [string] $Renderer = ''   # default: the one in autotest.ltx
)

$ErrorActionPreference = 'Stop'

if (-not $GameDir) { $GameDir = Join-Path $PSScriptRoot '..\..\..\..\engine_tests_infernis' }  # $PSScriptRoot is empty in param defaults on PS 5.1
$GameDir  = (Resolve-Path $GameDir).Path
$Scenario = (Resolve-Path $Scenario).Path
$exe      = Join-Path $GameDir "$Bin\xrEngine.exe"

# The application data folder is whatever fsgame.ltx says ($app_data_root$)
$appdataName = 'userdata'
$fsgame = Join-Path $GameDir 'fsgame.ltx'
if (Test-Path $fsgame) {
    $line = Select-String -Path $fsgame -Pattern '^\s*\$app_data_root\$\s*=' | Select-Object -First 1
    if ($line) {
        $parts = $line.Line.Split('=', 2)[1].Split('|')
        if ($parts.Count -ge 4 -and $parts[3].Trim()) { $appdataName = $parts[3].Trim().TrimEnd('\') }
    }
}
$appdata = Join-Path $GameDir $appdataName
$result  = Join-Path $appdata 'logs\autotest_result.json'

if ($Scenario -match ' ') { throw "The scenario path may not contain spaces: $Scenario" }
if ($result -match ' ')   { throw "The game folder may not contain spaces: $GameDir" }
if (-not (Test-Path $exe)) { throw "No game binary: $exe" }
if (Get-Process xrEngine -ErrorAction SilentlyContinue) {
    throw 'xrEngine.exe is already running. Close it first (it also locks the binaries).'
}

# The autotest settings: a copy of user.ltx, windowed and small
$ltx = Join-Path $appdata 'autotest.ltx'
if (-not (Test-Path $ltx)) {
    $user = Join-Path $appdata 'user.ltx'
    if (-not (Test-Path $user)) { throw "No ${user}: start the game once (or copy a user.ltx there) first." }
    $lines = Get-Content $user | ForEach-Object {
        if ($_ -match '^vid_mode ') { 'vid_mode 1280x720' }
        else { $_ }
    }
    Set-Content -Path $ltx -Value $lines -Encoding ascii
    Write-Host "Created $ltx (windowed, 1280x720)"
}

# Every run starts the same way, whatever the last scenario left in the file: in a window
# (rs_screenmode; rs_fullscreen is the old switch), without the frame rate limit (frame
# times are measured without it), and a loaded level starts at once instead of waiting
# for a key on the load screen
$lines = @(Get-Content $ltx | Where-Object { $_ -notmatch '^(keypress_on_start|rs_fullscreen|rs_screenmode|rs_fps_limit) ' }) +
    'keypress_on_start 0' + 'rs_screenmode screenmode_windowed' + 'rs_fps_limit off'
Set-Content -Path $ltx -Value $lines -Encoding ascii

# Another renderer: a temporary copy of the settings with that renderer
$ltxName = 'autotest.ltx'
$ltxTemp = $null
if ($Renderer) {
    $ltxName = "autotest_$Renderer.ltx"
    $ltxTemp = Join-Path $appdata $ltxName
    $lines = @(Get-Content $ltx | Where-Object { $_ -notmatch '^renderer ' }) + "renderer renderer_$Renderer"
    Set-Content -Path $ltxTemp -Value $lines -Encoding ascii
}

New-Item -ItemType Directory -Force (Split-Path $result) | Out-Null
Remove-Item $result -ErrorAction SilentlyContinue
$arguments = "-autotest $Scenario -autotest_out $result -autotest_timeout $Timeout -ltx $ltxName -nointro -i $ExtraArgs"
Write-Host "Running: $exe $arguments"

$started = Get-Date
try {
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $GameDir -PassThru
    $null = $p.Handle    # keep the handle, or ExitCode is lost once the process ends
    # The game has its own watchdog (-autotest_timeout); this is the last resort
    if (-not $p.WaitForExit(($Timeout + 120) * 1000)) {
        Write-Warning 'The game did not exit in time; killing it.'
        $p.Kill()
        $p.WaitForExit()
    }
    $code = $p.ExitCode
} finally {
    if ($ltxTemp) { Remove-Item $ltxTemp -ErrorAction SilentlyContinue }
}
$secs = [int]((Get-Date) - $started).TotalSeconds

if (-not (Test-Path $result)) {
    Write-Host "No result file ($result). Exit code $code after $secs s."
    if ($code -eq 0) { $code = 1 }
    exit $code
}

$r = Get-Content $result -Raw -Encoding UTF8 | ConvertFrom-Json   # the result is UTF-8
Write-Host ''
Write-Host ("{0}  (exit code {1}, {2} s, {3} frames)" -f $r.status.ToUpper(), $code, $secs, $r.frames)
if ($r.reason)      { Write-Host "reason:  $($r.reason)" }
if ($r.status -ne 'passed') { Write-Host "step:    $($r.step)/$($r.steps_total) (line $($r.step_line)): $($r.step_text)" }
if ($r.level)       { Write-Host "level:   $($r.level)" }
foreach ($pf in $r.perf) {
    Write-Host ("perf:    {0}: {1} frames, avg {2} ms ({3} fps), p95 {4} ms, p99 {5} ms, max {6} ms" -f `
        $pf.label, $pf.frames, $pf.avg_ms, $pf.avg_fps, $pf.p95_ms, $pf.p99_ms, $pf.max_ms)
}
foreach ($s in $r.screenshots) { Write-Host "image:   $s" }
foreach ($s in $r.ui_dumps)    { Write-Host "ui dump: $s" }
Write-Host ("memory:  peak working set {0} MB, peak commit {1} MB" -f $r.peak_working_set_mb, $r.peak_commit_mb)
Write-Host "errors:  $($r.log_errors) log lines starting with '!'"
if ($r.dump)        { Write-Host "dump:    $($r.dump)" }
Write-Host "log:     $($r.log)"
Write-Host "result:  $result"
exit $code
