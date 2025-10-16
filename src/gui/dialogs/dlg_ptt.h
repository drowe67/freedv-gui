//==========================================================================
// Name:            dlg_ptt.h
// Purpose:         Subclasses dialog GUI for PTT Config.
//                  
// Created:         May. 11, 2012
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
#ifndef __COMPORTS_DIALOG__
#define __COMPORTS_DIALOG__

#include "main.h"
#include <wx/settings.h>
#include <wx/xrc/xmlres.h>
#include <wx/xrc/xh_bmp.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/spinctrl.h>
#include <wx/propgrid/property.h>
#include <wx/propgrid/props.h>


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class ComPortsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class ComPortsDlg : public wxDialog
{
     public:
        ComPortsDlg(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("CAT and PTT Config"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(450,300), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
        virtual ~ComPortsDlg();
        void    ExchangeData(int inout);

    protected:
        wxCheckBox* m_ckLeftChannelVoxTone;

        /* Hamlib settings.*/

        wxCheckBox *m_ckUseHamlibPTT;
        wxComboBox *m_cbRigName;
        wxComboBox *m_cbSerialPort;
        wxComboBox *m_cbPttSerialPort;
        wxComboBox *m_cbSerialRate;
        wxStaticText *m_stIcomCIVHex;
        wxTextCtrl *m_tcIcomCIVHex;
        wxComboBox *m_cbPttMethod;

        /* Serial Settings */

        wxCheckBox    *m_ckUseSerialPTT;
        wxStaticText  *m_staticText12;
        wxComboBox    *m_cbCtlDevicePath;
        wxRadioButton *m_rbUseDTR;
        wxCheckBox    *m_ckRTSPos;
        wxRadioButton *m_rbUseRTS;
        wxCheckBox    *m_ckDTRPos;
        
        /* PTT In Settings */
        wxCheckBox    *m_ckUsePTTInput;
        wxStaticText  *m_pttInSerialDeviceLabel;
        wxCheckBox    *m_ckCTSPos;
        wxComboBox    *m_cbCtlDevicePathPttIn;

#if defined(WIN32)
        wxCheckBox    *m_ckUseOmniRig;
        wxComboBox    *m_cbOmniRigRigId;
#endif // defined(WIN32)
        
        /* Test - Ok - Cancel - Apply */

        wxButton* m_buttonTest;
        wxButton* m_buttonOK;
        wxButton* m_buttonCancel;
        wxButton* m_buttonApply;


protected:
        void populatePortList();
        void populateBaudRateList(int min = 0, int max = 0);

        void PTTUseHamLibClicked(wxCommandEvent& event);
        void PTTUseSerialClicked(wxCommandEvent& event);
        
#if defined(WIN32)
        void PTTUseOmniRigClicked(wxCommandEvent& event);
#endif // deinfed(WIN32)
        
        void PTTUseSerialInputClicked(wxCommandEvent& event);
        void HamlibRigNameChanged(wxCommandEvent& event);
        void OnHamlibSerialPortChanged(wxCommandEvent& event);
        void OnHamlibPttMethodChanged(wxCommandEvent& event);
        void resetIcomCIVStatus();
        bool savePttSettings();
        
        void OnTest(wxCommandEvent& event);

        void OnOK(wxCommandEvent& event);
        void OnCancel(wxCommandEvent& event);
        void OnApply(wxCommandEvent& event);
        virtual void OnInitDialog(wxInitDialogEvent& event);
        
private:
        void updateControlState();
        
        bool isTesting_;
    
};

#endif // __COMPORTS_DIALOG__
