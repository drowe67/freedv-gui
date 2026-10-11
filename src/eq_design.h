/*
    eq_design.h

    On the fly microphone and speaker equaliser design. Kept free of any
    GUI dependencies so that it can be unit tested.
*/

#ifndef EQ_DESIGN_H
#define EQ_DESIGN_H

struct EQBandSettings
{
    float bassFreqHz;
    float bassGaindB;
    float trebleFreqHz;
    float trebleGaindB;
    float midFreqHz;
    float midGaindB;
    float midQ;
};

// Designs a single filter. filterType is one of "bass", "treble", "equalizer" or "vol".
// Returns nullptr if the resulting filter would be a no-op. Q is only used by "equalizer"
// and freqHz is not used by "vol". The filter must be run at the sample rate given here.
void* designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q, int sampleRate);

// Designs the bass, treble and mid filters for a single channel (mic in or speaker out).
void designEQBandFilters(EQBandSettings const& settings, int sampleRate, void** bassFilter, void** trebleFilter, void** midFilter);

#endif // EQ_DESIGN_H
