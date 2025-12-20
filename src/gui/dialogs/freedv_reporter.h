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
#include <deque>
#include <mutex>

#include <wx/tipwin.h>
#include <wx/dataview.h>

#include "../../main.h"
#include "defines.h"
#include "reporting/FreeDVReporter.h"
#include "../controls/ReportMessageRenderer.h"

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
        
        void setReporter(std::shared_ptr<FreeDVReporter> const& reporter);
        void refreshQSYButtonState();
        void refreshLayout();
        
        void setBandFilter(FilterFrequency freq);
        
        bool isTextMessageFieldInFocus();
    
        void Unselect(wxDataViewItem& dvi) { m_listSpots->Unselect(dvi); }
    
#if defined(WIN32)
        void autosizeColumns();
#endif // defined(WIN32)
            
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

        void OnItemSelectionChanged(wxDataViewEvent& event);
        void OnColumnClick(wxDataViewEvent& event);
        void OnItemDoubleClick(wxDataViewEvent& event);
        void OnItemRightClick(wxDataViewEvent& event);
        void OnColumnReordered(wxDataViewEvent& event);

        void OnTimer(wxTimerEvent& event);
        void DeselectItem();
        void DeselectItem(wxMouseEvent& event);
        void AdjustToolTip(wxMouseEvent& event);
        void OnFilterTrackingEnable(wxCommandEvent& event);
        void OnCopyUserMessage(wxCommandEvent& event);
        void SkipMouseEvent(wxMouseEvent& event);
        void AdjustMsgColWidth(wxListEvent& event);
        void OnRightClickSpotsList(wxContextMenuEvent& event);

        void OnShowColumn(wxCommandEvent& event);
                
        // Main list box that shows spots
        wxDataViewCtrl*   m_listSpots;
        wxObjectDataPtr<wxDataViewModel> spotsDataModel_;
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
        wxTimer* m_resortTimer;
        wxTimer* m_highlightClearTimer;
        wxTimer* m_deleteTimer;

        wxTipWindow* tipWindow_;

        // Menu bar and menu options
        wxMenuBar* menuBar_;
        wxMenu* showMenu_;
        
     private:
        const wxString UNKNOWN_STR;
        const wxString SNR_FORMAT_STR;
        const wxString ALL_LETTERS_RGX;
        const wxString MS_REMOVAL_RGX;
        const wxString TIMEZONE_RGX;
        const wxString TZ_OFFSET_STR;
        const wxString EMPTY_STR;
        const wxString TIME_FORMAT_STR;

         class FreeDVReporterDataModel : public wxDataViewModel
         {
         public:
             FreeDVReporterDataModel(FreeDVReporterDialog* parent);
             virtual ~FreeDVReporterDataModel();

             void setReporter(std::shared_ptr<FreeDVReporter> const& reporter);
             void setBandFilter(FilterFrequency freq);
             void refreshAllRows();
             void requestQSY(wxDataViewItem selectedItem, uint64_t frequency, wxString const& customText);
             void updateHighlights();
             void triggerResort();
             void deallocateRemovedItems();
             void updateMessage(wxString const& statusMsg)
             {
                 if (reporter_)
                 {
                     reporter_->updateMessage((const char*)statusMsg.utf8_str());
                 }
             }

             uint64_t getFrequency(wxDataViewItem item)
             {
                if (item.IsOk())
                {
                    auto data = (ReporterData*)item.GetID();
                    return data->frequency;
                }

                return 0;
             }

             FilterFrequency getCurrentBandFilter() const { return currentBandFilter_; }
             wxString getCallsign(wxDataViewItem& item)
             {
                 if (item.IsOk())
                 {
                     auto data = (ReporterData*)item.GetID();
                     return data->callsign;
                 }
                 return "";
             }
             
             wxString getUserMessage(wxDataViewItem& item)
             {
                 if (item.IsOk())
                 {
                     auto data = (ReporterData*)item.GetID();
                     return data->userMessage;
                 }
                 return "";
             }
             
             bool isValidForReporting()
             {
                 return reporter_ && reporter_->isValidForReporting();
             }
             
             // Required overrides to implement functionality
             virtual bool HasDefaultCompare() const override;
             virtual int Compare (const wxDataViewItem &item1, const wxDataViewItem &item2, unsigned int column, bool ascending) const override;
             virtual bool GetAttr (const wxDataViewItem &item, unsigned int col, wxDataViewItemAttr &attr) const override;
             virtual unsigned int GetChildren (const wxDataViewItem &item, wxDataViewItemArray &children) const override;
             virtual wxDataViewItem GetParent (const wxDataViewItem &item) const override;
             virtual void GetValue (wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override;
             virtual bool IsContainer (const wxDataViewItem &item) const override;
             virtual bool SetValue (const wxVariant &variant, const wxDataViewItem &item, unsigned int col) override;

#if !wxCHECK_VERSION(3,2,0)
             virtual unsigned int GetColumnCount () const override;
             virtual wxString GetColumnType (unsigned int col) const override;
#endif // !wxCHECK_VERSION(3,2,0)

             bool sortOnNextTimerInterval;

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
                int snrVal;
                wxString lastUpdate;
                wxDateTime lastUpdateDate;
                wxString userMessage;
                wxDateTime lastUpdateUserMessage;
                wxDateTime connectTime;

                // Controls whether this row has been filtered
                bool isVisible;
                
                // Controls whether this row is pending deletion
                bool isPendingDelete;
                wxDateTime deleteTime;

                // Controls whether this row is pending update
                bool isPendingUpdate;

                // Controls the current highlight color
                wxColour foregroundColor;
                wxColour backgroundColor;

                ReporterData()
                    : lastTxDate(wxInvalidDateTime)
                    , lastRxDate(wxInvalidDateTime)
                    , lastUpdateDate(wxInvalidDateTime)
                    , lastUpdateUserMessage(wxInvalidDateTime)
                    , connectTime(wxInvalidDateTime)
                    , deleteTime(wxInvalidDateTime)
                { /* empty */ }
            };

            struct CallbackHandler
            {
                std::string sid;
                std::string lastUpdate;
                std::string callsign;
                std::string gridSquare;
                std::string version;
                bool rxOnly;
                uint64_t frequencyHz;
                std::string txMode;
                bool transmitting;
                std::string lastTxDate;
                std::string receivedCallsign;
                float snr;
                std::string rxMode;
                std::string message;
                std::function<void(CallbackHandler&)> fn;
            };

            std::shared_ptr<FreeDVReporter> reporter_;
            std::map<std::string, ReporterData*> allReporterData_;
            std::deque<CallbackHandler> fnQueue_;
            std::mutex fnQueueMtx_;
            std::recursive_mutex dataMtx_;
            bool isConnected_;
            FreeDVReporterDialog* parent_;

            FilterFrequency currentBandFilter_;
            bool filterSelfMessageUpdates_;
            uint64_t filteredFrequency_;

            bool isFiltered_(uint64_t freq);

            void clearAllEntries_();

            void onReporterConnect_();
            void onReporterDisconnect_();
            void onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly);
            void onUserDisconnectFn_(std::string sid, std::string const& lastUpdate, std::string const& callsign, std::string const& gridSquare, std::string const& version, bool rxOnly);
            void onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string const& callsign, std::string const& gridSquare, uint64_t frequencyHz);
            void onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string const& callsign, std::string const& gridSquare, std::string txMode, bool transmitting, std::string lastTxDate);
            void onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string const& callsign, std::string const& gridSquare, std::string receivedCallsign, float snr, std::string rxMode);
            void onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message);

            void onConnectionSuccessfulFn_();
            void onAboutToShowSelfFn_();
            
            void onRecvEndFn_();

            void execQueuedAction_();

            wxString makeValidTime_(std::string const& timeStr, wxDateTime& timeObj);
            double calculateDistance_(wxString gridSquare1, wxString gridSquare2);
            double calculateBearingInDegrees_(wxString gridSquare1, wxString gridSquare2);
            void calculateLatLonFromGridSquare_(wxString gridSquare, double& lat, double& lon);

            static double DegreesToRadians_(double degrees);
            static double RadiansToDegrees_(double radians);
            static wxString GetCardinalDirection_(int degrees);
        };

        bool isSelectionPossible_;

        void createColumn_(int col, bool visible);
        wxDataViewColumn* getColumnForModelColId_(unsigned int col);

        FilterFrequency getFilterForFrequency_(uint64_t freq);
        wxColour msgRowBackgroundColor;
        wxColour msgRowForegroundColor;
        wxColour txRowBackgroundColor;
        wxColour txRowForegroundColor;
        wxColour rxRowBackgroundColor;
        wxColour rxRowForegroundColor;
};

#endif // __FREEDV_REPORTER_DIALOG__
