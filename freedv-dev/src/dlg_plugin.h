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
#ifndef __PLUGIN_DIALOG__
#define __PLUGIN_DIALOG__

#include "fdmdv2_main.h"
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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlugInDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlugInDlg : public wxDialog
{
    public:
    PlugInDlg(const wxString& title = _("PTT Config"), int numParams = 0, wxString params[]=NULL, wxWindow* parent=NULL, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(450,300), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
        virtual ~PlugInDlg();
        void    ExchangeData(int inout);

    protected:
        wxTextCtrl   *m_txtCtrlPlugIn1;
        wxTextCtrl   *m_txtCtrlPlugIn2;
        wxTextCtrl   *m_txtCtrlPlugIn3;

        /* Ok - Cancel */

        wxButton* m_buttonOK;
        wxButton* m_buttonCancel;


protected:

        void OnOK(wxCommandEvent& event);
        void OnCancel(wxCommandEvent& event);
        virtual void OnInitDialog(wxInitDialogEvent& event);
};

#endif // __PLUGIN_DIALOG__
