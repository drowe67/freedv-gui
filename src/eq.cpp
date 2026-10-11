/*
    eq.cpp 
    
    On the fly microphone and speaker equaliser design.
*/
    
#include "main.h"
#include "eq_design.h"

#include <functional>
using namespace std::placeholders;

extern int g_nSoundCards;

static EQBandSettings getBandSettings_(auto& channel)
{
    return EQBandSettings {
        .bassFreqHz = channel.bassFreqHz,
        .bassGaindB = channel.bassGaindB,
        .trebleFreqHz = channel.trebleFreqHz,
        .trebleGaindB = channel.trebleGaindB,
        .midFreqHz = channel.midFreqHz,
        .midGaindB = channel.midGainDB,
        .midQ = channel.midQ,
    };
}

void  MainFrame::designEQFilters(paCallBackData *cb, int rxSampleRate, int txSampleRate)
{
    cb->micEqLock.lock();

    // Volume can be adjusted via main window without enabling filters
    if (wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB != 0 && g_nSoundCards > 1)
    {
        cb->sbqMicInVol    = designAnEQFilter("vol", 0, wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB, 0, txSampleRate);
    }
    
    // init Mic In Equaliser Filters
    if (cb->micInEQEnable.load(std::memory_order_relaxed) && g_nSoundCards > 1) {
        assert(cb->sbqMicInBass == nullptr && cb->sbqMicInTreble == nullptr && cb->sbqMicInMid == nullptr);
        //printf("designing new Min In filters\n");
        designEQBandFilters(getBandSettings_(wxGetApp().appConfiguration.filterConfiguration.micInChannel), txSampleRate, &cb->sbqMicInBass, &cb->sbqMicInTreble, &cb->sbqMicInMid);
        
        // Note: vol can be a no-op!
        assert(cb->sbqMicInBass != nullptr && cb->sbqMicInTreble != nullptr && cb->sbqMicInMid != nullptr);
    }

    cb->micEqLock.unlock();
    cb->spkEqLock.lock();

    // Volume can be adjusted via main window without enabling filters
    if (wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB != 0)
    {
        cb->sbqSpkOutVol    = designAnEQFilter("vol", 0, wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB, 0, rxSampleRate);
    }

    // init Spk Out Equaliser Filters

    if (cb->spkOutEQEnable.load(std::memory_order_relaxed)) {
        assert(cb->sbqSpkOutBass == nullptr && cb->sbqSpkOutTreble == nullptr && cb->sbqSpkOutMid == nullptr);
        //printf("designing new Spk Out filters\n");
        //printf("designEQFilters: wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz: %f\n",wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz);
        designEQBandFilters(getBandSettings_(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel), rxSampleRate, &cb->sbqSpkOutBass, &cb->sbqSpkOutTreble, &cb->sbqSpkOutMid);
        
        // Note: vol can be a no-op!
        assert(cb->sbqSpkOutBass != nullptr && cb->sbqSpkOutTreble != nullptr && cb->sbqSpkOutMid != nullptr);
    }
    cb->spkEqLock.unlock();
}

#define VERIFY_AND_DESTROY(x) if (x != nullptr) { sox_biquad_destroy(x); x = nullptr; }

void  MainFrame::deleteEQFilters(paCallBackData *cb)
{
    cb->micEqLock.lock();
    VERIFY_AND_DESTROY(cb->sbqMicInBass);
    VERIFY_AND_DESTROY(cb->sbqMicInTreble);
    VERIFY_AND_DESTROY(cb->sbqMicInMid);
    VERIFY_AND_DESTROY(cb->sbqMicInVol);
    cb->micEqLock.unlock();
    
    cb->spkEqLock.lock();
    VERIFY_AND_DESTROY(cb->sbqSpkOutBass);
    VERIFY_AND_DESTROY(cb->sbqSpkOutTreble);
    VERIFY_AND_DESTROY(cb->sbqSpkOutMid);
    VERIFY_AND_DESTROY(cb->sbqSpkOutVol);
    cb->spkEqLock.unlock();
}


