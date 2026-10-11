#ifndef PIPELINE_TEST_UTIL_STUBS_H
#define PIPELINE_TEST_UTIL_STUBS_H

// util.cpp is compiled into some pipeline tests to provide the real
// freq_shift_coh() and resample_for_plot() that the steps call. In the
// application those functions share a translation unit with MainFrame and
// MainApp methods whose other dependencies live in main.cpp and topFrame.cpp;
// no test reaches those methods, so the leftovers are stubbed here just
// enough to link.

#include <atomic>

#include "../../main.h"

// Defined by main.cpp in the application.
int g_nSoundCards = 1;
std::atomic<float> g_RxFreqOffsetHz(0.0f);
std::atomic<float> g_TxFreqOffsetHz(0.0f);
wxString testName;

// wxIMPLEMENT_APP() in main.cpp defines this; no test reaches it.
MainApp* g_testApp = nullptr;
MainApp& wxGetApp() { return *g_testApp; }

void MainFrame::onRigPttChange_(bool)
{
    // empty
}

void MainFrame::togglePTT()
{
    // empty
}

#endif // PIPELINE_TEST_UTIL_STUBS_H
