/*
    eq.cpp 
    
    On the fly microphone and speaker equaliser design.
*/
    
#include "main.h"

#define SBQ_MAX_ARGS 5

void *MainFrame::designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q, int sampleRate)
{
    char  *arg[SBQ_MAX_ARGS];
    char   argstorage[SBQ_MAX_ARGS][80];
    void  *sbq;
    int    i, argc;

    assert((strcmp(filterType, "bass") == 0)   ||
           (strcmp(filterType, "treble") == 0) ||
           (strcmp(filterType, "equalizer") == 0));

    for(i=0; i<SBQ_MAX_ARGS; i++) {
        arg[i] = &argstorage[i][0];
    }

    argc = 0;

    if ((strcmp(filterType, "bass") == 0) || (strcmp(filterType, "treble") == 0)) {
        sprintf(arg[argc++], "%s", filterType);
        sprintf(arg[argc++], "%f", gaindB+1E-6);
        sprintf(arg[argc++], "%f", freqHz);
        sprintf(arg[argc++], "%d", sampleRate);
    }

    if (strcmp(filterType, "equalizer") == 0) {
        sprintf(arg[argc++], "%s", filterType);
        sprintf(arg[argc++], "%f", freqHz);
        sprintf(arg[argc++], "%f", Q);
        sprintf(arg[argc++], "%f", gaindB+1E-6);
        sprintf(arg[argc++], "%d", sampleRate);
    }

    assert(argc <= SBQ_MAX_ARGS);
    // Note - the argc count doesn't include the command!
    sbq = sox_biquad_create(argc-1, (const char **)arg);
    assert(sbq != NULL);

    return sbq;
}

void  MainFrame::designEQFilters(paCallBackData *cb, int rxSampleRate, int txSampleRate)
{
    // init Mic In Equaliser Filters

    if (m_newMicInFilter) {
        //printf("designing new Min In filters\n");
        cb->sbqMicInBass   = designAnEQFilter("bass", wxGetApp().m_MicInBassFreqHz, wxGetApp().m_MicInBassGaindB, txSampleRate);
        cb->sbqMicInTreble = designAnEQFilter("treble", wxGetApp().m_MicInTrebleFreqHz, wxGetApp().m_MicInTrebleGaindB, txSampleRate);
        cb->sbqMicInMid    = designAnEQFilter("equalizer", wxGetApp().m_MicInMidFreqHz, wxGetApp().m_MicInMidGaindB, wxGetApp().m_MicInMidQ, txSampleRate);
    }

    // init Spk Out Equaliser Filters

    if (m_newSpkOutFilter) {
        //printf("designing new Spk Out filters\n");
        //printf("designEQFilters: wxGetApp().m_SpkOutBassFreqHz: %f\n",wxGetApp().m_SpkOutBassFreqHz);
        cb->sbqSpkOutBass   = designAnEQFilter("bass", wxGetApp().m_SpkOutBassFreqHz, wxGetApp().m_SpkOutBassGaindB, rxSampleRate);
        cb->sbqSpkOutTreble = designAnEQFilter("treble", wxGetApp().m_SpkOutTrebleFreqHz, wxGetApp().m_SpkOutTrebleGaindB, rxSampleRate);
        cb->sbqSpkOutMid    = designAnEQFilter("equalizer", wxGetApp().m_SpkOutMidFreqHz, wxGetApp().m_SpkOutMidGaindB, wxGetApp().m_SpkOutMidQ, rxSampleRate);
    }
}

void  MainFrame::deleteEQFilters(paCallBackData *cb)
{
    if (m_newMicInFilter) {
        sox_biquad_destroy(cb->sbqMicInBass);
        sox_biquad_destroy(cb->sbqMicInTreble);
        sox_biquad_destroy(cb->sbqMicInMid);
    }
    if (m_newSpkOutFilter) {
        sox_biquad_destroy(cb->sbqSpkOutBass);
        sox_biquad_destroy(cb->sbqSpkOutTreble);
        sox_biquad_destroy(cb->sbqSpkOutMid);
    }
}


