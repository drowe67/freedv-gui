#include <cmath>
#include <string>

#include "codec2_fdmdv.h"
#include "codec2_fifo.h"

#include "../../defines.h"
#include "UtilStubs.h"
#include "../../util/logging/ulog.h"
#include "../../test/UnitTestCommon.h"

// The parts of util.cpp that don't need the application's windows.

void clickTune(float freq);

namespace {

bool closeTo(const char* description, float actual, float expected)
{
    if (std::fabs(actual - expected) < 0.001f) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

bool offsetsAre(float txOffsetHz, float rxOffsetHz)
{
    return 
        closeTo("TX offset", g_TxFreqOffsetHz.load(), txOffsetHz) && 
        closeTo("RX offset", g_RxFreqOffsetHz.load(), rxOffsetHz);
}

// Clicking on the spectrum tells FreeDV where the signal is. Received audio 
// is shifted so that it ends up where the modem expects it, and transmitted
// audio is shifted the other way so that both stations are on the same frequency.
bool clickTuneShiftsReceiveAndTransmitInOppositeDirections()
{
    bool result = true;

    clickTune(FDMDV_FCENTRE + 300);
    result &= offsetsAre(300, -300);

    clickTune(FDMDV_FCENTRE - 450.5f);
    result &= offsetsAre(-450.5f, 450.5f);

    clickTune(FDMDV_FCENTRE);
    result &= offsetsAre(0, 0);
    return result;
}

bool clickTuneSnapsToCenterWhenClose()
{
    // It's hard to click exactly on the center to get back to no offset at all.
    bool result = true;
    for (float offsetHz : { 9.5f, -9.5f, 1.0f, -0.25f })
    {
        clickTune(FDMDV_FCENTRE + 200);
        clickTune(FDMDV_FCENTRE + offsetHz);
        result &= offsetsAre(0, 0);
    }

    for (float offsetHz : { 10.5f, -10.5f })
    {
        clickTune(FDMDV_FCENTRE + offsetHz);
        result &= offsetsAre(offsetHz, -offsetHz);
    }
    return result;
}

// The modem asks for text to send one character at a time.
bool txTextIsTakenFromQueueInOrder()
{
    GenericFIFO<short> fifo(16);
    g_txDataInFifo.store(&fifo);

    std::string text = "CQ de K6AQ\r";
    for (char ch : text)
    {
        short value = ch;
        fifo.write(&value, 1);
    }

    std::string sent;
    for (size_t index = 0; index < text.size(); index++)
    {
        sent += my_get_next_tx_char(nullptr);
    }

    // With nothing left to send, the modem's given the "no character" value.
    bool result = sent == text && my_get_next_tx_char(nullptr) == 0 && my_get_next_tx_char(nullptr) == 0;
    if (sent != text) std::cout << "[sent \"" << sent << "\"] ";

    g_txDataInFifo.store(nullptr);
    return result;
}

bool rxTextIsQueuedInOrder()
{
    g_rxDataOutFifo.store(new GenericFIFO<short>(16));

    // Includes a character with its top bit set, which mustn't turn negative.
    const unsigned char TEXT[] = { 'H', 'i', 0xE9, '\r' };
    for (unsigned char ch : TEXT)
    {
        my_put_next_rx_char(nullptr, (char)ch);
    }

    bool result = g_rxDataOutFifo.load()->numUsed() == 4;
    for (unsigned char expected : TEXT)
    {
        short value = -1;
        result &= g_rxDataOutFifo.load()->read(&value, 1) == 0 && value == expected;
    }
    if (!result) std::cout << "[queued text didn't match what was received] ";

    delete g_rxDataOutFifo.load();
    g_rxDataOutFifo.store(nullptr);
    return result;
}

bool rxTextIsDroppedWhenQueueIsFull()
{
    g_rxDataOutFifo.store(new GenericFIFO<short>(4));

    for (char ch : std::string("ABCDEFGH"))
    {
        my_put_next_rx_char(nullptr, ch);
    }

    // What fit is kept as is.
    std::string queued;
    short value = 0;
    while (g_rxDataOutFifo.load()->read(&value, 1) == 0)
    {
        queued += (char)value;
    }

    delete g_rxDataOutFifo.load();
    g_rxDataOutFifo.store(nullptr);

    if (queued.empty() || std::string("ABCDEFGH").compare(0, queued.size(), queued) != 0)
    {
        std::cout << "[queue held \"" << queued << "\"] ";
        return false;
    }
    return true;
}

}

int main()
{
    ulog_set_quiet(true);

    executeTestCase("clickTuneShiftsReceiveAndTransmitInOppositeDirections", clickTuneShiftsReceiveAndTransmitInOppositeDirections);
    executeTestCase("clickTuneSnapsToCenterWhenClose", clickTuneSnapsToCenterWhenClose);
    executeTestCase("txTextIsTakenFromQueueInOrder", txTextIsTakenFromQueueInOrder);
    executeTestCase("rxTextIsQueuedInOrder", rxTextIsQueuedInOrder);
    executeTestCase("rxTextIsDroppedWhenQueueIsFull", rxTextIsDroppedWhenQueueIsFull);
    return testResult();
}
