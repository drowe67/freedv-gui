//==========================================================================
// Name:            freedv_reporter.cpp
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

#include <wx/datetime.h>
#include "freedv_reporter.h"

#define UNKNOWN_STR "--"
#define NUM_COLS (11)

using namespace std::placeholders;

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
    , reporter_(nullptr)
{
    for (int col = 0; col < NUM_COLS; col++)
    {
        columnLengths_[col] = 0;
    }
    
    // Create top-level of control hierarchy.
    wxFlexGridSizer* sectionSizer = new wxFlexGridSizer(2, 1, 0, 0);
    sectionSizer->AddGrowableRow(0);
    
    // Main list box
    // =============================
    m_listSpots = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_SINGLE_SEL | wxLC_REPORT | wxLC_HRULES);
    m_listSpots->InsertColumn(0, wxT("Callsign"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(1, wxT("Grid Square"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(2, wxT("Version"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(3, wxT("Frequency"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(4, wxT("Status"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(5, wxT("TX Mode"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(6, wxT("Last TX"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(7, wxT("Last RX Callsign"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(8, wxT("Last RX Mode"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(9, wxT("SNR"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);
    m_listSpots->InsertColumn(10, wxT("Last Update"), wxLIST_FORMAT_CENTER, wxLIST_AUTOSIZE_USEHEADER);

    sectionSizer->Add(m_listSpots, 0, wxALL | wxEXPAND, 2);
    
    // Bottom buttons
    // =============================
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(this, wxID_OK, _("Close"));
    buttonSizer->Add(m_buttonOK, 0, wxALL, 2);

    m_buttonSendQSY = new wxButton(this, wxID_ANY, _("Request QSY"));
    m_buttonSendQSY->Enable(false); // disable by default unless we get a valid selection
    buttonSizer->Add(m_buttonSendQSY, 0, wxALL, 2);

    m_buttonDisplayWebpage = new wxButton(this, wxID_ANY, _("Open Website"));
    buttonSizer->Add(m_buttonDisplayWebpage, 0, wxALL, 2);

    sectionSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 2);
    
    this->SetMinSize(GetBestSize());
    
    // Trigger auto-layout of window.
    // ==============================
    this->SetSizerAndFit(sectionSizer);

    // Move FreeDV Reporter window back into last saved position
    SetSize(wxSize(
        wxGetApp().appConfiguration.reporterWindowWidth,
        wxGetApp().appConfiguration.reporterWindowHeight));
    SetPosition(wxPoint(
        wxGetApp().appConfiguration.reporterWindowLeft,
        wxGetApp().appConfiguration.reporterWindowTop));
    
    this->Layout();
    
    // Hook in events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(FreeDVReporterDialog::OnSize));
    this->Connect(wxEVT_MOVE, wxMoveEventHandler(FreeDVReporterDialog::OnMove));
    this->Connect(wxEVT_SHOW, wxShowEventHandler(FreeDVReporterDialog::OnShow));
    
    m_listSpots->Connect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);

    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
}

FreeDVReporterDialog::~FreeDVReporterDialog()
{
    this->Disconnect(wxEVT_SIZE, wxSizeEventHandler(FreeDVReporterDialog::OnSize));
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    this->Disconnect(wxEVT_MOVE, wxMoveEventHandler(FreeDVReporterDialog::OnMove));
    this->Disconnect(wxEVT_SHOW, wxShowEventHandler(FreeDVReporterDialog::OnShow));
    
    m_listSpots->Disconnect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Disconnect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);
    
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
}

void FreeDVReporterDialog::setReporter(FreeDVReporter* reporter)
{
    if (reporter_ != nullptr)
    {
        reporter_->setOnReporterConnectFn(FreeDVReporter::ReporterConnectionFn());
        reporter_->setOnReporterDisconnectFn(FreeDVReporter::ReporterConnectionFn());
        
        reporter_->setOnUserConnectFn(FreeDVReporter::ConnectionDataFn());
        reporter_->setOnUserDisconnectFn(FreeDVReporter::ConnectionDataFn());
        reporter_->setOnFrequencyChangeFn(FreeDVReporter::FrequencyChangeFn());
        reporter_->setOnTransmitUpdateFn(FreeDVReporter::TxUpdateFn());
        reporter_->setOnReceiveUpdateFn(FreeDVReporter::RxUpdateFn());
    }
    
    reporter_ = reporter;
    
    if (reporter_ != nullptr)
    {
        reporter_->setOnReporterConnectFn(std::bind(&FreeDVReporterDialog::onReporterConnect_, this));
        reporter_->setOnReporterDisconnectFn(std::bind(&FreeDVReporterDialog::onReporterDisconnect_, this));
    
        reporter_->setOnUserConnectFn(std::bind(&FreeDVReporterDialog::onUserConnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnUserDisconnectFn(std::bind(&FreeDVReporterDialog::onUserDisconnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnFrequencyChangeFn(std::bind(&FreeDVReporterDialog::onFrequencyChangeFn_, this, _1, _2, _3, _4, _5));
        reporter_->setOnTransmitUpdateFn(std::bind(&FreeDVReporterDialog::onTransmitUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));
        reporter_->setOnReceiveUpdateFn(std::bind(&FreeDVReporterDialog::onReceiveUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));
    }
    else
    {
        // Spot list no longer valid, delete the items currently on there
        m_listSpots->DeleteAllItems();
    }
}

void FreeDVReporterDialog::OnInitDialog(wxInitDialogEvent& event)
{
    // TBD
}

void FreeDVReporterDialog::OnShow(wxShowEvent& event)
{
    wxGetApp().appConfiguration.reporterWindowVisible = true;
}

void FreeDVReporterDialog::OnSize(wxSizeEvent& event)
{
    auto sz = GetSize();
    
    wxGetApp().appConfiguration.reporterWindowWidth = sz.GetWidth();
    wxGetApp().appConfiguration.reporterWindowHeight = sz.GetHeight();

    Layout();
}

void FreeDVReporterDialog::OnMove(wxMoveEvent& event)
{
    auto pos = event.GetPosition();
   
    wxGetApp().appConfiguration.reporterWindowLeft = pos.x;
    wxGetApp().appConfiguration.reporterWindowTop = pos.y;
}

void FreeDVReporterDialog::OnOK(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnSendQSY(wxCommandEvent& event)
{
    auto selectedIndex = m_listSpots->GetFirstSelected();
    if (selectedIndex >= 0)
    {
        auto selectedCallsign = m_listSpots->GetItemText(selectedIndex);
    
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(selectedIndex);
        std::string sid = *sidPtr;
    
        reporter_->requestQSY(sid, wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency, ""); // Custom message TBD.
    
        wxString fullMessage = wxString::Format(_("QSY request sent to %s"), selectedCallsign);
        wxMessageBox(fullMessage, wxT("FreeDV Reporter"), wxOK | wxICON_INFORMATION, this);
    }
}

void FreeDVReporterDialog::OnOpenWebsite(wxCommandEvent& event)
{
    std::string url = "https://" + wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->ToStdString() + "/";
    wxLaunchDefaultBrowser(url);
}

void FreeDVReporterDialog::OnClose(wxCloseEvent& event)
{
    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnItemSelected(wxListEvent& event)
{
   refreshQSYButtonState();
}

void FreeDVReporterDialog::OnItemDeselected(wxListEvent& event)
{
    m_buttonSendQSY->Enable(false);
}

void FreeDVReporterDialog::refreshQSYButtonState()
{
    bool enabled = false;
    auto selectedIndex = m_listSpots->GetFirstSelected();
    if (selectedIndex >= 0)
    {
        auto selectedCallsign = m_listSpots->GetItemText(selectedIndex);
    
        if (selectedCallsign != wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign && 
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
        {
            wxString theirFreqString = m_listSpots->GetItemText(selectedIndex, 3);
            wxRegEx mhzRegex(" MHz$");
            mhzRegex.Replace(&theirFreqString, "");
            
            uint64_t theirFreq = wxAtof(theirFreqString) * 1000 * 1000;
            enabled = theirFreq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
        }
    }
    
    m_buttonSendQSY->Enable(enabled);
}

// =================================================================================
// Note: these methods below do not run under the UI thread, so we need to make sure
// UI actions happen there.
// =================================================================================

void FreeDVReporterDialog::onReporterConnect_()
{
    CallAfter([&]() {
        m_listSpots->Freeze();
        
        for (auto index = m_listSpots->GetItemCount() - 1; index >= 0; index--)
        {
            delete (std::string*)m_listSpots->GetItemData(index);
            m_listSpots->DeleteItem(index);
        }
        
        m_listSpots->Thaw();
    });
}

void FreeDVReporterDialog::onReporterDisconnect_()
{
    CallAfter([&]() {
        m_listSpots->Freeze();
        
        for (auto index = m_listSpots->GetItemCount() - 1; index >= 0; index--)
        {
            delete (std::string*)m_listSpots->GetItemData(index);
            m_listSpots->DeleteItem(index);
        }
        
        m_listSpots->Thaw();
    });
}

void FreeDVReporterDialog::onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    CallAfter([&, sid, lastUpdate, callsign, gridSquare, version, rxOnly]() {
        m_listSpots->Freeze();
        
        auto itemIndex = m_listSpots->InsertItem(m_listSpots->GetItemCount(), wxString(callsign).Upper());
        m_listSpots->SetItem(itemIndex, 1, gridSquare);
        m_listSpots->SetItem(itemIndex, 2, version);
        m_listSpots->SetItem(itemIndex, 3, UNKNOWN_STR);
        
        if (rxOnly)
        {
            m_listSpots->SetItem(itemIndex, 4, "Receive Only");
            m_listSpots->SetItem(itemIndex, 5, "N/A");
            m_listSpots->SetItem(itemIndex, 6, "N/A");
        }
        else
        {
            m_listSpots->SetItem(itemIndex, 4, UNKNOWN_STR);
            m_listSpots->SetItem(itemIndex, 5, UNKNOWN_STR);
            m_listSpots->SetItem(itemIndex, 6, UNKNOWN_STR);
        }
        m_listSpots->SetItem(itemIndex, 7, UNKNOWN_STR);
        m_listSpots->SetItem(itemIndex, 8, UNKNOWN_STR);
        m_listSpots->SetItem(itemIndex, 9, UNKNOWN_STR);
        
        auto lastUpdateTime = makeValidTime_(lastUpdate);
        m_listSpots->SetItem(itemIndex, 10, lastUpdateTime);
        
        m_listSpots->SetItemPtrData(itemIndex, (wxUIntPtr)new std::string(sid));
        
        // Resize all columns to the longest value.
        checkColumnsAndResize_();
        
        m_listSpots->Thaw();

        Layout();
    });
}

void FreeDVReporterDialog::onUserDisconnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    CallAfter([&, sid]() {
        for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
        {
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            if (sid == *sidPtr)
            {
                delete (std::string*)m_listSpots->GetItemData(index);
                m_listSpots->DeleteItem(index);
                break;
            }
        }
    });
}

void FreeDVReporterDialog::onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz)
{
    CallAfter([&, sid, frequencyHz, lastUpdate]() {
        for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
        {
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            if (sid == *sidPtr)
            {
                double frequencyMHz = frequencyHz / 1000000.0;
            
                m_listSpots->Freeze();
                
                wxString frequencyMHzString = wxString::Format(_("%.04f MHz"), frequencyMHz);
                m_listSpots->SetItem(index, 3, frequencyMHzString);
            
                auto lastUpdateTime = makeValidTime_(lastUpdate);
                m_listSpots->SetItem(index, 10, lastUpdateTime);
                
                // Resize all columns to the longest value.
                checkColumnsAndResize_();
            
                m_listSpots->Thaw();
                
                break;
            }
        }
    });
}

void FreeDVReporterDialog::onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate)
{
    CallAfter([&, sid, txMode, transmitting, lastTxDate, lastUpdate]() {
        for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
        {
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            if (sid == *sidPtr)
            {
                std::string txStatus = "Receiving";
                if (transmitting)
                {
                    txStatus = "Transmitting";
                    
                    wxColour lightRed(0xfc, 0x45, 0x00);
                    m_listSpots->SetItemBackgroundColour(index, lightRed);
                    m_listSpots->SetItemTextColour(index, *wxWHITE);
                }
                else
                {
                    m_listSpots->SetItemBackgroundColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
                    m_listSpots->SetItemTextColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
                }
            
                m_listSpots->Freeze();
                
                if (m_listSpots->GetItemText(index, 4) != _("Receive Only"))
                {
                    m_listSpots->SetItem(index, 4, txStatus);
                    m_listSpots->SetItem(index, 5, txMode);
                
                    auto lastTxTime = makeValidTime_(lastTxDate);
                    m_listSpots->SetItem(index, 6, lastTxTime);
                }
                
                auto lastUpdateTime = makeValidTime_(lastUpdate);
                m_listSpots->SetItem(index, 10, lastUpdateTime);
                
                // Resize all columns to the longest value.
                checkColumnsAndResize_();
                
                m_listSpots->Thaw();
            
                break;
            }
        }
    });
}

void FreeDVReporterDialog::onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode)
{
    CallAfter([&, sid, lastUpdate, receivedCallsign, snr, rxMode]() {
        for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
        {
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            if (sid == *sidPtr)
            {
                m_listSpots->Freeze();
                
                m_listSpots->SetItem(index, 7, receivedCallsign);
                m_listSpots->SetItem(index, 8, rxMode);
            
                wxString snrString = wxString::Format(_("%.01f"), snr);
                if (receivedCallsign == "" && rxMode == "")
                {
                    // Frequency change--blank out SNR too.
                    m_listSpots->SetItem(index, 7, UNKNOWN_STR);
                    m_listSpots->SetItem(index, 8, UNKNOWN_STR);
                    m_listSpots->SetItem(index, 9, UNKNOWN_STR);
                }
                else
                {
                    m_listSpots->SetItem(index, 9, snrString);
                }
 
                auto lastUpdateTime = makeValidTime_(lastUpdate);
                m_listSpots->SetItem(index, 10, lastUpdateTime);
                
                // Resize all columns to the longest value.
                checkColumnsAndResize_();
                
                m_listSpots->Thaw();
                
                break;
            }
        }
    });
}

wxString FreeDVReporterDialog::makeValidTime_(std::string timeStr)
{    
    wxRegEx millisecondsRemoval(_("\\.[^+-]+"));
    wxString tmp = timeStr;
    millisecondsRemoval.Replace(&tmp, _(""));
    
    wxRegEx timezoneRgx(_("([+-])([0-9]+):([0-9]+)$"));
    wxDateTime::TimeZone timeZone(0); // assume UTC by default
    if (timezoneRgx.Matches(tmp))
    {
        auto tzOffset = timezoneRgx.GetMatch(tmp, 1);
        auto hours = timezoneRgx.GetMatch(tmp, 2);
        auto minutes = timezoneRgx.GetMatch(tmp, 3);
        
        int tzMinutes = wxAtoi(hours) * 60;
        tzMinutes += wxAtoi(minutes);
        
        if (tzOffset == "-")
        {
            tzMinutes = -tzMinutes;
        }
        
        timezoneRgx.Replace(&tmp, _(""));
        
        timeZone = wxDateTime::TimeZone(tzMinutes);
    }
    
    wxDateTime tmpDate;
    if (tmpDate.ParseISOCombined(tmp))
    {
        tmpDate.MakeFromTimezone(timeZone);
        if (wxGetApp().appConfiguration.reportingConfiguration.useUTCForReporting)
        {
            timeZone = wxDateTime::TimeZone(wxDateTime::TZ::UTC);
        }
        else
        {
            timeZone = wxDateTime::TimeZone(wxDateTime::TZ::Local);
        }
        return tmpDate.Format(wxDefaultDateTimeFormat, timeZone);
    }
    else
    {
        return _(UNKNOWN_STR);
    }
}

void FreeDVReporterDialog::checkColumnsAndResize_()
{
    std::map<int, bool> shouldResize;
    
    // Process all data in table and determine which columns now have longer text 
    // (and thus should be auto-resized).
    for (int index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        for (int col = 0; col < NUM_COLS; col++)
        {
            auto str = m_listSpots->GetItemText(index, col);
            auto strLength = str.length();
            auto itemFont = m_listSpots->GetItemFont(index);
            auto charWidth = itemFont.GetPixelSize();
            auto textWidth = charWidth * strLength;
            if (textWidth > columnLengths_[col])
            {
                shouldResize[col] = true;
                columnLengths_[col] = textWidth;
            }
        }
    }
    
    // Trigger auto-resize for columns as needed
    for (auto& kvp : shouldResize)
    {
        // Note: we don't add anything to shouldResize that is false, so
        // no need to check for shouldResize == true here.
        m_listSpots->SetColumnWidth(kvp.first, wxLIST_AUTOSIZE_USEHEADER);
    }
}
