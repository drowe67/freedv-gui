<#
  .SYNOPSIS
  Generates PGO (profile-guided optimization) profile data for an instrumented FreeDV build.

  .DESCRIPTION
  This script feeds a known test WAV file through FreeDV's RADE TX pipeline while recording the
  resulting analog audio via SoX from the "computer to radio" device. The recording is then fed
  straight back into FreeDV's RX pipeline from disk (bypassing the sound card) so that both TX and
  RX code paths are exercised. FreeDV is expected to have been built with PGO instrumentation
  enabled, so each run will emit a *.profraw file that can later be merged and used to guide a
  final optimized build.

  This script must be run from the folder containing freedv.exe, and expects freedv-pgo.conf.tmpl
  to be present in that same folder.

  .INPUTS
  None. You can't pipe objects to this script.

  .OUTPUTS
  FreeDV's console output is written to the host. The script exits with the exit code of the RX pass.

  .EXAMPLE
  PS> .\GeneratePGOProfiles.ps1 -RadioToComputerDevice "CABLE Output (VB-Audio Virtual Cable)" -ComputerToRadioDevice "Speakers (VB-Audio Virtual Cable)" -ComputerToSpeakerDevice "Line 1 (Virtual Audio Cable)" -MicrophoneToComputerDevice "Line 1 (Virtual Audio Cable)"

#>

param (
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]
    # The sound device to receive RX audio from.
    $RadioToComputerDevice,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]
    # The sound device to emit decoded audio to.
    $ComputerToSpeakerDevice,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]
    # The sound device to receive analog audio from.
    $MicrophoneToComputerDevice,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]
    # The sound device to emit TX audio to. This is also recorded from to obtain the RX input.
    $ComputerToRadioDevice)

$current_loc = Get-Location

# Best-effort: wait for the configured audio endpoints to be enumerable before
# starting FreeDV, so a virtual cable that has briefly dropped out of the
# MMDevice list doesn't make the UT abort with a fatal "device cannot be found"
# message box. The CI workflow does a stricter, failing check up front; this is
# just a short guard against a mid-script drop between the TX and RX passes.
function Wait-ForAudioDevices {
    param (
        [string[]] $Names,
        [int] $TimeoutSeconds = 90
    )

    if (-not (Get-Module -ListAvailable -Name AudioDeviceCmdlets)) { return }
    Import-Module AudioDeviceCmdlets -ErrorAction SilentlyContinue

    $wanted = @($Names | Where-Object { $_ } | Select-Object -Unique)
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($true) {
        try { $have = @((Get-AudioDevice -List).Name) } catch { $have = @() }
        $missing = @($wanted | Where-Object { $have -notcontains $_ })
        if ($missing.Count -eq 0) { return }
        if ((Get-Date) -ge $deadline) {
            Write-Host "WARNING: audio device(s) still missing after ${TimeoutSeconds}s: $($missing -join ', ')"
            return
        }
        Start-Sleep -Seconds 3
    }
}

$allDevices = @($RadioToComputerDevice, $ComputerToRadioDevice, $MicrophoneToComputerDevice, $ComputerToSpeakerDevice)

# Clone the RADE test corpus if not already present, then resample the TX test file to 48 kHz
# to reduce CPU usage during the run.
if (-not (Test-Path "$current_loc\rade_src")) {
    & git clone -b main https://github.com/drowe67/radae.git "$current_loc\rade_src"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone RADE test corpus"
    }
}

& sox.exe "$current_loc\rade_src\wav\all.wav" -r 48000 "$current_loc\tx_in.wav"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to resample TX test file"
}

# Generate new conf
$conf_tmpl = Get-Content "$current_loc\freedv-pgo.conf.tmpl"
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_RADIO_TO_COMPUTER_DEVICE@", $RadioToComputerDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_RADIO_DEVICE@", $ComputerToRadioDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@", $MicrophoneToComputerDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@", $ComputerToSpeakerDevice)
$conf_file = "$current_loc\freedv-pgo.conf"
$conf_tmpl | Set-Content -Path $conf_file
$quoted_conf_filename = "`"" + $conf_file + "`""

# Start recording the TX output via SoX so that it can be fed back in as the RX input file.
$soxPsi = New-Object System.Diagnostics.ProcessStartInfo
$soxPsi.CreateNoWindow = $true
$soxPsi.UseShellExecute = $false
$soxPsi.RedirectStandardError = $false
$soxPsi.RedirectStandardOutput = $false
$soxPsi.FileName = "sox.exe"
$soxPsi.WorkingDirectory = $current_loc
$quoted_device = "`"" + $RadioToComputerDevice + "`""
$soxPsi.Arguments = @("-t waveaudio $quoted_device -c 1 -r 48000 -t wav `"$current_loc\test.wav`"")

$soxProcess = New-Object System.Diagnostics.Process
$soxProcess.StartInfo = $soxPsi
[void]$soxProcess.Start()

# Start FreeDV in test mode to record TX
Wait-ForAudioDevices -Names $allDevices

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.CreateNoWindow = $true
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true
$psi.FileName = "$current_loc\freedv.exe"
$psi.WorkingDirectory = $current_loc
$psi.Arguments = @("/f $quoted_conf_filename /ut tx /utmode RADEV1 /txfile `"$current_loc\tx_in.wav`" /txfeaturefile `"$current_loc\txfeatures.f32`"")

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $psi
[void]$process.Start()
$process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal

# Read output from the TX run
$err_output = $process.StandardError.ReadToEnd()
$output = $process.StandardOutput.ReadToEnd()
$process.WaitForExit()

Write-Host "$output"
Write-Host "$err_output"

# Stop recording, play back in RX mode
try {
    $soxProcess.Kill()
} catch {
    # Ignore failure as SoX may have already exited on its own
}
$soxProcess.WaitForExit()

# Killing SoX above can briefly disturb the shared audio engine; make sure the
# endpoints are back before the RX pass starts.
Wait-ForAudioDevices -Names $allDevices

$psi.Arguments = @("/f $quoted_conf_filename /ut rx /utmode RADEV1 /rxfile `"$current_loc\test.wav`" /rxfeaturefile `"$current_loc\rxfeatures.f32`"")

$conf_tmpl = Get-Content "$current_loc\freedv-pgo.conf.tmpl"
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_RADIO_TO_COMPUTER_DEVICE@", $RadioToComputerDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_RADIO_DEVICE@", $ComputerToRadioDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@", $MicrophoneToComputerDevice)
$conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@", $ComputerToSpeakerDevice)
$conf_file = "$current_loc\freedv-pgo.conf"
$conf_tmpl | Set-Content -Path $conf_file

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $psi
[void]$process.Start()
$process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal

# Read output from the RX run
$err_output = $process.StandardError.ReadToEnd()
$output = $process.StandardOutput.ReadToEnd()
$process.WaitForExit()
$freedv_exit_code = $process.ExitCode

Write-Host "$output"
Write-Host "$err_output"

exit $freedv_exit_code
