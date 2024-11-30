#ifndef AUDIO_PIPELINE_PA_CALLBACK_DATA_H
#define AUDIO_PIPELINE_PA_CALLBACK_DATA_H

#include "codec2_fifo.h"

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
        , rxinfifo(nullptr)
        , rxoutfifo(nullptr)
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
    struct FIFO    *infifo1;
    struct FIFO    *outfifo1;

    // FIFOs attached to second sound card
    struct FIFO    *infifo2;
    struct FIFO    *outfifo2;

    // FIFOs for rx process
    struct FIFO    *rxinfifo;
    struct FIFO    *rxoutfifo;

    // EQ filter states
    void           *sbqMicInBass;
    void           *sbqMicInTreble;
    void           *sbqMicInMid;
    void           *sbqMicInVol;
    void           *sbqSpkOutBass;
    void           *sbqSpkOutTreble;
    void           *sbqSpkOutMid;
    void           *sbqSpkOutVol;

    bool            micInEQEnable;
    bool            spkOutEQEnable;

    // optional loud tone on left channel to reliably trigger vox
    bool            leftChannelVoxTone;
    float           voxTonePhase;
} paCallBackData;

#endif // AUDIO_PIPELINE_PA_CALLBACK_DATA_H
