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

#include <math.h>
#include <wx/datetime.h>
#include "freedv_reporter.h"

#include "freedv_interface.h"

extern FreeDVInterface freedvInterface;

#define UNKNOWN_STR "--"
#define NUM_COLS (13)
#define RX_ONLY_STATUS "RX Only"
#define RX_COLORING_TIMEOUT_SEC (20)

using namespace std::placeholders;

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
    , reporter_(nullptr)
    , currentBandFilter_(FreeDVReporterDialog::BAND_ALL)
    , currentSortColumn_(-1)
    , sortAscending_(false)
{
    for (int col = 0; col < NUM_COLS; col++)
    {
        columnLengths_[col] = 0;
    }
    
    // Create top-level of control hierarchy.
    wxFlexGridSizer* sectionSizer = new wxFlexGridSizer(2, 1, 0, 0);
    sectionSizer->AddGrowableRow(0);
    sectionSizer->AddGrowableCol(0);
        
    // Main list box
    // =============================
    m_listSpots = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_SINGLE_SEL | wxLC_REPORT | wxLC_HRULES);
    m_listSpots->InsertColumn(0, wxT("Callsign"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(1, wxT("Locator"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(2, wxT("KM"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(3, wxT("Version"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(4, wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz ? wxT("KHz") : wxT("MHz"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(5, wxT("Status"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(6, wxT("Msg"), wxLIST_FORMAT_CENTER, 120);
    m_listSpots->InsertColumn(7, wxT("Last TX"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(8, wxT("Mode"), wxLIST_FORMAT_CENTER, 80);
    m_listSpots->InsertColumn(9, wxT("RX Call"), wxLIST_FORMAT_CENTER, 120);
    m_listSpots->InsertColumn(10, wxT("Mode"), wxLIST_FORMAT_CENTER, 120);
    m_listSpots->InsertColumn(11, wxT("SNR"), wxLIST_FORMAT_CENTER, 40);
    m_listSpots->InsertColumn(12, wxT("Last Update"), wxLIST_FORMAT_CENTER, 120);

    // On Windows, the last column will end up taking a lot more space than desired regardless
    // of the space we actually need. Create a "dummy" column to take that space instead.
    m_listSpots->InsertColumn(13, wxT(""), wxLIST_FORMAT_CENTER, 1);

    sectionSizer->Add(m_listSpots, 0, wxALL | wxEXPAND, 2);

    // Add sorting up/down arrows.
    wxArtProvider provider;
    m_sortIcons = new wxImageList(16, 16, true, 2);
    auto upIcon = provider.GetBitmap("wxART_GO_UP");
    assert(upIcon.IsOk());
    upIconIndex_ = m_sortIcons->Add(upIcon);
    assert(upIconIndex_ >= 0);

    auto downIcon = provider.GetBitmap("wxART_GO_DOWN");
    assert(downIcon.IsOk());
    downIconIndex_ = m_sortIcons->Add(downIcon);
    assert(downIconIndex_ >= 0);

    m_listSpots->AssignImageList(m_sortIcons, wxIMAGE_LIST_SMALL);
    
    // Bottom buttons
    // =============================
    wxBoxSizer* bottomRowSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* reportingSettingsSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    // Band filter list    
    wxString bandList[] = {
        _("All"),
        _("160 m"),
        _("80 m"),
        _("60 m"),
        _("40 m"),
        _("30 m"),
        _("20 m"),
        _("17 m"),
        _("15 m"),
        _("12 m"),
        _("10 m"),
        _(">= 6 m"),
        _("Other"),
    };
    
    reportingSettingsSizer->Add(new wxStaticText(this, wxID_ANY, _("Show stations on:"), wxDefaultPosition, wxDefaultSize, 0), 
                          0, wxALIGN_CENTER_VERTICAL, 20);
    
    m_bandFilter = new wxComboBox(
        this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
        sizeof(bandList) / sizeof(wxString), bandList, wxCB_DROPDOWN | wxCB_READONLY);
    m_bandFilter->SetSelection(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter);
    setBandFilter((FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get());
    
    reportingSettingsSizer->Add(m_bandFilter, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    
    m_trackFrequency = new wxCheckBox(this, wxID_ANY, _("Track current:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    reportingSettingsSizer->Add(m_trackFrequency, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    
    m_trackFreqBand = new wxRadioButton(this, wxID_ANY, _("band"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    reportingSettingsSizer->Add(m_trackFreqBand, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackFreqBand->Enable(false);
    
    m_trackExactFreq = new wxRadioButton(this, wxID_ANY, _("frequency"), wxDefaultPosition, wxDefaultSize, 0);
    reportingSettingsSizer->Add(m_trackExactFreq, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackExactFreq->Enable(false);
    
    m_trackFrequency->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);

    auto statusMessageLabel = new wxStaticText(this, wxID_ANY, _("Msg:"), wxDefaultPosition, wxDefaultSize);
    reportingSettingsSizer->Add(statusMessageLabel, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_statusMessage = new wxTextCtrl(this, wxID_ANY, _(""), wxDefaultPosition, wxSize(175, -1));
    reportingSettingsSizer->Add(m_statusMessage, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    bottomRowSizer->Add(reportingSettingsSizer, 0, wxALL | wxALIGN_CENTER, 0);
    
    m_buttonOK = new wxButton(this, wxID_OK, _("Close"));
    buttonSizer->Add(m_buttonOK, 0, wxALL, 2);

    m_buttonSendQSY = new wxButton(this, wxID_ANY, _("Request QSY"));
    m_buttonSendQSY->Enable(false); // disable by default unless we get a valid selection
    buttonSizer->Add(m_buttonSendQSY, 0, wxALL, 2);

    m_buttonDisplayWebpage = new wxButton(this, wxID_ANY, _("Open Website"));
    buttonSizer->Add(m_buttonDisplayWebpage, 0, wxALL, 2);

    bottomRowSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 0);

    sectionSizer->Add(bottomRowSizer, 0, wxALL | wxALIGN_CENTER, 2);
    
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
    
    // Set up highlight clear timer
    m_highlightClearTimer = new wxTimer(this);
    m_highlightClearTimer->Start(1000);
    
    // Hook in events
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(FreeDVReporterDialog::OnTimer), NULL, this);
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(FreeDVReporterDialog::OnSize));
    this->Connect(wxEVT_MOVE, wxMoveEventHandler(FreeDVReporterDialog::OnMove));
    this->Connect(wxEVT_SHOW, wxShowEventHandler(FreeDVReporterDialog::OnShow));
    
    m_listSpots->Connect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_COL_CLICK, wxListEventHandler(FreeDVReporterDialog::OnSortColumn), NULL, this);
    m_listSpots->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(FreeDVReporterDialog::OnDoubleClick), NULL, this);
    
    m_statusMessage->Connect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextChange), NULL, this);

    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
    
    m_bandFilter->Connect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnBandFilterChange), NULL, this);
    m_trackFrequency->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackFreqBand->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackExactFreq->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    
    // Trigger sorting on last sorted column
    sortColumn_(wxGetApp().appConfiguration.reporterWindowCurrentSort, wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    
    // Update status message
    m_statusMessage->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText);

    // Trigger filter update if we're starting with tracking enabled
    m_trackFreqBand->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFreqBand);
    m_trackExactFreq->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq);
    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency)
    {
        m_bandFilter->Enable(false);
        m_trackFreqBand->Enable(true);
        m_trackExactFreq->Enable(true);
        
        FilterFrequency freq = 
            getFilterForFrequency_(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
        setBandFilter(freq);
    }
}

FreeDVReporterDialog::~FreeDVReporterDialog()
{
    m_highlightClearTimer->Stop();
    
    m_trackFrequency->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackFreqBand->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackExactFreq->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    
    this->Disconnect(wxEVT_TIMER, wxTimerEventHandler(FreeDVReporterDialog::OnTimer), NULL, this);
    this->Disconnect(wxEVT_SIZE, wxSizeEventHandler(FreeDVReporterDialog::OnSize));
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    this->Disconnect(wxEVT_MOVE, wxMoveEventHandler(FreeDVReporterDialog::OnMove));
    this->Disconnect(wxEVT_SHOW, wxShowEventHandler(FreeDVReporterDialog::OnShow));
    
    m_listSpots->Disconnect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Disconnect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);
    m_listSpots->Disconnect(wxEVT_LIST_COL_CLICK, wxListEventHandler(FreeDVReporterDialog::OnSortColumn), NULL, this);
    m_listSpots->Disconnect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(FreeDVReporterDialog::OnDoubleClick), NULL, this);
    
    m_statusMessage->Disconnect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextChange), NULL, this);

    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
    
    m_bandFilter->Disconnect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnBandFilterChange), NULL, this);
}

void FreeDVReporterDialog::refreshLayout()
{
    wxListItem item;
    m_listSpots->GetColumn(2, item);

    if (wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances)
    {
        item.SetText("KM");
    }
    else
    {
        item.SetText("Miles");
    }

    m_listSpots->SetColumn(2, item);
    
    // Refresh frequency units as appropriate.
    m_listSpots->GetColumn(4, item);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        item.SetText("KHz");
    }
    else
    {
        item.SetText("MHz");
    }
    m_listSpots->SetColumn(4, item);

    for (auto& kvp : allReporterData_)
    {
        double frequencyUserReadable = kvp.second->frequency / 1000.0;
        wxString frequencyString;
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            frequencyString = wxString::Format(_("%.01f"), frequencyUserReadable);
        }
        else
        {
            frequencyUserReadable /= 1000.0;
            frequencyString = wxString::Format(_("%.04f"), frequencyUserReadable);
        }
        
        kvp.second->freqString = frequencyString;
        
        addOrUpdateListIfNotFiltered_(kvp.second);
    }
}

void FreeDVReporterDialog::setReporter(std::shared_ptr<FreeDVReporter> reporter)
{
    if (reporter_)
    {
        reporter_->setOnReporterConnectFn(FreeDVReporter::ReporterConnectionFn());
        reporter_->setOnReporterDisconnectFn(FreeDVReporter::ReporterConnectionFn());
        
        reporter_->setOnUserConnectFn(FreeDVReporter::ConnectionDataFn());
        reporter_->setOnUserDisconnectFn(FreeDVReporter::ConnectionDataFn());
        reporter_->setOnFrequencyChangeFn(FreeDVReporter::FrequencyChangeFn());
        reporter_->setOnTransmitUpdateFn(FreeDVReporter::TxUpdateFn());
        reporter_->setOnReceiveUpdateFn(FreeDVReporter::RxUpdateFn());

        reporter_->setMessageUpdateFn(FreeDVReporter::MessageUpdateFn());
    }
    
    reporter_ = reporter;
    
    if (reporter_)
    {
        reporter_->setOnReporterConnectFn(std::bind(&FreeDVReporterDialog::onReporterConnect_, this));
        reporter_->setOnReporterDisconnectFn(std::bind(&FreeDVReporterDialog::onReporterDisconnect_, this));
    
        reporter_->setOnUserConnectFn(std::bind(&FreeDVReporterDialog::onUserConnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnUserDisconnectFn(std::bind(&FreeDVReporterDialog::onUserDisconnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnFrequencyChangeFn(std::bind(&FreeDVReporterDialog::onFrequencyChangeFn_, this, _1, _2, _3, _4, _5));
        reporter_->setOnTransmitUpdateFn(std::bind(&FreeDVReporterDialog::onTransmitUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));
        reporter_->setOnReceiveUpdateFn(std::bind(&FreeDVReporterDialog::onReceiveUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));

        reporter_->setMessageUpdateFn(std::bind(&FreeDVReporterDialog::onMessageUpdateFn_, this, _1, _2, _3));

        // Update status message
        m_statusMessage->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText);
    }
    else
    {
        // Spot list no longer valid, delete the items currently on there
        clearAllEntries_(true);
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

void FreeDVReporterDialog::OnSortColumn(wxListEvent& event)
{
    int col = event.GetColumn();

    if (col > 12)
    {
        // Don't allow sorting by "fake" columns.
        col = -1;
    }

    sortColumn_(col);
}

void FreeDVReporterDialog::OnBandFilterChange(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter = 
        m_bandFilter->GetSelection();
    
    FilterFrequency freq = 
        (FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get();
    setBandFilter(freq);
}

void FreeDVReporterDialog::OnTimer(wxTimerEvent& event)
{
    // Iterate across all visible rows. If a row is currently highlighted
    // green and it's been more than >20 seconds, clear coloring.
    auto curDate = wxDateTime::Now().ToUTC();
    for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
        auto reportData = allReporterData_[*sidPtr];
        
        if (!reportData->transmitting &&
            (!reportData->lastRxDate.IsValid() || !reportData->lastRxDate.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, RX_COLORING_TIMEOUT_SEC))))
        {
            m_listSpots->SetItemBackgroundColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
            m_listSpots->SetItemTextColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
        }
    }
}

void FreeDVReporterDialog::OnFilterTrackingEnable(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency
        = m_trackFrequency->GetValue();
    m_bandFilter->Enable(
        !wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);
    
    m_trackFreqBand->Enable(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);
    m_trackExactFreq->Enable(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);
    
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFreqBand =
        m_trackFreqBand->GetValue();
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq =
        m_trackExactFreq->GetValue();
    
    FilterFrequency freq;
    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency)
    {
        freq = 
            getFilterForFrequency_(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
    }
    else
    {
        freq = 
            (FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get();
    }
    
    setBandFilter(freq);
}

void FreeDVReporterDialog::OnDoubleClick(wxMouseEvent& event)
{
    auto selectedIndex = m_listSpots->GetFirstSelected();
    if (selectedIndex >= 0 && wxGetApp().rigFrequencyController && 
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
    {
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(selectedIndex);
        
        wxGetApp().rigFrequencyController->setFrequency(allReporterData_[*sidPtr]->frequency);
    }
}

void FreeDVReporterDialog::OnStatusTextChange(wxCommandEvent& event)
{
    auto statusMsg = m_statusMessage->GetValue();

    if (reporter_)
    {
        reporter_->updateMessage(statusMsg.ToStdString());
    }

    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText = statusMsg;
}

void FreeDVReporterDialog::refreshQSYButtonState()
{
    // Update filter if the frequency's changed.
    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency)
    {
        FilterFrequency freq = 
            getFilterForFrequency_(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
        
        if (currentBandFilter_ != freq || wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq)
        {
            setBandFilter(freq);
        }
    }
    
    bool enabled = false;
    auto selectedIndex = m_listSpots->GetFirstSelected();
    if (selectedIndex >= 0)
    {
        auto selectedCallsign = m_listSpots->GetItemText(selectedIndex);
    
        if (reporter_->isValidForReporting() &&
            selectedCallsign != wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign && 
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 &&
            freedvInterface.isRunning())
        {
            wxString theirFreqString = m_listSpots->GetItemText(selectedIndex, 4);
            
            uint64_t theirFreq = 0;
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
            {
                theirFreq = wxAtof(theirFreqString) * 1000;
            }
            else
            {
                theirFreq = wxAtof(theirFreqString) * 1000 * 1000;
            }
            enabled = theirFreq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
        }
    }
    
    m_buttonSendQSY->Enable(enabled);
}

void FreeDVReporterDialog::setBandFilter(FilterFrequency freq)
{
    currentBandFilter_ = freq;
    
    // Update displayed list based on new filter criteria.
    clearAllEntries_(false);
    for (auto& kvp : allReporterData_)
    {
        addOrUpdateListIfNotFiltered_(kvp.second);
    }
}

void FreeDVReporterDialog::sortColumn_(int col)
{
    bool direction = true;
    
    if (currentSortColumn_ != col)
    {
        direction = true;
    }
    else if (sortAscending_)
    {
        direction = false;
    }
    else
    {
        col = -1;
    }

    sortColumn_(col, direction);
    wxGetApp().appConfiguration.reporterWindowCurrentSort = col;
    wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = direction;
}

void FreeDVReporterDialog::sortColumn_(int col, bool direction)
{
    if (currentSortColumn_ != -1)
    {
        m_listSpots->ClearColumnImage(currentSortColumn_);
    }

    sortAscending_ = direction;
    currentSortColumn_ = col;
    
    if (currentSortColumn_ != -1)
    {
        m_listSpots->SetColumnImage(currentSortColumn_, direction ? upIconIndex_ : downIconIndex_);
        m_listSpots->SortItems(&FreeDVReporterDialog::ListCompareFn_, (wxIntPtr)this);
    }
}

void FreeDVReporterDialog::clearAllEntries_(bool clearForAllBands)
{
    if (clearForAllBands)
    {
        for (auto& kvp : allReporterData_)
        {
            delete kvp.second;
        }
        allReporterData_.clear();
    }
    
    for (auto index = m_listSpots->GetItemCount() - 1; index >= 0; index--)
    {
        delete (std::string*)m_listSpots->GetItemData(index);
        m_listSpots->DeleteItem(index);
    }
    
    // Reset lengths to force auto-resize on (re)connect.
    for (int col = 0; col < NUM_COLS; col++)
    {
        columnLengths_[col] = 0;
    }
}

double FreeDVReporterDialog::calculateDistance_(wxString gridSquare1, wxString gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0 ;
    
    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare_(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare_(gridSquare2, lat2, lon2);
    
    // Use Haversine formula to calculate distance. See
    // https://stackoverflow.com/questions/27928/calculate-distance-between-two-latitude-longitude-points-haversine-formula.
    const double EARTH_RADIUS = 6371;
    double dLat = DegreesToRadians_(lat2-lat1);
    double dLon = DegreesToRadians_(lon2-lon1); 
    double a = 
        sin(dLat/2) * sin(dLat/2) +
        cos(DegreesToRadians_(lat1)) * cos(DegreesToRadians_(lat2)) * 
        sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a)); 
    return EARTH_RADIUS * c;
}

void FreeDVReporterDialog::calculateLatLonFromGridSquare_(wxString gridSquare, double& lat, double& lon)
{
    char charA = 'A';
    char char0 = '0';
    
    // Uppercase grid square for easier processing
    gridSquare.MakeUpper();
    
    // Start from antimeridian South Pole (e.g. over the Pacific, not over the UK)
    lon = -180.0;
    lat = -90.0;
    
    // Process first two characters
    lon += ((char)gridSquare.GetChar(0) - charA) * 20;
    lat += ((char)gridSquare.GetChar(1) - charA) * 10;
    
    // Then next two
    lon += ((char)gridSquare.GetChar(2) - char0) * 2;
    lat += ((char)gridSquare.GetChar(3) - char0) * 1;
    
    // If grid square is 6 or more letters, THEN use the next two.
    // Otherwise, optional.
    if (gridSquare.Length() >= 6)
    {
        lon += ((char)gridSquare.GetChar(4) - charA) * 5.0 / 60;
        lat += ((char)gridSquare.GetChar(5) - charA) * 2.5 / 60;
    }
    
    // Center in middle of grid square
    if (gridSquare.Length() >= 6)
    {
        lon += 5.0 / 60 / 2;
        lat += 2.5 / 60 / 2;
    }
    else
    {
        lon += 2 / 2;
        lat += 1.0 / 2;
    }
}

double FreeDVReporterDialog::DegreesToRadians_(double degrees)
{
    return degrees * (M_PI / 180.0);
}

int FreeDVReporterDialog::ListCompareFn_(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
{
    FreeDVReporterDialog* thisPtr = (FreeDVReporterDialog*)sortData;
    std::string* leftSid = (std::string*)item1;
    std::string* rightSid = (std::string*)item2;
    auto leftData = thisPtr->allReporterData_[*leftSid];
    auto rightData = thisPtr->allReporterData_[*rightSid];

    int result = 0;

    switch(thisPtr->currentSortColumn_)
    {
        case 0:
            result = leftData->callsign.CmpNoCase(rightData->callsign);
            break;
        case 1:
            result = leftData->gridSquare.CmpNoCase(rightData->gridSquare);
            break;
        case 2:
            result = leftData->distanceVal - rightData->distanceVal;
            break;
        case 3:
            result = leftData->version.CmpNoCase(rightData->version);
            break;
        case 4:
            result = leftData->frequency - rightData->frequency;
            break;
        case 5:
            result = leftData->status.CmpNoCase(rightData->status);
            break;
        case 6:
            result = leftData->userMessage.CmpNoCase(rightData->userMessage);
            break;
        case 7:
            if (leftData->lastTxDate.IsValid() && rightData->lastTxDate.IsValid())
            {
                if (leftData->lastTxDate.IsEarlierThan(rightData->lastTxDate))
                {
                    result = -1;
                }
                else if (leftData->lastTxDate.IsLaterThan(rightData->lastTxDate))
                {
                    result = 1;
                }
                else
                {
                    result = 0;
                }
            }
            else if (!leftData->lastTxDate.IsValid() && rightData->lastTxDate.IsValid())
            {
                result = -1;
            }
            else if (leftData->lastTxDate.IsValid() && !rightData->lastTxDate.IsValid())
            {
                result = 1;
            }
            else
            {
                result = 0;
            }
            break;
        case 8:
            result = leftData->txMode.CmpNoCase(rightData->txMode);
            break;
        case 9:
            result = leftData->lastRxCallsign.CmpNoCase(rightData->lastRxCallsign);
            break;
        case 10:
            result = leftData->lastRxMode.CmpNoCase(rightData->lastRxMode);
            break;
        case 11:
            result = leftData->snr.CmpNoCase(rightData->snr);
            break;
        case 12:
            if (leftData->lastUpdateDate.IsValid() && rightData->lastUpdateDate.IsValid())
            {
                if (leftData->lastUpdateDate.IsEarlierThan(rightData->lastUpdateDate))
                {
                    result = -1;
                }
                else if (leftData->lastUpdateDate.IsLaterThan(rightData->lastUpdateDate))
                {
                    result = 1;
                }
                else
                {
                    result = 0;
                }
            }
            else if (!leftData->lastUpdateDate.IsValid() && rightData->lastUpdateDate.IsValid())
            {
                result = -1;
            }
            else if (leftData->lastUpdateDate.IsValid() && !rightData->lastUpdateDate.IsValid())
            {
                result = 1;
            }
            else
            {
                result = 0;
            }
            break;
        default:
            assert(false);
            break;
    }

    if (!thisPtr->sortAscending_)
    {
        result = -result;
    }

    return result;
}

// =================================================================================
// Note: these methods below do not run under the UI thread, so we need to make sure
// UI actions happen there.
// =================================================================================

void FreeDVReporterDialog::onReporterConnect_()
{
    CallAfter([&]() {
        clearAllEntries_(true);
    });
}

void FreeDVReporterDialog::onReporterDisconnect_()
{
    CallAfter([&]() {
        clearAllEntries_(true);        
    });
}

void FreeDVReporterDialog::onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    CallAfter([&, sid, lastUpdate, callsign, gridSquare, version, rxOnly]() {
        // Initially populate stored report data, but don't add to the viewable list just yet. 
        // We only add on frequency update and only if the filters check out.
        ReporterData* temp = new ReporterData;
        assert(temp != nullptr);
        
        wxString gridSquareWxString = gridSquare;
        
        temp->sid = sid;
        temp->callsign = wxString(callsign).Upper();
        temp->gridSquare = gridSquareWxString.Left(2).Upper() + gridSquareWxString.Mid(2);

        if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare == "")
        {
            // Invalid grid square means we can't calculate a distance.
            temp->distance = UNKNOWN_STR;
            temp->distanceVal = 0;
        }
        else
        {
            temp->distanceVal = calculateDistance_(wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare, gridSquareWxString);

            if (!wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances)
            {
                // Convert to miles for those who prefer it
                // (calculateDistance_() returns distance in km).
                temp->distanceVal *= 0.621371;
            }

            if (temp->distanceVal < 10.0)
            {
                temp->distance = wxString::Format("%.01f", temp->distanceVal);
            }
            else
            {
                temp->distance = wxString::Format("%.0f", temp->distanceVal);
            }
        }

        temp->version = version;
        temp->freqString = UNKNOWN_STR;
        temp->transmitting = false;
        
        if (rxOnly)
        {
            temp->status = RX_ONLY_STATUS;
            temp->txMode = UNKNOWN_STR;
            temp->lastTx = UNKNOWN_STR;
        }
        else
        {
            temp->status = UNKNOWN_STR;
            temp->txMode = UNKNOWN_STR;
            temp->lastTx = UNKNOWN_STR;
        }
        
        temp->lastRxCallsign = UNKNOWN_STR;
        temp->lastRxMode = UNKNOWN_STR;
        temp->snr = UNKNOWN_STR;
        
        auto lastUpdateTime = makeValidTime_(lastUpdate, temp->lastUpdateDate);
        temp->lastUpdate = lastUpdateTime;
        
        allReporterData_[sid] = temp;
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
        
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            delete allReporterData_[sid];
            allReporterData_.erase(iter);
        }
    });
}

void FreeDVReporterDialog::onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz)
{
    CallAfter([&, sid, frequencyHz, lastUpdate]() {
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            double frequencyUserReadable = frequencyHz / 1000.0;
            wxString frequencyString;
            
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
            {
                frequencyString = wxString::Format(_("%.01f"), frequencyUserReadable);
            }
            else
            {
                frequencyUserReadable /= 1000.0;
                frequencyString = wxString::Format(_("%.04f"), frequencyUserReadable);
            }
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            
            iter->second->frequency = frequencyHz;
            iter->second->freqString = frequencyString;
            iter->second->lastUpdate = lastUpdateTime;
            
            addOrUpdateListIfNotFiltered_(iter->second);
        }
    });
}

void FreeDVReporterDialog::onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate)
{
    CallAfter([&, sid, txMode, transmitting, lastTxDate, lastUpdate]() {
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            iter->second->transmitting = transmitting;
            
            std::string txStatus = "RX";
            if (transmitting)
            {
                txStatus = "TX";
            }
            
            if (iter->second->status != _(RX_ONLY_STATUS))
            {
                iter->second->status = txStatus;
                iter->second->txMode = txMode;
                
                auto lastTxTime = makeValidTime_(lastTxDate, iter->second->lastTxDate);
                iter->second->lastTx = lastTxTime;
            }
            
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;
            
            addOrUpdateListIfNotFiltered_(iter->second);
        }
    });
}

void FreeDVReporterDialog::onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode)
{
    CallAfter([&, sid, lastUpdate, receivedCallsign, snr, rxMode]() {
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            iter->second->lastRxCallsign = receivedCallsign;
            iter->second->lastRxMode = rxMode;
            
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;
            
            wxString snrString = wxString::Format(_("%.01f"), snr);
            if (receivedCallsign == "" && rxMode == "")
            {
                // Frequency change--blank out SNR too.
                iter->second->lastRxCallsign = UNKNOWN_STR;
                iter->second->lastRxMode = UNKNOWN_STR;
                iter->second->snr = UNKNOWN_STR;
                iter->second->lastRxDate = wxDateTime();
            }
            else
            {
                iter->second->snr = snrString;
                iter->second->lastRxDate = iter->second->lastUpdateDate;
            }
            
            addOrUpdateListIfNotFiltered_(iter->second);
        }
    });
}

void FreeDVReporterDialog::onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message)
{
    CallAfter([&, sid, lastUpdate, message]() {
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            iter->second->userMessage = message;
            
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;
            
            addOrUpdateListIfNotFiltered_(iter->second);
        }
    });
}

wxString FreeDVReporterDialog::makeValidTime_(std::string timeStr, wxDateTime& timeObj)
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
        timeObj = tmpDate;
        if (wxGetApp().appConfiguration.reportingConfiguration.useUTCForReporting)
        {
            timeZone = wxDateTime::TimeZone(wxDateTime::TZ::UTC);
        }
        else
        {
            timeZone = wxDateTime::TimeZone(wxDateTime::TZ::Local);
        }
        return tmpDate.Format(_("%x %X"), timeZone);
    }
    else
    {
        timeObj = wxDateTime();
        return _(UNKNOWN_STR);
    }
}

void FreeDVReporterDialog::addOrUpdateListIfNotFiltered_(ReporterData* data)
{
    bool filtered = isFiltered_(data->frequency);
    int itemIndex = -1;
    
    for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
        if (data->sid == *sidPtr)
        {
            itemIndex = index;
            break;
        }
    }
    
    bool needResort = false;

    if (itemIndex >= 0 && filtered)
    {
        // Remove as it has been filtered out.       
        delete (std::string*)m_listSpots->GetItemData(itemIndex);
        m_listSpots->DeleteItem(itemIndex);       
        return;
    }
    else if (itemIndex == -1 && !filtered)
    {
        itemIndex = m_listSpots->InsertItem(m_listSpots->GetItemCount(), data->callsign);
        m_listSpots->SetItemPtrData(itemIndex, (wxUIntPtr)new std::string(data->sid));
        needResort = currentSortColumn_ == 0;
    }
    else if (filtered)
    {
        // Don't add for now as it's not supposed to display.
        return;
    }
    
    bool changed = setColumnForRow_(itemIndex, 1, data->gridSquare);
    needResort |= changed && currentSortColumn_ == 1;
    changed = setColumnForRow_(itemIndex, 2, data->distance);
    needResort |= changed && currentSortColumn_ == 2;
    changed = setColumnForRow_(itemIndex, 3, data->version);
    needResort |= changed && currentSortColumn_ == 3;
    changed = setColumnForRow_(itemIndex, 4, data->freqString);
    needResort |= changed && currentSortColumn_ == 4;
    changed = setColumnForRow_(itemIndex, 5, data->status);
    needResort |= changed && currentSortColumn_ == 5;
    changed = setColumnForRow_(itemIndex, 6, data->userMessage);
    needResort |= changed && currentSortColumn_ == 6;
    changed = setColumnForRow_(itemIndex, 7, data->lastTx);
    needResort |= changed && currentSortColumn_ == 7;
    changed = setColumnForRow_(itemIndex, 8, data->txMode);
    needResort |= changed && currentSortColumn_ == 8;
    changed = setColumnForRow_(itemIndex, 9, data->lastRxCallsign);
    needResort |= changed && currentSortColumn_ == 9;
    changed = setColumnForRow_(itemIndex, 10, data->lastRxMode);
    needResort |= changed && currentSortColumn_ == 10;
    changed = setColumnForRow_(itemIndex, 11, data->snr);
    needResort |= changed && currentSortColumn_ == 11;
    changed = setColumnForRow_(itemIndex, 12, data->lastUpdate);
    needResort |= changed && currentSortColumn_ == 12;
    
    if (data->transmitting)
    {
        wxColour txBackgroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor);
        wxColour txForegroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor);
        m_listSpots->SetItemBackgroundColour(itemIndex, txBackgroundColor);
        m_listSpots->SetItemTextColour(itemIndex, txForegroundColor);
    }
    else if (data->lastRxDate.IsValid() && data->lastRxDate.ToUTC().IsEqualUpTo(wxDateTime::Now().ToUTC(), wxTimeSpan(0, 0, RX_COLORING_TIMEOUT_SEC)))
    {
        wxColour rxBackgroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor);
        wxColour rxForegroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor);
        m_listSpots->SetItemBackgroundColour(itemIndex, rxBackgroundColor);
        m_listSpots->SetItemTextColour(itemIndex, rxForegroundColor);
    }
    else
    {
        m_listSpots->SetItemBackgroundColour(itemIndex, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        m_listSpots->SetItemTextColour(itemIndex, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
    }

    if (needResort)
    {
        m_listSpots->SortItems(&FreeDVReporterDialog::ListCompareFn_, (wxIntPtr)this);
    }
}

bool FreeDVReporterDialog::setColumnForRow_(int row, int col, wxString val)
{
    bool result = false;
    auto oldText = m_listSpots->GetItemText(row, col);
    
    if (oldText != val)
    {
        result = true;
        m_listSpots->SetItem(row, col, val);
    
        auto itemFont = m_listSpots->GetItemFont(row);
        int textWidth = 0;
        int textHeight = 0; // Note: unused
    
        // Note: if the font is invalid we should just use the default.
        if (itemFont.IsOk())
        {
            GetTextExtent(val, &textWidth, &textHeight, nullptr, nullptr, &itemFont);
        }
        else
        {
            GetTextExtent(val, &textWidth, &textHeight);
        }
    
        if (textWidth > columnLengths_[col])
        {
            columnLengths_[col] = textWidth;
            m_listSpots->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
        }
    }

    return result;
}

FreeDVReporterDialog::FilterFrequency FreeDVReporterDialog::getFilterForFrequency_(uint64_t freq)
{
    auto bandForFreq = FilterFrequency::BAND_OTHER;
    
    if (freq >= 1800000 && freq <= 2000000)
    {
        bandForFreq = FilterFrequency::BAND_160M;
    }
    else if (freq >= 3500000 && freq <= 4000000)
    {
        bandForFreq = FilterFrequency::BAND_80M;
    }
    else if (freq >= 5250000 && freq <= 5450000)
    {
        bandForFreq = FilterFrequency::BAND_60M;
    }
    else if (freq >= 7000000 && freq <= 7300000)
    {
        bandForFreq = FilterFrequency::BAND_40M;
    }
    else if (freq >= 10100000 && freq <= 10150000)
    {
        bandForFreq = FilterFrequency::BAND_30M;
    }
    else if (freq >= 14000000 && freq <= 14350000)
    {
        bandForFreq = FilterFrequency::BAND_20M;
    }
    else if (freq >= 18068000 && freq <= 18168000)
    {
        bandForFreq = FilterFrequency::BAND_17M;
    }
    else if (freq >= 21000000 && freq <= 21450000)
    {
        bandForFreq = FilterFrequency::BAND_15M;
    }
    else if (freq >= 24890000 && freq <= 24990000)
    {
        bandForFreq = FilterFrequency::BAND_12M;
    }
    else if (freq >= 28000000 && freq <= 29700000)
    {
        bandForFreq = FilterFrequency::BAND_10M;
    }
    else if (freq >= 50000000)
    {
        bandForFreq = FilterFrequency::BAND_VHF_UHF;
    }
    
    return bandForFreq;
}

bool FreeDVReporterDialog::isFiltered_(uint64_t freq)
{
    auto bandForFreq = getFilterForFrequency_(freq);
    
    if (currentBandFilter_ == FilterFrequency::BAND_ALL)
    {
        return false;
    }
    else
    {
        return 
            (bandForFreq != currentBandFilter_) ||
            (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency &&
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq &&
                freq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
    }
}
