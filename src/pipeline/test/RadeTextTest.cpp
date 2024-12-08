#include <cassert>
#include <wx/string.h>
#include "../RADEReceiveStep.h"
#include "../RADETransmitStep.h"
#include "../rade_text.h"
#include "../../util/logging/ulog.h"

bool testPassed = false;
wxString utRxFeatureFile;
wxString utTxFeatureFile;

void OnRadeTextRx(rade_text_t rt, const char* txt_ptr, int length, void* state) 
{
    log_info("Callsign received: %s", txt_ptr);
    testPassed = !strncmp(txt_ptr, "K6AQ", length);
}

int main()
{
    struct rade* rade;
    LPCNetEncState *encState;
    FARGANState fargan;

    // Initialize FARGAN
    float zeros[320] = {0};
    float in_features[5*NB_TOTAL_FEATURES] = {0};
    fargan_init(&fargan);
    fargan_cont(&fargan, zeros, in_features);
    encState = lpcnet_encoder_create();
    assert(encState != nullptr);

    // Initialize RADE
    char modelFile[1];
    modelFile[0] = 0;
    rade_initialize();
    rade = rade_open(modelFile, RADE_USE_C_ENCODER | RADE_USE_C_DECODER);
    assert(rade != nullptr);

    // Initialize RADE text
    rade_text_t txt = rade_text_create();
    assert(txt != nullptr);
    float txSyms[rade_n_eoo_bits(rade)];
    for (int index = 0; index < rade_n_eoo_bits(rade); index++)
    {
        txSyms[index] = index % 2 ? 1 : 0;
    }
    rade_text_generate_tx_string(txt, "K6AQ", 4, txSyms);
    rade_text_set_rx_callback(txt, OnRadeTextRx, nullptr);
    rade_tx_set_eoo_bits(rade, txSyms);

    // Initialize RADE steps
    RADEReceiveStep* recvStep = new RADEReceiveStep(rade, &fargan, txt);
    assert(recvStep != nullptr);
    RADETransmitStep* txStep = new RADETransmitStep(rade, encState);
    assert(txStep != nullptr);

    // "Transmit" ~1 second of audio (including EOO) and immediately receive it.
    int nout = 0;
    int noutRx = 0;
    short* inputSamples = new short[16384];
    assert(inputSamples != nullptr);
    memset(inputSamples, 0, sizeof(short) * 16384);
    auto inputSamplesPtr = std::shared_ptr<short>(inputSamples, std::default_delete<short[]>());
    auto outputSamples = txStep->execute(inputSamplesPtr, 16384, &nout);
    recvStep->execute(outputSamples, nout, &noutRx);

    txStep->restartVocoder();
    while (nout > 0)
    {
        outputSamples = txStep->execute(inputSamplesPtr, 0, &nout);
        recvStep->execute(outputSamples, nout, &noutRx);
    }

    // Send silence through to RX step to trigger EOO processing
    recvStep->execute(inputSamplesPtr, 16384, &noutRx);

    rade_close(rade); 
    rade_finalize();
    return testPassed ? 0 : 1;
}
