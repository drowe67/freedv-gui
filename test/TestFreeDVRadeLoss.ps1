<#
  .SYNOPSIS
  Executes RADE loss test of FreeDV.

  .DESCRIPTION
  This script pushes a known test file through FreeDV's TX pipeline while recording the resulting audio from
  the real Windows sound card path, then plays that recording back into FreeDV's RX pipeline. The RADE features
  captured on the TX and RX sides are compared using the loss.py tool from the radae repository, which prints
  PASS or FAIL depending on whether the feature loss introduced by the round trip through real audio hardware
  is below the given threshold. This is the PowerShell equivalent of test/test_rade_loss.sh.

  .INPUTS
  None. You can't pipe objects to this script.

  .OUTPUTS
  The script outputs FreeDV and loss.py logging as well as a final Passed/Failures summary to the console.

  .EXAMPLE
  PS> .\TestFreeDVRadeLoss.ps1 -RadioToComputerDevice "CABLE Output (VB-Audio Virtual Cable)" -ComputerToRadioDevice "Speakers (VB-Audio Virtual Cable)" -ComputerToSpeakerDevice "Line 1 (Virtual Audio Cable)" -MicrophoneToComputerDevice "Line 1 (Virtual Audio Cable)"

#>

param (
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]
    # The sound device to receive RX audio from. Also used to record FreeDV's TX output, as it is the
    # recording half of the same virtual cable that ComputerToRadioDevice plays into.
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
    # The sound device to emit TX audio to. Also used to play the recorded TX audio back into
    # FreeDV's RX input, as it is the playback half of the same virtual cable that RadioToComputerDevice records from.
    $ComputerToRadioDevice,

    [double]
    # The maximum acceptable RADE feature loss fraction before the test is considered failed.
    $LossThreshold = 0.0895,

    [string]
    # Path or filename of the Python interpreter used to run loss.py.
    $PythonBinary = "python.exe")

<#
    .Description
    Performs the actual test by cloning the radae test corpus, transmitting a known test file through FreeDV,
    recording the resulting audio, playing it back through FreeDV's RX pipeline, and comparing the TX/RX RADE
    features using loss.py.
#>
function Test-RadeLoss {
    param (
        $RadioToComputerDevice,
        $ComputerToSpeakerDevice,
        $MicrophoneToComputerDevice,
        $ComputerToRadioDevice,
        $LossThreshold,
        $PythonBinary
    )

    $current_loc = Get-Location

    # Clone radae repo (contains the test audio and the loss.py comparison tool) if not already present.
    if (-not (Test-Path "$current_loc\rade_src")) {
        & git.exe clone -b main https://github.com/drowe67/radae.git "$current_loc\rade_src"
    }

    # Resample test file to 48 kHz. Needed for CI environment to reduce CPU usage.
    & sox.exe "$current_loc\rade_src\wav\all.wav" -r 48000 "$current_loc\tx_in.wav"

    # Generate new conf
    $conf_tmpl = Get-Content "$current_loc\freedv-ctest-loss.conf.tmpl"
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_RADIO_TO_COMPUTER_DEVICE@", $RadioToComputerDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_RADIO_DEVICE@", $ComputerToRadioDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@", $MicrophoneToComputerDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@", $ComputerToSpeakerDevice)
    $tmp_file = New-TemporaryFile
    $conf_tmpl | Set-Content -Path $tmp_file.FullName
    $quoted_tmp_filename = "`"" + $tmp_file.FullName + "`""

    # Start recording FreeDV's TX output
    $recordPsi = New-Object System.Diagnostics.ProcessStartInfo
    $recordPsi.CreateNoWindow = $true
    $recordPsi.UseShellExecute = $false
    $recordPsi.RedirectStandardError = $false
    $recordPsi.RedirectStandardOutput = $false
    $recordPsi.FileName = "sox.exe"
    $recordPsi.WorkingDirectory = $current_loc
    $quoted_record_device = "`"" + $RadioToComputerDevice + "`""
    $recordPsi.Arguments = @("--buffer 32768 -t waveaudio $quoted_record_device -c 1 -t wav -r 48000 `"$current_loc\test.wav`"")

    $recordProcess = New-Object System.Diagnostics.Process
    $recordProcess.StartInfo = $recordPsi
    [void]$recordProcess.Start()

    # Start freedv.exe in TX mode
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.CreateNoWindow = $true
    $psi.UseShellExecute = $false
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardOutput = $true
    $psi.FileName = "$current_loc\freedv.exe"
    $psi.WorkingDirectory = $current_loc
    $psi.Arguments = @("/f $quoted_tmp_filename /ut tx /utmode RADEV1 /txfile `"$current_loc\tx_in.wav`" /txfeaturefile `"$current_loc\txfeatures.f32`"")

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal

    # Read output from TX run
    $err_output = $process.StandardError.ReadToEnd();
    $output = $process.StandardOutput.ReadToEnd();
    $process.WaitForExit()

    Write-Host "$err_output"

    $txExitCode = $process.ExitCode

    # Stop recording
    try {
        $recordProcess.Kill()
    } catch {
        # Ignore failure as the process may have already exited
    }
    $recordProcess.WaitForExit()

    if ($txExitCode -ne 0) {
        return $false
    }

    # Restart FreeDV in RX mode, reading live from the sound card so that any dropouts introduced by the
    # real audio path get captured in the RX feature file (mirrors test/test_rade_loss.sh).
    $psi.Arguments = @("/f $quoted_tmp_filename /ut rx /utmode RADEV1 /txtime 60 /rxfeaturefile `"$current_loc\rxfeatures.f32`"")

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal

    Start-Sleep -Seconds 5

    # Play the recorded TX audio back into FreeDV's RX input
    $playPsi = New-Object System.Diagnostics.ProcessStartInfo
    $playPsi.CreateNoWindow = $true
    $playPsi.UseShellExecute = $false
    $playPsi.RedirectStandardError = $false
    $playPsi.RedirectStandardOutput = $false
    $playPsi.FileName = "sox.exe"
    $playPsi.WorkingDirectory = $current_loc
    $quoted_play_device = "`"" + $ComputerToRadioDevice + "`""
    $playPsi.Arguments = @("-t wav `"$current_loc\test.wav`" -t waveaudio $quoted_play_device")

    $playProcess = New-Object System.Diagnostics.Process
    $playProcess.StartInfo = $playPsi
    [void]$playProcess.Start()

    # Read output from RX run
    $err_output = $process.StandardError.ReadToEnd();
    $output = $process.StandardOutput.ReadToEnd();
    $process.WaitForExit()

    Write-Host "$err_output"

    $rxExitCode = $process.ExitCode

    try {
        if (-not $playProcess.HasExited) {
            $playProcess.Kill()
        }
    } catch {
        # Ignore failure as the process may have already exited
    }
    $playProcess.WaitForExit()

    if ($rxExitCode -ne 0) {
        return $false
    }

    # Compare TX/RX RADE features using loss.py from the radae repo
    $lossPsi = New-Object System.Diagnostics.ProcessStartInfo
    $lossPsi.CreateNoWindow = $true
    $lossPsi.UseShellExecute = $false
    $lossPsi.RedirectStandardError = $true
    $lossPsi.RedirectStandardOutput = $true
    $lossPsi.FileName = $PythonBinary
    $lossPsi.WorkingDirectory = $current_loc
    $lossPsi.Arguments = @("`"$current_loc\rade_src\loss.py`" `"$current_loc\txfeatures.f32`" `"$current_loc\rxfeatures.f32`" --loss_test $LossThreshold --clip_start 200 --clip_end 300")

    $lossProcess = New-Object System.Diagnostics.Process
    $lossProcess.StartInfo = $lossPsi
    [void]$lossProcess.Start()

    $loss_output = $lossProcess.StandardOutput.ReadToEnd();
    $loss_err = $lossProcess.StandardError.ReadToEnd();
    $lossProcess.WaitForExit()

    Write-Host "$loss_output"
    Write-Host "$loss_err"

    $lossPasses = ($loss_output -split "`r?`n") | Where { $_.Contains("PASS") }
    if (($lossProcess.ExitCode -eq 0) -and ($lossPasses.Count -ge 1)) {
        return $true
    }
    return $false
}

$passes = 0
$fails = 0

$result = Test-RadeLoss `
    -RadioToComputerDevice $RadioToComputerDevice `
    -ComputerToSpeakerDevice $ComputerToSpeakerDevice `
    -MicrophoneToComputerDevice $MicrophoneToComputerDevice `
    -ComputerToRadioDevice $ComputerToRadioDevice `
    -LossThreshold $LossThreshold `
    -PythonBinary $PythonBinary
if ($result -eq $true)
{
    $passes++
}
else
{
    $fails++
}

Write-Host "Mode: RADEV1, Passed: $passes, Failures: $fails"

if ($fails -gt 0) {
    throw "Test failed"
    exit 1
}
