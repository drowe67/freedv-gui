//==========================================================================
// Name:            log_entry.h
// Purpose:         Dialog for confirming log entry.
// Created:         Dec 17, 2025
// Authors:         Mooneer Salem
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

#ifndef LOG_ENTRY_DIALOG_H
#define LOG_ENTRY_DIALOG_H

#include "main.h"
#include "defines.h"
#include "../../logging/ILogger.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class LogEntryDialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class LogEntryDialog : public wxDialog
{
    public:
        LogEntryDialog( wxWindow* parent,
                wxWindowID id = wxID_ANY, const wxString& title = _("Confirm Log Entry"), 
                const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxSize(250,-1), 
                long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxTAB_TRAVERSAL );
        virtual ~LogEntryDialog();

        void ShowDialog(wxString const& dxCall, wxString const& dxGrid, wxDateTime const& logTime, int64_t freqHz);
        
    protected:

        // Handlers for events.
        void    OnOK(wxCommandEvent& event);
        void    OnCancel(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
       
        wxTextCtrl *dxCall_;
        wxTextCtrl *dxGrid_;
        wxStaticText *myCall_;
        wxStaticText *myGrid_;
        wxTextCtrl *frequency_;
        wxStaticText* labelFrequency_;
        wxTextCtrl *rxReport_;
        wxTextCtrl *txReport_;
        wxTextCtrl *name_;
        wxTextCtrl *comments_;
        wxStaticText* labelTimeVal_;

        wxButton* m_buttonOK;
        wxButton* m_buttonCancel;

     private:
        std::shared_ptr<ILogger> logger_;
        wxDateTime logTime_;
};

#endif // LOG_ENTRY_DIALOG_H
