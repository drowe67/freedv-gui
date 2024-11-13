function Test-FreeDV {
    param (
        $ModeToTest
    )

    $current_loc = Get-Location

    # Generate new conf 
    $conf_tmpl = Get-Content "$current_loc\freedv-ctest-fullduplex.conf.tmpl"
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_RADIO_TO_COMPUTER_DEVICE@", $FREEDV_RADIO_TO_COMPUTER_DEVICE)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_RADIO_DEVICE@", $FREEDV_COMPUTER_TO_RADIO_DEVICE)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@", $FREEDV_MICROPHONE_TO_COMPUTER_DEVICE)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@", $FREEDV_COMPUTER_TO_SPEAKER_DEVICE)
    $tmp_file = New-TemporaryFile
    $conf_tmpl | Set-Content -Path $tmp_file.FullName

    # Start freedv.exe
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.CreateNoWindow = $true
    $psi.UseShellExecute = $false
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardOutput = $true
    $psi.FileName = "$current_loc\freedv.exe"
    $psi.WorkingDirectory = $current_loc
    $quoted_tmp_filename = "`"" + $tmp_file.FullName + "`""
    $psi.Arguments = @("/f $quoted_tmp_filename /ut txrx /utmode $mode_to_test")

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()

    # Read output from process
    $err_output = $process.StandardError.ReadToEnd();
    $output = $process.StandardOutput.ReadToEnd();
    $process.WaitForExit()

    $syncs = $err_output.Split([Environment]::NewLine) | Where { $_.Contains("Sync changed") }
    if ($syncs.Count -eq 1) {
        return $true
    }
    return $false
}

$modes = @("700D", "700E", "1600", "RADE")

$FREEDV_RADIO_TO_COMPUTER_DEVICE = "Microphone (iMic USB audio system)"
$FREEDV_COMPUTER_TO_RADIO_DEVICE = "Speakers (iMic USB audio system)"
$FREEDV_MICROPHONE_TO_COMPUTER_DEVICE = "Microphone Array (Realtek High Definition Audio(SST))"
$FREEDV_COMPUTER_TO_SPEAKER_DEVICE = "Speakers (Realtek High Definition Audio(SST))"

Foreach ($mode_to_test in $modes)
{
    $passes = 0
    $fails = 0

    for (($i = 0); $i -lt 10; $i++)
    {
        $result = Test-FreeDV -ModeToTest $mode_to_test
        if ($result -eq $true)
        {
            $passes++
        }
        else
        {
            $fails++
        }
    }

    Write-Host "Mode: $mode_to_test, Passed: $passes, Failures: $fails"
}