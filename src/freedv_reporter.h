//==========================================================================
// Name:            freedv_reporter.h
// Purpose:         Dialog that displays current FreeDV Reporter spots
// Created:         Jun 12, 2023
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

#ifndef __FREEDV_REPORTER_DIALOG__
#define __FREEDV_REPORTER_DIALOG__

#include <string>
#include <map>

#include "main.h"
#include "defines.h"
#include "reporting/FreeDVReporter.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class FreeDVReporterDialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class FreeDVReporterDialog : public wxDialog
{
    public:
        FreeDVReporterDialog( wxWindow* parent,
                wxWindowID id = wxID_ANY, const wxString& title = _("FreeDV Reporter"), 
                const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize, 
                long style = wxDEFAULT_DIALOG_STYLE | wxTAB_TRAVERSAL | wxMINIMIZE_BOX | wxRESIZE_BORDER);
        ~FreeDVReporterDialog();
        
        void setReporter(FreeDVReporter* reporter);
        void refreshQSYButtonState();
        
    protected:

        // Handlers for events.
        void    OnOK(wxCommandEvent& event);
        void    OnSendQSY(wxCommandEvent& event);
        void    OnOpenWebsite(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
        void    OnSize(wxSizeEvent& event);
        void    OnMove(wxMoveEvent& event);
        void    OnShow(wxShowEvent& event);
        
        void OnItemSelected(wxListEvent& event);
        void OnItemDeselected(wxListEvent& event);
        
        // Main list box that shows spots
        wxListView*   m_listSpots;
        
        // QSY text
        wxTextCtrl *m_qsyText;
        
        // Step 4: test/save/cancel setup
        wxButton* m_buttonOK;
        wxButton* m_buttonSendQSY;
        wxButton* m_buttonDisplayWebpage;

     private:
         FreeDVReporter* reporter_;
         
         void onReporterConnect_();
         void onReporterDisconnect_();
         void onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOmly);
         void onUserDisconnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly);
         void onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz);
         void onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate);
         void onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode);
         
         wxString makeValidTime_(std::string timeStr);
};

#endif // __FREEDV_REPORTER_DIALOG__
