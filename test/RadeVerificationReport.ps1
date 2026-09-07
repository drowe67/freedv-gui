<#
  .SYNOPSIS
  Begins populating the RADE integration verification report for FreeDV.

  .DESCRIPTION
  Fills in the "Application Under Test", "Signal Path Declaration",
  "Baseline Loss (Step 1)" and "Level 1 - Software Loopback" sections of the RADE
  integration verification template
  (https://github.com/drowe67/radae/blob/dr-tx-bpf/doc/verification/template.md).
  The hardware sections (Level 2 - OTAC, Level 3 - OTC) are left as blank template
  fields for the tester to complete by hand.

  The Level 1 result is produced by test/TestFreeDVRadeLoss.ps1, which is also how
  Windows CI exercises RADE loss (see .github/workflows/cmake-windows.yml). The
  baseline is produced with rade_tx_wav / rade_rx_wav from a rade_c build; those
  tools are not built for Windows, so on Windows pass -BaselineLoss (for example
  the value emitted by the rade-loss-baseline CI job) or point -RadeCToolsDir at a
  build that has them.

  This is the PowerShell equivalent of test/rade_verification_report.sh.

  .EXAMPLE
  PS> .\RadeVerificationReport.ps1 `
        -RadioToComputerDevice "CABLE Output (VB-Audio Virtual Cable)" `
        -ComputerToRadioDevice "Speakers (VB-Audio Virtual Cable)" `
        -ComputerToSpeakerDevice "Line 1 (Virtual Audio Cable)" `
        -MicrophoneToComputerDevice "Line 1 (Virtual Audio Cable)" `
        -BaselineLoss 0.113 -Tester "AB1CDE"
#>

param (
    [Parameter(Mandatory = $true)] [ValidateNotNullOrEmpty()] [string]
    # The sound device to receive RX audio from (also records FreeDV's TX output).
    $RadioToComputerDevice,

    [Parameter(Mandatory = $true)] [ValidateNotNullOrEmpty()] [string]
    # The sound device to emit decoded audio to.
    $ComputerToSpeakerDevice,

    [Parameter(Mandatory = $true)] [ValidateNotNullOrEmpty()] [string]
    # The sound device to receive analog audio from.
    $MicrophoneToComputerDevice,

    [Parameter(Mandatory = $true)] [ValidateNotNullOrEmpty()] [string]
    # The sound device to emit TX audio to (also replays it into FreeDV's RX input).
    $ComputerToRadioDevice,

    [string]
    # Where to write the report (default: .\rade_verification_report.md).
    $OutputFile = ".\rade_verification_report.md",

    [double]
    # Baseline loss figure. When 0 (default) the script tries to compute it with
    # rade_tx_wav.exe / rade_rx_wav.exe.
    $BaselineLoss = 0,

    [string]
    # Directory containing rade_tx_wav.exe / rade_rx_wav.exe (from a rade_c build).
    $RadeCToolsDir = "",

    [string]
    # Tester name / callsign for the report.
    $Tester = "",

    [switch]
    # Fill software info + baseline only; do not run the Level 1 loopback.
    $SkipLevel1,

    [string]
    # Parse Level 1 from this existing TestFreeDVRadeLoss.ps1 log instead of
    # running the loopback again.
    $Level1Log = "",

    [string]
    # FreeDV source tree, used for the version / git hash. Defaults to the parent
    # of this script's directory.
    $RepoRoot = "",

    [string]
    # Override the "rade_c repo commit hash" field.
    $RadeCCommit = "",

    [string]
    # Override the "radae repo commit hash" field.
    $RadaeCommit = "",

    [string]
    # Path or filename of the Python interpreter used to run loss.py.
    $PythonBinary = "python.exe")

$ErrorActionPreference = "Stop"

# Native tools (sox, rade_*_wav, git, TestFreeDVRadeLoss.ps1's children) report
# failure via exit code, which this script checks explicitly. Don't let
# PowerShell 7.4+ turn a non-zero native exit into a terminating error. On
# Windows PowerShell 5.1 this simply defines an unused variable.
$PSNativeCommandUseErrorActionPreference = $false

# Non-ASCII glyphs used in the report, built from code points so this script
# stays pure ASCII and parses identically under Windows PowerShell 5.1 and pwsh.
$PM    = [string][char]0x00B1   # plus-minus
$MDASH = [string][char]0x2014   # em dash

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = if ($RepoRoot) { $RepoRoot } else { Split-Path -Parent $scriptDir }
$current   = (Get-Location).Path

# Resolve output path before anything else changes directories.
if (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $OutputFile = Join-Path $current $OutputFile
}

Write-Host "RADE verification report -> $OutputFile"
Write-Host "Working directory: $current"

function Get-GitOutput {
    param ($dir, [string[]] $gitArgs)
    try {
        $out = & git.exe -C $dir @gitArgs 2>$null
        if ($LASTEXITCODE -eq 0 -and $out) {
            return ($out | Select-Object -First 1).ToString().Trim()
        }
    } catch { }
    return "unknown"
}

# ---------------------------------------------------------------------------
# radae repo (all.wav + loss.py), needed only to compute the baseline here.
# Cloned the same way TestFreeDVRadeLoss.ps1 clones it.
# ---------------------------------------------------------------------------
$radeSrc = Join-Path $current "rade_src"
if (($BaselineLoss -le 0) -and -not (Test-Path $radeSrc)) {
    Write-Host "Cloning radae repo into $radeSrc ..."
    & git.exe clone -b main https://github.com/drowe67/radae.git $radeSrc
}

# ---------------------------------------------------------------------------
# Software / platform information.
# ---------------------------------------------------------------------------
$appName = "FreeDV"

$projectVersion = ""
$cmakeLists = Join-Path $repoRoot "CMakeLists.txt"
if (Test-Path $cmakeLists) {
    $m = [regex]::Match((Get-Content -Raw $cmakeLists), 'set\(PROJECT_VERSION ([0-9.]+)\)')
    if ($m.Success) { $projectVersion = $m.Groups[1].Value }
}
$freedvGit = Get-GitOutput $repoRoot @('describe', '--tags', '--always', '--dirty')
if ($projectVersion) {
    $appVersion = "$projectVersion-dev (git $freedvGit)"
} else {
    $appVersion = "git $freedvGit"
}

try {
    $os = Get-CimInstance Win32_OperatingSystem
    $platform = "$($os.Caption) $($os.Version) (build $($os.BuildNumber)) $env:PROCESSOR_ARCHITECTURE"
} catch {
    $platform = "$([System.Environment]::OSVersion.VersionString) $env:PROCESSOR_ARCHITECTURE"
}

$reportDate = (Get-Date -Format "yyyy-MM-dd")

$radaeCommit = if ($RadaeCommit) { $RadaeCommit } else { "unknown" }
if (-not $RadaeCommit -and (Test-Path (Join-Path $radeSrc ".git"))) {
    $radaeCommit = Get-GitOutput $radeSrc @('rev-parse', 'HEAD')
}

# rade_c source (only present when a local build tree exists).
$radeCSrcDir = ""
foreach ($d in @(
        (Join-Path $current "_deps\freedv_backend-build\rade_src"),
        (Join-Path $repoRoot "build_windows\_deps\freedv_backend-build\rade_src"))) {
    if (Test-Path (Join-Path $d ".git")) { $radeCSrcDir = $d; break }
}
$radeCCommit = if ($RadeCCommit) { $RadeCCommit } elseif ($radeCSrcDir) { Get-GitOutput $radeCSrcDir @('rev-parse', 'HEAD') } else { "unknown" }

Write-Host "  Application  : $appName $appVersion"
Write-Host "  Platform     : $platform"
Write-Host "  radae commit : $radaeCommit"
Write-Host "  rade_c commit: $radeCCommit"

# ---------------------------------------------------------------------------
# Baseline Loss (Step 1).
# ---------------------------------------------------------------------------
$baselineCmd = @'
rade_tx_wav --v2 -f baseline_txfeatures.f32 all.wav baseline_tx.wav
rade_rx_wav --v2 -f baseline_rxfeatures.f32 baseline_tx.wav baseline_decoded.wav
python3 loss.py baseline_txfeatures.f32 baseline_rxfeatures.f32 --clip_start 100 --clip_end 300
'@

function Measure-BaselineLoss {
    param ($current, $radeSrc, $RadeCToolsDir, $PythonBinary)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($RadeCToolsDir) { $candidates.Add($RadeCToolsDir) }
    $candidates.Add($current)
    $candidates.Add((Join-Path $current "_deps\freedv_backend-build\rade_build\src"))
    $candidates.Add((Join-Path $repoRoot "build_windows\_deps\freedv_backend-build\rade_build\src"))
    $hit = Get-ChildItem -Path $repoRoot -Recurse -Filter "rade_tx_wav.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { $candidates.Add($hit.DirectoryName) }

    $toolsDir = $null
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c "rade_tx_wav.exe")) -and (Test-Path (Join-Path $c "rade_rx_wav.exe"))) {
            $toolsDir = $c; break
        }
    }
    if (-not $toolsDir) { return $null }

    $allWav = Join-Path $radeSrc "wav\all.wav"
    if (-not (Test-Path $allWav)) { return $null }

    $baseIn      = Join-Path $current "baseline_in.wav"
    $baseTxWav   = Join-Path $current "baseline_tx.wav"
    $baseDecoded = Join-Path $current "baseline_decoded.wav"
    $baseTxF     = Join-Path $current "baseline_txfeatures.f32"
    $baseRxF     = Join-Path $current "baseline_rxfeatures.f32"

    $o = & sox.exe $allWav -r 16000 -c 1 -b 16 -e signed-integer $baseIn 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host "$o"; return $null }

    $oldPath = $env:PATH
    $env:PATH = "$toolsDir;$env:PATH"
    try {
        # --v2: FreeDV opens RADE with RADE_MODE_V2 unconditionally on this branch
        # (src/freedv_interface.cpp), so the baseline must use V2 to be comparable.
        $o = & (Join-Path $toolsDir "rade_tx_wav.exe") --v2 -f $baseTxF $baseIn $baseTxWav 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Host "$o"; return $null }
        $o = & (Join-Path $toolsDir "rade_rx_wav.exe") --v2 -f $baseRxF $baseTxWav $baseDecoded 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Host "$o"; return $null }
    } finally {
        $env:PATH = $oldPath
    }

    $lossOut = & $PythonBinary (Join-Path $radeSrc "loss.py") $baseTxF $baseRxF --clip_start 100 --clip_end 300 2>&1
    Write-Host "software-only baseline: $lossOut"
    $m = [regex]::Match(($lossOut -join "`n"), 'loss:\s*([0-9]+\.?[0-9]*)')
    if (-not $m.Success) { return $null }
    return [double]$m.Groups[1].Value
}

$baselineNote = ""
if ($BaselineLoss -le 0) {
    Write-Host "Computing baseline loss with rade_tx_wav.exe / rade_rx_wav.exe ..."
    $measured = Measure-BaselineLoss -current $current -radeSrc $radeSrc -RadeCToolsDir $RadeCToolsDir -PythonBinary $PythonBinary
    if ($null -ne $measured) {
        $BaselineLoss = $measured
        Write-Host "  Baseline loss: $BaselineLoss"
    } else {
        $baselineNote = "Could not run rade_tx_wav.exe / rade_rx_wav.exe automatically (they are not built for Windows). Pass -BaselineLoss (for example the value from the rade-loss-baseline CI job) or run test/rade_verification_report.sh on Linux/macOS."
        Write-Host "  Baseline loss: (unavailable)"
    }
}

if ($BaselineLoss -gt 0) {
    $baselineField   = ("{0:0.###}" -f $BaselineLoss)
    $tol = [math]::Round($BaselineLoss * 0.10, 4)
    $lo  = [math]::Round($BaselineLoss * 0.90, 4)
    $hi  = [math]::Round($BaselineLoss * 1.10, 4)
    $toleranceField  = "baseline $PM $tol  ($lo $MDASH $hi)"
    $threshold       = $hi
} else {
    $baselineField   = "_not determined_"
    $toleranceField  = "baseline $PM 10%"
    $threshold       = 0
}

# ---------------------------------------------------------------------------
# Level 1 - Software Loopback: run TestFreeDVRadeLoss.ps1 and harvest its loss.
# ---------------------------------------------------------------------------
$level1Check   = "- [ ] Pass (loss within ${PM}10% of baseline)"
$level1Loss    = "_not run_"
$level1Summary = "PASS / FAIL / N/A"
$level1Repro   = "Not run by RadeVerificationReport.ps1 (-SkipLevel1 was set)."
$level1Log     = Join-Path $current "rade_verification_level1.log"

$level1FromExisting = $false
if (-not $SkipLevel1) {
    if ($Level1Log -and -not (Test-Path $Level1Log)) {
        Write-Host "WARNING: -Level1Log $Level1Log does not exist"
        "Level1Log not found: $Level1Log" | Set-Content -Path $level1Log
        $level1FromExisting = $true
    } elseif ($Level1Log) {
        Write-Host "Reading Level 1 result from existing log: $Level1Log"
        $level1Log = (Resolve-Path $Level1Log).Path
        $level1FromExisting = $true
    } else {
        Write-Host "Running Level 1 software loopback via test/TestFreeDVRadeLoss.ps1 ..."
        $lossScript = Join-Path $scriptDir "TestFreeDVRadeLoss.ps1"
        $psArgs = @{
            RadioToComputerDevice      = $RadioToComputerDevice
            ComputerToSpeakerDevice    = $ComputerToSpeakerDevice
            MicrophoneToComputerDevice = $MicrophoneToComputerDevice
            ComputerToRadioDevice      = $ComputerToRadioDevice
            PythonBinary               = $PythonBinary
        }
        if ($threshold -gt 0) { $psArgs["LossThreshold"] = $threshold }

        # Tee every stream to the log as it flows, so a terminating "Test failed"
        # thrown by TestFreeDVRadeLoss.ps1 at the end still leaves the loss line on disk.
        if (Test-Path $level1Log) { Remove-Item $level1Log }
        try {
            & $lossScript @psArgs *>&1 | Tee-Object -FilePath $level1Log | Out-Null
        } catch {
            ($_ | Out-String) | Add-Content -Path $level1Log
        }
    }
    $level1Text = if (Test-Path $level1Log) { Get-Content -Raw $level1Log } else { "" }
    if (-not $level1Text) { $level1Text = "" }

    $lm = [regex]::Matches($level1Text, 'loss:\s*([0-9]+\.?[0-9]*)')
    $lossValue = if ($lm.Count -gt 0) { $lm[$lm.Count - 1].Groups[1].Value } else { "" }
    $level1Result = if ($level1Text -match 'Failures:\s*0') { "PASS" }
                    elseif ($level1Text -match '(?m)^\s*FAIL\s*$') { "FAIL" }
                    else { "ERROR" }

    Write-Host "  Level 1 result: $level1Result (loss $lossValue)"
    Write-Host "  Full log: $level1Log"

    if ($lossValue) { $level1Loss = "$lossValue  ($level1Result)" }
    else            { $level1Loss = "$level1Result (see log)" }

    switch ($level1Result) {
        "PASS" { $level1Check = "- [x] Pass (loss within ${PM}10% of baseline)"; $level1Summary = "PASS" }
        "FAIL" { $level1Check = "- [ ] Pass (loss within ${PM}10% of baseline)  <!-- FAIL -->"; $level1Summary = "FAIL" }
        default { $level1Summary = "FAIL" }
    }

    $thrPart = if ($threshold -gt 0) { " -LossThreshold $threshold" } else { "" }
    # Single-quoted here-string: backticks and other characters are literal.
    $level1Repro = @'
{{PARSED}}# From the FreeDV install / build directory (see
# .github/workflows/cmake-windows.yml for the virtual audio setup):
.\TestFreeDVRadeLoss.ps1 `
    -RadioToComputerDevice "<device>" -ComputerToRadioDevice "<device>" `
    -MicrophoneToComputerDevice "<device>" -ComputerToSpeakerDevice "<device>"{{THR}}

# loss.py invocation performed inside TestFreeDVRadeLoss.ps1:
python loss.py txfeatures.f32 rxfeatures.f32 `
    --loss_test <baseline x 1.10> --clip_start 100 --clip_end 300
'@
    $parsedNote = if ($level1FromExisting) { "# Result parsed from an existing run of the command below.`n" } else { "" }
    $level1Repro = $level1Repro.Replace('{{PARSED}}', $parsedNote).Replace('{{THR}}', $thrPart)
}

# ---------------------------------------------------------------------------
# Emit the report.
# ---------------------------------------------------------------------------
$template = @'
# RADE Integration Verification Report

## Application Under Test

| Field | Value |
|---|---|
| Application name | {{APP_NAME}} |
| Application version / git hash | {{APP_VERSION}} |
| Platform (OS + version) | {{PLATFORM}} |
| Tester name / callsign | {{TESTER}} |
| Date | {{DATE}} |
| radae repo commit hash | {{RADAE_COMMIT}} |
| rade_c repo commit hash | {{RADE_C_COMMIT}} |

## Signal Path Declaration

- [x] No additional signal processing (AGC, noise gate, resampler, EQ,
      compression) between WAV file input and RADE encoder input during
      this verification test
      <!-- Asserted automatically: test/freedv-ctest-loss.conf.tmpl disables
           AGC, the mic/speaker EQ and Speex noise suppression, and the RADE
           path runs at a fixed 8/16 kHz with no resampling. -->

## Baseline Loss (Step 1)

Re-run with the current repository version before filling in this section.

| Field | Value |
|---|---|
| Baseline loss | {{BASELINE}} |
| 10% tolerance window | {{TOLERANCE}} |

Command used:
```
{{BASELINE_CMD}}
```
{{BASELINE_NOTE}}
## Level 1 {{MDASH}} Software Loopback (mandatory)

{{LEVEL1_CHECK}}

| Field | Value |
|---|---|
| Loss result | {{LEVEL1_LOSS}} |

Command used / reproduction notes:
```
{{LEVEL1_REPRO}}
```

## Level 2 {{MDASH}} OTAC: Over The Audio Cable (mandatory for hardware integrations)

- [ ] Pass (loss within {{PM}}10% of baseline)
- [ ] N/A (software-only integration)

| Field | Value |
|---|---|
| Loss result | |
| Sound card (Tx) | |
| Sound card (Rx) | |
| Cable description | |

Photo of test setup:
_(attach or link)_

Reproduction notes:
```
(paste notes here)
```

## Level 3 {{MDASH}} OTC: Over The Coax (optional)

- [ ] Pass (loss within {{PM}}10% of baseline)
- [ ] Not performed

| Field | Value |
|---|---|
| Loss result | |
| Tx radio | |
| Rx radio | |
| Attenuator(s) | |

Photo of test setup:
_(attach or link)_

Reproduction notes:
```
(paste notes here)
```

## Summary

| Level | Result |
|---|---|
| Level 1 {{MDASH}} Software loopback | {{LEVEL1_SUMMARY}} |
| Level 2 {{MDASH}} OTAC | PASS / FAIL / N/A |
| Level 3 {{MDASH}} OTC | PASS / FAIL / N/A |

Additional notes:
'@

$noteBlock = if ($baselineNote) { "`n_" + $baselineNote + "_`n" } else { "" }

# String.Replace() is a literal substitution (no regex / no $1 handling), so
# values containing '$' or '\' are inserted verbatim.
$report = $template.
    Replace('{{APP_NAME}}',      $appName).
    Replace('{{APP_VERSION}}',   $appVersion).
    Replace('{{PLATFORM}}',      $platform).
    Replace('{{TESTER}}',        $Tester).
    Replace('{{DATE}}',          $reportDate).
    Replace('{{RADAE_COMMIT}}',  $radaeCommit).
    Replace('{{RADE_C_COMMIT}}', $radeCCommit).
    Replace('{{BASELINE}}',      $baselineField).
    Replace('{{TOLERANCE}}',     $toleranceField).
    Replace('{{BASELINE_CMD}}',  $baselineCmd.Trim()).
    Replace('{{BASELINE_NOTE}}', $noteBlock).
    Replace('{{LEVEL1_CHECK}}',  $level1Check).
    Replace('{{LEVEL1_LOSS}}',   $level1Loss).
    Replace('{{LEVEL1_REPRO}}',  $level1Repro.Trim()).
    Replace('{{LEVEL1_SUMMARY}}', $level1Summary).
    Replace('{{MDASH}}',         $MDASH).
    Replace('{{PM}}',            $PM)

Set-Content -Path $OutputFile -Value $report -Encoding UTF8
Write-Host "Wrote $OutputFile"
