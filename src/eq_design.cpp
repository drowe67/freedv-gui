/*
    eq_design.cpp

    On the fly microphone and speaker equaliser design.
*/

#include "eq_design.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "sox_biquad.h"

#define SBQ_MAX_ARGS 5

void* designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q, int sampleRate)
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

void designEQBandFilters(EQBandSettings const& settings, int sampleRate, void** bassFilter, void** trebleFilter, void** midFilter)
{
    *bassFilter   = designAnEQFilter("bass", settings.bassFreqHz, settings.bassGaindB, 0, sampleRate);
    *trebleFilter = designAnEQFilter("treble", settings.trebleFreqHz, settings.trebleGaindB, 0, sampleRate);
    *midFilter    = designAnEQFilter("equalizer", settings.midFreqHz, settings.midGaindB, settings.midQ, sampleRate);
}
