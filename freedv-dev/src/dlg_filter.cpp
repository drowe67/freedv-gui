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
#include "dlg_filter.h"

#define SLIDER_MAX 100
#define SLIDER_LENGTH 100

#define FILTER_MIN_MAG_DB -20.0
#define FILTER_MAX_MAG_DB  20.0

#define MAX_FREQ_BASS      600.00
#define MAX_FREQ_TREBLE   3900.00
#define MAX_FREQ_DEF      3000.00

#define MIN_GAIN          -20
#define MAX_GAIN           20

#define MAX_LOG10_Q         1.0
#define MIN_LOG10_Q        -1.0 

// DFT parameters

#define IMP_AMP           2000.0                     // amplitude of impulse
#define NIMP              50                         // number of samples in impulse response
#define F_STEP_DFT        10.0                       // frequency steps to sample spectrum
#define F_MAG_N           (int)(MAX_F_HZ/F_STEP_DFT) // number of frequency steps

extern struct freedv      *g_pfreedv;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class FilterDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
FilterDlg::FilterDlg(wxWindow* parent, bool running, bool *newMicInFilter, bool *newSpkOutFilter,
                     wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    m_running = running;
    m_newMicInFilter = newMicInFilter;
    m_newSpkOutFilter = newSpkOutFilter;

    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);

    // LPC Post Filter --------------------------------------------------------

    wxStaticBoxSizer* lpcpfs = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("LPC Post Filter")), wxHORIZONTAL);

    wxBoxSizer* left = new wxBoxSizer(wxVERTICAL);

    m_codec2LPCPostFilterEnable = new wxCheckBox(this, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    left->Add(m_codec2LPCPostFilterEnable);

    m_codec2LPCPostFilterBassBoost = new wxCheckBox(this, wxID_ANY, _("0-1 kHz 3dB Boost"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    left->Add(m_codec2LPCPostFilterBassBoost);
    lpcpfs->Add(left, 0, wxALL, 5);

    newLPCPFControl(&m_codec2LPCPostFilterBeta, &m_staticTextBeta, lpcpfs, "Beta");
    newLPCPFControl(&m_codec2LPCPostFilterGamma, &m_staticTextGamma, lpcpfs, "Gamma");

    m_LPCPostFilterDefault = new wxButton(this, wxID_ANY, wxT("Default"));
    lpcpfs->Add(m_LPCPostFilterDefault, 0, wxALL|wxALIGN_CENTRE_HORIZONTAL|wxALIGN_CENTRE_VERTICAL, 5);

    bSizer30->Add(lpcpfs, 0, wxALL, 0);

    // Speex pre-processor --------------------------------------------------

    wxStaticBoxSizer* sbSizer_speexpp;
    wxStaticBox *sb_speexpp = new wxStaticBox(this, wxID_ANY, _("Speex Mic Audio Pre-Processor"));
    sbSizer_speexpp = new wxStaticBoxSizer(sb_speexpp, wxVERTICAL);

    m_ckboxSpeexpp = new wxCheckBox(this, wxID_ANY, _("Enable"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sb_speexpp->SetToolTip(_("Enable noise supression, dereverberation, AGC of mic signal"));
    sbSizer_speexpp->Add(m_ckboxSpeexpp, wxALIGN_LEFT, 2);

    bSizer30->Add(sbSizer_speexpp, 0, wxALL, 0);   

    // EQ Filters -----------------------------------------------------------

    wxStaticBoxSizer* eqMicInSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Mic In Equaliser")), wxVERTICAL);
    wxBoxSizer* eqMicInSizer1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqMicInSizer2 = new wxBoxSizer(wxHORIZONTAL);

    m_MicInBass   = newEQ(eqMicInSizer1, "Bass"  , MAX_FREQ_BASS, disableQ);
    m_MicInTreble = newEQ(eqMicInSizer1, "Treble", MAX_FREQ_TREBLE, disableQ);
    eqMicInSizer->Add(eqMicInSizer1);

    m_MicInEnable = new wxCheckBox(this, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    eqMicInSizer2->Add(m_MicInEnable,0,wxALIGN_CENTRE_VERTICAL|wxRIGHT,10);
    m_MicInMid    = newEQ(eqMicInSizer2, "Mid"   , MAX_FREQ_DEF, enableQ);
    m_MicInDefault = new wxButton(this, wxID_ANY, wxT("Default"));
    eqMicInSizer2->Add(m_MicInDefault,0,wxALIGN_CENTRE_VERTICAL|wxLEFT,20);
    eqMicInSizer->Add(eqMicInSizer2);

    wxStaticBoxSizer* eqSpkOutSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Speaker Out Equaliser")), wxVERTICAL);
    wxBoxSizer* eqSpkOutSizer1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* eqSpkOutSizer2 = new wxBoxSizer(wxHORIZONTAL);

    m_SpkOutBass   = newEQ(eqSpkOutSizer1, "Bass"  , MAX_FREQ_BASS, disableQ);
    m_SpkOutTreble = newEQ(eqSpkOutSizer1, "Treble", MAX_FREQ_TREBLE, disableQ);
    eqSpkOutSizer->Add(eqSpkOutSizer1);

    m_SpkOutEnable = new wxCheckBox(this, wxID_ANY, _("Enable"), wxDefaultPosition,wxDefaultSize, wxCHK_2STATE);
    eqSpkOutSizer2->Add(m_SpkOutEnable,0,wxALIGN_CENTRE_VERTICAL|wxRIGHT,10);
    m_SpkOutMid    = newEQ(eqSpkOutSizer2, "Mid"   , MAX_FREQ_DEF, enableQ);
    m_SpkOutDefault = new wxButton(this, wxID_ANY, wxT("Default"));
    eqSpkOutSizer2->Add(m_SpkOutDefault,0,wxALIGN_CENTRE_VERTICAL|wxLEFT,20);
    eqSpkOutSizer->Add(eqSpkOutSizer2);
    
    bSizer30->Add(eqMicInSizer, 0, wxALL, 0);
    bSizer30->Add(eqSpkOutSizer, 0, wxALL, 0);

    // Storgage for spectrum magnitude plots ------------------------------------

    m_MicInMagdB = new float[F_MAG_N];
    for(int i=0; i<F_MAG_N; i++)
        m_MicInMagdB[i] = 0.0;

    m_SpkOutMagdB = new float[F_MAG_N];
    for(int i=0; i<F_MAG_N; i++)
        m_SpkOutMagdB[i] = 0.0;

    // Spectrum Plots -----------------------------------------------------------

    long nb_style = wxAUI_NB_BOTTOM | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
    m_auiNotebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxSize(-1,200), nb_style);
    m_auiNotebook->SetFont(wxFont(8, 70, 90, 90, false, wxEmptyString));

    bSizer30->Add(m_auiNotebook, 0, wxEXPAND|wxALL, 3);
    
    m_MicInFreqRespPlot = new PlotSpectrum((wxFrame*) m_auiNotebook, m_MicInMagdB, F_MAG_N, FILTER_MIN_MAG_DB, FILTER_MAX_MAG_DB);
    m_auiNotebook->AddPage(m_MicInFreqRespPlot, _("Microphone In Equaliser"));

    m_SpkOutFreqRespPlot = new PlotSpectrum((wxFrame*)m_auiNotebook, m_SpkOutMagdB, F_MAG_N, FILTER_MIN_MAG_DB, FILTER_MAX_MAG_DB);
    m_auiNotebook->AddPage(m_SpkOutFreqRespPlot, _("Speaker Out Equaliser"));

    //  OK - Cancel buttons at the bottom --------------------------

    wxBoxSizer* bSizer31 = new wxBoxSizer(wxHORIZONTAL);

    m_sdbSizer5OK = new wxButton(this, wxID_OK);
    bSizer31->Add(m_sdbSizer5OK, 0, wxALL, 2);

    m_sdbSizer5Cancel = new wxButton(this, wxID_CANCEL);
    bSizer31->Add(m_sdbSizer5Cancel, 0, wxALL, 2);

    bSizer30->Add(bSizer31, 0, wxALIGN_RIGHT|wxALL, 0);

    this->SetSizer(bSizer30);
    this->Layout();

    this->Centre(wxBOTH);
 
    // Connect Events -------------------------------------------------------

    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FilterDlg::OnInitDialog));

    m_codec2LPCPostFilterEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnEnable), NULL, this);
    m_codec2LPCPostFilterBassBoost->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnBassBoost), NULL, this);
    m_codec2LPCPostFilterBeta->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnBetaScroll), NULL, this);
    m_codec2LPCPostFilterGamma->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnGammaScroll), NULL, this);
    m_LPCPostFilterDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnLPCPostFilterDefault), NULL, this);

    m_ckboxSpeexpp->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpeexppEnable), NULL, this);

    m_MicInBass.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInBassFreqScroll), NULL, this);
    m_MicInBass.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInBassGainScroll), NULL, this);
    m_MicInTreble.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInTrebleFreqScroll), NULL, this);
    m_MicInTreble.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInTrebleGainScroll), NULL, this);
    m_MicInMid.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidFreqScroll), NULL, this);
    m_MicInMid.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidGainScroll), NULL, this);
    m_MicInMid.sliderQ->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidQScroll), NULL, this);
    m_MicInEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnMicInEnable), NULL, this);
    m_MicInDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnMicInDefault), NULL, this);

    m_SpkOutBass.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutBassFreqScroll), NULL, this);
    m_SpkOutBass.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutBassGainScroll), NULL, this);
    m_SpkOutTreble.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleFreqScroll), NULL, this);
    m_SpkOutTreble.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleGainScroll), NULL, this);
    m_SpkOutMid.sliderFreq->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidFreqScroll), NULL, this);
    m_SpkOutMid.sliderGain->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidGainScroll), NULL, this);
    m_SpkOutMid.sliderQ->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidQScroll), NULL, this);
    m_SpkOutEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpkOutEnable), NULL, this);
    m_SpkOutDefault->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnSpkOutDefault), NULL, this);

    m_sdbSizer5Cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnCancel), NULL, this);
    m_sdbSizer5OK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnOK), NULL, this);

}

//-------------------------------------------------------------------------
// ~FilterDlg()
//-------------------------------------------------------------------------
FilterDlg::~FilterDlg()
{
    delete m_MicInMagdB;
    delete m_SpkOutMagdB;

    // Disconnect Events

    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FilterDlg::OnInitDialog));

    m_codec2LPCPostFilterEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnEnable), NULL, this);
    m_codec2LPCPostFilterBassBoost->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnBassBoost), NULL, this);
    m_codec2LPCPostFilterBeta->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnBetaScroll), NULL, this);
    m_codec2LPCPostFilterGamma->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnGammaScroll), NULL, this);
    m_LPCPostFilterDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnLPCPostFilterDefault), NULL, this);

    m_MicInBass.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInBassFreqScroll), NULL, this);
    m_MicInBass.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInBassGainScroll), NULL, this);
    m_MicInTreble.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInTrebleFreqScroll), NULL, this);
    m_MicInTreble.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInTrebleGainScroll), NULL, this);
    m_MicInMid.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidFreqScroll), NULL, this);
    m_MicInMid.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidGainScroll), NULL, this);
    m_MicInMid.sliderQ->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnMicInMidQScroll), NULL, this);
    m_MicInEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnMicInEnable), NULL, this);
    m_MicInDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnMicInDefault), NULL, this);

    m_SpkOutBass.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutBassFreqScroll), NULL, this);
    m_SpkOutBass.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutBassGainScroll), NULL, this);
    m_SpkOutTreble.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleFreqScroll), NULL, this);
    m_SpkOutTreble.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutTrebleGainScroll), NULL, this);
    m_SpkOutMid.sliderFreq->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidFreqScroll), NULL, this);
    m_SpkOutMid.sliderGain->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidGainScroll), NULL, this);
    m_SpkOutMid.sliderQ->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(FilterDlg::OnSpkOutMidQScroll), NULL, this);
    m_SpkOutEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(FilterDlg::OnSpkOutEnable), NULL, this);
    m_SpkOutDefault->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnSpkOutDefault), NULL, this);

    m_sdbSizer5Cancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnCancel), NULL, this);
    m_sdbSizer5OK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FilterDlg::OnOK), NULL, this);
}

void FilterDlg::newLPCPFControl(wxSlider **slider, wxStaticText **stValue, wxSizer *s, wxString controlName)
{
    wxBoxSizer *bs = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* st = new wxStaticText(this, wxID_ANY, controlName, wxDefaultPosition, wxSize(70,-1), wxALIGN_RIGHT);
    bs->Add(st, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 2);

    *slider = new wxSlider(this, wxID_ANY, 0, 0, SLIDER_MAX, wxDefaultPosition, wxSize(SLIDER_LENGTH,wxDefaultCoord));
    bs->Add(*slider, 1, wxALIGN_CENTER_VERTICAL|wxALL, 2);

    *stValue = new wxStaticText(this, wxID_ANY, wxT("0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    bs->Add(*stValue, 1, wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT|wxALL, 2);

    s->Add(bs, 0);
}

void FilterDlg::newEQControl(wxSlider** slider, wxStaticText** value, wxStaticBoxSizer *bs, wxString controlName)
{
    wxStaticText* label = new wxStaticText(this, wxID_ANY, controlName, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    bs->Add(label, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 0);

    *slider = new wxSlider(this, wxID_ANY, 0, 0, SLIDER_MAX, wxDefaultPosition, wxSize(SLIDER_LENGTH,wxDefaultCoord));
    bs->Add(*slider, 1, wxALIGN_CENTER_VERTICAL|wxALL, 0);

    *value = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(40,-1), wxALIGN_LEFT);
    bs->Add(*value, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxRIGHT, 5);
}

EQ FilterDlg::newEQ(wxSizer *bs, wxString eqName, float maxFreqHz, bool enableQ)
{
    EQ eq;

    wxStaticBoxSizer *bsEQ = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, eqName), wxHORIZONTAL);

    newEQControl(&eq.sliderFreq, &eq.valueFreq, bsEQ, "Freq"); 
    eq.maxFreqHz = maxFreqHz; 
    eq.sliderFreqId = eq.sliderFreq->GetId();

    newEQControl(&eq.sliderGain, &eq.valueGain, bsEQ, "Gain");
    if (enableQ)
        newEQControl(&eq.sliderQ, &eq.valueQ, bsEQ, "Q");
    else
        eq.sliderQ = NULL;

    bs->Add(bsEQ);

    return eq;
}

//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void FilterDlg::ExchangeData(int inout, bool storePersistent)
{
    wxConfigBase *pConfig = wxConfigBase::Get();
    if(inout == EXCHANGE_DATA_IN)
    {
        // LPC Post filter

        m_codec2LPCPostFilterEnable->SetValue(wxGetApp().m_codec2LPCPostFilterEnable);
        m_codec2LPCPostFilterBassBoost->SetValue(wxGetApp().m_codec2LPCPostFilterBassBoost);
        m_beta = wxGetApp().m_codec2LPCPostFilterBeta; setBeta();
        m_gamma = wxGetApp().m_codec2LPCPostFilterGamma; setGamma();

        // Speex Pre-Processor

        m_ckboxSpeexpp->SetValue(wxGetApp().m_speexpp_enable);

        // Mic In Equaliser

        m_MicInBass.freqHz = wxGetApp().m_MicInBassFreqHz; 
        m_MicInBass.freqHz = limit(m_MicInBass.freqHz, 1.0, MAX_FREQ_BASS);
        setFreq(&m_MicInBass);
        m_MicInBass.gaindB = wxGetApp().m_MicInBassGaindB;
        m_MicInBass.gaindB = limit(m_MicInBass.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInBass);

        m_MicInTreble.freqHz = wxGetApp().m_MicInTrebleFreqHz;
        m_MicInTreble.freqHz = limit(m_MicInTreble.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_MicInTreble);
        m_MicInTreble.gaindB = wxGetApp().m_MicInTrebleGaindB; 
        m_MicInTreble.gaindB = limit(m_MicInTreble.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInTreble);

        m_MicInMid.freqHz = wxGetApp().m_MicInMidFreqHz; 
        m_MicInMid.freqHz = limit(m_MicInMid.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_MicInMid);
        m_MicInMid.gaindB = wxGetApp().m_MicInMidGaindB; 
        m_MicInMid.gaindB = limit(m_MicInMid.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_MicInMid);
        m_MicInMid.Q = wxGetApp().m_MicInMidQ;
        m_MicInMid.Q = limit(m_MicInMid.Q, pow(10.0,MIN_LOG10_Q), pow(10.0, MAX_LOG10_Q));
        setQ(&m_MicInMid);

        m_MicInEnable->SetValue(wxGetApp().m_MicInEQEnable);

        plotMicInFilterSpectrum();
 
        // Spk Out Equaliser

        m_SpkOutBass.freqHz = wxGetApp().m_SpkOutBassFreqHz;
        m_SpkOutBass.freqHz = limit(m_SpkOutBass.freqHz, 1.0, MAX_FREQ_BASS);
        setFreq(&m_SpkOutBass);
        m_SpkOutBass.gaindB = wxGetApp().m_SpkOutBassGaindB; 
        m_SpkOutBass.gaindB = limit(m_SpkOutBass.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutBass);

        m_SpkOutTreble.freqHz = wxGetApp().m_SpkOutTrebleFreqHz; 
        m_SpkOutTreble.freqHz = limit(m_SpkOutTreble.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_SpkOutTreble);
        m_SpkOutTreble.gaindB = wxGetApp().m_SpkOutTrebleGaindB; 
        m_SpkOutTreble.gaindB = limit(m_SpkOutTreble.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutTreble);

        m_SpkOutMid.freqHz = wxGetApp().m_SpkOutMidFreqHz;
        m_SpkOutMid.freqHz = limit(m_SpkOutMid.freqHz, 1.0, MAX_FREQ_TREBLE);
        setFreq(&m_SpkOutMid);
        m_SpkOutMid.gaindB = wxGetApp().m_SpkOutMidGaindB;
        m_SpkOutMid.gaindB = limit(m_SpkOutMid.gaindB, MIN_GAIN, MAX_GAIN);
        setGain(&m_SpkOutMid);
        m_SpkOutMid.Q = wxGetApp().m_SpkOutMidQ;
        m_SpkOutMid.Q = limit(m_SpkOutMid.Q, pow(10.0,MIN_LOG10_Q), pow(10.0, MAX_LOG10_Q));
        setQ(&m_SpkOutMid);

        m_SpkOutEnable->SetValue(wxGetApp().m_SpkOutEQEnable);

        plotSpkOutFilterSpectrum();
    }
    if(inout == EXCHANGE_DATA_OUT)
    {
        // LPC Post filter

        wxGetApp().m_codec2LPCPostFilterEnable     = m_codec2LPCPostFilterEnable->GetValue();
        wxGetApp().m_codec2LPCPostFilterBassBoost  = m_codec2LPCPostFilterBassBoost->GetValue();
        wxGetApp().m_codec2LPCPostFilterBeta       = m_beta;
        wxGetApp().m_codec2LPCPostFilterGamma      = m_gamma;

        // Speex Pre-Processor

        wxGetApp().m_speexpp_enable = m_ckboxSpeexpp->GetValue();

        // Mic In Equaliser

        wxGetApp().m_MicInBassFreqHz = m_MicInBass.freqHz;
        wxGetApp().m_MicInBassGaindB = m_MicInBass.gaindB;

        wxGetApp().m_MicInTrebleFreqHz = m_MicInTreble.freqHz;
        wxGetApp().m_MicInTrebleGaindB = m_MicInTreble.gaindB;

        wxGetApp().m_MicInMidFreqHz = m_MicInMid.freqHz;
        wxGetApp().m_MicInMidGaindB = m_MicInMid.gaindB;
        wxGetApp().m_MicInMidQ = m_MicInMid.Q;

        // Spk Out Equaliser

        wxGetApp().m_SpkOutBassFreqHz = m_SpkOutBass.freqHz;
        wxGetApp().m_SpkOutBassGaindB = m_SpkOutBass.gaindB;

        wxGetApp().m_SpkOutTrebleFreqHz = m_SpkOutTreble.freqHz;
        wxGetApp().m_SpkOutTrebleGaindB = m_SpkOutTreble.gaindB;

        wxGetApp().m_SpkOutMidFreqHz = m_SpkOutMid.freqHz;
        wxGetApp().m_SpkOutMidGaindB = m_SpkOutMid.gaindB;
        wxGetApp().m_SpkOutMidQ = m_SpkOutMid.Q;

        if (storePersistent) {
            pConfig->Write(wxT("/Filter/codec2LPCPostFilterEnable"),     wxGetApp().m_codec2LPCPostFilterEnable);
            pConfig->Write(wxT("/Filter/codec2LPCPostFilterBassBoost"),  wxGetApp().m_codec2LPCPostFilterBassBoost);
            pConfig->Write(wxT("/Filter/codec2LPCPostFilterBeta"),       (int)(m_beta*100.0));
            pConfig->Write(wxT("/Filter/codec2LPCPostFilterGamma"),      (int)(m_gamma*100.0));

            pConfig->Write(wxT("/Filter/speexpp_enable"),                wxGetApp().m_speexpp_enable);

            pConfig->Write(wxT("/Filter/MicInBassFreqHz"), (int)m_MicInBass.freqHz);
            pConfig->Write(wxT("/Filter/MicInBassGaindB"), (int)(10.0*m_MicInBass.gaindB));
            pConfig->Write(wxT("/Filter/MicInTrebleFreqHz"), (int)m_MicInTreble.freqHz);
            pConfig->Write(wxT("/Filter/MicInTrebleGaindB"), (int)(10.0*m_MicInTreble.gaindB));
            pConfig->Write(wxT("/Filter/MicInMidFreqHz"), (int)m_MicInMid.freqHz);
            pConfig->Write(wxT("/Filter/MicInMidGaindB"), (int)(10.0*m_MicInMid.gaindB));
            pConfig->Write(wxT("/Filter/MicInMidQ"), (int)(100.0*m_MicInMid.Q));

            pConfig->Write(wxT("/Filter/SpkOutBassFreqHz"), (int)m_SpkOutBass.freqHz);
            pConfig->Write(wxT("/Filter/SpkOutBassGaindB"), (int)(10.0*m_SpkOutBass.gaindB));
            pConfig->Write(wxT("/Filter/SpkOutTrebleFreqHz"), (int)m_SpkOutTreble.freqHz);
            pConfig->Write(wxT("/Filter/SpkOutTrebleGaindB"), (int)(10.0*m_SpkOutTreble.gaindB));
            pConfig->Write(wxT("/Filter/SpkOutMidQ"), (int)(100.0*m_SpkOutMid.Q));
            pConfig->Write(wxT("/Filter/SpkOutMidFreqHz"), (int)m_SpkOutMid.freqHz);
            pConfig->Write(wxT("/Filter/SpkOutMidGaindB"), (int)(10.0*m_SpkOutMid.gaindB));

            pConfig->Flush();
        }
    }
    delete wxConfigBase::Set((wxConfigBase *) NULL);
}

float FilterDlg::limit(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void FilterDlg::OnCancel(wxCommandEvent& event)
{
    this->EndModal(wxID_CANCEL);
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

    plotMicInFilterSpectrum();    
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

    plotSpkOutFilterSpectrum();    
}

//-------------------------------------------------------------------------
// OnOK()
//-------------------------------------------------------------------------
void FilterDlg::OnOK(wxCommandEvent& event)
{
    //printf("FilterDlg::OnOK\n");
    ExchangeData(EXCHANGE_DATA_OUT, true);
    this->EndModal(wxID_OK);
}

//-------------------------------------------------------------------------
// OnClose()
//-------------------------------------------------------------------------
void FilterDlg::OnClose(wxCloseEvent& event)
{
    this->EndModal(wxID_OK);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void FilterDlg::OnInitDialog(wxInitDialogEvent& event)
{
    //printf("FilterDlg::OnInitDialog\n");
    ExchangeData(EXCHANGE_DATA_IN, false);
    //printf("m_beta: %f\n", m_beta);
}

void FilterDlg::setBeta(void) {
    wxString buf;
    buf.Printf(wxT("%3.2f"), m_beta);
    m_staticTextBeta->SetLabel(buf);
    int slider = (int)(m_beta*SLIDER_MAX + 0.5);
    m_codec2LPCPostFilterBeta->SetValue(slider);
}

void FilterDlg::setCodec2(void) {
    if (m_running) {
        codec2_set_lpc_post_filter(freedv_get_codec2(g_pfreedv), 
                               m_codec2LPCPostFilterEnable->GetValue(), 
                               m_codec2LPCPostFilterBassBoost->GetValue(), 
                               m_beta, m_gamma);
    }
}

void FilterDlg::setGamma(void) {
    wxString buf;
    buf.Printf(wxT("%3.2f"), m_gamma);
    m_staticTextGamma->SetLabel(buf);
    int slider = (int)(m_gamma*SLIDER_MAX + 0.5);
    m_codec2LPCPostFilterGamma->SetValue(slider);
}

void FilterDlg::OnEnable(wxScrollEvent& event) {
    setCodec2();    
}

void FilterDlg::OnBassBoost(wxScrollEvent& event) {
    setCodec2();    
}

void FilterDlg::OnBetaScroll(wxScrollEvent& event) {
    m_beta = (float)m_codec2LPCPostFilterBeta->GetValue()/SLIDER_MAX;
    setBeta();
    setCodec2();
}

void FilterDlg::OnGammaScroll(wxScrollEvent& event) {
    m_gamma = (float)m_codec2LPCPostFilterGamma->GetValue()/SLIDER_MAX;
    setGamma();
    setCodec2();
}

// immediately change enable flags rather using ExchangeData() so we can switch on and off at run time

void FilterDlg::OnSpeexppEnable(wxScrollEvent& event) {
    wxGetApp().m_speexpp_enable = m_ckboxSpeexpp->GetValue();
}

void FilterDlg::OnMicInEnable(wxScrollEvent& event) {
    wxGetApp().m_MicInEQEnable = m_MicInEnable->GetValue();
}

void FilterDlg::OnSpkOutEnable(wxScrollEvent& event) {
    wxGetApp().m_SpkOutEQEnable = m_SpkOutEnable->GetValue();
    //printf("wxGetApp().m_SpkOutEQEnable: %d\n", wxGetApp().m_SpkOutEQEnable);
}

void FilterDlg::setFreq(EQ *eq)
{
    wxString buf;
    buf.Printf(wxT("%3.0f"), eq->freqHz);
    eq->valueFreq->SetLabel(buf);
    int slider = (int)((eq->freqHz/eq->maxFreqHz)*SLIDER_MAX + 0.5);
    eq->sliderFreq->SetValue(slider);
}

void FilterDlg::sliderToFreq(EQ *eq, bool micIn)
{
    eq->freqHz = ((float)eq->sliderFreq->GetValue()/SLIDER_MAX)*eq->maxFreqHz;
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
    int slider = (int)(((eq->gaindB-MIN_GAIN)/(MAX_GAIN-MIN_GAIN))*SLIDER_MAX + 0.5);
    eq->sliderGain->SetValue(slider);
}

void FilterDlg::sliderToGain(EQ *eq, bool micIn)
{
    float range = MAX_GAIN-MIN_GAIN;
    
    eq->gaindB = MIN_GAIN + range*((float)eq->sliderGain->GetValue()/SLIDER_MAX);
    //printf("gaindB: %f\n", eq->gaindB);
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

    int slider = (int)(((log10(eq->Q+1E-6)-MIN_LOG10_Q)/log10_range)*SLIDER_MAX + 0.5);
    eq->sliderQ->SetValue(slider);
}

void FilterDlg::sliderToQ(EQ *eq, bool micIn)
{
    float log10_range = MAX_LOG10_Q - MIN_LOG10_Q;
    
    float sliderNorm = (float)eq->sliderQ->GetValue()/SLIDER_MAX;
    float log10Q =  MIN_LOG10_Q + sliderNorm*(log10_range);
    eq->Q = pow(10.0, log10Q);
    //printf("log10Q: %f eq->Q: %f\n", log10Q, eq->Q);
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
    plotFilterSpectrum(&m_MicInBass, &m_MicInMid, &m_MicInTreble, m_MicInFreqRespPlot, m_MicInMagdB);
}

void FilterDlg::plotSpkOutFilterSpectrum(void) {
    plotFilterSpectrum(&m_SpkOutBass, &m_SpkOutMid, &m_SpkOutTreble, m_SpkOutFreqRespPlot, m_SpkOutMagdB);
}

void FilterDlg::adjRunTimeMicInFilter(void) {
    // signal an adjustment in running filter coeffs

    if (m_running) {
        ExchangeData(EXCHANGE_DATA_OUT, false);
        *m_newMicInFilter = true;
    }
}
        
void FilterDlg::adjRunTimeSpkOutFilter(void) {
    // signal an adjustment in running filter coeffs

    if (m_running) {
        ExchangeData(EXCHANGE_DATA_OUT, false);
        *m_newSpkOutFilter = true;
    }
}
        

void FilterDlg::plotFilterSpectrum(EQ *eqBass, EQ *eqMid, EQ *eqTreble, PlotSpectrum* freqRespPlot, float *magdB) {
    char  *argBass[10];
    char  *argTreble[10];
    char  *argMid[10];
    char   argstorage[10][80];
    float magBass[F_MAG_N];
    float magTreble[F_MAG_N];
    float magMid[F_MAG_N];
    int   i;

    for(i=0; i<10; i++) {
        argBass[i] = &argstorage[i][0];
        argTreble[i] = &argstorage[i][0];
        argMid[i] = &argstorage[i][0];
    }
    sprintf(argBass[0], "bass");                
    sprintf(argBass[1], "%f", eqBass->gaindB+1E-6);
    sprintf(argBass[2], "%f", eqBass->freqHz);      

    calcFilterSpectrum(magBass, 2, argBass);

    sprintf(argTreble[0], "treble");                
    sprintf(argTreble[1], "%f", eqTreble->gaindB+1E-6);
    sprintf(argTreble[2], "%f", eqTreble->freqHz);      

    calcFilterSpectrum(magTreble, 2, argTreble);

    sprintf(argTreble[0], "equalizer");                
    sprintf(argTreble[1], "%f", eqMid->freqHz);      
    sprintf(argTreble[2], "%f", eqMid->Q);      
    sprintf(argTreble[3], "%f", eqMid->gaindB+1E-6);

    calcFilterSpectrum(magMid, 3, argMid);

    for(i=0; i<F_MAG_N; i++)
        magdB[i] = magBass[i] + magMid[i] + magTreble[i];
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

    //printf("argv[0]: %s argv[1]: %s\n", argv[0], argv[1]);
    sbq = sox_biquad_create(argc, (const char **)argv);

    sox_biquad_filter(sbq, out, in, NIMP);
    //for(i=0; i<NIMP; i++)
    //    printf("%d\n", out[i]);
   
    sox_biquad_destroy(sbq);

    //for(i=0; i<NIMP; i++)
    //    out[i] = 0.0;
    //out[0] = IMP_AMP;

    // calculate discrete time continous frequency Fourer transform
    // doing this from first principles rather than FFT for no good reason

    for(f=0,i=0; f<MAX_F_HZ; f+=F_STEP_DFT,i++) {
        w = M_PI*f/(FS/2);
        X[i].real = 0.0; X[i].imag = 0.0;
        for(k=0; k<NIMP; k++) {
            X[i].real += ((float)out[k]/IMP_AMP) * cos(w*k);
            X[i].imag -= ((float)out[k]/IMP_AMP) * sin(w*k);
        }
        magdB[i] = 10.0*log10(X[i].real* X[i].real + X[i].imag*X[i].imag + 1E-12);
        //printf("f: %f X[%d] = %f %f magdB = %f\n", f, i, X[i].real, X[i].imag,  magdB[i]);
    }
}
