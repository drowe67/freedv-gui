<#
  .SYNOPSIS
  Runs FreeDV's unit tests.

  .DESCRIPTION
  This script runs every unit test executable (*Test.exe) in a folder, one after another, and
  reports which of them failed. A test fails if it exits with anything other than zero or if it
  takes longer than the allowed time. The folder is expected to be the one produced by the
  "collect_unit_tests" build target, which also contains the DLLs that the tests need.

  .INPUTS
  None. You can't pipe objects to this script.

  .OUTPUTS
  The script outputs each test's own output followed by a summary to the console, and exits
  with 1 if any test failed.

  .EXAMPLE
  PS> .\RunUnitTests.ps1 -TestFolder C:\freedv\unit_tests

#>

param (
    [ValidateNotNullOrEmpty()]
    [string]
    # The folder containing the unit test executables and their DLLs.
    $TestFolder = $PSScriptRoot,

    [ValidateRange(1, 86400)]
    [int]
    # The longest that any one test is allowed to run for, in seconds.
    $TimeoutSeconds = 900
)

$tests = @(Get-ChildItem -Path $TestFolder -Filter "*Test.exe" | Sort-Object Name)
if ($tests.Count -eq 0) {
    Write-Host "No unit tests found in $TestFolder"
    exit 1
}

$failed = @()
foreach ($test in $tests) {
    Write-Host "=== $($test.Name)"

    $outputFile = Join-Path $TestFolder "$($test.BaseName).stdout.log"
    $errorFile = Join-Path $TestFolder "$($test.BaseName).stderr.log"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $process = Start-Process -FilePath $test.FullName -WorkingDirectory $TestFolder -NoNewWindow -PassThru `
        -RedirectStandardOutput $outputFile -RedirectStandardError $errorFile

    # Without this, ExitCode isn't available once the process has ended.
    $handle = $process.Handle

    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }

    # Also waits for the test's output to finish being written.
    $process.WaitForExit()
    $stopwatch.Stop()

    Get-Content -Path $outputFile -ErrorAction SilentlyContinue | Write-Host
    Get-Content -Path $errorFile -ErrorAction SilentlyContinue | Write-Host

    $seconds = [int]$stopwatch.Elapsed.TotalSeconds
    if ($timedOut) {
        Write-Host "=== $($test.Name): FAILED (still running after $TimeoutSeconds seconds)"
        $failed += $test.Name
    }
    elseif ($process.ExitCode -ne 0) {
        Write-Host "=== $($test.Name): FAILED (exit code $($process.ExitCode), $seconds seconds)"
        $failed += $test.Name
    }
    else {
        Write-Host "=== $($test.Name): passed ($seconds seconds)"
    }
}

Write-Host ""
if ($failed.Count -eq 0) {
    Write-Host "All $($tests.Count) unit tests passed"
    exit 0
}

Write-Host "$($failed.Count) of $($tests.Count) unit tests FAILED:"
foreach ($name in $failed) {
    Write-Host "    $name"
}
exit 1
