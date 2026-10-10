#include <wx/init.h>
#include <wx/stdpaths.h>

#include "../FreeDVConfiguration.h"
#include "ConfigTestCommon.h"

// Verifies that configuration written by older versions of FreeDV is picked
// up by the current one, and that the configuration as a whole survives
// being saved and loaded again.

namespace {

using StringList = std::vector<wxString>;

// Loading the reporting configuration creates a folder for the CSV log if
// one isn't configured, so every configuration used here has one.
const char* const BASE_CONFIG = "[Reporting/CSV]\nLogFilePath=/nonexistent/freedv_rx_log.csv\n";

std::unique_ptr<wxFileConfig> makeTestConfig(wxString const& contents = "")
{
    return makeConfig(wxString(BASE_CONFIG) + contents);
}

bool audioSampleRatesDefaultToUnset()
{
    auto config = makeTestConfig();
    AudioConfiguration audio;
    audio.load(config.get());

    bool result = true;
    result &= check("sound card 1 in rate", audio.soundCard1In.sampleRate.get(), -1);
    result &= check("sound card 1 out rate", audio.soundCard1Out.sampleRate.get(), -1);
    result &= check("sound card 2 in rate", audio.soundCard2In.sampleRate.get(), -1);
    result &= check("sound card 2 out rate", audio.soundCard2Out.sampleRate.get(), -1);
    result &= check("sound card 1 in device", audio.soundCard1In.deviceName.get(), "none");
    result &= check("sound card 2 out device", audio.soundCard2Out.deviceName.get(), "none");
    return result;
}

bool audioSampleRatesMigrateFromPerCardRates()
{
    // Older versions had a single sample rate for each sound card.
    auto config = makeTestConfig("[Audio]\nsoundCard1SampleRate=44100\nsoundCard2SampleRate=48000\n");
    AudioConfiguration audio;
    audio.load(config.get());

    bool result = true;
    result &= check("sound card 1 in rate", audio.soundCard1In.sampleRate.get(), 44100);
    result &= check("sound card 1 out rate", audio.soundCard1Out.sampleRate.get(), 44100);
    result &= check("sound card 2 in rate", audio.soundCard2In.sampleRate.get(), 48000);
    result &= check("sound card 2 out rate", audio.soundCard2Out.sampleRate.get(), 48000);
    if (!result) return false;

    // Once saved, the migration isn't needed any more.
    audio.save(config.get());
    auto reloaded = reload(*config);
    reloaded->DeleteEntry("/Audio/soundCard1SampleRate");
    reloaded->DeleteEntry("/Audio/soundCard2SampleRate");

    AudioConfiguration audioAfterRestart;
    audioAfterRestart.load(reloaded.get());
    result &= check("saved sound card 1 in rate", audioAfterRestart.soundCard1In.sampleRate.get(), 44100);
    result &= check("saved sound card 2 out rate", audioAfterRestart.soundCard2Out.sampleRate.get(), 48000);
    return result;
}

bool audioPerDirectionRatesTakePrecedence()
{
    auto config = makeTestConfig(
        "[Audio]\nsoundCard1SampleRate=44100\nsoundCard2SampleRate=48000\n"
        "soundCard1InSampleRate=16000\nsoundCard2OutSampleRate=8000\n");
    AudioConfiguration audio;
    audio.load(config.get());

    bool result = true;
    result &= check("sound card 1 in rate", audio.soundCard1In.sampleRate.get(), 16000);
    result &= check("sound card 1 out rate", audio.soundCard1Out.sampleRate.get(), 44100);
    result &= check("sound card 2 in rate", audio.soundCard2In.sampleRate.get(), 48000);
    result &= check("sound card 2 out rate", audio.soundCard2Out.sampleRate.get(), 8000);
    return result;
}

bool equalizerDefaults()
{
    auto config = makeTestConfig();
    FilterConfiguration filter;
    filter.load(config.get());

    bool result = true;
    for (auto channel : { 0, 1 })
    {
        // Both channels have the same defaults.
        auto bassFreq = channel == 0 ? filter.micInChannel.bassFreqHz.get() : filter.spkOutChannel.bassFreqHz.get();
        auto trebleFreq = channel == 0 ? filter.micInChannel.trebleFreqHz.get() : filter.spkOutChannel.trebleFreqHz.get();
        auto midFreq = channel == 0 ? filter.micInChannel.midFreqHz.get() : filter.spkOutChannel.midFreqHz.get();
        auto midQ = channel == 0 ? filter.micInChannel.midQ.get() : filter.spkOutChannel.midQ.get();
        auto bassGain = channel == 0 ? filter.micInChannel.bassGaindB.get() : filter.spkOutChannel.bassGaindB.get();
        auto vol = channel == 0 ? filter.micInChannel.volInDB.get() : filter.spkOutChannel.volInDB.get();
        auto enable = channel == 0 ? filter.micInChannel.eqEnable.get() : filter.spkOutChannel.eqEnable.get();

        result &= check("bass frequency", bassFreq, 100.0f);
        result &= check("treble frequency", trebleFreq, 3000.0f);
        result &= check("mid frequency", midFreq, 1500.0f);
        result &= check("mid Q", midQ, 1.0f);
        result &= check("bass gain", bassGain, 0.0f);
        result &= check("volume", vol, 0.0f);
        result &= check("EQ enable", enable, false);
    }
    return result;
}

bool equalizerMigratesFromIntegerSettings()
{
    // Older versions stored these as integers: gains in tenths of a dB and
    // Q in hundredths.
    auto config = makeTestConfig(
        "[Filter]\n"
        "MicInBassFreqHz=150\nMicInBassGaindB=-35\n"
        "MicInTrebleFreqHz=2500\nMicInTrebleGaindB=60\n"
        "MicInMidFreqHz=1200\nMicInMidGaindB=15\nMicInMidQ=250\n"
        "MicInVolInDB=-120\n"
        "SpkOutBassFreqHz=80\nSpkOutBassGaindB=20\n"
        "SpkOutTrebleFreqHz=3500\nSpkOutTrebleGaindB=-45\n"
        "SpkOutMidFreqHz=1800\nSpkOutMidGaindB=-5\nSpkOutMidQ=75\n"
        "SpkOutVolInDB=30\n");
    FilterConfiguration filter;
    filter.load(config.get());

    bool result = true;
    result &= check("mic bass frequency", filter.micInChannel.bassFreqHz.get(), 150.0f);
    result &= check("mic bass gain", filter.micInChannel.bassGaindB.get(), -3.5f);
    result &= check("mic treble frequency", filter.micInChannel.trebleFreqHz.get(), 2500.0f);
    result &= check("mic treble gain", filter.micInChannel.trebleGaindB.get(), 6.0f);
    result &= check("mic mid frequency", filter.micInChannel.midFreqHz.get(), 1200.0f);
    result &= check("mic mid gain", filter.micInChannel.midGainDB.get(), 1.5f);
    result &= check("mic mid Q", filter.micInChannel.midQ.get(), 2.5f);
    result &= check("mic volume", filter.micInChannel.volInDB.get(), -12.0f);
    result &= check("speaker bass frequency", filter.spkOutChannel.bassFreqHz.get(), 80.0f);
    result &= check("speaker bass gain", filter.spkOutChannel.bassGaindB.get(), 2.0f);
    result &= check("speaker treble frequency", filter.spkOutChannel.trebleFreqHz.get(), 3500.0f);
    result &= check("speaker treble gain", filter.spkOutChannel.trebleGaindB.get(), -4.5f);
    result &= check("speaker mid frequency", filter.spkOutChannel.midFreqHz.get(), 1800.0f);
    result &= check("speaker mid gain", filter.spkOutChannel.midGainDB.get(), -0.5f);
    result &= check("speaker mid Q", filter.spkOutChannel.midQ.get(), 0.75f);
    result &= check("speaker volume", filter.spkOutChannel.volInDB.get(), 3.0f);
    return result;
}

bool equalizerCurrentSettingsTakePrecedence()
{
    auto config = makeTestConfig(
        "[Filter]\nMicInBassFreqHz=150\nMicInBassGaindB=-35\n"
        "[Filter/MicIn]\nBassFreqHz=200\nBassGaindB=4.5\nEQEnable=1\n");
    FilterConfiguration filter;
    filter.load(config.get());

    return
        check("bass frequency", filter.micInChannel.bassFreqHz.get(), 200.0f) &&
        check("bass gain", filter.micInChannel.bassGaindB.get(), 4.5f) &&
        check("EQ enable", filter.micInChannel.eqEnable.get(), true);
}

bool equalizerSettingsSurviveRestart()
{
    auto config = makeTestConfig();
    FilterConfiguration filter;
    filter.load(config.get());
    filter.micInChannel.bassGaindB = -7.5f;
    filter.micInChannel.midQ = 0.3f;
    filter.micInChannel.eqEnable = true;
    filter.spkOutChannel.trebleFreqHz = 2750.0f;
    filter.spkOutChannel.volInDB = 12.1f;
    filter.save(config.get());

    auto reloaded = reload(*config);
    FilterConfiguration filterAfterRestart;
    filterAfterRestart.load(reloaded.get());

    bool result = true;
    result &= check("mic bass gain", filterAfterRestart.micInChannel.bassGaindB.get(), -7.5f);
    result &= check("mic mid Q", filterAfterRestart.micInChannel.midQ.get(), 0.3f);
    result &= check("mic EQ enable", filterAfterRestart.micInChannel.eqEnable.get(), true);
    result &= check("speaker treble frequency", filterAfterRestart.spkOutChannel.trebleFreqHz.get(), 2750.0f);
    result &= check("speaker volume", filterAfterRestart.spkOutChannel.volInDB.get(), 12.1f);
    result &= check("speaker EQ enable", filterAfterRestart.spkOutChannel.eqEnable.get(), false);
    return result;
}

bool postFilterDefaults()
{
    auto config = makeTestConfig();
    FilterConfiguration filter;
    filter.load(config.get());

    return
        check("gamma", filter.codec2LPCPostFilterGamma.get(), 0.5f) &&
        check("beta", filter.codec2LPCPostFilterBeta.get(), 0.2f);
}

bool postFilterMigratesFromOldNames()
{
    // Stored as a percentage, both before and after the rename.
    auto config = makeTestConfig("[Filter]\ncodec2LPCPostFilterGamma=60\ncodec2LPCPostFilterBeta=30\n");
    FilterConfiguration filter;
    filter.load(config.get());

    return
        check("gamma", filter.codec2LPCPostFilterGamma.get(), 0.6f) &&
        check("beta", filter.codec2LPCPostFilterBeta.get(), 0.3f);
}

bool postFilterCurrentNamesTakePrecedence()
{
    auto config = makeTestConfig(
        "[Filter]\ncodec2LPCPostFilterGamma=60\ncodec2LPCPostFilterBeta=30\n"
        "[Filter/codec2LPCPostFilter]\nGamma=55\nBeta=25\n");
    FilterConfiguration filter;
    filter.load(config.get());

    return
        check("gamma", filter.codec2LPCPostFilterGamma.get(), 0.55f) &&
        check("beta", filter.codec2LPCPostFilterBeta.get(), 0.25f);
}

bool postFilterIsStoredAsPercentage()
{
    auto config = makeTestConfig();
    FilterConfiguration filter;
    filter.load(config.get());
    filter.codec2LPCPostFilterGamma = 0.75f;
    filter.codec2LPCPostFilterBeta = 0.25f;
    filter.save(config.get());

    double rawGamma = 0;
    double rawBeta = 0;
    config->Read("/Filter/codec2LPCPostFilter/Gamma", &rawGamma);
    config->Read("/Filter/codec2LPCPostFilter/Beta", &rawBeta);
    if (!check("stored gamma", rawGamma, 75.0) || !check("stored beta", rawBeta, 25.0)) return false;

    auto reloaded = reload(*config);
    FilterConfiguration filterAfterRestart;
    filterAfterRestart.load(reloaded.get());
    return
        check("gamma", filterAfterRestart.codec2LPCPostFilterGamma.get(), 0.75f) &&
        check("beta", filterAfterRestart.codec2LPCPostFilterBeta.get(), 0.25f);
}

bool reportingMigratesFromPskReporterSettings()
{
    auto config = makeTestConfig(
        "[PSKReporter]\nEnable=1\nCallsign=K6AQ\nGridSquare=DM12\nFrequencyHzStr=14236000\n");
    ReportingConfiguration reporting;
    reporting.load(config.get());

    bool result = true;
    result &= check("enabled", reporting.reportingEnabled.get(), true);
    result &= check("callsign", reporting.reportingCallsign.get(), "K6AQ");
    result &= check("grid square", reporting.reportingGridSquare.get(), "DM12");
    result &= check("frequency", reporting.reportingFrequency.get(), (uint64_t)14236000);
    return result;
}

bool reportingCurrentSettingsTakePrecedence()
{
    auto config = makeTestConfig(
        "[PSKReporter]\nEnable=1\nCallsign=K6AQ\nGridSquare=DM12\nFrequencyHzStr=14236000\n"
        "[Reporting]\nEnable=0\nCallsign=VK5DGR\nGridSquare=PF95\nFrequency=7177000\n");
    ReportingConfiguration reporting;
    reporting.load(config.get());

    bool result = true;
    result &= check("enabled", reporting.reportingEnabled.get(), false);
    result &= check("callsign", reporting.reportingCallsign.get(), "VK5DGR");
    result &= check("grid square", reporting.reportingGridSquare.get(), "PF95");
    result &= check("frequency", reporting.reportingFrequency.get(), (uint64_t)7177000);
    return result;
}

bool reportingDefaultsWithNothingConfigured()
{
    auto config = makeTestConfig();
    ReportingConfiguration reporting;
    reporting.load(config.get());

    bool result = true;
    result &= check("enabled", reporting.reportingEnabled.get(), false);
    result &= check("callsign", reporting.reportingCallsign.get(), "");
    result &= check("grid square", reporting.reportingGridSquare.get(), "");
    result &= check("frequency", reporting.reportingFrequency.get(), (uint64_t)0);
    result &= check("CSV log path", reporting.csvLogFilePath.get(), "/nonexistent/freedv_rx_log.csv");
    return result;
}

bool reportingFrequencySurvivesRestart()
{
    // Includes one that doesn't fit in 32 bits (QO-100).
    for (uint64_t freq : { (uint64_t)0, (uint64_t)14236000, (uint64_t)10489640000ULL })
    {
        auto config = makeTestConfig();
        ReportingConfiguration reporting;
        reporting.load(config.get());
        reporting.reportingFrequency = freq;
        reporting.save(config.get());

        auto reloaded = reload(*config);
        ReportingConfiguration reportingAfterRestart;
        reportingAfterRestart.load(reloaded.get());
        if (!check("frequency", reportingAfterRestart.reportingFrequency.get(), freq)) return false;
    }
    return true;
}

bool frequencyListLoadsInConfiguredUnits()
{
    // The units have to be known before the list can be presented.
    auto config = makeTestConfig("[Reporting]\nFrequencyAsKHz=1\nFrequencyList=14.236000,7.177000\n");
    ReportingConfiguration reporting;
    reporting.load(config.get());
    if (!check("list in kHz", reporting.reportingFrequencyList.get(), StringList { "14236.0", "7177.0" })) return false;

    config = makeTestConfig("[Reporting]\nFrequencyAsKHz=0\nFrequencyList=14.236000,7.177000\n");
    ReportingConfiguration reportingInMhz;
    reportingInMhz.load(config.get());
    return check("list in MHz", reportingInMhz.reportingFrequencyList.get(), StringList { "14.2360", "7.1770" });
}

bool frequencyListSurvivesRestart()
{
    for (bool asKhz : { false, true })
    {
        auto config = makeTestConfig();
        ReportingConfiguration reporting;
        reporting.load(config.get());
        reporting.reportingFrequencyAsKhz = asKhz;

        StringList list =
            asKhz ?
            StringList { "14236.0", "2002.0", "7177.5" } :
            StringList { "14.2360", "2.0020", "7.1775" };
        reporting.reportingFrequencyList = list;
        reporting.save(config.get());

        // Always stored as MHz regardless of what's displayed.
        if (!check("stored list", config->Read("/Reporting/FrequencyList", ""), "14.236000,2.002000,7.177500")) return false;

        auto reloaded = reload(*config);
        ReportingConfiguration reportingAfterRestart;
        reportingAfterRestart.load(reloaded.get());
        if (!check("list", reportingAfterRestart.reportingFrequencyList.get(), list)) return false;
    }
    return true;
}

bool hamlibPttPortDefaultsToSerialPort()
{
    auto config = makeTestConfig("[Hamlib]\nSerialPort=/dev/ttyUSB0\n");
    RigControlConfiguration rigControl;
    rigControl.load(config.get());
    if (!check("PTT port", rigControl.hamlibPttSerialPort.get(), "/dev/ttyUSB0")) return false;

    config = makeTestConfig("[Hamlib]\nSerialPort=/dev/ttyUSB0\nPttSerialPort=/dev/ttyUSB1\n");
    RigControlConfiguration rigControlWithPttPort;
    rigControlWithPttPort.load(config.get());
    if (!check("explicit PTT port", rigControlWithPttPort.hamlibPttSerialPort.get(), "/dev/ttyUSB1")) return false;

    config = makeTestConfig();
    RigControlConfiguration rigControlWithNothing;
    rigControlWithNothing.load(config.get());
    return check("PTT port with nothing configured", rigControlWithNothing.hamlibPttSerialPort.get(), "");
}

bool voiceKeyerFileMigratesFromFullPath()
{
    // Older versions stored the full path in the file name setting.
    auto config = makeTestConfig("[VoiceKeyer]\nWaveFile=/home/me/My Audio/cq.wav\n");
    FreeDVConfiguration freedv;
    freedv.load(config.get());

    return
        check("path", freedv.voiceKeyerWaveFilePath.get(), "/home/me/My Audio") &&
        check("file", freedv.voiceKeyerWaveFile.get(), "cq.wav");
}

bool voiceKeyerFileWithoutExtensionMigrates()
{
    auto config = makeTestConfig("[VoiceKeyer]\nWaveFile=/home/me/cq\n");
    FreeDVConfiguration freedv;
    freedv.load(config.get());

    return
        check("path", freedv.voiceKeyerWaveFilePath.get(), "/home/me") &&
        check("file", freedv.voiceKeyerWaveFile.get(), "cq");
}

bool voiceKeyerDefaultsToDocumentsFolder()
{
    for (auto contents : { "", "[VoiceKeyer]\nWaveFile=voicekeyer.wav\n" })
    {
        auto config = makeTestConfig(contents);
        FreeDVConfiguration freedv;
        freedv.load(config.get());

        if (!check("path", freedv.voiceKeyerWaveFilePath.get(), wxStandardPaths::Get().GetDocumentsDir()) ||
            !check("file", freedv.voiceKeyerWaveFile.get(), "voicekeyer.wav"))
        {
            return false;
        }
    }
    return true;
}

bool voiceKeyerFileNameHasPathRemoved()
{
    auto config = makeTestConfig("[VoiceKeyer]\nWaveFilePath=/home/me/audio\nWaveFile=/home/me/audio/cq.wav\n");
    FreeDVConfiguration freedv;
    freedv.load(config.get());
    if (!check("path", freedv.voiceKeyerWaveFilePath.get(), "/home/me/audio") ||
        !check("file", freedv.voiceKeyerWaveFile.get(), "cq.wav"))
    {
        return false;
    }

    config = makeTestConfig("[VoiceKeyer]\nWaveFilePath=/home/me/audio\nWaveFile=cq.wav\n");
    FreeDVConfiguration freedvWithNameOnly;
    freedvWithNameOnly.load(config.get());
    return
        check("path (name only)", freedvWithNameOnly.voiceKeyerWaveFilePath.get(), "/home/me/audio") &&
        check("file (name only)", freedvWithNameOnly.voiceKeyerWaveFile.get(), "cq.wav");
}

bool voiceKeyerSettingsSurviveRestart()
{
    auto config = makeTestConfig("[VoiceKeyer]\nWaveFile=/home/me/My Audio/cq.wav\n");
    FreeDVConfiguration freedv;
    freedv.load(config.get());
    freedv.save(config.get());

    auto reloaded = reload(*config);
    FreeDVConfiguration freedvAfterRestart;
    freedvAfterRestart.load(reloaded.get());
    return
        check("path", freedvAfterRestart.voiceKeyerWaveFilePath.get(), "/home/me/My Audio") &&
        check("file", freedvAfterRestart.voiceKeyerWaveFile.get(), "cq.wav");
}

bool settingsSurviveRestart()
{
    // A sampling of each type of setting from each part of the configuration.
    auto config = makeTestConfig();
    FreeDVConfiguration freedv;
    freedv.load(config.get());

    freedv.firstTimeUse = false;
    freedv.mainWindowWidth = 1234;
    freedv.squelchLevel = -7;
    freedv.tabLayout = "1:0:0:0:100:0:0:0,1,2:";
    freedv.txAttenByBand = std::map<wxString, int> { { "20m", -30 }, { "40m", -12 } };
    freedv.audioConfiguration.soundCard1In.deviceName = wxString::FromUTF8("USB Audio CODEC (\xe3\x83\x9e\xe3\x82\xa4\xe3\x82\xaf)");
    freedv.audioConfiguration.soundCard1In.sampleRate = 48000;
    freedv.filterConfiguration.micInChannel.bassGaindB = 3.5f;
    freedv.filterConfiguration.codec2LPCPostFilterGamma = 0.4f;
    freedv.rigControlConfiguration.hamlibUseForPTT = true;
    freedv.rigControlConfiguration.hamlibIcomCIVAddress = 0x94;
    freedv.rigControlConfiguration.hamlibSerialPort = "/dev/cu.usbserial-1410";
    freedv.reportingConfiguration.reportingCallsign = "K6AQ";
    freedv.reportingConfiguration.freedvReporterStatusText = "QRV from $HOME";
    freedv.reportingConfiguration.reportingFrequency = 10489640000ULL;
    freedv.reportingConfiguration.freedvReporterRecentStatusTexts = StringList { "CQ, CQ", "QRV 20m", "back\\slash" };
    freedv.reportingConfiguration.freedvReporterColumnOrder = std::vector<int> { 2, 0, 1 };
    freedv.reportingConfiguration.freedvReporterColumnVisibility = std::vector<bool> { true, false, true };
    freedv.save(config.get());

    auto reloaded = reload(*config);
    FreeDVConfiguration after;
    after.load(reloaded.get());

    bool result = true;
    result &= check("first time use", after.firstTimeUse.get(), false);
    result &= check("main window width", (long)after.mainWindowWidth.get(), 1234L);
    result &= check("squelch level", (long)after.squelchLevel.get(), -7L);
    result &= check("tab layout", after.tabLayout.get(), "1:0:0:0:100:0:0:0,1,2:");
    result &= check("TX attenuation by band", after.txAttenByBand.get(), std::map<wxString, int> { { "20m", -30 }, { "40m", -12 } });
    result &= check("tune attenuation by band", after.tuneAttenByBand.get(), std::map<wxString, int> {});
    result &= check("device name", after.audioConfiguration.soundCard1In.deviceName.get(), "USB Audio CODEC (\xe3\x83\x9e\xe3\x82\xa4\xe3\x82\xaf)");
    result &= check("sample rate", after.audioConfiguration.soundCard1In.sampleRate.get(), 48000);
    result &= check("bass gain", after.filterConfiguration.micInChannel.bassGaindB.get(), 3.5f);
    result &= check("post filter gamma", after.filterConfiguration.codec2LPCPostFilterGamma.get(), 0.4f);
    result &= check("Hamlib PTT", after.rigControlConfiguration.hamlibUseForPTT.get(), true);
    result &= check("CI-V address", after.rigControlConfiguration.hamlibIcomCIVAddress.get(), 0x94u);
    result &= check("serial port", after.rigControlConfiguration.hamlibSerialPort.get(), "/dev/cu.usbserial-1410");
    result &= check("callsign", after.reportingConfiguration.reportingCallsign.get(), "K6AQ");
    result &= check("status text", after.reportingConfiguration.freedvReporterStatusText.get(), "QRV from $HOME");
    result &= check("frequency", after.reportingConfiguration.reportingFrequency.get(), (uint64_t)10489640000ULL);
    result &= check("status texts", after.reportingConfiguration.freedvReporterRecentStatusTexts.get(), StringList { "CQ, CQ", "QRV 20m", "back\\slash" });
    result &= check("column order", after.reportingConfiguration.freedvReporterColumnOrder.get(), std::vector<int> { 2, 0, 1 });
    result &= check("column visibility", after.reportingConfiguration.freedvReporterColumnVisibility.get(), std::vector<bool> { true, false, true });
    return result;
}

bool savingIsRepeatable()
{
    // Loading what was just saved and saving it again shouldn't change
    // anything, whether starting from defaults or from an old configuration.
    for (auto contents : {
        "",
        "[Audio]\nsoundCard1SampleRate=44100\n"
        "[Filter]\nMicInBassGaindB=-35\ncodec2LPCPostFilterGamma=60\n"
        "[PSKReporter]\nEnable=1\nCallsign=K6AQ\nFrequencyHzStr=14236000\n"
        "[VoiceKeyer]\nWaveFile=/home/me/cq.wav\n" })
    {
        auto config = makeTestConfig(contents);
        FreeDVConfiguration first;
        first.load(config.get());
        first.save(config.get());
        wxString firstContents = contentsOf(*config);

        auto reloaded = reload(*config);
        FreeDVConfiguration second;
        second.load(reloaded.get());
        second.save(reloaded.get());
        wxString secondContents = contentsOf(*reloaded);

        if (firstContents != secondContents)
        {
            // Point out the first line that's different.
            auto firstLines = wxSplit(firstContents, '\n');
            auto secondLines = wxSplit(secondContents, '\n');
            for (size_t index = 0; index < std::max(firstLines.size(), secondLines.size()); index++)
            {
                wxString a = index < firstLines.size() ? firstLines[index] : wxString("(nothing)");
                wxString b = index < secondLines.size() ? secondLines[index] : wxString("(nothing)");
                if (a != b)
                {
                    std::cout << "[first saved " << a << ", then " << b << "] ";
                    break;
                }
            }
            return false;
        }
    }
    return true;
}

}

int main()
{
    wxInitializer initializer;
    if (!initializer.IsOk())
    {
        std::cout << "Could not initialize wxWidgets" << std::endl;
        return -1;
    }

    executeTestCase("audioSampleRatesDefaultToUnset", audioSampleRatesDefaultToUnset);
    executeTestCase("audioSampleRatesMigrateFromPerCardRates", audioSampleRatesMigrateFromPerCardRates);
    executeTestCase("audioPerDirectionRatesTakePrecedence", audioPerDirectionRatesTakePrecedence);
    executeTestCase("equalizerDefaults", equalizerDefaults);
    executeTestCase("equalizerMigratesFromIntegerSettings", equalizerMigratesFromIntegerSettings);
    executeTestCase("equalizerCurrentSettingsTakePrecedence", equalizerCurrentSettingsTakePrecedence);
    executeTestCase("equalizerSettingsSurviveRestart", equalizerSettingsSurviveRestart);
    executeTestCase("postFilterDefaults", postFilterDefaults);
    executeTestCase("postFilterMigratesFromOldNames", postFilterMigratesFromOldNames);
    executeTestCase("postFilterCurrentNamesTakePrecedence", postFilterCurrentNamesTakePrecedence);
    executeTestCase("postFilterIsStoredAsPercentage", postFilterIsStoredAsPercentage);
    executeTestCase("reportingMigratesFromPskReporterSettings", reportingMigratesFromPskReporterSettings);
    executeTestCase("reportingCurrentSettingsTakePrecedence", reportingCurrentSettingsTakePrecedence);
    executeTestCase("reportingDefaultsWithNothingConfigured", reportingDefaultsWithNothingConfigured);
    executeTestCase("reportingFrequencySurvivesRestart", reportingFrequencySurvivesRestart);
    executeTestCase("frequencyListLoadsInConfiguredUnits", frequencyListLoadsInConfiguredUnits);
    executeTestCase("frequencyListSurvivesRestart", frequencyListSurvivesRestart);
    executeTestCase("hamlibPttPortDefaultsToSerialPort", hamlibPttPortDefaultsToSerialPort);
    executeTestCase("voiceKeyerFileMigratesFromFullPath", voiceKeyerFileMigratesFromFullPath);
    executeTestCase("voiceKeyerFileWithoutExtensionMigrates", voiceKeyerFileWithoutExtensionMigrates);
    executeTestCase("voiceKeyerDefaultsToDocumentsFolder", voiceKeyerDefaultsToDocumentsFolder);
    executeTestCase("voiceKeyerFileNameHasPathRemoved", voiceKeyerFileNameHasPathRemoved);
    executeTestCase("voiceKeyerSettingsSurviveRestart", voiceKeyerSettingsSurviveRestart);
    executeTestCase("settingsSurviveRestart", settingsSurviveRestart);
    executeTestCase("savingIsRepeatable", savingIsRepeatable);
    return testResult();
}
