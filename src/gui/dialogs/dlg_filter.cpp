//==========================================================================
// Name:            dlg_filter.cpp
// Purpose:         Dialog for controlling Codec audio filtering
// Date:            Nov 25 2012
// Authors:         David Rowe, David Witten
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#include <atomic>

#include "dlg_filter.h"
#include "gui/util/LabelOverrideAccessible.h"

#define SLIDER_MAX_FREQ_BASS 600
#define SLIDER_MAX_FREQ 3900
#define SLIDER_MAX_GAIN 400
#define SLIDER_MAX_Q 100
#define SLIDER_MAX_BETA_GAMMA 100
#define SLIDER_LENGTH 100

#define FILTER_MIN_MAG_DB -20.0
#define FILTER_MAX_MAG_DB  20.0

#define MAX_FREQ_BASS      600.00
#define MAX_FREQ_TREBLE   3900.00
#define MAX_FREQ_DEF      3000.00

#define MIN_GAIN          -20.0
#define MAX_GAIN           20.0

#define MAX_LOG10_Q         1.0
#define MIN_LOG10_Q        -1.0

// DFT parameters

#define IMP_AMP           2000.0                     // amplitude of impulse
#define NIMP              50                         // number of samples in impulse response
#define F_STEP_DFT        10.0                       // frequency steps to sample spectrum
#define F_MAG_N           (int)(MAX_F_HZ/F_STEP_DFT) // number of frequency steps

extern FreeDVInterface freedvInterface;
extern wxConfigBase *pConfig;
extern wxMutex g_mutexProtectingCallbackData;
extern std::atomic<bool> g_agcEnabled;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class FilterDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
FilterDlg::FilterDlg(wxWindow* parent, bool running, bool *newMicInFilter, bool *newSpkOutFilter,
                     wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", title, wxGetApp().customConfigFileName));
    }
    
    m_running = running;
    m_newMicInFilter = newMicInFilter;
    m_newSpkOutFilter = newSpkOutFilter;

    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);

    // LPC Post Filter --------------------------------------------------------
    wxStaticBox* lpcPostFilterBox = new wxStaticBox(this, wxID_ANY, _("FreeDV 1600 LPC Post Filter"));
    wxStaticBoxSizer* lpcpfs = new wxStaticBoxSizer(lpcPostFilterBox, wxHORIZONTAL);

    wxBoxSizer* left = new wxBoxSizer(wxVERTICAL);

    m_codec2LPCPostFilterEnable = new wxCheckBox(lpcPostFilterBox, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    left->Add(m_codec2LPCPostFilterEnable, 0, wxALL, 5);

    m_codec2LPCPostFilterBassBoost = new wxCheckBox(lpcPostFilterBox, wxID_ANY, _("0-1 kHz 3dB Boost"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    left->Add(m_codec2LPCPostFilterBassBoost, 0, wxALL, 5);
    lpcpfs->Add(left, 0, wxALL | wxALIGN_CENTRE_HORIZONTAL | wxALIGN_CENTRE_VERTICAL, 5);

    wxBoxSizer* sizerBetaGamma = new wxBoxSizer(wxVERTICAL);
    newLPCPFControl(&m_codec2LPCPostFilterBeta, &m_staticTextBeta, lpcPostFilterBox, sizerBetaGamma, "Beta");
    newLPCPFControl(&m_codec2LPCPostFilterGamma, &m_staticTextGamma, lpcPostFilterBox, sizerBetaGamma, "Gamma");
    lpcpfs->Add(sizerBetaGamma, 1, wxALL | wxEXPAND, 5);
    
    m_LPCPostFilterDefault = new wxButton(lpcPostFilterBox, wxID_ANY, wxT("Default"));
    lpcpfs->Add(m_LPCPostFilterDefault, 0, wxALL | wxALIGN_CENTRE_HORIZONTAL | wxALIGN_CENTRE_VERTICAL, 5);

    bSizer30->Add(lpcpfs, 0, wxALL | wxEXPAND, 5);

    // Speex pre-processor --------------------------------------------------

    wxStaticBoxSizer* sbSizer_speexpp;
    wxStaticBox *sb_speexpp = new wxStaticBox(this, wxID_ANY, _("Mic Audio Pre-Processing"));
    sbSizer_speexpp = new wxStaticBoxSizer(sb_speexpp, wxHORIZONTAL);

    m_ckboxSpeexpp = new wxCheckBox(sb_speexpp, wxID_ANY, _("Speex Noise Suppression"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_speexpp->Add(m_ckboxSpeexpp, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckboxSpeexpp->SetToolTip(_("Enable noise suppression, dereverberation, AGC of mic signal"));
    
    m_ckboxAgcEnabled = new wxCheckBox(sb_speexpp, wxID_ANY, _("AGC"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_speexpp->Add(m_ckboxAgcEnabled, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckboxAgcEnabled->SetToolTip(_("Automatic gain control for microphone"));
    
    m_ckbox700C_EQ = new wxCheckBox(sb_speexpp, wxID_ANY, _("700D/700E Auto EQ"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_speexpp->Add(m_ckbox700C_EQ, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckbox700C_EQ->SetToolTip(_("Automatic equalisation for FreeDV 700D/700E Codec input audio"));

    bSizer30->Add(sbSizer_speexpp, 0, wxALL | wxEXPAND, 5);   

    // EQ Filters -----------------------------------------------------------

    long nb_style = wxNB_BOTTOM;
    m_auiNotebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, nb_style);
    wxPanel* panelMicInEqualizer = new wxPanel(m_auiNotebook, wxID_ANY);
    wxPanel* panelSpkOutEqualizer = new wxPanel(m_auiNotebook, wxID_ANY);
   
    panelMicInEqualizer->SetFont(GetFont()); 
    panelSpkOutEqualizer->SetFont(GetFont()); 

    wxBoxSizer* eqMicInSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqMicInSizerEnable = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqMicInSizerSliders = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqMicInSizerVol = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqMicInSizerBass = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqMicInSizerTreble = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqMicInSizerMid = new wxBoxSizer(wxVERTICAL);
    
    m_MicInEnable = new wxCheckBox(panelMicInEqualizer, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    eqMicInSizerEnable->Add(m_MicInEnable,0,wxALIGN_CENTRE_VERTICAL|wxALL,5);
    m_MicInDefault = new wxButton(panelMicInEqualizer, wxID_ANY, wxT("Default"));
    eqMicInSizerEnable->Add(m_MicInDefault,0,wxALL | wxALIGN_CENTRE_VERTICAL,5);
    eqMicInSizer->Add(eqMicInSizerEnable,0,wxEXPAND);
    
    m_MicInVol    = newEQ(panelMicInEqualizer, eqMicInSizerVol, "Vol", MAX_FREQ_TREBLE, disableQ, disableFreq, SLIDER_MAX_FREQ);
    eqMicInSizerSliders->Add(eqMicInSizerVol, 1, wxALL, 7);
    
    m_MicInBass   = newEQ(panelMicInEqualizer, eqMicInSizerBass, "Bass", MAX_FREQ_BASS, disableQ, enableFreq, SLIDER_MAX_FREQ_BASS);
    eqMicInSizerSliders->Add(eqMicInSizerBass, 1, wxALL, 7);
    
    m_MicInMid    = newEQ(panelMicInEqualizer, eqMicInSizerMid, "Mid", MAX_FREQ_DEF, enableQ, enableFreq, SLIDER_MAX_FREQ);
    eqMicInSizerSliders->Add(eqMicInSizerMid, 1, wxALL, 7);
        
    m_MicInTreble = newEQ(panelMicInEqualizer, eqMicInSizerTreble, "Treble", MAX_FREQ_TREBLE, disableQ, enableFreq, SLIDER_MAX_FREQ);
    eqMicInSizerSliders->Add(eqMicInSizerTreble, 1, wxALL, 7);

    eqMicInSizer->Add(eqMicInSizerSliders, 0, wxEXPAND, 0);
    
    wxBoxSizer* eqSpkOutSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqSpkOutSizerEnable = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqSpkOutSizerSliders = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqSpkOutSizerVol = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqSpkOutSizerBass = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqSpkOutSizerTreble = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* eqSpkOutSizerMid = new wxBoxSizer(wxVERTICAL);

    m_SpkOutEnable = new wxCheckBox(panelSpkOutEqualizer, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    eqSpkOutSizerEnable->Add(m_SpkOutEnable,0,wxALIGN_CENTRE_VERTICAL|wxALL,5);
    m_SpkOutDefault = new wxButton(panelSpkOutEqualizer, wxID_ANY, wxT("Default"));
    eqSpkOutSizerEnable->Add(m_SpkOutDefault,0,wxALL | wxALIGN_CENTRE_VERTICAL,5);
    eqSpkOutSizer->Add(eqSpkOutSizerEnable,0,wxEXPAND);
    
    m_SpkOutVol    = newEQ(panelSpkOutEqualizer, eqSpkOutSizerVol, "Vol", MAX_FREQ_TREBLE, disableQ, disableFreq, SLIDER_MAX_FREQ);
    eqSpkOutSizerSliders->Add(eqSpkOutSizerVol, 1, wxALL, 7);
    
    m_SpkOutBass   = newEQ(panelSpkOutEqualizer, eqSpkOutSizerBass, "Bass"  , MAX_FREQ_BASS, disableQ, enableFreq, SLIDER_MAX_FREQ_BASS);
    eqSpkOutSizerSliders->Add(eqSpkOutSizerBass, 1, wxALL, 7);
    
    m_SpkOutMid    = newEQ(panelSpkOutEqualizer, eqSpkOutSizerMid, "Mid"   , MAX_FREQ_DEF, enableQ, enableFreq, SLIDER_MAX_FREQ);
    eqSpkOutSizerSliders->Add(eqSpkOutSizerMid, 1, wxALL, 7);
        
    m_SpkOutTreble = newEQ(panelSpkOutEqualizer, eqSpkOutSizerTreble, "Treble", MAX_FREQ_TREBLE, disableQ, enableFreq, SLIDER_MAX_FREQ);
    eqSpkOutSizerSliders->Add(eqSpkOutSizerTreble, 1, wxALL, 7);

    eqSpkOutSizer->Add(eqSpkOutSizerSliders, 0, wxEXPAND, 0);

    // Storgage for spectrum magnitude plots ------------------------------------

    m_MicInMagdB = new float[F_MAG_N];
    for(int i=0; i<F_MAG_N; i++)
        m_MicInMagdB[i] = 0.0;

    m_SpkOutMagdB = new float[F_MAG_N];
    for(int i=0; i<F_MAG_N; i++)
        m_SpkOutMagdB[i] = 0.0;

    // Spectrum Plots -----------------------------------------------------------

    m_MicInFreqRespPlot = new PlotSpectrum(panelMicInEqualizer, m_MicInMagdB, F_MAG_N, FILTER_MIN_MAG_DB, FILTER_MAX_MAG_DB);
    m_MicInFreqRespPlot->SetMinSize(wxSize(600, 200));
    eqMicInSizer->Add(m_MicInFreqRespPlot, 1, wxEXPAND, 0);
    panelMicInEqualizer->SetSizer(eqMicInSizer);
    m_auiNotebook->AddPage(panelMicInEqualizer, _("Microphone In Equaliser"));

    m_SpkOutFreqRespPlot = new PlotSpectrum(panelSpkOutEqualizer, m_SpkOutMagdB, F_MAG_N, FILTER_MIN_MAG_DB, FILTER_MAX_MAG_DB);
    m_SpkOutFreqRespPlot->SetMinSize(wxSize(600, 200));
    eqSpkOutSizer->Add(m_SpkOutFreqRespPlot, 1, wxEXPAND, 0);
    panelSpkOutEqualizer->SetSizer(eqSpkOutSizer);
    m_auiNotebook->AddPage(panelSpkOutEqualizer, _("Speaker Out Equaliser"));
    
    bSizer30->Add(m_auiNotebook, 1, wxEXPAND|wxALL, 3);
    

    //  OK - Cancel buttons at the bottom --------------------------

    wxBoxSizer* bSizer31 = new wxBoxSizer(wxHORIZONTAL);

    m_sdbSizer5OK = new wxButton(this, wxID_CLOSE);
    bSizer31->Add(m_sdbSizer5OK, 0, wxALL, 2);

    bSizer30->Add(bSizer31, 0, wxALL | wxALIGN_CENTER, 3);

    this->SetSizerAndFit(bSizer30);
    this->Layout();

    this->Centre(wxBOTH);

    // Workaround to prevent Default button from jumping around
    // when selecting different tabs.
    wxSize curSize = this->GetSize();
    auto w = curSize.GetWidth();
    auto h = curSize.GetHeight();
    CallAfter([=]()
    {
        SetSize(w, h);
    });
    CallAfter([=]()
    {
        SetSize(w + 1, h + 1);
    });
    CallAfter([=]()
    {
        SetSize(w, h);
    });

    // Connect Events -------------------------------------------------------

    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FilterDlg::OnInitDialog));

    m_codec2LPCPostFilterEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnEnable), NULL, this);
    m_codec2LPCPostFilterBassBoost->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnBassBoost), NULL, this);
    m_LPCPostFilterDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnLPCPostFilterDefault), NULL, this);

    m_ckboxSpeexpp->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpeexppEnable), NULL, this);
    m_ckboxAgcEnabled->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnAgcEnable), NULL, this);
    m_ckbox700C_EQ->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::On700C_EQ), NULL, this);

    int events[] = {
        wxEVT_SCROLL_TOP, wxEVT_SCROLL_BOTTOM, wxEVT_SCROLL_LINEUP, wxEVT_SCROLL_LINEDOWN, wxEVT_SCROLL_PAGEUP, 
        wxEVT_SCROLL_PAGEDOWN, wxEVT_SCROLL_THUMBTRACK, wxEVT_SCROLL_THUMBRELEASE, wxEVT_SCROLL_CHANGED
    };
    
    for (auto event : events)
    {
        m_MicInBass.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInBassFreqScroll), NULL, this);
        m_MicInBass.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInBassGainScroll), NULL, this);
        m_MicInTreble.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInTrebleFreqScroll), NULL, this);
        m_MicInTreble.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInTrebleGainScroll), NULL, this);
        m_MicInMid.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInMidFreqScroll), NULL, this);
        m_MicInMid.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInMidGainScroll), NULL, this);
        m_MicInMid.sliderQ->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInMidQScroll), NULL, this);
        m_MicInVol.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnMicInVolGainScroll), NULL, this);
        
        m_SpkOutBass.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutBassFreqScroll), NULL, this);
        m_SpkOutBass.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutBassGainScroll), NULL, this);
        m_SpkOutTreble.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleFreqScroll), NULL, this);
        m_SpkOutTreble.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleGainScroll), NULL, this);
        m_SpkOutMid.sliderFreq->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidFreqScroll), NULL, this);
        m_SpkOutMid.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidGainScroll), NULL, this);
        m_SpkOutMid.sliderQ->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidQScroll), NULL, this);
        m_SpkOutVol.sliderGain->Connect(event, wxScrollEventHandler(FilterDlg::OnSpkOutVolGainScroll), NULL, this);
        
        m_codec2LPCPostFilterBeta->Connect(event, wxScrollEventHandler(FilterDlg::OnBetaScroll), NULL, this);
        m_codec2LPCPostFilterGamma->Connect(event, wxScrollEventHandler(FilterDlg::OnGammaScroll), NULL, this);
    }
    
    m_MicInEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnMicInEnable), NULL, this);
    m_MicInDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnMicInDefault), NULL, this);

    m_SpkOutEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpkOutEnable), NULL, this);
    m_SpkOutDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnSpkOutDefault), NULL, this);

    m_sdbSizer5OK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnOK), NULL, this);
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FilterDlg::OnClose), NULL, this);

}

//-------------------------------------------------------------------------
// ~FilterDlg()
//-------------------------------------------------------------------------
FilterDlg::~FilterDlg()
{
    delete[] m_MicInMagdB;
    delete[] m_SpkOutMagdB;

    // Disconnect Events
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FilterDlg::OnClose), NULL, this);
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FilterDlg::OnInitDialog));

    m_codec2LPCPostFilterEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnEnable), NULL, this);
    m_codec2LPCPostFilterBassBoost->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnBassBoost), NULL, this);
    m_LPCPostFilterDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnLPCPostFilterDefault), NULL, this);

    m_ckboxSpeexpp->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpeexppEnable), NULL, this);
    m_ckboxAgcEnabled->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnAgcEnable), NULL, this);
    
    int events[] = {
        wxEVT_SCROLL_TOP, wxEVT_SCROLL_BOTTOM, wxEVT_SCROLL_LINEUP, wxEVT_SCROLL_LINEDOWN, wxEVT_SCROLL_PAGEUP, 
        wxEVT_SCROLL_PAGEDOWN, wxEVT_SCROLL_THUMBTRACK, wxEVT_SCROLL_THUMBRELEASE, wxEVT_SCROLL_CHANGED
    };
    
    for (auto event : events)
    {
        m_MicInBass.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInBassFreqScroll), NULL, this);
        m_MicInBass.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInBassGainScroll), NULL, this);
        m_MicInTreble.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInTrebleFreqScroll), NULL, this);
        m_MicInTreble.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInTrebleGainScroll), NULL, this);
        m_MicInMid.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInMidFreqScroll), NULL, this);
        m_MicInMid.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInMidGainScroll), NULL, this);
        m_MicInMid.sliderQ->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInMidQScroll), NULL, this);
        m_MicInVol.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnMicInVolGainScroll), NULL, this);
        
        m_SpkOutBass.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutBassFreqScroll), NULL, this);
        m_SpkOutBass.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutBassGainScroll), NULL, this);
        m_SpkOutTreble.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleFreqScroll), NULL, this);
        m_SpkOutTreble.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleGainScroll), NULL, this);
        m_SpkOutMid.sliderFreq->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidFreqScroll), NULL, this);
        m_SpkOutMid.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidGainScroll), NULL, this);
        m_SpkOutMid.sliderQ->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutMidQScroll), NULL, this);
        m_SpkOutVol.sliderGain->Disconnect(event, wxScrollEventHandler(FilterDlg::OnSpkOutVolGainScroll), NULL, this);
        
        m_codec2LPCPostFilterBeta->Disconnect(event, wxScrollEventHandler(FilterDlg::OnBetaScroll), NULL, this);
        m_codec2LPCPostFilterGamma->Disconnect(event, wxScrollEventHandler(FilterDlg::OnGammaScroll), NULL, this);
    }
    
    m_MicInEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnMicInEnable), NULL, this);
    m_MicInDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnMicInDefault), NULL, this);

    m_SpkOutEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpkOutEnable), NULL, this);
    m_SpkOutDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnSpkOutDefault), NULL, this);

    m_sdbSizer5OK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnOK), NULL, this);
}

void FilterDlg::newLPCPFControl(wxSlider **slider, wxStaticText **stValue, wxWindow* parent, wxSizer *s, wxString controlName)
{
    wxBoxSizer *bs = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* st = new wxStaticText(parent, wxID_ANY, controlName, wxDefaultPosition, wxSize(70,-1), wxALIGN_RIGHT);
    bs->Add(st, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2);

    *slider = new wxSlider(parent, wxID_ANY, 0, 0, SLIDER_MAX_BETA_GAMMA, wxDefaultPosition); 
    (*slider)->SetMinSize(wxSize(SLIDER_LENGTH,wxDefaultCoord));

    bs->Add(*slider, 1, wxALL|wxEXPAND, 2);

    *stValue = new wxStaticText(parent, wxID_ANY, wxT("0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    bs->Add(*stValue, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT|wxALL, 2);

    s->Add(bs, 0, wxALL | wxEXPAND, 0);

#if wxUSE_ACCESSIBILITY
    auto lpcAccessible = new LabelOverrideAccessible([stValue]() {
        return (*stValue)->GetLabel();
    });
    (*slider)->SetAccessible(lpcAccessible);
#endif // wxUSE_ACCESSIBILITY
}

void FilterDlg::newEQControl(wxWindow* parent, wxSlider** slider, wxStaticText** value, wxSizer *sizer, wxString controlName, int max)
{
    wxStaticText* label = new wxStaticText(parent, wxID_ANY, controlName, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    sizer->Add(label, 0, wxALIGN_CENTER|wxALL, 0);

    *slider = new wxSlider(parent, wxID_ANY, 0, 0, max, wxDefaultPosition, wxSize(wxDefaultCoord,SLIDER_LENGTH), wxSL_VERTICAL|wxSL_INVERSE|wxALIGN_CENTER);
    sizer->Add(*slider, 1, wxALIGN_CENTER|wxALL, 0);

    *value = new wxStaticText(parent, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(40,-1), wxALIGN_CENTER);
    sizer->Add(*value, 0, wxALIGN_CENTER, 5);
}

EQ FilterDlg::newEQ(wxWindow* parent, wxSizer *bs, wxString eqName, float maxFreqHz, bool enableQ, bool enableFreq, int maxSliderFreq)
{
    EQ eq;

    eq.eqBox = new wxStaticBox(parent, wxID_ANY, eqName);
    wxStaticBoxSizer *bsEQ = new wxStaticBoxSizer(eq.eqBox, wxHORIZONTAL);

    if (enableFreq)
    {
        wxSizer* sizerFreq = new wxBoxSizer(wxVERTICAL);
        newEQControl(eq.eqBox, &eq.sliderFreq, &eq.valueFreq, sizerFreq, "Freq", maxSliderFreq);
        bsEQ->Add(sizerFreq, 1, wxEXPAND);
        eq.maxFreqHz = maxFreqHz;
        eq.sliderFreqId = eq.sliderFreq->GetId();

#if wxUSE_ACCESSIBILITY
        auto freqAccessible = new LabelOverrideAccessible([&, eq]() {
            return eq.valueFreq->GetLabel();
        });
        eq.sliderFreq->SetAccessible(freqAccessible);
#endif // wxUSE_ACCESSIBILITY
    }
    else
    {
        eq.sliderFreq = NULL;
    }
    
    wxSizer* sizerGain = new wxBoxSizer(wxVERTICAL);
    newEQControl(eq.eqBox, &eq.sliderGain, &eq.valueGain, sizerGain, "Gain", SLIDER_MAX_GAIN);
    bsEQ->Add(sizerGain, 1, wxEXPAND);
    
#if wxUSE_ACCESSIBILITY
    auto gainAccessible = new LabelOverrideAccessible([&, eq]() {
        return eq.valueGain->GetLabel();
    });
    eq.sliderGain->SetAccessible(gainAccessible);
#endif // wxUSE_ACCESSIBILITY

    if (enableQ)
    {
        wxSizer* sizerQ = new wxBoxSizer(wxVERTICAL);
        newEQControl(eq.eqBox, &eq.sliderQ, &eq.valueQ, sizerQ, "Q", SLIDER_MAX_Q);
        bsEQ->Add(sizerQ, 1, wxEXPAND);

#if wxUSE_ACCESSIBILITY
        auto qAccessible = new LabelOverrideAccessible([&, eq]() {
            return eq.valueQ->GetLabel();
        });
        eq.sliderQ->SetAccessible(qAccessible);
#endif // wxUSE_ACCESSIBILITY
    }
    else
        eq.sliderQ = NULL;

    bs->Add(bsEQ, 0, wxEXPAND);

    return eq;
}

void FilterDlg::syncVolumes()
{
    m_MicInVol.gaindB = wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB;
    m_MicInVol.gaindB = limit(m_MicInVol.gaindB, MIN_GAIN, MAX_GAIN);
    setGain(&m_MicInVol);
    
    m_SpkOutVol.gaindB = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB;
    m_SpkOutVol.gaindB = limit(m_SpkOutVol.gaindB, MIN_GAIN, MAX_GAIN);
    setGain(&m_SpkOutVol);
}

//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void FilterDlg::ExchangeData(int inout)
{
    if(inout == EXCHANGE_DATA_IN)
    {
        // LPC Post filter

        m_codec2LPCPostFilterEnable->SetValue(wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterEnable);
        m_codec2LPCPostFilterBassBoost->SetValue(wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBassBoost);
        m_beta = wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBeta; setBeta();
        m_gamma = wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterGamma; setGamma();

        // Speex Pre-Processor

        m_ckboxSpeexpp->SetValue(wxGetApp().appConfiguration.filterConfiguration.speexppEnable);
        
        // AGC
        m_ckboxAgcEnabled->SetValue(wxGetApp().appConfiguration.filterConfiguration.agcEnabled);

        // Codec 2 700C EQ
        m_ckbox700C_EQ->SetValue(wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer);

        // Mic In Equaliser

        m_MicInBass.freqHz = wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassFreqHz;
        m_MicInBass.freqHz = limit(m_MicInBass.freqHz, 1.0, MAX_FREQ_BASS);
        setFreq(&m_MicInBass);
        m_MicInBass.gaindB = wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassGaindB;
        m_MicInBass.gaindB = limit(m_MicInBass.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInBass);

        m_MicInTreble.freqHz = wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleFreqHz;
        m_MicInTreble.freqHz = limit(m_MicInTreble.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_MicInTreble);
        m_MicInTreble.gaindB = wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleGaindB;
        m_MicInTreble.gaindB = limit(m_MicInTreble.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInTreble);

        m_MicInMid.freqHz = wxGetApp().appConfiguration.filterConfiguration.micInChannel.midFreqHz;
        m_MicInMid.freqHz = limit(m_MicInMid.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_MicInMid);
        m_MicInMid.gaindB = wxGetApp().appConfiguration.filterConfiguration.micInChannel.midGainDB;
        m_MicInMid.gaindB = limit(m_MicInMid.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInMid);
        m_MicInMid.Q = wxGetApp().appConfiguration.filterConfiguration.micInChannel.midQ;
        m_MicInMid.Q = limit(m_MicInMid.Q, pow(10.0,MIN_LOG10_Q), pow(10.0, MAX_LOG10_Q));
        setQ(&m_MicInMid);

        m_MicInVol.gaindB = wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB;
        m_MicInVol.gaindB = limit(m_MicInVol.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInVol);

        m_MicInEnable->SetValue(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);

        plotMicInFilterSpectrum();

        // Spk Out Equaliser

        m_SpkOutBass.freqHz = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz;
        m_SpkOutBass.freqHz = limit(m_SpkOutBass.freqHz, 1.0, MAX_FREQ_BASS);
        setFreq(&m_SpkOutBass);
        m_SpkOutBass.gaindB = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassGaindB;
        m_SpkOutBass.gaindB = limit(m_SpkOutBass.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutBass);

        m_SpkOutTreble.freqHz = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleFreqHz;
        m_SpkOutTreble.freqHz = limit(m_SpkOutTreble.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_SpkOutTreble);
        m_SpkOutTreble.gaindB = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleGaindB;
        m_SpkOutTreble.gaindB = limit(m_SpkOutTreble.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutTreble);

        m_SpkOutMid.freqHz = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midFreqHz;
        m_SpkOutMid.freqHz = limit(m_SpkOutMid.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_SpkOutMid);
        m_SpkOutMid.gaindB = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midGainDB;
        m_SpkOutMid.gaindB = limit(m_SpkOutMid.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutMid);
        m_SpkOutMid.Q = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midQ;
        m_SpkOutMid.Q = limit(m_SpkOutMid.Q, pow(10.0,MIN_LOG10_Q), pow(10.0, MAX_LOG10_Q));
        setQ(&m_SpkOutMid);
        
        m_SpkOutVol.gaindB = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB;
        m_SpkOutVol.gaindB = limit(m_SpkOutVol.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutVol);

        m_SpkOutEnable->SetValue(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);

        plotSpkOutFilterSpectrum();
        
        updateControlState();
    }
    if(inout == EXCHANGE_DATA_OUT)
    {
        // LPC Post filter

        wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterEnable     = m_codec2LPCPostFilterEnable->GetValue();
        wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBassBoost  = m_codec2LPCPostFilterBassBoost->GetValue();
        wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBeta       = m_beta;
        wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterGamma      = m_gamma;

        // Speex Pre-Processor
        wxGetApp().appConfiguration.filterConfiguration.speexppEnable = m_ckboxSpeexpp->GetValue();
        
        // AGC
        wxGetApp().appConfiguration.filterConfiguration.agcEnabled = m_ckboxAgcEnabled->GetValue();

        // Codec 2 700C EQ
        wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer = m_ckbox700C_EQ->GetValue();

        // Mic In Equaliser

        wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassFreqHz = (int)m_MicInBass.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.bassGaindB = m_MicInBass.gaindB;

        wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleFreqHz = (int)m_MicInTreble.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.trebleGaindB = m_MicInTreble.gaindB;

        wxGetApp().appConfiguration.filterConfiguration.micInChannel.midFreqHz = (int)m_MicInMid.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.midGainDB = m_MicInMid.gaindB;
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.midQ = m_MicInMid.Q;

        wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB = m_MicInVol.gaindB;
        
        // Spk Out Equaliser

        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassFreqHz = (int)m_SpkOutBass.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.bassGaindB = m_SpkOutBass.gaindB;

        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleFreqHz = (int)m_SpkOutTreble.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.trebleGaindB = m_SpkOutTreble.gaindB;

        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midFreqHz = (int)m_SpkOutMid.freqHz;
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midGainDB = m_SpkOutMid.gaindB;
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.midQ = m_SpkOutMid.Q;

        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB = m_SpkOutVol.gaindB;

        wxGetApp().appConfiguration.save(pConfig);
    }
}

float FilterDlg::limit(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

//-------------------------------------------------------------------------
// OnDefault()
//-------------------------------------------------------------------------

void FilterDlg::OnLPCPostFilterDefault(wxCommandEvent& event)
{
    m_beta = CODEC2_LPC_PF_BETA; setBeta();
    m_gamma = CODEC2_LPC_PF_GAMMA; setGamma();
    m_codec2LPCPostFilterEnable->SetValue(true);
    m_codec2LPCPostFilterBassBoost->SetValue(true);
    ExchangeData(EXCHANGE_DATA_OUT);
}

void FilterDlg::OnMicInDefault(wxCommandEvent& event)
{
    m_MicInBass.freqHz = 100.0;
    m_MicInBass.gaindB = 0.0;
    setFreq(&m_MicInBass); setGain(&m_MicInBass);

    m_MicInTreble.freqHz = 3000.0;
    m_MicInTreble.gaindB = 0.0;
    setFreq(&m_MicInTreble); setGain(&m_MicInTreble);

    m_MicInMid.freqHz = 1500.0;
    m_MicInMid.gaindB = 0.0;
    m_MicInMid.Q = 1.0;
    setFreq(&m_MicInMid); setGain(&m_MicInMid); setQ(&m_MicInMid);

    m_MicInVol.gaindB = 0.0;
    setGain(&m_MicInVol);
    
    plotMicInFilterSpectrum();
    adjRunTimeMicInFilter();
}

void FilterDlg::OnSpkOutDefault(wxCommandEvent& event)
{
    m_SpkOutBass.freqHz = 100.0;
    m_SpkOutBass.gaindB = 0.0;
    setFreq(&m_SpkOutBass); setGain(&m_SpkOutBass);

    m_SpkOutTreble.freqHz = 3000.0;
    m_SpkOutTreble.gaindB = 0.0;
    setFreq(&m_SpkOutTreble); setGain(&m_SpkOutTreble);

    m_SpkOutMid.freqHz = 1500.0;
    m_SpkOutMid.gaindB = 0.0;
    m_SpkOutMid.Q = 1.0;
    setFreq(&m_SpkOutMid); setGain(&m_SpkOutMid); setQ(&m_SpkOutMid);

    m_SpkOutVol.gaindB = 0.0;
    setGain(&m_SpkOutVol);
    
    plotSpkOutFilterSpectrum();
    adjRunTimeSpkOutFilter();
}

//-------------------------------------------------------------------------
// OnOK()
//-------------------------------------------------------------------------
void FilterDlg::OnOK(wxCommandEvent& event)
{
    Close();
}

//-------------------------------------------------------------------------
// OnClose()
//-------------------------------------------------------------------------
void FilterDlg::OnClose(wxCloseEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
    
    ((MainFrame*)wxGetApp().GetTopWindow())->m_filterDialog = nullptr;
    Destroy();
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void FilterDlg::OnInitDialog(wxInitDialogEvent& event)
{
    ExchangeData(EXCHANGE_DATA_IN);
}

void FilterDlg::setBeta(void) {
    wxString buf;
    buf.Printf(wxT("%3.2f"), m_beta);
    m_staticTextBeta->SetLabel(buf);
    int slider = (int)(m_beta*SLIDER_MAX_BETA_GAMMA + 0.5);
    m_codec2LPCPostFilterBeta->SetValue(slider);
}

void FilterDlg::setCodec2(void) {
    if (m_running) {
        freedvInterface.setLpcPostFilter(
                                       m_codec2LPCPostFilterEnable->GetValue(),
                                       m_codec2LPCPostFilterBassBoost->GetValue(),
                                       m_beta, m_gamma);
    }
    ExchangeData(EXCHANGE_DATA_OUT);
}

void FilterDlg::setGamma(void) {
    wxString buf;
    buf.Printf(wxT("%3.2f"), m_gamma);
    m_staticTextGamma->SetLabel(buf);
    int slider = (int)(m_gamma*SLIDER_MAX_BETA_GAMMA + 0.5);
    m_codec2LPCPostFilterGamma->SetValue(slider);
}

void FilterDlg::OnEnable(wxScrollEvent& event) {
    setCodec2();
    updateControlState();
}

void FilterDlg::OnBassBoost(wxScrollEvent& event) {
    setCodec2();
    updateControlState();
}

void FilterDlg::OnBetaScroll(wxScrollEvent& event) {
    m_beta = (float)m_codec2LPCPostFilterBeta->GetValue()/SLIDER_MAX_BETA_GAMMA;
    setBeta();
    setCodec2();
}

void FilterDlg::OnGammaScroll(wxScrollEvent& event) {
    m_gamma = (float)m_codec2LPCPostFilterGamma->GetValue()/SLIDER_MAX_BETA_GAMMA;
    setGamma();
    setCodec2();
}

void FilterDlg::OnSpeexppEnable(wxScrollEvent& event) {
    wxGetApp().appConfiguration.filterConfiguration.speexppEnable = m_ckboxSpeexpp->GetValue();
    ExchangeData(EXCHANGE_DATA_OUT);
    updateControlState();
}

void FilterDlg::OnAgcEnable(wxScrollEvent& event) {
    wxGetApp().appConfiguration.filterConfiguration.agcEnabled = m_ckboxAgcEnabled->GetValue();
    g_agcEnabled = wxGetApp().appConfiguration.filterConfiguration.agcEnabled; // forces immediate change at pipeline level
    ExchangeData(EXCHANGE_DATA_OUT);
}

void FilterDlg::On700C_EQ(wxScrollEvent& event) {
    wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer = m_ckbox700C_EQ->GetValue();
    if (m_running) {
        freedvInterface.setEq(wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer);
    }
    ExchangeData(EXCHANGE_DATA_OUT);
}

void FilterDlg::updateControlState()
{
    // AGC currently requires Speex.
    m_ckboxAgcEnabled->Enable(wxGetApp().appConfiguration.filterConfiguration.speexppEnable);
    
    m_MicInBass.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    m_MicInBass.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    
    m_MicInMid.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    m_MicInMid.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    m_MicInMid.sliderQ->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    
    m_MicInTreble.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    m_MicInTreble.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    
    m_MicInVol.sliderGain->Enable(true);
    
    m_MicInDefault->Enable(wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable);
    
    m_SpkOutBass.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    m_SpkOutBass.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    
    m_SpkOutMid.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    m_SpkOutMid.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    m_SpkOutMid.sliderQ->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    
    m_SpkOutTreble.sliderFreq->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    m_SpkOutTreble.sliderGain->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    
    m_SpkOutVol.sliderGain->Enable(true);
    
    m_SpkOutDefault->Enable(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable);
    
    m_codec2LPCPostFilterBeta->Enable(m_codec2LPCPostFilterEnable->GetValue());
    m_codec2LPCPostFilterBassBoost->Enable(m_codec2LPCPostFilterEnable->GetValue());
    m_codec2LPCPostFilterGamma->Enable(m_codec2LPCPostFilterEnable->GetValue());
    m_LPCPostFilterDefault->Enable(m_codec2LPCPostFilterEnable->GetValue());
}

void FilterDlg::OnMicInEnable(wxScrollEvent& event) {
    adjRunTimeMicInFilter();
}

void FilterDlg::OnSpkOutEnable(wxScrollEvent& event) {
    adjRunTimeSpkOutFilter();
}

void FilterDlg::setFreq(EQ *eq)
{
    wxString buf;
    buf.Printf(wxT("%3.0f"), eq->freqHz);
    eq->valueFreq->SetLabel(buf);
    int slider = (int)((eq->freqHz/eq->maxFreqHz)*eq->sliderFreq->GetMax() + 0.5);
    eq->sliderFreq->SetValue(slider);
}

void FilterDlg::sliderToFreq(EQ *eq, bool micIn)
{
    eq->freqHz = ((float)eq->sliderFreq->GetValue()/eq->sliderFreq->GetMax())*eq->maxFreqHz;
    if (eq->freqHz < 1.0) eq->freqHz = 1.0; // sox doesn't like 0 Hz;
    setFreq(eq);
    if (micIn) {
        plotMicInFilterSpectrum();
        adjRunTimeMicInFilter();
    }
    else {
        plotSpkOutFilterSpectrum();
        adjRunTimeSpkOutFilter();
    }
}

void FilterDlg::setGain(EQ *eq)
{
    wxString buf;
    buf.Printf(wxT("%3.1f"), eq->gaindB);
    eq->valueGain->SetLabel(buf);
    int slider = (int)(((eq->gaindB-MIN_GAIN)/(MAX_GAIN-MIN_GAIN))*SLIDER_MAX_GAIN + 0.5);
    eq->sliderGain->SetValue(slider);
}

void FilterDlg::sliderToGain(EQ *eq, bool micIn)
{
    float range = MAX_GAIN-MIN_GAIN;

    eq->gaindB = MIN_GAIN + range*((float)eq->sliderGain->GetValue()/SLIDER_MAX_GAIN);
    setGain(eq);
    if (micIn) {
        plotMicInFilterSpectrum();
        adjRunTimeMicInFilter();
    }
    else {
        plotSpkOutFilterSpectrum();
        adjRunTimeSpkOutFilter();
    }

}

void FilterDlg::setQ(EQ *eq)
{
    wxString buf;
    buf.Printf(wxT("%2.1f"), eq->Q);
    eq->valueQ->SetLabel(buf);

    float log10_range = MAX_LOG10_Q - MIN_LOG10_Q;

    int slider = (int)(((log10(eq->Q+1E-6)-MIN_LOG10_Q)/log10_range)*SLIDER_MAX_Q + 0.5);
    eq->sliderQ->SetValue(slider);
}

void FilterDlg::sliderToQ(EQ *eq, bool micIn)
{
    float log10_range = MAX_LOG10_Q - MIN_LOG10_Q;

    float sliderNorm = (float)eq->sliderQ->GetValue()/SLIDER_MAX_Q;
    float log10Q =  MIN_LOG10_Q + sliderNorm*(log10_range);
    eq->Q = pow(10.0, log10Q);
    setQ(eq);
    if (micIn) {
        plotMicInFilterSpectrum();
        adjRunTimeMicInFilter();
    }
    else {
        plotSpkOutFilterSpectrum();
        adjRunTimeSpkOutFilter();
    }
}

void FilterDlg::plotMicInFilterSpectrum(void) {
    plotFilterSpectrum(&m_MicInBass, &m_MicInMid, &m_MicInTreble, &m_MicInVol, m_MicInFreqRespPlot, m_MicInMagdB);
}

void FilterDlg::plotSpkOutFilterSpectrum(void) {
    plotFilterSpectrum(&m_SpkOutBass, &m_SpkOutMid, &m_SpkOutTreble, &m_SpkOutVol, m_SpkOutFreqRespPlot, m_SpkOutMagdB);
}

void FilterDlg::adjRunTimeMicInFilter(void) {
    // signal an adjustment in running filter coeffs

    g_mutexProtectingCallbackData.Lock();
    ExchangeData(EXCHANGE_DATA_OUT);
    if (m_running) {
        *m_newMicInFilter = true;
    }
    wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable = m_MicInEnable->GetValue();
    g_mutexProtectingCallbackData.Unlock();

    updateControlState();
}

void FilterDlg::adjRunTimeSpkOutFilter(void) {
    // signal an adjustment in running filter coeffs

    g_mutexProtectingCallbackData.Lock();
    ExchangeData(EXCHANGE_DATA_OUT);
    if (m_running) {
        *m_newSpkOutFilter = true;
    }
    wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable = m_SpkOutEnable->GetValue();
    g_mutexProtectingCallbackData.Unlock();

    updateControlState();
}


void FilterDlg::plotFilterSpectrum(EQ *eqBass, EQ *eqMid, EQ *eqTreble, EQ* eqVol, PlotSpectrum* freqRespPlot, float *magdB) {
    const int MAX_ARG_STORAGE_LEN = 80;
    
    char  *argBass[10];
    char  *argTreble[10];
    char  *argMid[10];
    char  *argVol[10];
    char   argstorage[10][MAX_ARG_STORAGE_LEN];
    float magBass[F_MAG_N];
    float magTreble[F_MAG_N];
    float magMid[F_MAG_N];
    float magVol[F_MAG_N];
    int   i;

    for(i=0; i<10; i++) {
        argBass[i] = &argstorage[i][0];
        argTreble[i] = &argstorage[i][0];
        argMid[i] = &argstorage[i][0];
        argVol[i] = &argstorage[i][0];
    }
    snprintf(argBass[0], MAX_ARG_STORAGE_LEN, "bass");
    snprintf(argBass[1], MAX_ARG_STORAGE_LEN, "%f", eqBass->gaindB+1E-6);
    snprintf(argBass[2], MAX_ARG_STORAGE_LEN, "%f", eqBass->freqHz);

    calcFilterSpectrum(magBass, 2, argBass);

    snprintf(argTreble[0], MAX_ARG_STORAGE_LEN, "treble");
    snprintf(argTreble[1], MAX_ARG_STORAGE_LEN, "%f", eqTreble->gaindB+1E-6);
    snprintf(argTreble[2], MAX_ARG_STORAGE_LEN, "%f", eqTreble->freqHz);

    calcFilterSpectrum(magTreble, 2, argTreble);

    snprintf(argMid[0], MAX_ARG_STORAGE_LEN, "equalizer");
    snprintf(argMid[1], MAX_ARG_STORAGE_LEN, "%f", eqMid->freqHz);
    snprintf(argMid[2], MAX_ARG_STORAGE_LEN, "%f", eqMid->Q);
    snprintf(argMid[3], MAX_ARG_STORAGE_LEN, "%f", eqMid->gaindB+1E-6);

    calcFilterSpectrum(magMid, 3, argMid);

    snprintf(argVol[0], MAX_ARG_STORAGE_LEN, "vol");
    snprintf(argVol[1], MAX_ARG_STORAGE_LEN, "%f", eqVol->gaindB);
    snprintf(argVol[2], MAX_ARG_STORAGE_LEN, "%s", "dB");
    snprintf(argVol[3], MAX_ARG_STORAGE_LEN, "%f", 0.05);
    
    calcFilterSpectrum(magVol, 3, argVol);
    
    for(i=0; i<F_MAG_N; i++)
        magdB[i] = magBass[i] + magMid[i] + magTreble[i] + magVol[i];
    freqRespPlot->m_newdata = true;
    freqRespPlot->Refresh();
}

void FilterDlg::calcFilterSpectrum(float magdB[], int argc, char *argv[]) {
    void       *sbq;
    short       in[NIMP];
    short       out[NIMP];
    COMP        X[F_MAG_N];
    float       f, w;
    int         i, k;

    // find impulse response -----------------------------------

    for(i=0; i<NIMP; i++)
        in[i] = 0;
    in[0] = IMP_AMP;

    sbq = sox_biquad_create(argc, (const char **)argv);

    if (sbq != NULL)
    {
        sox_biquad_filter(sbq, out, in, NIMP);
        sox_biquad_destroy(sbq);
    }
    else
    {
        for(i=0; i<NIMP; i++)
            out[i] = 0;
        out[0] = IMP_AMP;
    }
    
    //for(i=0; i<NIMP; i++)
    //    out[i] = 0.0;
    //out[0] = IMP_AMP;

    // calculate discrete time continuous frequency Fourer transform
    // doing this from first principles rather than FFT for no good reason

    for(f=0,i=0; f<MAX_F_HZ; f+=F_STEP_DFT,i++) {
        w = M_PI*f/(FS/2);
        X[i].real = 0.0; X[i].imag = 0.0;
        for(k=0; k<NIMP; k++) {
            X[i].real += ((float)out[k]/IMP_AMP) * cos(w*k);
            X[i].imag -= ((float)out[k]/IMP_AMP) * sin(w*k);
        }
        magdB[i] = 10.0*log10(X[i].real* X[i].real + X[i].imag*X[i].imag + 1E-12);
    }
}
