//==========================================================================
// Name:            dlg_setup_wizard.cpp
// Purpose:         First-run setup wizard dialog
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.
//
//==========================================================================

#include <algorithm>
#include <vector>

#include "dlg_setup_wizard.h"

#ifdef __WXMSW__
#include <wx/msw/registry.h>
#else
#include <glob.h>
#include <string.h>
#endif

#include "audio/AudioEngineFactory.h"
#include "audio/IAudioDevice.h"
#include "rig_control/HamlibRigController.h"

extern wxConfigBase* pConfig;

// --------------------------------------------------------------------------
// helpers
// --------------------------------------------------------------------------

static wxSizer* makePageHeader(wxWindow* parent, const wxString& title, const wxString& desc)
{
    wxBoxSizer* hs = new wxBoxSizer(wxVERTICAL);
    wxStaticText* t = new wxStaticText(parent, wxID_ANY, title);
    wxFont f = t->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    t->SetFont(f);
    hs->Add(t, 0, wxBOTTOM, 4);
    hs->Add(new wxStaticText(parent, wxID_ANY, desc), 0, wxBOTTOM, 8);
    hs->Add(new wxStaticLine(parent), 0, wxEXPAND | wxBOTTOM, 8);
    return hs;
}

// --------------------------------------------------------------------------
// SetupWizard constructor
// --------------------------------------------------------------------------

SetupWizard::SetupWizard(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("FreeDV Setup Wizard"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_currentPage(0)
{
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    // Book
    m_book = new wxSimplebook(this, wxID_ANY);
    m_book->AddPage(makeReceivePage(),   wxEmptyString);
    m_book->AddPage(makeTxPage(),        wxEmptyString);
    m_book->AddPage(makeRadioPage(),     wxEmptyString);
    m_book->AddPage(makeReportingPage(), wxEmptyString);
    topSizer->Add(m_book, 1, wxEXPAND | wxALL, 10);

    // Navigation bar
    topSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    wxBoxSizer* navSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    m_btnPrev   = new wxButton(this, wxID_ANY, _("< Previous"));
    m_btnNext   = new wxButton(this, wxID_ANY, _("Next >"));
    m_btnFinish = new wxButton(this, wxID_ANY, _("Finish"));
    navSizer->Add(btnCancel,   0, wxRIGHT, 4);
    navSizer->AddStretchSpacer();
    navSizer->Add(m_btnPrev,   0, wxRIGHT, 4);
    navSizer->Add(m_btnNext,   0, wxRIGHT, 4);
    navSizer->Add(m_btnFinish, 0);
    topSizer->Add(navSizer, 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(topSizer);
    SetMinSize(wxSize(560, 460));
    SetSize(wxSize(580, 500));

    // Populate serial ports and load saved config
    populateSerialPorts();
    loadConfig();
    updateNavButtons();
    updateTxState();
    updateRadioState();
    updateReportingState();

    // Wire events
    m_btnPrev->Bind(wxEVT_BUTTON,   &SetupWizard::OnPrev,   this);
    m_btnNext->Bind(wxEVT_BUTTON,   &SetupWizard::OnNext,   this);
    m_btnFinish->Bind(wxEVT_BUTTON, &SetupWizard::OnFinish, this);

    m_ckReceiveOnly->Bind(wxEVT_CHECKBOX, &SetupWizard::OnReceiveOnlyChanged, this);

    m_ckHamlib->Bind(wxEVT_CHECKBOX,    &SetupWizard::OnHamlibChanged,    this);
    m_ckSerialPTT->Bind(wxEVT_CHECKBOX, &SetupWizard::OnSerialPTTChanged, this);
    m_cbRigName->Bind(wxEVT_COMBOBOX,   &SetupWizard::OnRigNameChanged,   this);
#if defined(WIN32)
    m_ckOmniRig->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        m_ckHamlib->SetValue(false);
        m_ckSerialPTT->SetValue(false);
        updateRadioState();
    });
#endif

    m_ckReportingEnable->Bind(wxEVT_CHECKBOX, &SetupWizard::OnReportingEnableChanged, this);
}

// --------------------------------------------------------------------------
// Page factories
// --------------------------------------------------------------------------

wxPanel* SetupWizard::makeReceivePage()
{
    wxPanel* page = new wxPanel(m_book);
    wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);

    vs->Add(makePageHeader(page,
        _("Step 1 of 4: Receive Audio"),
        _("Select the audio devices used for receiving transmissions.")),
        0, wxEXPAND);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 8, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(page, wxID_ANY, _("Radio Receive\n(Sound Card 1 Input):")),
              0, wxALIGN_CENTER_VERTICAL);
    m_cbRadioIn = new wxComboBox(page, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                  wxCB_DROPDOWN | wxCB_READONLY);
    populateAudioCombo(m_cbRadioIn, IAudioEngine::AUDIO_ENGINE_IN);
    grid->Add(m_cbRadioIn, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    grid->Add(new wxStaticText(page, wxID_ANY, _("Speaker / Headphones\n(Sound Card 1 Output):")),
              0, wxALIGN_CENTER_VERTICAL);
    m_cbSpeakerOut = new wxComboBox(page, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                     wxCB_DROPDOWN | wxCB_READONLY);
    populateAudioCombo(m_cbSpeakerOut, IAudioEngine::AUDIO_ENGINE_OUT);
    grid->Add(m_cbSpeakerOut, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    vs->Add(grid, 0, wxEXPAND);
    page->SetSizer(vs);
    return page;
}

wxPanel* SetupWizard::makeTxPage()
{
    wxPanel* page = new wxPanel(m_book);
    wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);

    vs->Add(makePageHeader(page,
        _("Step 2 of 4: Transmit Audio"),
        _("Select the audio devices used for transmitting. Skip if you\nonly want to receive.")),
        0, wxEXPAND);

    m_ckReceiveOnly = new wxCheckBox(page, wxID_ANY,
        _("Receive only (I will not be transmitting)"));
    vs->Add(m_ckReceiveOnly, 0, wxBOTTOM, 12);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 8, 8);
    grid->AddGrowableCol(1, 1);

    m_stMicIn = new wxStaticText(page, wxID_ANY, _("Microphone\n(Sound Card 2 Input):"));
    grid->Add(m_stMicIn, 0, wxALIGN_CENTER_VERTICAL);
    m_cbMicIn = new wxComboBox(page, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                wxCB_DROPDOWN | wxCB_READONLY);
    populateAudioCombo(m_cbMicIn, IAudioEngine::AUDIO_ENGINE_IN);
    grid->Add(m_cbMicIn, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    m_stRadioOut = new wxStaticText(page, wxID_ANY, _("Radio Transmit\n(Sound Card 2 Output):"));
    grid->Add(m_stRadioOut, 0, wxALIGN_CENTER_VERTICAL);
    m_cbRadioOut = new wxComboBox(page, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                   wxCB_DROPDOWN | wxCB_READONLY);
    populateAudioCombo(m_cbRadioOut, IAudioEngine::AUDIO_ENGINE_OUT);
    grid->Add(m_cbRadioOut, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    vs->Add(grid, 0, wxEXPAND);
    page->SetSizer(vs);
    return page;
}

wxPanel* SetupWizard::makeRadioPage()
{
    wxPanel* page = new wxPanel(m_book);
    wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);

    vs->Add(makePageHeader(page,
        _("Step 3 of 4: Radio Control"),
        _("Configure PTT (Push-To-Talk) control for your radio.\nLeave all options unchecked if you do not need PTT control.")),
        0, wxEXPAND);

    // --- Hamlib section ---
    wxStaticBoxSizer* hamlibBox = new wxStaticBoxSizer(wxVERTICAL,
        page, _("CAT Control via Hamlib"));
    wxStaticBox* hamlibSB = hamlibBox->GetStaticBox();

    m_ckHamlib = new wxCheckBox(hamlibSB, wxID_ANY, _("Enable CAT control via Hamlib"));
    hamlibBox->Add(m_ckHamlib, 0, wxALL, 4);

    wxFlexGridSizer* hGrid = new wxFlexGridSizer(5, 2, 6, 8);
    hGrid->AddGrowableCol(1, 1);

    m_stRigName = new wxStaticText(hamlibSB, wxID_ANY, _("Rig Model:"));
    hGrid->Add(m_stRigName, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbRigName = new wxComboBox(hamlibSB, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxSize(220, -1), 0, nullptr,
                                  wxCB_DROPDOWN | wxCB_READONLY);
    {
        int numRigs = HamlibRigController::GetNumberSupportedRadios();
        for (int i = 0; i < numRigs; i++)
            m_cbRigName->Append(HamlibRigController::RigIndexToName(i));
    }
    hGrid->Add(m_cbRigName, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    m_stSerialPort = new wxStaticText(hamlibSB, wxID_ANY, _("Serial Device:"));
    hGrid->Add(m_stSerialPort, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbSerialPort = new wxComboBox(hamlibSB, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                     wxCB_DROPDOWN);
    m_cbSerialPort->SetMinSize(wxSize(140, -1));
    hGrid->Add(m_cbSerialPort, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    m_stSerialRate = new wxStaticText(hamlibSB, wxID_ANY, _("Serial Rate:"));
    hGrid->Add(m_stSerialRate, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbSerialRate = new wxComboBox(hamlibSB, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxSize(110, -1), 0, nullptr,
                                     wxCB_DROPDOWN);
    hGrid->Add(m_cbSerialRate, 0, wxALIGN_CENTER_VERTICAL);

    m_stPttMethod = new wxStaticText(hamlibSB, wxID_ANY, _("PTT uses:"));
    hGrid->Add(m_stPttMethod, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbPttMethod = new wxComboBox(hamlibSB, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(120, -1), 0, nullptr,
                                    wxCB_DROPDOWN | wxCB_READONLY);
    m_cbPttMethod->Append(_("CAT"));
    m_cbPttMethod->Append(_("RTS"));
    m_cbPttMethod->Append(_("DTR"));
    m_cbPttMethod->Append(_("None"));
    m_cbPttMethod->Append(_("CAT via Data port"));
    hGrid->Add(m_cbPttMethod, 0, wxALIGN_CENTER_VERTICAL);

    m_stPttSerialPort = new wxStaticText(hamlibSB, wxID_ANY, _("PTT Serial Device:"));
    hGrid->Add(m_stPttSerialPort, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbPttSerialPort = new wxComboBox(hamlibSB, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                        wxCB_DROPDOWN);
    m_cbPttSerialPort->SetMinSize(wxSize(140, -1));
    hGrid->Add(m_cbPttSerialPort, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    hamlibBox->Add(hGrid, 0, wxEXPAND | wxALL, 4);
    vs->Add(hamlibBox, 0, wxEXPAND | wxBOTTOM, 8);

    // --- Serial PTT section ---
    wxStaticBoxSizer* serialBox = new wxStaticBoxSizer(wxVERTICAL,
        page, _("Serial Port PTT"));
    wxStaticBox* serialSB = serialBox->GetStaticBox();

    m_ckSerialPTT = new wxCheckBox(serialSB, wxID_ANY, _("Enable serial port PTT"));
    serialBox->Add(m_ckSerialPTT, 0, wxALL, 4);

    wxFlexGridSizer* sGrid = new wxFlexGridSizer(2, 2, 6, 8);
    sGrid->AddGrowableCol(1, 1);

    m_stCtlDevice = new wxStaticText(serialSB, wxID_ANY, _("Control Device:"));
    sGrid->Add(m_stCtlDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbCtlDevicePath = new wxComboBox(serialSB, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                        wxCB_DROPDOWN);
    m_cbCtlDevicePath->SetMinSize(wxSize(140, -1));
    sGrid->Add(m_cbCtlDevicePath, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    sGrid->Add(new wxStaticText(serialSB, wxID_ANY, _("Signal:")),
               0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    wxBoxSizer* signalRow = new wxBoxSizer(wxHORIZONTAL);
    m_rbUseRTS = new wxRadioButton(serialSB, wxID_ANY, _("RTS"), wxDefaultPosition,
                                    wxDefaultSize, wxRB_GROUP);
    m_ckRTSPos = new wxCheckBox(serialSB, wxID_ANY, _("Inverted"));
    m_rbUseDTR = new wxRadioButton(serialSB, wxID_ANY, _("DTR"));
    m_ckDTRPos = new wxCheckBox(serialSB, wxID_ANY, _("Inverted"));
    signalRow->Add(m_rbUseRTS, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    signalRow->Add(m_ckRTSPos, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 12);
    signalRow->Add(m_rbUseDTR, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    signalRow->Add(m_ckDTRPos, 0, wxALIGN_CENTER_VERTICAL);
    sGrid->Add(signalRow, 0, wxALIGN_CENTER_VERTICAL);

    serialBox->Add(sGrid, 0, wxEXPAND | wxALL, 4);
    vs->Add(serialBox, 0, wxEXPAND | wxBOTTOM, 8);

#if defined(WIN32)
    // --- OmniRig section (Windows only) ---
    wxStaticBoxSizer* omniBox = new wxStaticBoxSizer(wxVERTICAL,
        page, _("OmniRig"));
    wxStaticBox* omniSB = omniBox->GetStaticBox();

    m_ckOmniRig = new wxCheckBox(omniSB, wxID_ANY, _("Enable OmniRig"));
    omniBox->Add(m_ckOmniRig, 0, wxALL, 4);

    wxFlexGridSizer* oGrid = new wxFlexGridSizer(1, 2, 6, 8);
    m_stOmniRigId = new wxStaticText(omniSB, wxID_ANY, _("Rig:"));
    oGrid->Add(m_stOmniRigId, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    m_cbOmniRigRigId = new wxComboBox(omniSB, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                       wxCB_DROPDOWN | wxCB_READONLY);
    m_cbOmniRigRigId->Append(_("Rig 1"));
    m_cbOmniRigRigId->Append(_("Rig 2"));
    oGrid->Add(m_cbOmniRigRigId, 0, wxALIGN_CENTER_VERTICAL);
    omniBox->Add(oGrid, 0, wxEXPAND | wxALL, 4);
    vs->Add(omniBox, 0, wxEXPAND | wxBOTTOM, 8);
#endif

    page->SetSizer(vs);
    return page;
}

wxPanel* SetupWizard::makeReportingPage()
{
    wxPanel* page = new wxPanel(m_book);
    wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);

    vs->Add(makePageHeader(page,
        _("Step 4 of 4: Reporting"),
        _("Optionally report your station to FreeDV Reporter and PSK Reporter.\nA valid callsign and grid square are required.")),
        0, wxEXPAND);

    m_ckReportingEnable = new wxCheckBox(page, wxID_ANY, _("Enable station reporting"));
    vs->Add(m_ckReportingEnable, 0, wxBOTTOM, 10);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 8, 8);
    grid->AddGrowableCol(1, 1);

    m_stCallsign = new wxStaticText(page, wxID_ANY, _("Callsign:"));
    grid->Add(m_stCallsign, 0, wxALIGN_CENTER_VERTICAL);
    m_txtCallsign = new wxTextCtrl(page, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(160, -1));
    grid->Add(m_txtCallsign, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    m_stGridSquare = new wxStaticText(page, wxID_ANY, _("Grid Square:"));
    grid->Add(m_stGridSquare, 0, wxALIGN_CENTER_VERTICAL);
    m_txtGridSquare = new wxTextCtrl(page, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxSize(100, -1));
    grid->Add(m_txtGridSquare, 0, wxALIGN_CENTER_VERTICAL);

    vs->Add(grid, 0, wxEXPAND | wxBOTTOM, 10);

    page->SetSizer(vs);
    return page;
}

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

void SetupWizard::populateAudioCombo(wxComboBox* combo, IAudioEngine::AudioDirection dir)
{
    combo->Clear();
    auto engine = AudioEngineFactory::GetAudioEngine();
    for (auto& dev : engine->getAudioDeviceList(dir))
        combo->Append(dev.name);
    combo->Append("none");
}

void SetupWizard::populateSerialPorts()
{
    std::vector<wxString> portList;

#ifdef __WXMSW__
    wxRegKey key(wxRegKey::HKLM, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"));
    if (key.Exists() && key.Open(wxRegKey::Read))
    {
        size_t subkeys, values;
        if (key.GetKeyInfo(&subkeys, nullptr, &values, nullptr) && key.HasValues())
        {
            wxString key_name;
            long el = 1;
            key.GetFirstValue(key_name, el);
            for (unsigned int i = 0; i < values; i++)
            {
                wxString key_data;
                key.QueryValue(key_name, key_data);
                portList.push_back(key_data);
                key.GetNextValue(key_name, el);
            }
        }
    }
#endif
#if defined(__WXGTK__) || defined(__WXOSX__)
#if defined(__FreeBSD__) || defined(__WXOSX__)
    glob_t gl;
#if defined(__FreeBSD__)
    if (glob("/dev/tty*", GLOB_MARK, nullptr, &gl) == 0 ||
#else
    if (glob("/dev/tty.*", GLOB_MARK, nullptr, &gl) == 0 || // NOLINT
#endif
        glob("/dev/cu.*", GLOB_MARK, nullptr, &gl) == 0) { // NOLINT
        for (unsigned int i = 0; i < gl.gl_pathc; i++) {
            if (gl.gl_pathv[i][strlen(gl.gl_pathv[i]) - 1] == '/')
                continue;
#if defined(__FreeBSD__)
            if (gl.gl_pathv[i][8] >= 'l' && gl.gl_pathv[i][8] <= 's') continue;
            if (gl.gl_pathv[i][8] >= 'L' && gl.gl_pathv[i][8] <= 'S') continue;
            if (gl.gl_pathv[i][8] == 'v') continue;
#else
            if (!strcmp("/dev/cu.wlan-debug", gl.gl_pathv[i])) continue;
#endif
#ifndef __WXOSX__
            if (strchr(gl.gl_pathv[i], '.') != nullptr) continue;
#endif
            portList.push_back(gl.gl_pathv[i]);
        }
        globfree(&gl);
    }
#else
    glob_t gl;
    if (glob("/sys/class/tty/*/device/driver", GLOB_MARK, nullptr, &gl) == 0) // NOLINT
    {
        wxRegEx pathRegex("/sys/class/tty/([^/]+)");
        for (unsigned int i = 0; i < gl.gl_pathc; i++) {
            wxString path = gl.gl_pathv[i];
            if (pathRegex.Matches(path))
                portList.push_back("/dev/" + pathRegex.GetMatch(path, 1));
        }
        globfree(&gl);
    }
    if (glob("/dev/serial/by-id/*", GLOB_MARK, nullptr, &gl) == 0) // NOLINT
    {
        for (unsigned int i = 0; i < gl.gl_pathc; i++)
            portList.push_back(gl.gl_pathv[i]);
        globfree(&gl);
    }
    if (glob("/dev/rfcomm*", GLOB_MARK, nullptr, &gl) == 0) // NOLINT
    {
        for (unsigned int i = 0; i < gl.gl_pathc; i++)
            portList.push_back(gl.gl_pathv[i]);
        globfree(&gl);
    }
#endif
#endif

    std::sort(portList.begin(), portList.end(),
        [](const wxString& a, const wxString& b) {
            wxRegEx re("^([^0-9]+)([0-9]+)$");
            wxString an, bn;
            int ai = 0, bi = 0;
            if (re.Matches(a)) { an = re.GetMatch(a, 1); ai = wxAtoi(re.GetMatch(a, 2)); }
            else { an = a; }
            if (re.Matches(b)) { bn = re.GetMatch(b, 1); bi = wxAtoi(re.GetMatch(b, 2)); }
            else { bn = b; }
            return (an < bn) || (an == bn && ai < bi);
        });

    for (auto& p : portList) {
        m_cbSerialPort->Append(p);
        m_cbPttSerialPort->Append(p);
        m_cbCtlDevicePath->Append(p);
    }
}

void SetupWizard::populateBaudRates(int rigIndex)
{
    wxString rates[] = {
        "default", "300", "1200", "2400", "4800", "9600",
        "19200", "38400", "57600", "115200", "230400",
        "460800", "500000", "576000", "921600", "1000000",
        "1152000", "1500000", "2000000"
    };

    int minRate = 0, maxRate = 0;
    if (rigIndex >= 0) {
        minRate = HamlibRigController::GetMinimumSerialBaudRate(rigIndex);
        maxRate = HamlibRigController::GetMaximumSerialBaudRate(rigIndex);
    }

    wxString prev = m_cbSerialRate->GetValue();
    m_cbSerialRate->Clear();

    for (unsigned int i = 0; i < WXSIZEOF(rates); i++) {
        int r = wxAtoi(rates[i]);
        if (i > 0 && minRate > 0 && maxRate > 0)
            if (r < minRate || r > maxRate) continue;
        m_cbSerialRate->Append(rates[i]);
        if (rates[i] == prev)
            m_cbSerialRate->SetSelection(m_cbSerialRate->GetCount() - 1);
        else if (i == 0 && prev.IsEmpty())
            m_cbSerialRate->SetSelection(0);
    }
    if (m_cbSerialRate->GetCurrentSelection() == wxNOT_FOUND)
        m_cbSerialRate->SetSelection(0);
}

void SetupWizard::loadConfig()
{
    auto& cfg = wxGetApp().appConfiguration;

    auto audioEngine = AudioEngineFactory::GetAudioEngine();

    // Page 0: Receive Audio
    m_cbRadioIn->SetValue(cfg.audioConfiguration.soundCard1In.deviceName);
    {
        wxString spk = cfg.audioConfiguration.soundCard1Out.deviceName;
        if (spk.IsEmpty() || spk == "none")
        {
            auto def = audioEngine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_OUT);
            if (def.isValid()) spk = def.name;
        }
        m_cbSpeakerOut->SetValue(spk);
    }

    // Page 1: Transmit Audio
    wxString sc2in  = cfg.audioConfiguration.soundCard2In.deviceName;
    wxString sc2out = cfg.audioConfiguration.soundCard2Out.deviceName;
    bool rxOnly = (sc2in.IsEmpty() || sc2in == "none") &&
                  (sc2out.IsEmpty() || sc2out == "none");
    m_ckReceiveOnly->SetValue(rxOnly);
    if (!rxOnly) {
        m_cbMicIn->SetValue(sc2in);
        m_cbRadioOut->SetValue(sc2out);
    } else {
        // Pre-fill microphone with system default so the user sees a sensible suggestion
        auto def = audioEngine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_IN);
        if (def.isValid()) m_cbMicIn->SetValue(def.name);
    }

    // Page 2: Radio Control — Hamlib
    m_ckHamlib->SetValue(cfg.rigControlConfiguration.hamlibUseForPTT);
    m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
    m_cbSerialPort->SetValue(cfg.rigControlConfiguration.hamlibSerialPort);
    m_cbPttSerialPort->SetValue(cfg.rigControlConfiguration.hamlibPttSerialPort);
    {
        int rig = m_cbRigName->GetCurrentSelection();
        populateBaudRates(rig >= 0 ? rig : -1);
        if (cfg.rigControlConfiguration.hamlibSerialRate == 0)
            m_cbSerialRate->SetSelection(0);
        else
            m_cbSerialRate->SetValue(
                wxString::Format("%i", cfg.rigControlConfiguration.hamlibSerialRate.get()));
    }
    m_cbPttMethod->SetSelection((int)cfg.rigControlConfiguration.hamlibPTTType);

    // Serial PTT
    m_ckSerialPTT->SetValue(cfg.rigControlConfiguration.useSerialPTT);
    m_cbCtlDevicePath->SetValue(cfg.rigControlConfiguration.serialPTTPort);
    m_rbUseRTS->SetValue(cfg.rigControlConfiguration.serialPTTUseRTS);
    m_ckRTSPos->SetValue(cfg.rigControlConfiguration.serialPTTPolarityRTS);
    m_rbUseDTR->SetValue(cfg.rigControlConfiguration.serialPTTUseDTR);
    m_ckDTRPos->SetValue(cfg.rigControlConfiguration.serialPTTPolarityDTR);

#if defined(WIN32)
    m_ckOmniRig->SetValue(cfg.rigControlConfiguration.useOmniRig);
    m_cbOmniRigRigId->SetSelection(cfg.rigControlConfiguration.omniRigRigId);
#endif

    // Page 3: Reporting
    m_ckReportingEnable->SetValue(cfg.reportingConfiguration.reportingEnabled);
    m_txtCallsign->SetValue(cfg.reportingConfiguration.reportingCallsign);
    m_txtGridSquare->SetValue(cfg.reportingConfiguration.reportingGridSquare);
}

void SetupWizard::saveConfig()
{
    auto& cfg = wxGetApp().appConfiguration;
    auto  audioEngine = AudioEngineFactory::GetAudioEngine();

    // Helper: look up default sample rate for a named device
    auto getSampleRate = [&](const wxString& name, IAudioEngine::AudioDirection dir) -> int {
        if (name.IsEmpty() || name == "none") return 0;
        for (auto& dev : audioEngine->getAudioDeviceList(dir))
            if (dev.name.IsSameAs(name)) return dev.defaultSampleRate;
        return 0;
    };

    // Page 0: Receive Audio
    wxString sc1in  = m_cbRadioIn->GetValue();
    wxString sc1out = m_cbSpeakerOut->GetValue();
    cfg.audioConfiguration.soundCard1In.deviceName  = sc1in;
    cfg.audioConfiguration.soundCard1Out.deviceName = sc1out;
    int r1in = getSampleRate(sc1in, IAudioEngine::AUDIO_ENGINE_IN);
    if (r1in > 0) cfg.audioConfiguration.soundCard1In.sampleRate = r1in;
    int r1out = getSampleRate(sc1out, IAudioEngine::AUDIO_ENGINE_OUT);
    if (r1out > 0) cfg.audioConfiguration.soundCard1Out.sampleRate = r1out;

    // Page 1: Transmit Audio
    wxString sc2in, sc2out;
    if (m_ckReceiveOnly->GetValue()) {
        sc2in = sc2out = "none";
    } else {
        sc2in  = m_cbMicIn->GetValue();
        sc2out = m_cbRadioOut->GetValue();
    }
    cfg.audioConfiguration.soundCard2In.deviceName  = sc2in;
    cfg.audioConfiguration.soundCard2Out.deviceName = sc2out;
    int r2in = getSampleRate(sc2in, IAudioEngine::AUDIO_ENGINE_IN);
    if (r2in > 0) cfg.audioConfiguration.soundCard2In.sampleRate = r2in;
    int r2out = getSampleRate(sc2out, IAudioEngine::AUDIO_ENGINE_OUT);
    if (r2out > 0) cfg.audioConfiguration.soundCard2Out.sampleRate = r2out;

    // Page 2: Radio Control — Hamlib
    cfg.rigControlConfiguration.hamlibUseForPTT  = m_ckHamlib->GetValue();
    wxGetApp().m_intHamlibRig = m_cbRigName->GetCurrentSelection();
    cfg.rigControlConfiguration.hamlibRigName =
        (wxGetApp().m_intHamlibRig >= 0)
            ? HamlibRigController::RigIndexToName(wxGetApp().m_intHamlibRig)
            : "";
    cfg.rigControlConfiguration.hamlibSerialPort    = m_cbSerialPort->GetValue();
    cfg.rigControlConfiguration.hamlibPttSerialPort = m_cbPttSerialPort->GetValue();
    {
        wxString rateStr = m_cbSerialRate->GetValue();
        if (rateStr == "default")
            cfg.rigControlConfiguration.hamlibSerialRate = 0;
        else {
            long r; rateStr.ToLong(&r);
            cfg.rigControlConfiguration.hamlibSerialRate = (unsigned int)r;
        }
    }
    cfg.rigControlConfiguration.hamlibPTTType = m_cbPttMethod->GetCurrentSelection();

    // Serial PTT
    cfg.rigControlConfiguration.useSerialPTT         = m_ckSerialPTT->GetValue();
    cfg.rigControlConfiguration.serialPTTPort         = m_cbCtlDevicePath->GetValue();
    cfg.rigControlConfiguration.serialPTTUseRTS       = m_rbUseRTS->GetValue();
    cfg.rigControlConfiguration.serialPTTPolarityRTS  = m_ckRTSPos->GetValue();
    cfg.rigControlConfiguration.serialPTTUseDTR       = m_rbUseDTR->GetValue();
    cfg.rigControlConfiguration.serialPTTPolarityDTR  = m_ckDTRPos->GetValue();

#if defined(WIN32)
    cfg.rigControlConfiguration.useOmniRig   = m_ckOmniRig->GetValue();
    cfg.rigControlConfiguration.omniRigRigId = m_cbOmniRigRigId->GetCurrentSelection();
#endif

    // Page 3: Reporting
    bool reportingOn = m_ckReportingEnable->GetValue();
    cfg.reportingConfiguration.reportingEnabled      = reportingOn;
    cfg.reportingConfiguration.reportingCallsign     = m_txtCallsign->GetValue();
    cfg.reportingConfiguration.reportingGridSquare   = m_txtGridSquare->GetValue();
    cfg.reportingConfiguration.freedvReporterEnabled = reportingOn;
    cfg.reportingConfiguration.pskReporterEnabled    = reportingOn;

    cfg.save(pConfig);
}

// --------------------------------------------------------------------------
// Navigation
// --------------------------------------------------------------------------

void SetupWizard::updateNavButtons()
{
    m_btnPrev->Enable(m_currentPage > 0);
    m_btnNext->Show(m_currentPage < NUM_PAGES - 1);
    m_btnFinish->Show(m_currentPage == NUM_PAGES - 1);
    Layout();
}

void SetupWizard::updateTxState()
{
    bool rxOnly = m_ckReceiveOnly->GetValue();
    m_stMicIn->Enable(!rxOnly);
    m_cbMicIn->Enable(!rxOnly);
    m_stRadioOut->Enable(!rxOnly);
    m_cbRadioOut->Enable(!rxOnly);
}

void SetupWizard::updateRadioState()
{
    bool hl = m_ckHamlib->GetValue();
    m_stRigName->Enable(hl);
    m_cbRigName->Enable(hl);
    m_stSerialPort->Enable(hl);
    m_cbSerialPort->Enable(hl);
    m_stSerialRate->Enable(hl);
    m_cbSerialRate->Enable(hl);
    m_stPttMethod->Enable(hl);
    m_cbPttMethod->Enable(hl);
    bool pttNeedsSep = hl &&
        m_cbPttMethod->GetValue() != _("CAT") &&
        m_cbPttMethod->GetValue() != _("None");
    m_stPttSerialPort->Enable(pttNeedsSep);
    m_cbPttSerialPort->Enable(pttNeedsSep);

    bool sp = m_ckSerialPTT->GetValue();
    m_stCtlDevice->Enable(sp);
    m_cbCtlDevicePath->Enable(sp);
    m_rbUseRTS->Enable(sp);
    m_ckRTSPos->Enable(sp);
    m_rbUseDTR->Enable(sp);
    m_ckDTRPos->Enable(sp);

#if defined(WIN32)
    bool omni = m_ckOmniRig->GetValue();
    m_stOmniRigId->Enable(omni);
    m_cbOmniRigRigId->Enable(omni);
#endif
}

void SetupWizard::updateReportingState()
{
    bool en = m_ckReportingEnable->GetValue();
    m_stCallsign->Enable(en);
    m_txtCallsign->Enable(en);
    m_stGridSquare->Enable(en);
    m_txtGridSquare->Enable(en);
}

// --------------------------------------------------------------------------
// Event handlers
// --------------------------------------------------------------------------

void SetupWizard::OnNext(wxCommandEvent&)
{
    if (m_currentPage < NUM_PAGES - 1) {
        m_currentPage++;
        m_book->SetSelection(m_currentPage);
        updateNavButtons();
    }
}

void SetupWizard::OnPrev(wxCommandEvent&)
{
    if (m_currentPage > 0) {
        m_currentPage--;
        m_book->SetSelection(m_currentPage);
        updateNavButtons();
    }
}

void SetupWizard::OnFinish(wxCommandEvent&)
{
    saveConfig();
    EndModal(wxID_OK);
}

void SetupWizard::OnReceiveOnlyChanged(wxCommandEvent&)
{
    updateTxState();
}

void SetupWizard::OnHamlibChanged(wxCommandEvent&)
{
    if (m_ckHamlib->GetValue()) {
        m_ckSerialPTT->SetValue(false);
#if defined(WIN32)
        m_ckOmniRig->SetValue(false);
#endif
    }
    updateRadioState();
}

void SetupWizard::OnSerialPTTChanged(wxCommandEvent&)
{
    if (m_ckSerialPTT->GetValue()) {
        m_ckHamlib->SetValue(false);
#if defined(WIN32)
        m_ckOmniRig->SetValue(false);
#endif
    }
    updateRadioState();
}

void SetupWizard::OnRigNameChanged(wxCommandEvent&)
{
    populateBaudRates(m_cbRigName->GetCurrentSelection());
    updateRadioState();
}

void SetupWizard::OnReportingEnableChanged(wxCommandEvent&)
{
    updateReportingState();
}
