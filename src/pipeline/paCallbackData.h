#ifndef AUDIO_PIPELINE_PA_CALLBACK_DATA_H
#define AUDIO_PIPELINE_PA_CALLBACK_DATA_H

#include "../util/GenericFIFO.h"

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
    std::shared_ptr<void> sbqMicInBass;
    std::shared_ptr<void> sbqMicInTreble;
    std::shared_ptr<void> sbqMicInMid;
    std::shared_ptr<void> sbqMicInVol;
    std::shared_ptr<void> sbqSpkOutBass;
    std::shared_ptr<void> sbqSpkOutTreble;
    std::shared_ptr<void> sbqSpkOutMid;
    std::shared_ptr<void> sbqSpkOutVol;

    bool            micInEQEnable;
    bool            spkOutEQEnable;

    // optional loud tone on left channel to reliably trigger vox
    bool            leftChannelVoxTone;
    float           voxTonePhase;

    // Temporary buffers for reading and writing
    std::unique_ptr<short[]> tmpReadBuffer_;
    std::unique_ptr<short[]> tmpWriteBuffer_;
} paCallBackData;

#endif // AUDIO_PIPELINE_PA_CALLBACK_DATA_H
