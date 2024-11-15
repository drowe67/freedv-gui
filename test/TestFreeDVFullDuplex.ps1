param (
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $RadioToComputerDevice, 

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ComputerToSpeakerDevice, 

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $MicrophoneToComputerDevice, 

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ComputerToRadioDevice,
    
    [ValidateSet("RADE", "700D", "700E", "1600")]
    [ValidateNotNullOrEmpty()]
    [string]$ModeToTest="RADE", 

    [int] $NumberOfRuns=10)


function Test-FreeDV {
    param (
        $ModeToTest,
        $RadioToComputerDevice,
        $ComputerToSpeakerDevice,
        $MicrophoneToComputerDevice,
        $ComputerToRadioDevice
    )

    $current_loc = Get-Location

    # Generate new conf 
    $conf_tmpl = Get-Content "$current_loc\freedv-ctest-fullduplex.conf.tmpl"
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_RADIO_TO_COMPUTER_DEVICE@", $RadioToComputerDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_RADIO_DEVICE@", $ComputerToRadioDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@", $MicrophoneToComputerDevice)
    $conf_tmpl = $conf_tmpl.Replace("@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@", $ComputerToSpeakerDevice)
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
    $psi.Arguments = @("/f $quoted_tmp_filename /ut txrx /utmode $ModeToTest")

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

$passes = 0
$fails = 0

for (($i = 0); $i -lt $NumberOfRuns; $i++)
{
    $result = Test-FreeDV `
        -ModeToTest $ModeToTest `
        -RadioToComputerDevice $RadioToComputerDevice `
        -ComputerToSpeakerDevice $ComputerToSpeakerDevice `
        -MicrophoneToComputerDevice $MicrophoneToComputerDevice `
        -ComputerToRadioDevice $ComputerToRadioDevice
    if ($result -eq $true)
    {
        $passes++
    }
    else
    {
        $fails++
    }
}

Write-Host "Mode: $ModeToTest, Total Runs: $NumberOfRuns, Passed: $passes, Failures: $fails"