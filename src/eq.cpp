/*
    eq.cpp 
    
    On the fly microphone and speaker equaliser design.
*/
    
#include "main.h"

#define SBQ_MAX_ARGS 5

void *MainFrame::designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q, int sampleRate)
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
    return sox_biquad_create(argc-1, (const char **)arg);
}

void  MainFrame::designEQFilters(paCallBackData *cb, int rxSampleRate, int txSampleRate)
{
    // init Mic In Equaliser Filters
    if (m_newMicInFilter) {
        assert(cb->sbqMicInBass == nullptr && cb->sbqMicInTreble == nullptr && cb->sbqMicInMid == nullptr);
        //printf("designing new Min In filters\n");
        cb->sbqMicInBass   = designAnEQFilter("bass", wxGetApp().m_MicInBassFreqHz, wxGetApp().m_MicInBassGaindB, txSampleRate);
        cb->sbqMicInTreble = designAnEQFilter("treble", wxGetApp().m_MicInTrebleFreqHz, wxGetApp().m_MicInTrebleGaindB, txSampleRate);
        cb->sbqMicInMid    = designAnEQFilter("equalizer", wxGetApp().m_MicInMidFreqHz, wxGetApp().m_MicInMidGaindB, wxGetApp().m_MicInMidQ, txSampleRate);
        cb->sbqMicInVol    = designAnEQFilter("vol", 0, wxGetApp().m_MicInVolInDB, 0, txSampleRate);
        
        // Note: vol can be a no-op!
        assert(cb->sbqMicInBass != nullptr && cb->sbqMicInTreble != nullptr && cb->sbqMicInMid != nullptr);
    }

    // init Spk Out Equaliser Filters

    if (m_newSpkOutFilter) {
        assert(cb->sbqSpkOutBass == nullptr && cb->sbqSpkOutTreble == nullptr && cb->sbqSpkOutMid == nullptr);
        //printf("designing new Spk Out filters\n");
        //printf("designEQFilters: wxGetApp().m_SpkOutBassFreqHz: %f\n",wxGetApp().m_SpkOutBassFreqHz);
        cb->sbqSpkOutBass   = designAnEQFilter("bass", wxGetApp().m_SpkOutBassFreqHz, wxGetApp().m_SpkOutBassGaindB, rxSampleRate);
        cb->sbqSpkOutTreble = designAnEQFilter("treble", wxGetApp().m_SpkOutTrebleFreqHz, wxGetApp().m_SpkOutTrebleGaindB, rxSampleRate);
        cb->sbqSpkOutMid    = designAnEQFilter("equalizer", wxGetApp().m_SpkOutMidFreqHz, wxGetApp().m_SpkOutMidGaindB, wxGetApp().m_SpkOutMidQ, rxSampleRate);
        cb->sbqSpkOutVol    = designAnEQFilter("vol", 0, wxGetApp().m_SpkOutVolInDB, 0, txSampleRate);
        
        // Note: vol can be a no-op!
        assert(cb->sbqSpkOutBass != nullptr && cb->sbqSpkOutTreble != nullptr && cb->sbqSpkOutMid != nullptr);
    }
    
    m_newMicInFilter = false;
    m_newSpkOutFilter = false;
}

void  MainFrame::deleteEQFilters(paCallBackData *cb)
{
    if (m_newMicInFilter) 
    {
        assert(cb->sbqMicInBass != nullptr && cb->sbqMicInTreble != nullptr && cb->sbqMicInMid != nullptr);
        
        sox_biquad_destroy(cb->sbqMicInBass);
        sox_biquad_destroy(cb->sbqMicInTreble);
        sox_biquad_destroy(cb->sbqMicInMid);
        
        if (cb->sbqMicInVol) sox_biquad_destroy(cb->sbqMicInVol);
        
        cb->sbqMicInBass = nullptr;
        cb->sbqMicInTreble = nullptr;
        cb->sbqMicInMid = nullptr;
        cb->sbqMicInVol = nullptr;
    }
    
    if (m_newSpkOutFilter) 
    {
        assert(cb->sbqSpkOutBass != nullptr && cb->sbqSpkOutTreble != nullptr && cb->sbqSpkOutMid != nullptr);
        
        sox_biquad_destroy(cb->sbqSpkOutBass);    
        sox_biquad_destroy(cb->sbqSpkOutTreble);
        sox_biquad_destroy(cb->sbqSpkOutMid);
        
        if (cb->sbqSpkOutVol) sox_biquad_destroy(cb->sbqSpkOutVol);

        cb->sbqSpkOutBass = nullptr;
        cb->sbqSpkOutTreble = nullptr;
        cb->sbqSpkOutMid = nullptr;
        cb->sbqSpkOutVol = nullptr;
    }
}


