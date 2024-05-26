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
#include <vector>

#include <wx/imaglist.h>
#include <wx/tipwin.h>

#include "main.h"
#include "defines.h"
#include "reporting/FreeDVReporter.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class FreeDVReporterDialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class FreeDVReporterDialog : public wxFrame
{
    public:
        enum FilterFrequency 
        {
            BAND_ALL,
            BAND_160M,
            BAND_80M,
            BAND_60M,
            BAND_40M,
            BAND_30M,
            BAND_20M,
            BAND_17M,
            BAND_15M,
            BAND_12M,
            BAND_10M,
            BAND_VHF_UHF,
            BAND_OTHER,            
        };
        
        FreeDVReporterDialog( wxWindow* parent,
                wxWindowID id = wxID_ANY, const wxString& title = _("FreeDV Reporter"), 
                const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize, 
                long style = wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU | wxMINIMIZE_BOX | wxRESIZE_BORDER);
        ~FreeDVReporterDialog();
        
        void setReporter(std::shared_ptr<FreeDVReporter> reporter);
        void refreshQSYButtonState();
        void refreshLayout();
        
        void setBandFilter(FilterFrequency freq);
        
        bool isTextMessageFieldInFocus();
        
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
        void    OnBandFilterChange(wxCommandEvent& event);
        void    OnStatusTextSend(wxCommandEvent& event);
        void    OnStatusTextSendAndSaveMessage(wxCommandEvent& event);
        void    OnStatusTextSaveMessage(wxCommandEvent& event);
        void    OnStatusTextSendContextMenu(wxContextMenuEvent& event);
        void    OnStatusTextClear(wxCommandEvent& event);
        void    OnStatusTextClearContextMenu(wxContextMenuEvent& event);
        void    OnStatusTextClearSelected(wxCommandEvent& event);
        void    OnStatusTextClearAll(wxCommandEvent& event);
        void    OnStatusTextChange(wxCommandEvent& event);
        void    OnSystemColorChanged(wxSysColourChangedEvent& event);

        void OnItemSelected(wxListEvent& event);
        void OnItemDeselected(wxListEvent& event);
        void OnSortColumn(wxListEvent& event);
        void OnTimer(wxTimerEvent& event);
        void AdjustToolTip(wxMouseEvent& event);
        void OnFilterTrackingEnable(wxCommandEvent& event);
        void OnRightClickSpotsList(wxContextMenuEvent& event);
        void OnCopyUserMessage(wxCommandEvent& event);
        void SkipMouseEvent(wxMouseEvent& event);
        void AdjustMsgColWidth(wxListEvent& event);
        
        void OnDoubleClick(wxMouseEvent& event);
        
        // Main list box that shows spots
        wxListView*   m_listSpots;
        wxImageList*  m_sortIcons;
        int upIconIndex_;
        int downIconIndex_;
        wxMenu* spotsPopupMenu_;
        wxString tempUserMessage_; // to store the currently hovering message prior to going on the clipboard

        // QSY text
        wxTextCtrl* m_qsyText;
        
        // Band filter
        wxComboBox* m_bandFilter;
        wxCheckBox* m_trackFrequency;
        wxRadioButton* m_trackFreqBand;
        wxRadioButton* m_trackExactFreq;

        // Status message
        wxComboBox* m_statusMessage;
        wxButton* m_buttonSend;
        wxButton* m_buttonClear;
        wxMenu* setPopupMenu_;
        wxMenu* clearPopupMenu_;
        
        // Step 4: test/save/cancel setup
        wxButton* m_buttonOK;
        wxButton* m_buttonSendQSY;
        wxButton* m_buttonDisplayWebpage;
        
        // Timer to unhighlight RX rows after 10s (like with web-based Reporter)
        wxTimer* m_highlightClearTimer;
        
        std::vector<std::function<void()> > fnQueue_;
        std::mutex fnQueueMtx_;

        wxTipWindow* tipWindow_;
        
     private:
         struct ReporterData
         {
             std::string sid;
             wxString callsign;
             wxString gridSquare;
             double distanceVal;
             wxString distance;
             double headingVal;
             wxString heading;
             wxString version;
             uint64_t frequency;
             wxString freqString;
             wxString status;
             wxString txMode;
             bool transmitting;
             wxString lastTx;
             wxDateTime lastTxDate;
             wxDateTime lastRxDate;
             wxString lastRxCallsign;
             wxString lastRxMode;
             wxString snr;
             wxString lastUpdate;
             wxDateTime lastUpdateDate;
             wxString userMessage;
             wxDateTime lastUpdateUserMessage;
         };
         
         std::shared_ptr<FreeDVReporter> reporter_;
         std::map<int, int> columnLengths_;
         std::map<std::string, ReporterData*> allReporterData_;
         FilterFrequency currentBandFilter_;
         int currentSortColumn_;
         bool sortAscending_;
         bool isConnected_;
         bool filterSelfMessageUpdates_;
         uint64_t filteredFrequency_;
         
         void clearAllEntries_(bool clearForAllBands);
         void onReporterConnect_();
         void onReporterDisconnect_();
         void onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly);
         void onUserDisconnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly);
         void onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz);
         void onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate);
         void onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode);
         void onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message);
         void onConnectionSuccessfulFn_();
         void onAboutToShowSelfFn_();

         wxString makeValidTime_(std::string timeStr, wxDateTime& timeObj);
         
         void addOrUpdateListIfNotFiltered_(ReporterData* data, std::map<int, int>& colResizeList);
         FilterFrequency getFilterForFrequency_(uint64_t freq);
         bool isFiltered_(uint64_t freq);
         
         bool setColumnForRow_(int row, int col, wxString val, std::map<int, int>& colResizeList);
         void resizeChangedColumns_(std::map<int, int>& colResizeList);

         void sortColumn_(int col);
         void sortColumn_(int col, bool direction);
         
         double calculateDistance_(wxString gridSquare1, wxString gridSquare2);
         double calculateBearingInDegrees_(wxString gridSquare1, wxString gridSquare2);
         void calculateLatLonFromGridSquare_(wxString gridSquare, double& lat, double& lon);
         
         void execQueuedAction_();

         void resizeAllColumns_();
         int getSizeForTableCellString_(wxString string);

         static wxCALLBACK int ListCompareFn_(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData);
         static double DegreesToRadians_(double degrees);
         static double RadiansToDegrees_(double radians);
         static wxString GetCardinalDirection_(int degrees);
};

#endif // __FREEDV_REPORTER_DIALOG__
