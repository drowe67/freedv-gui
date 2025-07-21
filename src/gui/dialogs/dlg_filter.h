//==========================================================================
// Name:            dlg_filter.h
// Purpose:         Dialog for controlling Codec audio filtering
// Created:         Nov 25 2012
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

#ifndef __FILTER_DIALOG__
#define __FILTER_DIALOG__

#include "main.h"

enum {disableQ = false, enableQ = true, disableFreq = false, enableFreq = true};

typedef struct { 
    wxStaticBox  *eqBox;
    wxSlider     *sliderFreq;
    wxStaticText *valueFreq;
    wxSlider     *sliderGain;
    wxStaticText *valueGain;
    wxSlider     *sliderQ;
    wxStaticText *valueQ;

    int       sliderFreqId;
    int       sliderGainId;
    int       sliderQId;
    
    float     freqHz;
    float     gaindB;
    float     Q;

    float     maxFreqHz;
} EQ;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class FilterDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class FilterDlg : public wxDialog
{
    public:
    FilterDlg( wxWindow* parent, bool running, bool *newMicInFilter, bool *newSpkOutFilter,
               wxWindowID id = wxID_ANY, const wxString& title = _("Filter"), 
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER );
        ~FilterDlg();

        void    ExchangeData(int inout);
        
        void syncVolumes();

    protected:
        // Handlers for events.
        void    OnOK(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
        void    OnLPCPostFilterDefault(wxCommandEvent& event);

        void    OnBetaScroll(wxScrollEvent& event);
        void    OnGammaScroll(wxScrollEvent& event);
        void    OnEnable(wxScrollEvent& event);
        void    OnBassBoost(wxScrollEvent& event);

        void    OnSpeexppEnable(wxScrollEvent& event);
        void    On700C_EQ(wxScrollEvent& event);

        void    OnMicInBassFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_MicInBass, true); }
        void    OnMicInBassGainScroll(wxScrollEvent& event) { sliderToGain(&m_MicInBass, true); }
        void    OnMicInTrebleFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_MicInTreble, true); }
        void    OnMicInTrebleGainScroll(wxScrollEvent& event) { sliderToGain(&m_MicInTreble, true); }
        void    OnMicInMidFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_MicInMid, true); }
        void    OnMicInMidGainScroll(wxScrollEvent& event) { sliderToGain(&m_MicInMid, true); }
        void    OnMicInMidQScroll(wxScrollEvent& event) { sliderToQ(&m_MicInMid, true); }
        void    OnMicInVolGainScroll(wxScrollEvent& event) { sliderToGain(&m_MicInVol, true); }
        
        void    OnMicInEnable(wxScrollEvent& event);
        void    OnMicInDefault(wxCommandEvent& event);

        void    OnSpkOutBassFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_SpkOutBass, false); }
        void    OnSpkOutBassGainScroll(wxScrollEvent& event) { sliderToGain(&m_SpkOutBass, false); }
        void    OnSpkOutTrebleFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_SpkOutTreble, false); }
        void    OnSpkOutTrebleGainScroll(wxScrollEvent& event) { sliderToGain(&m_SpkOutTreble, false); }
        void    OnSpkOutMidFreqScroll(wxScrollEvent& event) { sliderToFreq(&m_SpkOutMid, false); }
        void    OnSpkOutMidGainScroll(wxScrollEvent& event) { sliderToGain(&m_SpkOutMid, false); }
        void    OnSpkOutMidQScroll(wxScrollEvent& event) { sliderToQ(&m_SpkOutMid, false); }
        void    OnSpkOutVolGainScroll(wxScrollEvent& event) { sliderToGain(&m_SpkOutVol, false); }
        
        void    OnSpkOutEnable(wxScrollEvent& event);
        void    OnSpkOutDefault(wxCommandEvent& event);

        wxStaticText* m_staticText8;
        wxCheckBox*   m_codec2LPCPostFilterEnable;
        wxStaticText* m_staticText9;
        wxCheckBox*   m_codec2LPCPostFilterBassBoost;
        wxStaticText* m_staticText91;
        wxSlider*     m_codec2LPCPostFilterBeta;
        wxStaticText* m_staticTextBeta;
        wxStaticText* m_staticText911;
        wxSlider*     m_codec2LPCPostFilterGamma;
        wxStaticText* m_staticTextGamma;
        wxButton*     m_LPCPostFilterDefault;

        wxCheckBox*   m_ckboxSpeexpp;
        wxCheckBox*   m_ckbox700C_EQ;
        
        wxStdDialogButtonSizer* m_sdbSizer5;
        wxButton*     m_sdbSizer5OK;
        PlotSpectrum* m_MicInFreqRespPlot;
        PlotSpectrum* m_SpkOutFreqRespPlot;
        
        wxCheckBox*   m_MicInEnable;
        wxButton*     m_MicInDefault;
        wxCheckBox*   m_SpkOutEnable;
        wxButton*     m_SpkOutDefault;

        float        *m_MicInMagdB;
        float        *m_SpkOutMagdB;

     private:
        bool          m_running;
        float         m_beta;
        float         m_gamma;

        void          setBeta(void);  // sets slider and static text from m_beta
        void          setGamma(void); // sets slider and static text from m_gamma
        void          setCodec2(void);
 
        void          newEQControl(wxWindow* parent, wxSlider** slider, wxStaticText** value, wxSizer *sizer, wxString controlName, int max);
        EQ            newEQ(wxWindow* parent, wxSizer *bs, wxString eqName, float maxFreqHz, bool enableQ, bool enableFreq, int maxSliderBass);
        void          newLPCPFControl(wxSlider **slider, wxStaticText **stValue, wxWindow* parent, wxSizer *sbs, wxString controlName);
        wxNotebook    *m_auiNotebook;
        void          setFreq(EQ *eq);
        void          setGain(EQ *eq);
        void          setQ(EQ *eq);
        void          sliderToFreq(EQ *eq, bool micIn);
        void          sliderToGain(EQ *eq, bool micIn);
        void          sliderToQ(EQ *eq, bool micIn);
        void          plotFilterSpectrum(EQ *eqBass, EQ *eqMid, EQ* eqTreble, EQ* eqVol, PlotSpectrum* freqRespPlot, float *magdB);
        void          calcFilterSpectrum(float magdB[], int arc, char *argv[]);
        void          plotMicInFilterSpectrum(void);
        void          plotSpkOutFilterSpectrum(void);
        void          adjRunTimeMicInFilter(void);
        void          adjRunTimeSpkOutFilter(void);

        EQ            m_MicInBass;
        EQ            m_MicInMid;
        EQ            m_MicInTreble;
        EQ            m_MicInVol;

        EQ            m_SpkOutBass;
        EQ            m_SpkOutMid;
        EQ            m_SpkOutTreble;
        EQ            m_SpkOutVol;

        float         limit(float value, float min, float max);
 
        bool          *m_newMicInFilter;
        bool          *m_newSpkOutFilter;
private:
        void updateControlState();
};

#endif // __FILTER_DIALOG__
