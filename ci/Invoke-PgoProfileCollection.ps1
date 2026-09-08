<#
  .SYNOPSIS
  Runs GeneratePGOProfiles.ps1 with an audio-endpoint pre-check, retrying a few
  times.

  .DESCRIPTION
  On CI the virtual-cable audio endpoints (VB-Cable / VAC) intermittently fail to
  be enumerable when FreeDV starts a UT -- sometimes not yet up when the job
  begins, sometimes briefly dropping out mid-run right after SoX releases the
  capture device. Either way FreeDV aborts with a fatal "device cannot be found"
  message box.

  This wrapper waits for the required endpoints (ci/Wait-AudioDevices.ps1, which
  also restarts the audio stack), runs GeneratePGOProfiles.ps1, and retries the
  whole thing a few times before giving up. Must be run from the folder
  containing freedv.exe / GeneratePGOProfiles.ps1.

  .PARAMETER Attempts
  How many times to try the full collection. Default 3.
#>
param (
    [Parameter(Mandatory = $true)] [string] $RadioToComputerDevice,
    [Parameter(Mandatory = $true)] [string] $ComputerToRadioDevice,
    [Parameter(Mandatory = $true)] [string] $MicrophoneToComputerDevice,
    [Parameter(Mandatory = $true)] [string] $ComputerToSpeakerDevice,
    [int] $Attempts = 3
)

$ErrorActionPreference = 'Stop'

$waitScript = Join-Path $PSScriptRoot 'Wait-AudioDevices.ps1'

for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
    Write-Host "=== GeneratePGOProfiles attempt $attempt/$Attempts ==="

    # Playback (render) endpoints FreeDV emits to; recording (capture) endpoints
    # FreeDV / SoX read from. "Line 1 (Virtual Audio Cable)" is used for both.
    try {
        & $waitScript `
            -Playback  $ComputerToRadioDevice, $ComputerToSpeakerDevice `
            -Recording $RadioToComputerDevice, $MicrophoneToComputerDevice `
            -TimeoutSeconds 120
    }
    catch {
        Write-Host "::warning::Audio endpoint wait failed on attempt ${attempt}: $_"
    }

    $rc = 1
    try {
        & .\GeneratePGOProfiles.ps1 `
            -RadioToComputerDevice      $RadioToComputerDevice `
            -ComputerToRadioDevice      $ComputerToRadioDevice `
            -MicrophoneToComputerDevice $MicrophoneToComputerDevice `
            -ComputerToSpeakerDevice    $ComputerToSpeakerDevice
        $rc = $LASTEXITCODE
    }
    catch {
        Write-Host "::warning::GeneratePGOProfiles threw on attempt ${attempt}: $_"
        $rc = 1
    }

    if ($rc -eq 0) {
        Write-Host "PGO data generated on attempt $attempt."
        exit 0
    }

    Write-Host "::warning::GeneratePGOProfiles failed on attempt $attempt (exit $rc)."
    # Drop any partial .profraw so the retry (and the upload) start clean.
    Remove-Item -ErrorAction SilentlyContinue *.profraw
}

Write-Host "::error::GeneratePGOProfiles failed after $Attempts attempts."
exit 1
