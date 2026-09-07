<#
  .SYNOPSIS
  Executes RADE loss test of FreeDV.

  .DESCRIPTION
  This script pushes a known test file through FreeDV's TX pipeline while recording the resulting audio from
  the real Windows sound card path, then plays that recording back into FreeDV's RX pipeline. The RADE features
  captured on the TX and RX sides are compared using the loss.py tool from the radae repository, which prints
  PASS or FAIL depending on whether the feature loss introduced by the round trip through real audio hardware
  is below the given threshold. This is the PowerShell equivalent of test/test_rade_loss.sh.

  When -LossThreshold is not supplied, the threshold is derived from a software-only baseline as described in
  the RADE integration verification procedure
  (https://github.com/drowe67/radae/blob/dr-tx-bpf/doc/verification/verification_procedure.md): all.wav is run
  through rade_tx_wav + rade_rx_wav and loss.py, and the hardware round trip must stay within +10% of that
  baseline loss. rade_tx_wav / rade_rx_wav are not built for Windows in the rade_c fork, so Windows CI computes
  the baseline on a Linux runner and passes the value in via -LossThreshold; -RadeCToolsDir lets a local run
  point at a rade_c build that does have the tools.

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
    # When left at 0 (the default) it is computed from a software-only baseline (see .DESCRIPTION):
    # baseline loss from rade_tx_wav/rade_rx_wav on all.wav, times 1.10.
    $LossThreshold = 0,

    [string]
    # Directory containing rade_tx_wav.exe / rade_rx_wav.exe (from a rade_c build), used to compute
    # the baseline when -LossThreshold is not supplied. Auto-detected when possible.
    $RadeCToolsDir = "",

    [string]
    # Path or filename of the Python interpreter used to run loss.py.
    $PythonBinary = "python.exe")

# Fallback threshold used only when -LossThreshold is not supplied and the software-only
# baseline cannot be computed (e.g. rade_tx_wav / rade_rx_wav unavailable).
$FallbackLossThreshold = 0.0891
$LossTolerance = 1.10

<#
    .Description
    Runs all.wav through the rade_c software-only path (rade_tx_wav -> rade_rx_wav -> loss.py) and returns
    the baseline feature loss multiplied by $LossTolerance, i.e. the maximum loss the hardware round trip is
    allowed to introduce. Returns $null if the tools, test corpus or loss figure can't be found, in which
    case the caller should fall back to $FallbackLossThreshold. Mirrors compute_loss_threshold() in
    test/test_rade_loss.sh.
#>
function Get-RadeLossThreshold {
    param (
        $current_loc,
        $RadeCToolsDir,
        $PythonBinary,
        $Tolerance
    )

    # Locate rade_tx_wav.exe / rade_rx_wav.exe from a rade_c build.
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($RadeCToolsDir) { $candidates.Add($RadeCToolsDir) }
    $candidates.Add("$current_loc")
    $candidates.Add((Join-Path $current_loc "_deps\freedv_backend-build\rade_build\src"))
    $found = Get-ChildItem -Path $current_loc -Recurse -Filter "rade_tx_wav.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $candidates.Add($found.DirectoryName) }

    $toolsDir = $null
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c "rade_tx_wav.exe")) -and (Test-Path (Join-Path $c "rade_rx_wav.exe"))) {
            $toolsDir = $c
            break
        }
    }
    if (-not $toolsDir) {
        Write-Host "Could not find rade_tx_wav.exe / rade_rx_wav.exe; pass -RadeCToolsDir or -LossThreshold."
        return $null
    }

    $allWav = Join-Path $current_loc "rade_src\wav\all.wav"
    if (-not (Test-Path $allWav)) {
        Write-Host "Could not find $allWav for baseline computation."
        return $null
    }

    $baseIn      = Join-Path $current_loc "baseline_in.wav"
    $baseTxWav   = Join-Path $current_loc "baseline_tx.wav"
    $baseDecoded = Join-Path $current_loc "baseline_decoded.wav"
    $baseTxF     = Join-Path $current_loc "baseline_txfeatures.f32"
    $baseRxF     = Join-Path $current_loc "baseline_rxfeatures.f32"

    # rade_tx_wav requires 16 kHz mono 16-bit PCM (all.wav already is); normalise defensively.
    # Output of the native tools is captured (not left on the pipeline) so it can't corrupt the return value.
    $toolOut = & sox.exe $allWav -r 16000 -c 1 -b 16 -e signed-integer $baseIn 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host "$toolOut"; return $null }

    # Prepend the tools directory to PATH so librade.dll resolves next to the executables.
    $oldPath = $env:PATH
    $env:PATH = "$toolsDir;$env:PATH"
    try {
        # RADEV1 to match the mode this test exercises (rade_tx_wav defaults to V1).
        $toolOut = & (Join-Path $toolsDir "rade_tx_wav.exe") -f $baseTxF $baseIn $baseTxWav 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Host "$toolOut"; return $null }
        $toolOut = & (Join-Path $toolsDir "rade_rx_wav.exe") -f $baseRxF $baseTxWav $baseDecoded 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Host "$toolOut"; return $null }
    }
    finally {
        $env:PATH = $oldPath
    }

    $baselineOutput = & $PythonBinary (Join-Path $current_loc "rade_src\loss.py") $baseTxF $baseRxF --clip_start 100 --clip_end 300 2>&1
    Write-Host "software-only baseline: $baselineOutput"

    $m = [regex]::Match(($baselineOutput -join "`n"), 'loss:\s*([0-9]+\.?[0-9]*)')
    if (-not $m.Success) { return $null }

    $baselineLoss = [double]$m.Groups[1].Value
    if ($baselineLoss -le 0) { return $null }

    return [math]::Round($baselineLoss * $Tolerance, 4)
}

<#
    .Description
    Starts FreeDV in RX mode listening on the real sound card, plays the given wav file back into it, then
    compares the resulting RX RADE features against the reference txfeatures.f32 using loss.py. Returns
    $true/$false depending on whether the loss is within the given threshold. Factored out of Test-RadeLoss
    so it can be retried against a phase-shifted copy of the recording (see the RADEV2 sync-timing workaround
    below).
#>
function Invoke-RadeLossAttempt {
    param (
        $current_loc,
        $psi,
        $ComputerToRadioDevice,
        $PlaybackFile,
        $PythonBinary,
        $LossThreshold
    )

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal

    Start-Sleep -Milliseconds 4995

    # Play the recorded TX audio back into FreeDV's RX input
    $playPsi = New-Object System.Diagnostics.ProcessStartInfo
    $playPsi.CreateNoWindow = $true
    $playPsi.UseShellExecute = $false
    $playPsi.RedirectStandardError = $false
    $playPsi.RedirectStandardOutput = $false
    $playPsi.FileName = "sox.exe"
    $playPsi.WorkingDirectory = $current_loc
    $quoted_play_device = "`"" + $ComputerToRadioDevice + "`""
    $playPsi.Arguments = @("--buffer 128000 -t wav `"$PlaybackFile`" -t waveaudio $quoted_play_device")

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
    $lossPsi.Arguments = @("`"$current_loc\rade_src\loss.py`" `"$current_loc\txfeatures.f32`" `"$current_loc\rxfeatures.f32`" --loss_test $LossThreshold --clip_start 100 --clip_end 300")

    $lossProcess = New-Object System.Diagnostics.Process
    $lossProcess.StartInfo = $lossPsi
    [void]$lossProcess.Start()

    $loss_output = $lossProcess.StandardOutput.ReadToEnd();
    $loss_err = $lossProcess.StandardError.ReadToEnd();
    $lossProcess.WaitForExit()

    Write-Host "$loss_output"
    Write-Host "$loss_err"

    $lossPasses = ($loss_output -split "`r?`n") | Where { $_.Contains("PASS") }
    return (($lossProcess.ExitCode -eq 0) -and ($lossPasses.Count -ge 1))
}

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
        $RadeCToolsDir,
        $PythonBinary
    )

    $current_loc = Get-Location

    # Clone radae repo (contains the test audio and the loss.py comparison tool) if not already present.
    if (-not (Test-Path "$current_loc\rade_src")) {
        & git.exe clone -b main https://github.com/drowe67/radae.git "$current_loc\rade_src"
    }

    # Resample test file to 48 kHz. Needed for CI environment to reduce CPU usage.
    & sox.exe "$current_loc\rade_src\wav\all.wav" -r 48000 "$current_loc\tx_in.wav"

    # Resolve the loss threshold: use -LossThreshold when supplied, otherwise derive it from the
    # software-only baseline (see .DESCRIPTION), falling back to a fixed value if that can't be done.
    if ($LossThreshold -le 0) {
        $computed = Get-RadeLossThreshold -current_loc $current_loc -RadeCToolsDir $RadeCToolsDir -PythonBinary $PythonBinary -Tolerance $LossTolerance
        if ($null -ne $computed) {
            $LossThreshold = $computed
            Write-Host "RADE loss threshold: $LossThreshold (software-only baseline x $LossTolerance)"
        } else {
            $LossThreshold = $FallbackLossThreshold
            Write-Host "WARNING: could not compute RADE loss baseline; using fallback threshold $LossThreshold"
        }
    } else {
        Write-Host "RADE loss threshold: $LossThreshold (supplied via -LossThreshold)"
    }

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
    $recordPsi.Arguments = @("--buffer 128000 -t waveaudio $quoted_record_device -c 1 -t wav -r 48000 -b 16 -e signed-integer `"$current_loc\test.wav`"")

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

    # Workaround/performance improvement: strip silence at beginning and end of recording
    # As well as reducing the amount of audio that needs to be played back, it also helps
    # ensure we don't accidentally run into a potential RADEV2 bug (https://github.com/freedv/rade_c/issues/8)
    # Note: commands adapted from https://digitalcardboard.com/blog/2009/08/25/the-sox-of-silence/
    $recordPsi.Arguments = @("test.wav test_stripped.wav silence 1 0.1 1% reverse")
    $stripProcess = New-Object System.Diagnostics.Process
    $stripProcess.StartInfo = $recordPsi
    [void]$stripProcess.Start()
    $stripProcess.WaitForExit()

    $recordPsi.Arguments = @("test_stripped.wav test.wav silence 1 0.1 1% reverse")
    $stripProcess = New-Object System.Diagnostics.Process
    $stripProcess.StartInfo = $recordPsi
    [void]$stripProcess.Start()
    $stripProcess.WaitForExit()

    # Restart FreeDV in RX mode, reading live from the sound card so that any dropouts introduced by the
    # real audio path get captured in the RX feature file (mirrors test/test_rade_loss.sh).
    $psi.Arguments = @("/f $quoted_tmp_filename /ut rx /utmode RADEV1 /txtime 70 /rxfeaturefile `"$current_loc\rxfeatures.f32`"")

    $passed = Invoke-RadeLossAttempt -current_loc $current_loc -psi $psi -ComputerToRadioDevice $ComputerToRadioDevice -PlaybackFile "$current_loc\test.wav" -PythonBinary $PythonBinary -LossThreshold $LossThreshold

    return $passed
}

$passes = 0
$fails = 0

$result = Test-RadeLoss `
    -RadioToComputerDevice $RadioToComputerDevice `
    -ComputerToSpeakerDevice $ComputerToSpeakerDevice `
    -MicrophoneToComputerDevice $MicrophoneToComputerDevice `
    -ComputerToRadioDevice $ComputerToRadioDevice `
    -LossThreshold $LossThreshold `
    -RadeCToolsDir $RadeCToolsDir `
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
