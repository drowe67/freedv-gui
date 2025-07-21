/*
    eq.cpp 
    
    On the fly microphone and speaker equaliser design.
*/
    
#include "main.h"

#include <functional>
using namespace std::placeholders;

extern int g_nSoundCards;

#define SBQ_MAX_ARGS 5

std::shared_ptr<void> MainFrame::designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q, int sampleRate)
{
    const int STR_LENGTH = 80;
    
    char  *arg[SBQ_MAX_ARGS];
    char   argstorage[SBQ_MAX_ARGS][STR_LENGTH];
    int    i, argc;

    assert((strcmp(filterType, "bass") == 0)   ||
           (strcmp(filterType, "treble") == 0) ||
           (strcmp(filterType, "equalizer") == 0) ||
           (strcmp(filterType, "vol") == 0));

    for(i=0; i<SBQ_MAX_ARGS; i++) {
        arg[i] = &argstorage[i][0];
    }

    argc = 0;

    if ((strcmp(filterType, "bass") == 0) || (strcmp(filterType, "treble") == 0)) {
        snprintf(arg[argc++], STR_LENGTH, "%s", filterType);
        snprintf(arg[argc++], STR_LENGTH, "%f", gaindB+1E-6);
        snprintf(arg[argc++], STR_LENGTH, "%f", freqHz);
        snprintf(arg[argc++], STR_LENGTH, "%d", sampleRate);
    }

    if (strcmp(filterType, "equalizer") == 0) {
        snprintf(arg[argc++], STR_LENGTH, "%s", filterType);
        snprintf(arg[argc++], STR_LENGTH, "%f", freqHz);
        snprintf(arg[argc++], STR_LENGTH, "%f", Q);
        snprintf(arg[argc++], STR_LENGTH, "%f", gaindB+1E-6);
        snprintf(arg[argc++], STR_LENGTH, "%d", sampleRate);
    }
    
    if (strcmp(filterType, "vol") == 0)
    {
        snprintf(arg[argc++], STR_LENGTH, "%s", filterType);
        snprintf(arg[argc++], STR_LENGTH, "%f", gaindB);
        snprintf(arg[argc++], STR_LENGTH, "%s", "dB");
        snprintf(arg[argc++], STR_LENGTH, "%f", 0.05); // to prevent clipping
        snprintf(arg[argc++], STR_LENGTH, "%d", sampleRate);
    }

    assert(argc <= SBQ_MAX_ARGS);
    // Note - the argc count doesn't include the command!
    return std::shared_ptr<void>(sox_biquad_create(argc-1, (const char **)arg), [](void* p) { if (p != nullptr) sox_biquad_destroy(p); });
}

void  MainFrame::designEQFilters(paCallBackData *cb, int rxSampleRate, int txSampleRate)
{
    // Volume can be adjusted via main window without enabling filters
    if (wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB != 0 && g_nSoundCards > 1)
    {
        cb->sbqMicInVol    = designAnEQFilter("vol", 0, wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB, 0, txSampleRate);
    }
    
    // init Mic In Equaliser Filters
    if (cb->micInEQEnable && g_nSoundCards > 1) {
        assert(cb->sbqMicInBass == nullptr && cb->sbqMicInTreble == nullptr && cb->sbqMicInMid == nullptr);
        //printf("designing new Min In filters\n");
        cb->sbqMicInBass   = designAnEQFilter("bass", wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassFreqHz, wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassGaindB, txSampleRate);
        cb->sbqMicInTreble = designAnEQFilter("treble", wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleFreqHz, wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleGaindB, txSampleRate);
        cb->sbqMicInMid    = designAnEQFilter("equalizer", wxGetApp().appConfiguration.filterConfiguration.micInChannel.midFreqHz, wxGetApp().appConfiguration.filterConfiguration.micInChannel.midGainDB, wxGetApp().appConfiguration.filterConfiguration.micInChannel.midQ, txSampleRate);
        
        // Note: vol can be a no-op!
        assert(cb->sbqMicInBass != nullptr && cb->sbqMicInTreble != nullptr && cb->sbqMicInMid != nullptr);
    }

    // Volume can be adjusted via main window without enabling filters
    if (wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB != 0)
    {
        cb->sbqSpkOutVol    = designAnEQFilter("vol", 0, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB, 0, rxSampleRate);
    }
    
    // init Spk Out Equaliser Filters

    if (cb->spkOutEQEnable) {
        assert(cb->sbqSpkOutBass == nullptr && cb->sbqSpkOutTreble == nullptr && cb->sbqSpkOutMid == nullptr);
        //printf("designing new Spk Out filters\n");
        //printf("designEQFilters: wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz: %f\n",wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz);
        cb->sbqSpkOutBass   = designAnEQFilter("bass", wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassGaindB, rxSampleRate);
        cb->sbqSpkOutTreble = designAnEQFilter("treble", wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleFreqHz, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleGaindB, rxSampleRate);
        cb->sbqSpkOutMid    = designAnEQFilter("equalizer", wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midFreqHz, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midGainDB, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midQ, rxSampleRate);
        
        // Note: vol can be a no-op!
        assert(cb->sbqSpkOutBass != nullptr && cb->sbqSpkOutTreble != nullptr && cb->sbqSpkOutMid != nullptr);
    }
}

void  MainFrame::deleteEQFilters(paCallBackData *cb)
{
    cb->sbqMicInBass = nullptr;
    cb->sbqMicInTreble = nullptr;
    cb->sbqMicInMid = nullptr;
    cb->sbqMicInVol = nullptr;
    cb->sbqSpkOutBass = nullptr;
    cb->sbqSpkOutTreble = nullptr;
    cb->sbqSpkOutMid = nullptr;
    cb->sbqSpkOutVol = nullptr;
}


