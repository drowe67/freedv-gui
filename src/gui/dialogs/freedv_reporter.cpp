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
#include <wx/display.h>
#include <wx/clipbrd.h>
#include "freedv_reporter.h"

#include "freedv_interface.h"

extern FreeDVInterface freedvInterface;

#define CALLSIGN_COL (0)
#define GRID_SQUARE_COL (1)
#define DISTANCE_COL (2)
#define HEADING_COL (3)
#define VERSION_COL (4)
#define FREQUENCY_COL (5)
#define TX_MODE_COL (6)
#define STATUS_COL (7)
#define USER_MESSAGE_COL (8)
#define LAST_TX_DATE_COL (9)
#define LAST_RX_CALLSIGN_COL (10)
#define LAST_RX_MODE_COL (11)
#define SNR_COL (12)
#define LAST_UPDATE_DATE_COL (13)

#define UNKNOWN_STR ""
#if defined(WIN32)
#define NUM_COLS (LAST_UPDATE_DATE_COL + 2) /* Note: need empty column 0 to work around callsign truncation issue. */
#else
#define NUM_COLS (LAST_UPDATE_DATE_COL + 1)
#endif // defined(WIN32)
#define RX_ONLY_STATUS "RX Only"
#define RX_COLORING_LONG_TIMEOUT_SEC (20)
#define RX_COLORING_SHORT_TIMEOUT_SEC (2)
#define MSG_COLORING_TIMEOUT_SEC (5)
#define STATUS_MESSAGE_MRU_MAX_SIZE (10)
#define MESSAGE_COLUMN_ID (6)

using namespace std::placeholders;

static int DefaultColumnWidths_[] = {
#if defined(WIN32)
    1,
#endif // defined(WIN32)
    70,
    65,
    60,
    60,
    70,
    60,
    65,
    60,
    130,
    60,
    65,
    60,
    60,
    100,
    1
};

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxFrame(parent, id, title, pos, size, style)
    , tipWindow_(nullptr)
    , reporter_(nullptr)
    , currentBandFilter_(FreeDVReporterDialog::BAND_ALL)
    , currentSortColumn_(-1)
    , sortAscending_(false)
    , isConnected_(false)
    , filterSelfMessageUpdates_(false)
    , filteredFrequency_(0)
{
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", _("FreeDV Reporter"), wxGetApp().customConfigFileName));
    }
    
    for (int col = 0; col < NUM_COLS; col++)
    {
        columnLengths_[col] = DefaultColumnWidths_[col];
    }

    int userMsgDefaultColWidth = wxGetApp().appConfiguration.reportingUserMsgColWidth;
    int userColNum = USER_MESSAGE_COL;
#if defined(WIN32)
    userColNum++;
#endif // defined(WIN32)
    DefaultColumnWidths_[userColNum] = userMsgDefaultColWidth;

    // Create top-level of control hierarchy.
    wxFlexGridSizer* sectionSizer = new wxFlexGridSizer(2, 1, 0, 0);
    sectionSizer->AddGrowableRow(0);
    sectionSizer->AddGrowableCol(0);
        
    // Main list box
    // =============================
    int col = 0;
    m_listSpots = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_SINGLE_SEL | wxLC_REPORT | wxLC_HRULES);

#if defined(WIN32)
    // Create "hidden" column at the beginning. The column logic in wxWidgets
    // seems to want to add an image to column 0, which affects
    // autosizing.
    m_listSpots->InsertColumn(col, wxT(""), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
#endif // defined(WIN32)

    m_listSpots->InsertColumn(col, wxT("Callsign"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Locator"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("km"), wxLIST_FORMAT_RIGHT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Hdg"), wxLIST_FORMAT_RIGHT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Version"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz ? wxT("kHz") : wxT("MHz"), wxLIST_FORMAT_RIGHT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Mode"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Status"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Msg"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Last TX"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("RX Call"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Mode"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("SNR"), wxLIST_FORMAT_RIGHT, DefaultColumnWidths_[col]);
    col++;
    m_listSpots->InsertColumn(col, wxT("Last Update"), wxLIST_FORMAT_LEFT, DefaultColumnWidths_[col]);
    col++;

    // On Windows, the last column will end up taking a lot more space than desired regardless
    // of the space we actually need. Create a "dummy" column to take that space instead.
    m_listSpots->InsertColumn(col, wxT(""), wxLIST_FORMAT_LEFT, 1);
    col++;

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

    m_buttonOK = new wxButton(this, wxID_OK, _("Close"));
    m_buttonOK->SetToolTip(_("Closes FreeDV Reporter window."));
    reportingSettingsSizer->Add(m_buttonOK, 0, wxALL, 5);

    m_buttonSendQSY = new wxButton(this, wxID_ANY, _("Request QSY"));
    m_buttonSendQSY->SetToolTip(_("Asks selected user to switch to your frequency."));
    m_buttonSendQSY->Enable(false); // disable by default unless we get a valid selection
    reportingSettingsSizer->Add(m_buttonSendQSY, 0, wxALL, 5);

    m_buttonDisplayWebpage = new wxButton(this, wxID_ANY, _("Website"));
    m_buttonDisplayWebpage->SetToolTip(_("Opens FreeDV Reporter in your Web browser."));
    reportingSettingsSizer->Add(m_buttonDisplayWebpage, 0, wxALL, 5);

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
    
    reportingSettingsSizer->Add(new wxStaticText(this, wxID_ANY, _("Band:"), wxDefaultPosition, wxDefaultSize, 0), 
                          0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_bandFilter = new wxComboBox(
        this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
        sizeof(bandList) / sizeof(wxString), bandList, wxCB_DROPDOWN | wxCB_READONLY);
    m_bandFilter->SetSelection(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter);
    setBandFilter((FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get());
    
    reportingSettingsSizer->Add(m_bandFilter, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    
    m_trackFrequency = new wxCheckBox(this, wxID_ANY, _("Track:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    reportingSettingsSizer->Add(m_trackFrequency, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    
    m_trackFreqBand = new wxRadioButton(this, wxID_ANY, _("band"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    reportingSettingsSizer->Add(m_trackFreqBand, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackFreqBand->Enable(false);
    
    m_trackExactFreq = new wxRadioButton(this, wxID_ANY, _("freq."), wxDefaultPosition, wxDefaultSize, 0);
    reportingSettingsSizer->Add(m_trackExactFreq, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackExactFreq->Enable(false);
    
    m_trackFrequency->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);

    auto statusMessageLabel = new wxStaticText(this, wxID_ANY, _("Message:"), wxDefaultPosition, wxDefaultSize);
    reportingSettingsSizer->Add(statusMessageLabel, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_statusMessage = new wxComboBox(this, wxID_ANY, _(""), wxDefaultPosition, wxSize(180, -1), 0, nullptr, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    reportingSettingsSizer->Add(m_statusMessage, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonSend = new wxButton(this, wxID_ANY, _("Send"));
    m_buttonSend->SetToolTip(_("Sends message to FreeDV Reporter. Right-click for additional options."));
    reportingSettingsSizer->Add(m_buttonSend, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonClear = new wxButton(this, wxID_ANY, _("Clear"));
    m_buttonClear->SetToolTip(_("Clears message from FreeDV Reporter. Right-click for additional options."));
    reportingSettingsSizer->Add(m_buttonClear, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    bottomRowSizer->Add(reportingSettingsSizer, 0, wxALL | wxALIGN_CENTER, 0);

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

    // Make sure we didn't end up placing it off the screen in a location that can't
    // easily be brought back.
    auto displayNo = wxDisplay::GetFromWindow(this);
    if (displayNo == wxNOT_FOUND)
    {
        displayNo = 0;
    }
    wxDisplay currentDisplay(displayNo);
    wxPoint actualPos = GetPosition();
    wxSize actualWindowSize = GetSize();
    wxRect actualDisplaySize = currentDisplay.GetClientArea();
    if (actualPos.x < (-0.9 * actualWindowSize.GetWidth()) ||
        actualPos.x > (0.9 * actualDisplaySize.GetWidth()))
    {
        actualPos.x = 0;
    }
    if (actualPos.y < 0 ||
        actualPos.y > (0.9 * actualDisplaySize.GetHeight()))
    {
        actualPos.y = 0;
    }
    wxGetApp().appConfiguration.reporterWindowLeft = actualPos.x;
    wxGetApp().appConfiguration.reporterWindowTop = actualPos.y;
    SetPosition(actualPos);

    // Set up highlight clear timer
    m_highlightClearTimer = new wxTimer(this);
    m_highlightClearTimer->Start(1000);

    // Create Set popup menu
    setPopupMenu_ = new wxMenu();
    assert(setPopupMenu_ != nullptr);
    
    auto setSaveMenuItem = setPopupMenu_->Append(wxID_ANY, _("Send and save message"));
    setPopupMenu_->Connect(
        setSaveMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextSendAndSaveMessage),
        NULL,
        this);

    auto saveMenuItem = setPopupMenu_->Append(wxID_ANY, _("Only save message"));
    setPopupMenu_->Connect(
        saveMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextSaveMessage),
        NULL,
        this);

    // Create Clear popup menu
    clearPopupMenu_ = new wxMenu();
    assert(clearPopupMenu_ != nullptr);

    auto clearSelectedMenuItem = clearPopupMenu_->Append(wxID_ANY, _("Remove selected from list"));
    clearPopupMenu_->Connect(
        clearSelectedMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED,
        wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextClearSelected),
        NULL,
        this
    );

    auto clearAllMenuItem = clearPopupMenu_->Append(wxID_ANY, _("Clear all messages from list"));
    clearPopupMenu_->Connect(
        clearAllMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED,
        wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextClearAll),
        NULL,
        this
    );
        
    // Create popup menu for spots list
    spotsPopupMenu_ = new wxMenu();
    assert(spotsPopupMenu_ != nullptr);

    auto copyUserMessageMenuItem = spotsPopupMenu_->Append(wxID_ANY, _("Copy message"));
    spotsPopupMenu_->Connect(
        copyUserMessageMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnCopyUserMessage),
        NULL,
        this);
    
    // Hook in events
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(FreeDVReporterDialog::OnTimer), NULL, this);
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(FreeDVReporterDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(FreeDVReporterDialog::OnClose));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(FreeDVReporterDialog::OnSize));
    this->Connect(wxEVT_MOVE, wxMoveEventHandler(FreeDVReporterDialog::OnMove));
    this->Connect(wxEVT_SHOW, wxShowEventHandler(FreeDVReporterDialog::OnShow));
    this->Connect(wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEventHandler(FreeDVReporterDialog::OnSystemColorChanged));
    
    m_listSpots->Connect(wxEVT_LIST_ITEM_SELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemSelected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_ITEM_DESELECTED, wxListEventHandler(FreeDVReporterDialog::OnItemDeselected), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_COL_CLICK, wxListEventHandler(FreeDVReporterDialog::OnSortColumn), NULL, this);
    m_listSpots->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(FreeDVReporterDialog::OnDoubleClick), NULL, this);
    m_listSpots->Connect(wxEVT_MOTION, wxMouseEventHandler(FreeDVReporterDialog::AdjustToolTip), NULL, this);
    m_listSpots->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
    m_listSpots->Connect(wxEVT_LIST_COL_DRAGGING, wxListEventHandler(FreeDVReporterDialog::AdjustMsgColWidth), NULL, this);
    
    m_statusMessage->Connect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextChange), NULL, this);
    m_buttonSend->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextSend), NULL, this);
    m_buttonSend->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnStatusTextSendContextMenu), NULL, this);
    m_buttonClear->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextClear), NULL, this);
    m_buttonClear->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnStatusTextClearContextMenu), NULL, this);

    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
    
    m_bandFilter->Connect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnBandFilterChange), NULL, this);
    m_trackFrequency->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackFreqBand->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    m_trackExactFreq->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(FreeDVReporterDialog::OnFilterTrackingEnable), NULL, this);
    
    // Trigger sorting on last sorted column
    sortColumn_(wxGetApp().appConfiguration.reporterWindowCurrentSort, wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    
    // Update status message and MRU list
    m_statusMessage->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText);
    for (auto& msg : wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts.get())
    {
        m_statusMessage->Append(msg);
    }

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
    delete m_highlightClearTimer;

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
    m_listSpots->Disconnect(wxEVT_MOTION, wxMouseEventHandler(FreeDVReporterDialog::AdjustToolTip), NULL, this);
    m_listSpots->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
    m_listSpots->Disconnect(wxEVT_LIST_COL_DRAGGING, wxListEventHandler(FreeDVReporterDialog::AdjustMsgColWidth), NULL, this);
    
    m_statusMessage->Disconnect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextChange), NULL, this);
    m_buttonSend->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextSend), NULL, this);
    m_buttonClear->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnStatusTextClear), NULL, this);
    m_buttonSend->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnStatusTextSendContextMenu), NULL, this);
    m_buttonClear->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnStatusTextClearContextMenu), NULL, this);

    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOK), NULL, this);
    m_buttonSendQSY->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnSendQSY), NULL, this);
    m_buttonDisplayWebpage->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(FreeDVReporterDialog::OnOpenWebsite), NULL, this);
    
    m_bandFilter->Disconnect(wxEVT_TEXT, wxCommandEventHandler(FreeDVReporterDialog::OnBandFilterChange), NULL, this);
}

bool FreeDVReporterDialog::isTextMessageFieldInFocus()
{
    return m_statusMessage->HasFocus();
}

void FreeDVReporterDialog::refreshLayout()
{
    int colOffset = 0;

#if defined(WIN32)
    // Column 0 is hidden, so everything is shifted by 1 column.
    colOffset++;
#endif // defined(WIN32)

    wxListItem item;
    m_listSpots->GetColumn(DISTANCE_COL + colOffset, item);

    if (wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances)
    {
        item.SetText("km ");
    }
    else
    {
        item.SetText("Miles");
    }

    m_listSpots->SetColumn(DISTANCE_COL + colOffset, item);
    
    // Refresh frequency units as appropriate.
    m_listSpots->GetColumn(FREQUENCY_COL + colOffset, item);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        item.SetText("kHz");
    }
    else
    {
        item.SetText("MHz");
    }
    m_listSpots->SetColumn(FREQUENCY_COL + colOffset, item);

    // Change direction/heading column label based on preferences
    m_listSpots->GetColumn(HEADING_COL + colOffset, item);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
    {
        item.SetText("Dir");
        item.SetAlign(wxLIST_FORMAT_LEFT);
    }
    else
    {
        item.SetText("Hdg");
        item.SetAlign(wxLIST_FORMAT_RIGHT);
    }
    m_listSpots->SetColumn(HEADING_COL + colOffset, item);

    std::map<int, int> colResizeList;
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

        // Refresh cardinal vs. degree directions.
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare != kvp.second->gridSquare)
        {
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
            {
                kvp.second->heading = GetCardinalDirection_(kvp.second->headingVal);
            }
            else
            {
                kvp.second->heading = wxString::Format("%.0f", kvp.second->headingVal);
            }
        }
        
        addOrUpdateListIfNotFiltered_(kvp.second, colResizeList);
    }
    resizeChangedColumns_(colResizeList);

    // Update status controls.
    wxCommandEvent tmpEvent;
    OnStatusTextChange(tmpEvent);
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
        reporter_->setConnectionSuccessfulFn(FreeDVReporter::ConnectionSuccessfulFn());
        reporter_->setAboutToShowSelfFn(FreeDVReporter::AboutToShowSelfFn());
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
        reporter_->setConnectionSuccessfulFn(std::bind(&FreeDVReporterDialog::onConnectionSuccessfulFn_, this));
        reporter_->setAboutToShowSelfFn(std::bind(&FreeDVReporterDialog::onAboutToShowSelfFn_, this));

        // Update status message
        auto statusMsg = m_statusMessage->GetValue();
        reporter_->updateMessage(statusMsg.utf8_string());
    }
    else
    {
        // Spot list no longer valid, delete the items currently on there
        clearAllEntries_(true);
    }
}

void FreeDVReporterDialog::OnSystemColorChanged(wxSysColourChangedEvent& event)
{
    // Works around issues on wxWidgets with certain controls not changing backgrounds
    // when the user switches between light and dark mode.
    wxColour currentControlBackground = wxTransparentColour;

    m_listSpots->SetBackgroundColour(currentControlBackground);
#if !defined(WIN32)
    ((wxWindow*)m_listSpots->m_headerWin)->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif //!defined(WIN32)

    event.Skip();
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
    std::string url = "https://" + wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->utf8_string() + "/";
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
   
   // Bring up tooltip for longer reporting messages if the user happened to click on that column.
   wxMouseEvent dummyEvent;
   AdjustToolTip(dummyEvent);
}

void FreeDVReporterDialog::OnItemDeselected(wxListEvent& event)
{
    m_buttonSendQSY->Enable(false);
}

void FreeDVReporterDialog::AdjustMsgColWidth(wxListEvent& event)
{
    int col = event.GetColumn();
    int desiredCol = USER_MESSAGE_COL;
#if defined(WIN32)
    desiredCol++;
#endif // defined(WIN32)
    
    if (col != desiredCol)
    {
        return;
    }
    
    int currentColWidth = m_listSpots->GetColumnWidth(desiredCol);
    int textWidth = 0;

    wxGetApp().appConfiguration.reportingUserMsgColWidth = currentColWidth;

    // Iterate through and re-truncate column as required.
    for (int index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
        wxString tempUserMessage = _(" ") + allReporterData_[*sidPtr]->userMessage;  // note: extra space at beginning is to provide extra space from previous col
        
        textWidth = getSizeForTableCellString_(tempUserMessage);
        int tmpLength = allReporterData_[*sidPtr]->userMessage.Length() - 1;
        while (textWidth > currentColWidth && tmpLength >= 0)
        {
            tempUserMessage = allReporterData_[*sidPtr]->userMessage.SubString(0, tmpLength--) + _("...");
            textWidth = getSizeForTableCellString_(tempUserMessage);
        }
        
        if (tmpLength > 0 && tmpLength < (allReporterData_[*sidPtr]->userMessage.Length() - 1))
        {
            tempUserMessage = allReporterData_[*sidPtr]->userMessage.SubString(0, tmpLength) + _("...");
        }
        
        m_listSpots->SetItem(index, desiredCol, tempUserMessage);
    }
}

void FreeDVReporterDialog::OnSortColumn(wxListEvent& event)
{
    int col = event.GetColumn();

#if defined(WIN32)
    // The "hidden" column 0 is new as of 1.9.7. Automatically
    // assume the user is sorting by callsign.
    if (col == 0)
    {
        col = 1;
    }
#endif // defined(WIN32)

    if (col > (NUM_COLS - 1))
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
        
        bool isTransmitting = reportData->transmitting;
        bool isReceivingValidCallsign = 
            reportData->lastRxDate.IsValid() && 
            reportData->lastRxCallsign != "" &&
            reportData->lastRxDate.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, RX_COLORING_LONG_TIMEOUT_SEC));
        bool isReceivingNotValidCallsign = 
            reportData->lastRxDate.IsValid() && 
            reportData->lastRxCallsign == "" &&
            reportData->lastRxDate.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, RX_COLORING_SHORT_TIMEOUT_SEC));
        bool isMessaging = 
            reportData->lastUpdateUserMessage.IsValid() && 
            reportData->lastUpdateUserMessage.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, MSG_COLORING_TIMEOUT_SEC));
        
        // Messaging notifications take highest priority.
        wxColour backgroundColor;
        wxColour foregroundColor;
        
        if (isMessaging)
        {
            backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowBackgroundColor);
            foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowForegroundColor);
        }
        else if (isTransmitting)
        {
            backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor);
            foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor);
        }
        else if (isReceivingValidCallsign || isReceivingNotValidCallsign)
        {
            backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor);
            foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor);
        }
        else
        {
            backgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
            foregroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
        }

        m_listSpots->SetItemBackgroundColour(index, backgroundColor);
        m_listSpots->SetItemTextColour(index, foregroundColor);
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

    // Force refresh of filters since the user expects this to happen immediately after changing a
    // filter setting.
    filteredFrequency_ = 0;
    currentBandFilter_ = BAND_ALL;

    setBandFilter(freq);
}

void FreeDVReporterDialog::OnDoubleClick(wxMouseEvent& event)
{
    auto selectedIndex = m_listSpots->GetFirstSelected();
    if (selectedIndex >= 0 && wxGetApp().rigFrequencyController && 
        (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly))
    {
        std::string* sidPtr = (std::string*)m_listSpots->GetItemData(selectedIndex);
        
        wxGetApp().rigFrequencyController->setFrequency(allReporterData_[*sidPtr]->frequency);
    }
}

void FreeDVReporterDialog::AdjustToolTip(wxMouseEvent& event)
{
    const wxPoint pt = wxGetMousePosition();
    int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
    int mouseY = pt.y - m_listSpots->GetScreenPosition().y;
    
    wxRect rect;
    int desiredCol = USER_MESSAGE_COL;
#if defined(WIN32)
    desiredCol++;
#endif // defined(WIN32)
    
    for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        bool gotUserMessageColBounds = m_listSpots->GetSubItemRect(index, desiredCol, rect);
        bool mouseInBounds = gotUserMessageColBounds && rect.Contains(mouseX, mouseY);
    
        if (gotUserMessageColBounds && mouseInBounds)
        {
            // Show popup corresponding to the full message.
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            tempUserMessage_ = allReporterData_[*sidPtr]->userMessage;
            wxString userMessageTruncated = m_listSpots->GetItemText(index, desiredCol);
            userMessageTruncated = userMessageTruncated.SubString(1, userMessageTruncated.size() - 1);
        
            if (tipWindow_ == nullptr && tempUserMessage_ != userMessageTruncated)
            {
                // Use screen coordinates to determine bounds.
                auto pos = rect.GetPosition();
                rect.SetPosition(ClientToScreen(pos));
            
                tipWindow_ = new wxTipWindow(m_listSpots, tempUserMessage_, 1000, &tipWindow_, &rect);
                tipWindow_->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
                tipWindow_->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::SkipMouseEvent), NULL, this);
                
                // Make sure we actually override behavior of needed events inside the tooltip.
                for (auto& child : tipWindow_->GetChildren())
                {
                    child->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::SkipMouseEvent), NULL, this);
                    child->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
                }
            }
            
            break;
        }
        else
        {
            tempUserMessage_ = _("");
        }
    }
}

void FreeDVReporterDialog::SkipMouseEvent(wxMouseEvent& event)
{
    wxContextMenuEvent contextEvent;
    OnRightClickSpotsList(contextEvent);
}

void FreeDVReporterDialog::OnRightClickSpotsList( wxContextMenuEvent& event )
{
    if (tipWindow_ != nullptr)
    {
        tipWindow_->Close();
        tipWindow_ = nullptr;
    }

    if (tempUserMessage_ != _(""))
    {
        // Only show the popup if we're already hovering over a message.
        const wxPoint pt = wxGetMousePosition();
        int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
        int mouseY = pt.y - m_listSpots->GetScreenPosition().y;

        // 170 here has been determined via experimentation to avoid an issue 
        // on some KDE installations where the popup menu immediately closes after
        // right-clicking. This in effect causes the popup to open to the left of
        // the mouse pointer.
        m_listSpots->PopupMenu(spotsPopupMenu_, wxPoint(mouseX - 170, mouseY));
    }
}

void FreeDVReporterDialog::OnCopyUserMessage(wxCommandEvent& event)
{
    if (wxTheClipboard->Open())
    {
        // This data objects are held by the clipboard,
        // so do not delete them in the app.
        wxTheClipboard->SetData( new wxTextDataObject(tempUserMessage_) );
        wxTheClipboard->Close();
    }
}

void FreeDVReporterDialog::OnStatusTextChange(wxCommandEvent& event)
{
    auto statusMsg = m_statusMessage->GetValue();

    // Prevent entry of text longer than the character limit.
    if (statusMsg != m_statusMessage->GetValue())
    {
        m_statusMessage->SetValue(statusMsg);
    }
}

void FreeDVReporterDialog::OnStatusTextSend(wxCommandEvent& event)
{
    auto statusMsg = m_statusMessage->GetValue(); 

    if (reporter_)
    {
        reporter_->updateMessage(statusMsg.utf8_string());
    }

    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText = statusMsg;

    // If already on the list, move to the top of the list.
    auto location = m_statusMessage->FindString(statusMsg);
    if (location >= 0)
    {
        m_statusMessage->Delete(location);
        m_statusMessage->Insert(statusMsg, 0);

        // Reselect "new" first entry to avoid display issues
        m_statusMessage->SetSelection(0);

        // Preserve current state of the MRU list.
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->clear();
        for (unsigned int index = 0; index < m_statusMessage->GetCount(); index++)
        {
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->push_back(m_statusMessage->GetString(index));
        }
    }   
}

void FreeDVReporterDialog::OnStatusTextSendContextMenu(wxContextMenuEvent& event)
{
    m_buttonSend->PopupMenu(setPopupMenu_);
}

void FreeDVReporterDialog::OnStatusTextSendAndSaveMessage(wxCommandEvent& event)
{
    OnStatusTextSend(event);
    OnStatusTextSaveMessage(event);
}

void FreeDVReporterDialog::OnStatusTextSaveMessage(wxCommandEvent& event)
{
    auto statusMsg = m_statusMessage->GetValue(); 
    if (statusMsg.size() > 0)
    {
        auto location = m_statusMessage->FindString(statusMsg);
        if (location >= 0)
        {
            // Don't save if already in the list.
            return;
        }
        m_statusMessage->Insert(statusMsg, 0);

        // If we have more than the maximum number in the MRU list, 
        // remove from bottom.
        while (m_statusMessage->GetCount() > STATUS_MESSAGE_MRU_MAX_SIZE)
        {
            m_statusMessage->Delete(m_statusMessage->GetCount() - 1);
        }

        // Reselect "new" first entry to avoid display issues
        m_statusMessage->SetSelection(0);

        // Preserve current state of the MRU list.
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->clear();
        for (unsigned int index = 0; index < m_statusMessage->GetCount(); index++)
        {
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->push_back(m_statusMessage->GetString(index));
        }
    }
}

void FreeDVReporterDialog::OnStatusTextClear(wxCommandEvent& event)
{
    if (reporter_)
    {
        reporter_->updateMessage("");
    }

    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText = "";
    m_statusMessage->SetValue("");
}

void FreeDVReporterDialog::OnStatusTextClearContextMenu(wxContextMenuEvent& event)
{
    m_buttonClear->PopupMenu(clearPopupMenu_);
}

void FreeDVReporterDialog::OnStatusTextClearSelected(wxCommandEvent& event)
{
    auto statusMsg = m_statusMessage->GetValue(); 
    if (statusMsg.size() > 0)
    {
        // Remove from MRU list if there.
        auto location = m_statusMessage->FindString(statusMsg);
        if (location >= 0)
        {
            m_statusMessage->Delete(location);
        }

        // Preserve current state of the MRU list.
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->clear();
        for (unsigned int index = 0; index < m_statusMessage->GetCount(); index++)
        {
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->push_back(m_statusMessage->GetString(index));
        }
    }
}

void FreeDVReporterDialog::OnStatusTextClearAll(wxCommandEvent& event)
{
    m_statusMessage->Clear();
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRecentStatusTexts->clear();
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
            wxString theirFreqString = m_listSpots->GetItemText(selectedIndex, 5);
            
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
    if (filteredFrequency_ != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency ||
        currentBandFilter_ != freq)
    {
        filteredFrequency_ = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
        currentBandFilter_ = freq;

        // Update displayed list based on new filter criteria.
        clearAllEntries_(false);

        std::map<int, int> colResizeList;
        for (auto& kvp : allReporterData_)
        {
            addOrUpdateListIfNotFiltered_(kvp.second, colResizeList);
        }
        resizeChangedColumns_(colResizeList);
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
#if defined(WIN32)
    // The hidden column 0 is new in 1.9.7. Assume sort by the "old" column 0
    // (callsign).
    if (col == 0)
    {
        col = 1;
    }
#endif // defined(WIN32)

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
    
    std::vector<std::string*> stringItemsToDelete;
    for (auto index = m_listSpots->GetItemCount() - 1; index >= 0; index--)
    {
        stringItemsToDelete.push_back((std::string*)m_listSpots->GetItemData(index));
    }
    m_listSpots->DeleteAllItems();

    for (auto& ptr : stringItemsToDelete)
    {
        delete ptr;
    }

    // Reset lengths to force auto-resize on (re)connect.
    int userMsgCol = USER_MESSAGE_COL;
#if defined(WIN32)
    userMsgCol++;
#endif // defined(WIN32)
    
    for (int col = 0; col < NUM_COLS; col++)
    {
#if defined(WIN32)
        // First column is hidden, so don't auto-size.
        if (col == 0)
        {
            continue;
        }
#endif // defined(WIN32)

        if (col != userMsgCol)
        {
            columnLengths_[col] = DefaultColumnWidths_[col];
            m_listSpots->SetColumnWidth(col, columnLengths_[col]);
        }
    }
    
    m_listSpots->Update();
}

double FreeDVReporterDialog::calculateDistance_(wxString gridSquare1, wxString gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;
    
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
    wxString optionalSegment = gridSquare.Mid(4, 2);
    wxRegEx allLetters(_("^[A-Z]{2}$"));
    if (gridSquare.Length() >= 6 && allLetters.Matches(optionalSegment))
    {
        lon += ((char)gridSquare.GetChar(4) - charA) * 5.0 / 60;
        lat += ((char)gridSquare.GetChar(5) - charA) * 2.5 / 60;
    
        // Center in middle of grid square
        lon += 5.0 / 60 / 2;
        lat += 2.5 / 60 / 2;
    }
    else
    {
        lon += 2 / 2;
        lat += 1.0 / 2;
    }
}

double FreeDVReporterDialog::calculateBearingInDegrees_(wxString gridSquare1, wxString gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;
    
    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare_(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare_(gridSquare2, lat2, lon2);

    // Convert latitudes and longitudes into radians
    lat1 = DegreesToRadians_(lat1);
    lat2 = DegreesToRadians_(lat2);
    lon1 = DegreesToRadians_(lon1);
    lon2 = DegreesToRadians_(lon2);

    double diffLongitude = lon2 - lon1;
    double x = cos(lat2) * sin(diffLongitude);
    double y = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(diffLongitude));
    double radians = atan2(x, y);

    return RadiansToDegrees_(radians);
}

double FreeDVReporterDialog::DegreesToRadians_(double degrees)
{
    return degrees * (M_PI / 180.0);
}

double FreeDVReporterDialog::RadiansToDegrees_(double radians)
{
    auto result = (radians > 0 ? radians : (2*M_PI + radians)) * 360 / (2*M_PI);
    return (result == 360) ? 0 : result;
}

int FreeDVReporterDialog::ListCompareFn_(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
{
    FreeDVReporterDialog* thisPtr = (FreeDVReporterDialog*)sortData;
    std::string* leftSid = (std::string*)item1;
    std::string* rightSid = (std::string*)item2;
    auto leftData = thisPtr->allReporterData_[*leftSid];
    auto rightData = thisPtr->allReporterData_[*rightSid];

    int result = 0;

#if defined(WIN32)
    switch(thisPtr->currentSortColumn_ - 1)
#else
    switch(thisPtr->currentSortColumn_)
#endif //defined(WIN32)
    {
        case CALLSIGN_COL:
            result = leftData->callsign.CmpNoCase(rightData->callsign);
            break;
        case GRID_SQUARE_COL:
            result = leftData->gridSquare.CmpNoCase(rightData->gridSquare);
            break;
        case DISTANCE_COL:
            result = leftData->distanceVal - rightData->distanceVal;
            break;
        case HEADING_COL:
            result = leftData->headingVal - rightData->headingVal;
            break;
        case VERSION_COL:
            result = leftData->version.CmpNoCase(rightData->version);
            break;
        case FREQUENCY_COL:
            result = leftData->frequency - rightData->frequency;
            break;
        case TX_MODE_COL:
            result = leftData->txMode.CmpNoCase(rightData->txMode);
            break;
        case STATUS_COL:
            result = leftData->status.CmpNoCase(rightData->status);
            break;
        case USER_MESSAGE_COL:
            result = leftData->userMessage.CmpNoCase(rightData->userMessage);
            break;
        case LAST_TX_DATE_COL:
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
        case LAST_RX_CALLSIGN_COL:
            result = leftData->lastRxCallsign.CmpNoCase(rightData->lastRxCallsign);
            break;
        case LAST_RX_MODE_COL:
            result = leftData->lastRxMode.CmpNoCase(rightData->lastRxMode);
            break;
        case SNR_COL:
            result = leftData->snr.CmpNoCase(rightData->snr);
            break;
        case LAST_UPDATE_DATE_COL:
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

void FreeDVReporterDialog::execQueuedAction_()
{
    // This ensures that we handle server events in the order they're received.
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_[0]();
    fnQueue_.erase(fnQueue_.begin());
}

// =================================================================================
// Note: these methods below do not run under the UI thread, so we need to make sure
// UI actions happen there.
// =================================================================================

void FreeDVReporterDialog::onReporterConnect_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);

    fnQueue_.push_back([&]() {
        filterSelfMessageUpdates_ = false;
        clearAllEntries_(true);
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onReporterDisconnect_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&]() {
        isConnected_ = false;
        filterSelfMessageUpdates_ = false;
        clearAllEntries_(true);        
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);

    fnQueue_.push_back([&, sid, lastUpdate, callsign, gridSquare, version, rxOnly]() {
        // Initially populate stored report data, but don't add to the viewable list just yet. 
        // We only add on frequency update and only if the filters check out.
        ReporterData* temp = new ReporterData;
        assert(temp != nullptr);
        
        // Limit grid square display to six characters.
        wxString gridSquareWxString = wxString(gridSquare).Left(6);
        
        temp->sid = sid;
        temp->callsign = wxString(callsign).Upper();
        temp->gridSquare = gridSquareWxString.Left(2).Upper() + gridSquareWxString.Mid(2, 2);

        // Lowercase final letters of grid square per standard.
        if (gridSquareWxString.Length() >= 6)
        {
            temp->gridSquare += gridSquareWxString.Mid(4, 2).Lower();
        }

        wxRegEx gridSquareRegex(_("^[A-Za-z]{2}[0-9]{2}"));
        bool validCharactersInGridSquare = gridSquareRegex.Matches(temp->gridSquare);

        if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare == "" ||
            !validCharactersInGridSquare)
        {
            // Invalid grid square means we can't calculate a distance.
            temp->distance = UNKNOWN_STR;
            temp->distanceVal = 0;
            temp->heading = UNKNOWN_STR;
            temp->headingVal = 0;
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

            temp->distance = wxString::Format("%.0f", temp->distanceVal);

            if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare == gridSquareWxString)
            {
                temp->headingVal = 0;
                temp->heading = UNKNOWN_STR;
            }
            else
            {
                temp->headingVal = calculateBearingInDegrees_(wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare, gridSquareWxString);
                if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
                {
                    temp->heading = GetCardinalDirection_(temp->headingVal);
                }
                else
                {
                    temp->heading = wxString::Format("%.0f", temp->headingVal);
                }
            }
        }

        temp->version = version;
        temp->freqString = UNKNOWN_STR;
        temp->userMessage = UNKNOWN_STR;
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
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onConnectionSuccessfulFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&]() {
        // Enable highlighting now that we're fully connected.
        isConnected_ = true;
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::resizeAllColumns_()
{
    int userMsgCol = USER_MESSAGE_COL;
#if defined(WIN32)
    userMsgCol++;
#endif // defined(WIN32)
    
    std::map<int, int> newMaxSizes;
    std::map<int, int> colResizeList;
    for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
    {
        for (int i = 0; i < NUM_COLS; i++)
        {
            wxString colText = m_listSpots->GetItemText(index, i);
            auto newSize = std::max(getSizeForTableCellString_(colText), DefaultColumnWidths_[i]);
            if (i != userMsgCol)
            {
                if (newSize > newMaxSizes[i])
                {
                    newMaxSizes[i] = newSize;
                }

                colResizeList[i] = 1;
            }
        }
    }

    columnLengths_ = newMaxSizes;
    resizeChangedColumns_(colResizeList);
}

void FreeDVReporterDialog::onUserDisconnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&, sid]() {
        int indexToDelete = -1;
        for (auto index = 0; index < m_listSpots->GetItemCount(); index++)
        {
            std::string* sidPtr = (std::string*)m_listSpots->GetItemData(index);
            if (sid == *sidPtr)
            {
                indexToDelete = index;
                break;
            }
        }
        
        if (indexToDelete >= 0)
        {
            delete (std::string*)m_listSpots->GetItemData(indexToDelete);
            m_listSpots->DeleteItem(indexToDelete);

            resizeAllColumns_();
        }

        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            delete allReporterData_[sid];
            allReporterData_.erase(iter);
        }
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&, sid, frequencyHz, lastUpdate]() {
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
            
            std::map<int, int> colResizeList;
            addOrUpdateListIfNotFiltered_(iter->second, colResizeList);
            resizeChangedColumns_(colResizeList);
        }
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&, sid, txMode, transmitting, lastTxDate, lastUpdate]() {
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
            
            std::map<int, int> colResizeList;
            addOrUpdateListIfNotFiltered_(iter->second, colResizeList);
            resizeChangedColumns_(colResizeList);
        }
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&, sid, lastUpdate, receivedCallsign, snr, rxMode]() {
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
            
            std::map<int, int> colResizeList;
            addOrUpdateListIfNotFiltered_(iter->second, colResizeList);
            resizeChangedColumns_(colResizeList);
        }
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&, sid, lastUpdate, message]() {
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            if (message.size() == 0)
            {
                iter->second->userMessage = UNKNOWN_STR;
            }
            else
            {
                iter->second->userMessage = wxString::FromUTF8(message);
            }
            
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;

            // Only highlight on non-empty messages.
            bool ourCallsign = iter->second->callsign == wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign;
            bool filteringSelf = ourCallsign && filterSelfMessageUpdates_;
            
            if (message.size() > 0 && isConnected_ && !filteringSelf)
            {
                iter->second->lastUpdateUserMessage = iter->second->lastUpdateDate;
            }
            else if (ourCallsign && filteringSelf)
            {
                // Filter only until we show up again, then return to normal behavior.
                filterSelfMessageUpdates_ = false;
            }
            
            std::map<int, int> colResizeList;
            addOrUpdateListIfNotFiltered_(iter->second, colResizeList);
            resizeChangedColumns_(colResizeList);
        }
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
}

void FreeDVReporterDialog::onAboutToShowSelfFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    
    fnQueue_.push_back([&]() {
        filterSelfMessageUpdates_ = true;
    });
    
    CallAfter(std::bind(&FreeDVReporterDialog::execQueuedAction_, this));
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
        
        wxString formatStr = "%x %X";
        
#if __APPLE__
        // Workaround for weird macOS bug preventing .Format from working properly when double-clicking
        // on the .app in Finder. Running the app from Terminal seems to work fine with .Format for 
        // some reason. O_o
        struct tm tmpTm;
        auto tmpDateTm = tmpDate.GetTm(timeZone);
        
        tmpTm.tm_sec = tmpDateTm.sec;
        tmpTm.tm_min = tmpDateTm.min;
        tmpTm.tm_hour = tmpDateTm.hour;
        tmpTm.tm_mday = tmpDateTm.mday;
        tmpTm.tm_mon = tmpDateTm.mon;
        tmpTm.tm_year = tmpDateTm.year - 1900;
        tmpTm.tm_wday = tmpDateTm.GetWeekDay();
        tmpTm.tm_yday = tmpDateTm.yday;
        tmpTm.tm_isdst = -1;
        
        char buf[4096];
        strftime(buf, sizeof(buf), (const char*)formatStr.ToUTF8(), &tmpTm);
        return buf;
#else
        return tmpDate.Format(formatStr, timeZone);
#endif // __APPLE__
    }
    else
    {
        timeObj = wxDateTime();
        return _(UNKNOWN_STR);
    }
}

void FreeDVReporterDialog::resizeChangedColumns_(std::map<int, int>& colResizeList)
{
    for (auto& kvp : colResizeList)
    {
#if defined(WIN32)
        // The first column on Windows is hidden, so don't resize.
        if (kvp.first == 0)
        {
            continue;
        }
#endif // defined(WIN32)

        m_listSpots->SetColumnWidth(kvp.first, columnLengths_[kvp.first]);
    }

    // Call Update() to force immediate redraw of the list. This is needed
    // to work around a wxWidgets issue where the column headers don't resize,
    // but the widths of the column data do.
    m_listSpots->Update();
}

void FreeDVReporterDialog::addOrUpdateListIfNotFiltered_(ReporterData* data, std::map<int, int>& colResizeList)
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
        
        // Force resizing of all columns. This should reduce space if needed.
        resizeAllColumns_();
        
        return;
    }
    else if (itemIndex == -1 && !filtered)
    {
        itemIndex = m_listSpots->InsertItem(m_listSpots->GetItemCount(), _(""));
        m_listSpots->SetItemPtrData(itemIndex, (wxUIntPtr)new std::string(data->sid));
    }
    else if (filtered)
    {
        // Don't add for now as it's not supposed to display.
        return;
    }

    int col = 0;
#if defined(WIN32)
    // Column 0 is "hidden" to avoid column autosize issue. Callsign should be in column 1 instead.
    // Also, clear the item image for good measure as wxWidgets for Windows will set one for some
    // reason.
    m_listSpots->SetItemColumnImage(itemIndex, 0, -1);
    col = 1;
#endif // defined(WIN32)

    bool changed = setColumnForRow_(itemIndex, col++, " "+data->callsign, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->gridSquare, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, data->distance, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, data->heading, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->version, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, data->freqString, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->txMode, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->status, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    int textWidth = 0;
    int currentColWidth = m_listSpots->GetColumnWidth(col);
    
    wxString userMessageTruncated = _(" ") + data->userMessage; // note: extra space at beginning is to provide extra space from previous col
    textWidth = getSizeForTableCellString_(userMessageTruncated);
    int tmpLength = data->userMessage.Length() - 1;
    while (textWidth > currentColWidth && tmpLength > 0)
    {
        userMessageTruncated = data->userMessage.SubString(0, tmpLength--) + _("...");
        textWidth = getSizeForTableCellString_(userMessageTruncated);
    }
    
    if (tmpLength > 0 && tmpLength < (data->userMessage.Length() - 1))
    {
        userMessageTruncated = data->userMessage.SubString(0, tmpLength) + _("...");
    }
    
    changed = setColumnForRow_(itemIndex, col++, userMessageTruncated, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->lastTx, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->lastRxCallsign, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->lastRxMode, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, data->snr, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);

    changed = setColumnForRow_(itemIndex, col++, " "+data->lastUpdate, colResizeList);
    needResort |= changed && currentSortColumn_ == (col - 1);
    
    // Messaging updates take highest priority.
    auto curDate = wxDateTime::Now().ToUTC();
    wxColour backgroundColor;
    wxColour foregroundColor;
    if (data->lastUpdateUserMessage.IsValid() && data->lastUpdateUserMessage.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, MSG_COLORING_TIMEOUT_SEC)))
    {
        backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowBackgroundColor);
        foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowForegroundColor);
    }
    else if (data->transmitting)
    {
        backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor);
        foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor);
    }
    else if (data->lastRxDate.IsValid() && 
        ((data->lastRxDate.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, RX_COLORING_SHORT_TIMEOUT_SEC)) && data->lastRxCallsign == "") ||
         (data->lastRxDate.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, RX_COLORING_LONG_TIMEOUT_SEC)) && data->lastRxCallsign != "")))
    {
        backgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor);
        foregroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor);
    }
    else
    {
        backgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
        foregroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
    }

    m_listSpots->SetItemBackgroundColour(itemIndex, backgroundColor);
    m_listSpots->SetItemTextColour(itemIndex, foregroundColor);

    if (needResort)
    {
        m_listSpots->SortItems(&FreeDVReporterDialog::ListCompareFn_, (wxIntPtr)this);
    }
}

int FreeDVReporterDialog::getSizeForTableCellString_(wxString str)
{
    int textWidth = 0;
    int textHeight = 0; // note: unused

    m_listSpots->GetTextExtent(str, &textWidth, &textHeight);

    // Add buffer for sort indicator and to ensure wxWidgets doesn't truncate anything almost exactly
    // fitting the new column size.
    textWidth += m_sortIcons->GetIcon(upIconIndex_).GetSize().GetWidth();

    return textWidth;
}

bool FreeDVReporterDialog::setColumnForRow_(int row, int col, wxString val, std::map<int, int>& colResizeList)
{
    bool result = false;
    auto oldText = m_listSpots->GetItemText(row, col);
    
    if (oldText != val)
    {
        result = true;
        m_listSpots->SetItem(row, col, val);
    
        int textWidth = std::max(getSizeForTableCellString_(val), DefaultColumnWidths_[col]);

        // Resize column if not big enough.
        if (textWidth > columnLengths_[col])
        {
            columnLengths_[col] = textWidth;
            colResizeList[col]++;
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
                freq != filteredFrequency_);
    }
}

wxString FreeDVReporterDialog::GetCardinalDirection_(int degrees)
{
    int cardinalDirectionNumber( static_cast<int>( ( ( degrees / 360.0 ) * 16 ) + 0.5 )  % 16 );
    const char* const cardinalDirectionTexts[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    return cardinalDirectionTexts[cardinalDirectionNumber];
}
