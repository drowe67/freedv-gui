#ifndef AUDIO_PIPELINE_PA_CALLBACK_DATA_H
#define AUDIO_PIPELINE_PA_CALLBACK_DATA_H

#include "../util/GenericFIFO.h"
#include "../util/audio_spin_mutex.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// paCallBackData
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
typedef struct paCallBackData
{
    paCallBackData()
        : infifo1(nullptr)
        , outfifo1(nullptr)
        , infifo2(nullptr)
        , outfifo2(nullptr)
        , sbqMicInBass(nullptr)
        , sbqMicInTreble(nullptr)
        , sbqMicInMid(nullptr)
        , sbqMicInVol(nullptr)
        , sbqSpkOutBass(nullptr)
        , sbqSpkOutTreble(nullptr)
        , sbqSpkOutMid(nullptr)
        , sbqSpkOutVol(nullptr)
        , micInEQEnable(false)
        , spkOutEQEnable(false)
        , leftChannelVoxTone(false)
        , voxTonePhase(0.0)
    {
        // empty
    }

    // FIFOs attached to first sound card
    GenericFIFO<short>    *infifo1;
    GenericFIFO<short>    *outfifo1;

    // FIFOs attached to second sound card
    GenericFIFO<short>    *infifo2;
    GenericFIFO<short>    *outfifo2;

    // EQ filter states
    void* sbqMicInBass;
    void* sbqMicInTreble;
    void* sbqMicInMid;
    void* sbqMicInVol;
    void* sbqSpkOutBass;
    void* sbqSpkOutTreble;
    void* sbqSpkOutMid;
    void* sbqSpkOutVol;
    audio_spin_mutex micEqLock;
    audio_spin_mutex spkEqLock;

    bool            micInEQEnable;
    bool            spkOutEQEnable;

    // optional loud tone on left channel to reliably trigger vox
    std::atomic<bool> leftChannelVoxTone;
    float             voxTonePhase;

    // Temporary buffers for reading and writing
    std::unique_ptr<short[]> tmpReadRxBuffer_;
    std::unique_ptr<short[]> tmpReadTxBuffer_;
    std::unique_ptr<short[]> tmpWriteRxBuffer_;
    std::unique_ptr<short[]> tmpWriteTxBuffer_;
} paCallBackData;

#endif // AUDIO_PIPELINE_PA_CALLBACK_DATA_H
