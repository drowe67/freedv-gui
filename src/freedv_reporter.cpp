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

using namespace std::placeholders;

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
    , reporter_(nullptr)
{
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

    m_buttonOK = new wxButton(this, wxID_OK);
    buttonSizer->Add(m_buttonOK, 0, wxALL, 2);

    m_buttonSendQSY = new wxButton(this, wxID_ANY, _("Request QSY"));
    m_buttonSendQSY->Enable(false); // disable by default unless we get a valid selection
    buttonSizer->Add(m_buttonSendQSY, 0, wxALL, 2);

    m_buttonDisplayWebpage = new wxButton(this, wxID_ANY, _("Open Website"));
    buttonSizer->Add(m_buttonDisplayWebpage, 0, wxALL, 2);

    sectionSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 2);
    
    // Trigger auto-layout of window.
    // ==============================
    this->SetSizerAndFit(sectionSizer);
    
    this->Layout();
    this->Centre(wxBOTH);
    this->SetMinSize(GetBestSize());
    
    // Hook in events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    
    m_listSpots->Connect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);

    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
}

FreeDVReporterDialog::~FreeDVReporterDialog()
{
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    
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

void FreeDVReporterDialog::OnOK(wxCommandEvent& event)
{
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
    
        reporter_->requestQSY(sid, wxGetApp().m_reportingFrequency, ""); // Custom message TBD.
    
        wxString fullMessage = wxString::Format(_("QSY request sent to %s"), selectedCallsign);
        wxMessageBox(fullMessage, wxT("FreeDV Reporter"), wxOK | wxICON_INFORMATION, this);
    }
}

void FreeDVReporterDialog::OnOpenWebsite(wxCommandEvent& event)
{
    std::string url = "https://" + wxGetApp().m_freedvReporterHostname.ToStdString() + "/";
    wxLaunchDefaultBrowser(url);
}

void FreeDVReporterDialog::OnClose(wxCloseEvent& event)
{
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
    
        if (selectedCallsign != wxGetApp().m_reportingCallsign && wxGetApp().m_reportingFrequency > 0)
        {
            enabled = true;
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
        
        auto itemIndex = m_listSpots->InsertItem(m_listSpots->GetItemCount(), callsign);
        m_listSpots->SetItem(itemIndex, 1, gridSquare);
        m_listSpots->SetItem(itemIndex, 2, version);
        m_listSpots->SetItem(itemIndex, 3, "Unknown");
        
        if (rxOnly)
        {
            m_listSpots->SetItem(itemIndex, 4, "Receive Only");
            m_listSpots->SetItem(itemIndex, 5, "N/A");
            m_listSpots->SetItem(itemIndex, 6, "N/A");
        }
        else
        {
            m_listSpots->SetItem(itemIndex, 4, "Unknown");
            m_listSpots->SetItem(itemIndex, 5, "Unknown");
            m_listSpots->SetItem(itemIndex, 6, "Unknown");
        }
        m_listSpots->SetItem(itemIndex, 7, "Unknown");
        m_listSpots->SetItem(itemIndex, 8, "Unknown");
        m_listSpots->SetItem(itemIndex, 9, "Unknown");
        
        auto lastUpdateTime = makeValidTime_(lastUpdate);
        m_listSpots->SetItem(itemIndex, 10, lastUpdateTime);
        
        m_listSpots->SetItemPtrData(itemIndex, (wxUIntPtr)new std::string(sid));
        
        // Resize all columns to the biggest value.
        for (int col = 0; col <= 10; col++)
        {
            m_listSpots->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
        }
        
        m_listSpots->Thaw();
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
                    //nonRedBackground_ = m_listSpots->GetItemBackgroundColour(index);
                    //m_listSpots->SetItemBackgroundColour(index, *wxRED);
                }
                else
                {
                    //m_listSpots->SetItemBackgroundColour(index, nonRedBackground_);
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
                m_listSpots->SetItem(index, 9, snrString);
            
                auto lastUpdateTime = makeValidTime_(lastUpdate);
                m_listSpots->SetItem(index, 10, lastUpdateTime);
                
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
        if (wxGetApp().m_useUTCTime)
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
        return _("Unknown");
    }
}
