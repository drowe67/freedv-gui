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
#define RIGHTMOST_COL (LAST_UPDATE_DATE_COL + 1)

#define UNKNOWN_STR ""
#define NUM_COLS (LAST_UPDATE_DATE_COL + 1)
#define RX_ONLY_STATUS "RX Only"
#define RX_COLORING_LONG_TIMEOUT_SEC (20)
#define RX_COLORING_SHORT_TIMEOUT_SEC (5)
#define MSG_COLORING_TIMEOUT_SEC (5)
#define STATUS_MESSAGE_MRU_MAX_SIZE (10)
#define MESSAGE_COLUMN_ID (6)

using namespace std::placeholders;

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxFrame(parent, id, title, pos, size, style)
    , tipWindow_(nullptr)
    , sortRequired_(false)
{
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", _("FreeDV Reporter"), wxGetApp().customConfigFileName));
    }

    // Create top-level of control hierarchy.
    wxFlexGridSizer* sectionSizer = new wxFlexGridSizer(2, 1, 0, 0);
    sectionSizer->AddGrowableRow(0);
    sectionSizer->AddGrowableCol(0);
        
    // Main list box
    // =============================
    int col = 0;

    m_listSpots = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE);

    // Associate data model.
    spotsDataModel_ = new FreeDVReporterDataModel(this);
    m_listSpots->AssociateModel(spotsDataModel_.get());

    auto colObj = m_listSpots->AppendTextColumn(wxT("Callsign"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->SetMinWidth(70);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Locator"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(65);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("km"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_RIGHT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Hdg"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_RIGHT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Version"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(70);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz ? wxT("kHz") : wxT("MHz"), 
        col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_RIGHT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Mode"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(65);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Status"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
#if defined(WIN32)
    // Use ReportMessageRenderer only on Windows so that we can render emojis in color.
    colObj = new wxDataViewColumn(wxT("Msg"), new ReportMessageRenderer(), col++, wxCOL_WIDTH_DEFAULT, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    m_listSpots->AppendColumn(colObj);
#else
    colObj = m_listSpots->AppendTextColumn(wxT("Msg"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_DEFAULT, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
#endif // defined(WIN32)
    colObj->GetRenderer()->EnableEllipsize(wxELLIPSIZE_END);
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(130);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    colObj->SetWidth(wxGetApp().appConfiguration.reportingUserMsgColWidth);
    
    colObj = m_listSpots->AppendTextColumn(wxT("Last TX"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("RX Call"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(65);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Mode"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("SNR"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(60);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }
    
    colObj = m_listSpots->AppendTextColumn(wxT("Last Update"), col++, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    colObj->GetRenderer()->DisableEllipsize();
    colObj->GetRenderer()->SetAlignment(wxALIGN_LEFT);
    colObj->SetMinWidth(100);
    if ((col - 1) == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }

    m_listSpots->AppendTextColumn(wxT(" "), col++, wxDATAVIEW_CELL_INERT, 1, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE);

    sectionSizer->Add(m_listSpots, 0, wxALL | wxEXPAND, 2);
    
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
    m_highlightClearTimer->Start(100);

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
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::DeselectItem), NULL, this);
    
    m_listSpots->Connect(wxEVT_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler(FreeDVReporterDialog::OnItemSelectionChanged), NULL, this);
    m_listSpots->Connect(wxEVT_DATAVIEW_ITEM_ACTIVATED, wxDataViewEventHandler(FreeDVReporterDialog::OnItemDoubleClick), NULL, this);
    m_listSpots->Connect(wxEVT_MOTION, wxMouseEventHandler(FreeDVReporterDialog::AdjustToolTip), NULL, this);
    m_listSpots->Connect(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, wxDataViewEventHandler(FreeDVReporterDialog::OnItemRightClick), NULL, this);
    m_listSpots->Connect(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, wxDataViewEventHandler(FreeDVReporterDialog::OnColumnClick), NULL, this);

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
    this->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::DeselectItem), NULL, this);
    
    m_listSpots->Disconnect(wxEVT_DATAVIEW_SELECTION_CHANGED, wxDataViewEventHandler(FreeDVReporterDialog::OnItemSelectionChanged), NULL, this);
    m_listSpots->Disconnect(wxEVT_DATAVIEW_ITEM_ACTIVATED, wxDataViewEventHandler(FreeDVReporterDialog::OnItemDoubleClick), NULL, this);
    m_listSpots->Disconnect(wxEVT_MOTION, wxMouseEventHandler(FreeDVReporterDialog::AdjustToolTip), NULL, this);
    m_listSpots->Disconnect(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, wxDataViewEventHandler(FreeDVReporterDialog::OnItemRightClick), NULL, this);
    m_listSpots->Disconnect(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, wxDataViewEventHandler(FreeDVReporterDialog::OnColumnClick), NULL, this);
    
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
    wxDataViewColumn* item = m_listSpots->GetColumn(DISTANCE_COL);

    if (wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances)
    {
        item->SetTitle("km ");
    }
    else
    {
        item->SetTitle("Miles");
    }
    
    // Refresh frequency units as appropriate.
    item = m_listSpots->GetColumn(FREQUENCY_COL);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        item->SetTitle("kHz");
    }
    else
    {
        item->SetTitle("MHz");
    }

    // Change direction/heading column label based on preferences
    item = m_listSpots->GetColumn(HEADING_COL);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
    {
        item->SetTitle("Dir");
        item->SetAlignment(wxALIGN_LEFT);
    }
    else
    {
        item->SetTitle("Hdg");
        item->SetAlignment(wxALIGN_RIGHT);
    }

    // Refresh all data based on current settings and filters.
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->refreshAllRows();

    // Update status controls.
    wxCommandEvent tmpEvent;
    OnStatusTextChange(tmpEvent);
}

void FreeDVReporterDialog::setReporter(std::shared_ptr<FreeDVReporter> reporter)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->setReporter(reporter);
    
    if (reporter)
    {
        // Update status message
        auto statusMsg = m_statusMessage->GetValue();
        reporter->updateMessage(statusMsg.utf8_string());
    }
}

void FreeDVReporterDialog::DeselectItem(wxMouseEvent& event)
{
    DeselectItem();
    event.Skip();
}


void FreeDVReporterDialog::DeselectItem()
{
    wxDataViewItem item = m_listSpots->GetSelection();
    if (item.IsOk())
    {
        m_listSpots->Unselect(item);
    }
}

void FreeDVReporterDialog::OnSystemColorChanged(wxSysColourChangedEvent& event)
{
    // Works around issues on wxWidgets with certain controls not changing backgrounds
    // when the user switches between light and dark mode.
    wxColour currentControlBackground = wxTransparentColour;

    // TBD - see if this workaround is still necessary
#if 0
    m_listSpots->SetBackgroundColour(currentControlBackground);
#if !defined(WIN32)
    ((wxWindow*)m_listSpots->m_headerWin)->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif //!defined(WIN32)
#endif

    event.Skip();
}

void FreeDVReporterDialog::OnInitDialog(wxInitDialogEvent& event)
{
    // TBD
}

void FreeDVReporterDialog::OnShow(wxShowEvent& event)
{
    wxGetApp().appConfiguration.reporterWindowVisible = true;

    auto selected = m_listSpots->GetSelection();
    if (selected.IsOk())
    {
        m_listSpots->Unselect(selected);
    }
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
    // Preserve sort column/ordering
    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        auto colObj = m_listSpots->GetColumn(index);
        if (colObj != nullptr && colObj->IsSortKey())
        {
            wxGetApp().appConfiguration.reporterWindowCurrentSort = index;
            wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = colObj->IsSortOrderAscending();
            break;
        }
    }
    
    // Preserve Msg column width
    auto userMsgCol = m_listSpots->GetColumn(USER_MESSAGE_COL);
    wxGetApp().appConfiguration.reportingUserMsgColWidth = userMsgCol->GetWidth();

    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnSendQSY(wxCommandEvent& event)
{
    auto selected = m_listSpots->GetSelection();
    if (selected.IsOk())
    {
        FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
        model->requestQSY(selected, wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency, ""); // Custom message TBD
        
        wxString fullMessage = wxString::Format(_("QSY request sent to %s"), model->getCallsign(selected));
        wxMessageBox(fullMessage, wxT("FreeDV Reporter"), wxOK | wxICON_INFORMATION, this);

        m_listSpots->Unselect(selected);
    }
}

void FreeDVReporterDialog::OnOpenWebsite(wxCommandEvent& event)
{
    std::string url = "https://" + wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->utf8_string() + "/";
    wxLaunchDefaultBrowser(url);
    DeselectItem();
}

void FreeDVReporterDialog::OnClose(wxCloseEvent& event)
{
    // Preserve sort column/ordering
    bool found = false;
    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        auto colObj = m_listSpots->GetColumn(index);
        if (colObj != nullptr && colObj->IsSortKey())
        {
            found = true;
            wxGetApp().appConfiguration.reporterWindowCurrentSort = index;
            wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = colObj->IsSortOrderAscending();
            break;
        }
    }
   
    if (!found)
    {
        wxGetApp().appConfiguration.reporterWindowCurrentSort = -1;
        wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = true;
    }

    // Preserve Msg column width
    auto userMsgCol = m_listSpots->GetColumn(USER_MESSAGE_COL);
    wxGetApp().appConfiguration.reportingUserMsgColWidth = userMsgCol->GetWidth();
    
    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnItemSelectionChanged(wxDataViewEvent& event)
{
    if (event.GetItem().IsOk())
    {
        refreshQSYButtonState();

        // Bring up tooltip for longer reporting messages if the user happened to click on that column.
        wxMouseEvent dummyEvent;
        AdjustToolTip(dummyEvent);
    }
    else
    {
        m_buttonSendQSY->Enable(false);
    }
}

void FreeDVReporterDialog::OnBandFilterChange(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter = 
        m_bandFilter->GetSelection();
    
    FilterFrequency freq = 
        (FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get();
    setBandFilter(freq);

    // Defer deselection until after UI updates
    CallAfter([&]() { DeselectItem(); });
}

void FreeDVReporterDialog::FreeDVReporterDataModel::updateHighlights()
{
    std::unique_lock<std::recursive_mutex> lk(dataMtx_);

    // Iterate across all visible rows. If a row is currently highlighted
    // green and it's been more than >20 seconds, clear coloring.
    auto curDate = wxDateTime::Now().ToUTC();

    wxDataViewItemArray itemsChanged;
    for (auto& item : allReporterData_)
    {
        auto reportData = item.second;

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
#if defined(__APPLE__)
        else
        {
            // To ensure that the columns don't have a different color than the rest of the control.
            // Needed mainly for macOS.
            backgroundColor = wxColour(wxTransparentColour);
        }
#endif // defined(__APPLE__)

        bool isHighlightUpdated = 
            backgroundColor != reportData->backgroundColor ||
            foregroundColor != reportData->foregroundColor;
        if (isHighlightUpdated)
        {
            reportData->backgroundColor = backgroundColor;
            reportData->foregroundColor = foregroundColor;

            if (reportData->isVisible)
            {
                wxDataViewItem dvi(reportData);
                itemsChanged.Add(dvi);
            }
        }
    }

    ItemsChanged(itemsChanged);
}

void FreeDVReporterDialog::OnTimer(wxTimerEvent& event)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->updateHighlights();

    if (sortRequired_)
    {
        model->Resort();
        sortRequired_ = false;
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

    // Defer deselection until after UI updates
    CallAfter([&]() { DeselectItem(); });
}

void FreeDVReporterDialog::OnItemDoubleClick(wxDataViewEvent& event)
{
    if (event.GetItem().IsOk())
    {
        FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
        auto frequency = model->getFrequency(event.GetItem());
        if (wxGetApp().rigFrequencyController && 
            (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || 
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly))
        {
            wxGetApp().rigFrequencyController->setFrequency(frequency);
        }
        DeselectItem();
    }
}

void FreeDVReporterDialog::AdjustToolTip(wxMouseEvent& event)
{
    const wxPoint pt = wxGetMousePosition();
    int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
    int mouseY = pt.y - m_listSpots->GetScreenPosition().y;
    
    wxRect rect;
    unsigned int desiredCol = USER_MESSAGE_COL;
    
    wxDataViewItem item;
    wxDataViewColumn* col;
    m_listSpots->HitTest(wxPoint(mouseX, mouseY), item, col);
    if (item.IsOk())
    {
        // Show popup corresponding to the full message.
        FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
        tempUserMessage_ = model->getUserMessage(item);
    
        if (col->GetModelColumn() == desiredCol)
        {
            auto textSize = m_listSpots->GetTextExtent(tempUserMessage_);
            rect = m_listSpots->GetItemRect(item, col);
            if (tipWindow_ == nullptr && textSize.GetWidth() > col->GetWidth())
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
        }
        else
        {
            tempUserMessage_ = _("");
        }
    }
    else
    {
        tempUserMessage_ = _("");
    }
}

void FreeDVReporterDialog::OnRightClickSpotsList(wxContextMenuEvent& event)
{
    wxDataViewEvent contextEvent;
    OnItemRightClick(contextEvent);
}

void FreeDVReporterDialog::SkipMouseEvent(wxMouseEvent& event)
{
    wxDataViewEvent contextEvent;
    OnItemRightClick(contextEvent);
}

void FreeDVReporterDialog::OnColumnClick(wxDataViewEvent& event)
{
    DeselectItem();
    event.Skip();

#if 0
    auto col = event.GetDataViewColumn();
    if (col != nullptr)
    {
        if (col->IsSortKey() && !col->IsSortOrderAscending())
        {
            col->UnsetAsSortKey();
        }
        else if (!col->IsSortKey())
        {
            col->SetSortOrder(true);
        }
        else
        {
            col->SetSortOrder(false);
        }
        event.StopPropagation();
    }
#endif // 0
}

void FreeDVReporterDialog::OnItemRightClick(wxDataViewEvent& event)
{
    // Make sure item's deselected as it should only be selected on
    // left-click.
    DeselectItem();

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
    DeselectItem();
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

    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->updateMessage(statusMsg);

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

    DeselectItem();
}

void FreeDVReporterDialog::OnStatusTextSendContextMenu(wxContextMenuEvent& event)
{
    DeselectItem();
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
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->updateMessage("");

    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText = "";
    m_statusMessage->SetValue("");

    DeselectItem();
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
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();

    // Update filter if the frequency's changed.
    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency)
    {
        FilterFrequency freq = 
            getFilterForFrequency_(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
        
        if (model->getCurrentBandFilter() != freq || wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq)
        {
            setBandFilter(freq);
        }
    }
    
    bool enabled = false;
    auto selected = m_listSpots->GetSelection();
    if (selected.IsOk())
    {
        auto selectedCallsign = model->getCallsign(selected);
    
        if (model->isValidForReporting() &&
            selectedCallsign != wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign && 
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 &&
            freedvInterface.isRunning())
        {
            uint64_t theirFreq = model->getFrequency(selected);
            enabled = theirFreq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
        }
    }
    
    m_buttonSendQSY->Enable(enabled);
}

void FreeDVReporterDialog::setBandFilter(FilterFrequency freq)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->setBandFilter(freq);
}

void FreeDVReporterDialog::FreeDVReporterDataModel::setBandFilter(FilterFrequency freq)
{
    if (filteredFrequency_ != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency ||
        currentBandFilter_ != freq)
    {
        filteredFrequency_ = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
        currentBandFilter_ = freq;

        refreshAllRows();
    }
}

wxString FreeDVReporterDialog::FreeDVReporterDataModel::makeValidTime_(std::string timeStr, wxDateTime& timeObj)
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

double FreeDVReporterDialog::FreeDVReporterDataModel::calculateDistance_(wxString gridSquare1, wxString gridSquare2)
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

void FreeDVReporterDialog::FreeDVReporterDataModel::calculateLatLonFromGridSquare_(wxString gridSquare, double& lat, double& lon)
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

double FreeDVReporterDialog::FreeDVReporterDataModel::calculateBearingInDegrees_(wxString gridSquare1, wxString gridSquare2)
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

double FreeDVReporterDialog::FreeDVReporterDataModel::DegreesToRadians_(double degrees)
{
    return degrees * (M_PI / 180.0);
}

double FreeDVReporterDialog::FreeDVReporterDataModel::RadiansToDegrees_(double radians)
{
    auto result = (radians > 0 ? radians : (2*M_PI + radians)) * 360 / (2*M_PI);
    return (result == 360) ? 0 : result;
}

void FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_()
{
    // This ensures that we handle server events in the order they're received.
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_[0]();
    fnQueue_.erase(fnQueue_.begin());
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

bool FreeDVReporterDialog::FreeDVReporterDataModel::isFiltered_(uint64_t freq)
{
    auto bandForFreq = parent_->getFilterForFrequency_(freq);
    
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

wxString FreeDVReporterDialog::FreeDVReporterDataModel::GetCardinalDirection_(int degrees)
{
    int cardinalDirectionNumber( static_cast<int>( ( ( degrees / 360.0 ) * 16 ) + 0.5 )  % 16 );
    const char* const cardinalDirectionTexts[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    return cardinalDirectionTexts[cardinalDirectionNumber];
}

FreeDVReporterDialog::FreeDVReporterDataModel::FreeDVReporterDataModel(FreeDVReporterDialog* parent)
    : isConnected_(false)
    , parent_(parent)
    , currentBandFilter_(FreeDVReporterDialog::BAND_ALL)
    , filterSelfMessageUpdates_(false)
    , filteredFrequency_(0)
{
    // empty
}

FreeDVReporterDialog::FreeDVReporterDataModel::~FreeDVReporterDataModel()
{
    setReporter(nullptr);
}

void FreeDVReporterDialog::FreeDVReporterDataModel::setReporter(std::shared_ptr<FreeDVReporter> reporter)
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
        reporter_->setOnReporterConnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReporterConnect_, this));
        reporter_->setOnReporterDisconnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReporterDisconnect_, this));
    
        reporter_->setOnUserConnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onUserConnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnUserDisconnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onUserDisconnectFn_, this, _1, _2, _3, _4, _5, _6));
        reporter_->setOnFrequencyChangeFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onFrequencyChangeFn_, this, _1, _2, _3, _4, _5));
        reporter_->setOnTransmitUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onTransmitUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));
        reporter_->setOnReceiveUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReceiveUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));

        reporter_->setMessageUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onMessageUpdateFn_, this, _1, _2, _3));
        reporter_->setConnectionSuccessfulFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onConnectionSuccessfulFn_, this));
        reporter_->setAboutToShowSelfFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onAboutToShowSelfFn_, this));
    }
    else
    {
        // Spot list no longer valid, delete the items currently on there
        log_debug("Reporter object set to null");
        clearAllEntries_();
    }
}

void FreeDVReporterDialog::FreeDVReporterDataModel::clearAllEntries_()
{
    std::unique_lock<std::recursive_mutex> lk(dataMtx_);
    assert(wxThread::IsMain());

    for (auto& row : allReporterData_)
    {
        if (row.second->isVisible)
        {
            row.second->isVisible = false;
        }

        delete row.second;
    }
    allReporterData_.clear();
    
    Cleared();
}

bool FreeDVReporterDialog::FreeDVReporterDataModel::HasDefaultCompare() const
{
    // Will compare by connect time if nothing is selected for sorting
    return true;
}

int FreeDVReporterDialog::FreeDVReporterDataModel::Compare (const wxDataViewItem &item1, const wxDataViewItem &item2, unsigned int column, bool ascending) const
{
    std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    assert(wxThread::IsMain());

    if (!item1.IsOk() || !item2.IsOk())
    {
        int result = 0;
        if (!item1.IsOk())
        {
            result = 1;
        }
        else
        {
            result = -1;
        }

        result *= ascending ? 1 : -1;
        return result;

    }
    auto leftData = (ReporterData*)item1.GetID();
    auto rightData = (ReporterData*)item2.GetID();

    int result = 0;
    switch(column)
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
        case (unsigned)-1:
            if (leftData->connectTime.IsEarlierThan(rightData->connectTime))
            {
                result = -1;
            }
            else if (leftData->connectTime.IsLaterThan(rightData->connectTime))
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

    if (result == 0 && column != (unsigned)-1)
    {
        // Also compare by connect time to ensure that we have a sort that
        // appears stable.
        if (leftData->connectTime.IsEarlierThan(rightData->connectTime))
        {
            result = -1;
        }
        else if (leftData->connectTime.IsLaterThan(rightData->connectTime))
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }

    if (result == 0)
    {
        // As a last-ditch effort to prevent returning 0, use the pointer
        // values. Recommended by wxWidgets documentation.
        wxUIntPtr id1 = wxPtrToUInt(item1.GetID());
        wxUIntPtr id2 = wxPtrToUInt(item2.GetID());
        result = (ascending == (id1 > id2)) ? 1 : -1;
    }

    if (!ascending)
    {
        result = -result;
    }
    
    return result;
}

bool FreeDVReporterDialog::FreeDVReporterDataModel::GetAttr (const wxDataViewItem &item, unsigned int col, wxDataViewItemAttr &attr) const
{
    std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    bool result = false;
    assert(wxThread::IsMain());
    
    if (item.IsOk())
    {
        auto row = (ReporterData*)item.GetID();
        if (row->backgroundColor.IsOk())
        {
            attr.SetBackgroundColour(row->backgroundColor);
            result = true;
        }
        if (row->foregroundColor.IsOk())
        {
            attr.SetColour(row->foregroundColor);
            result = true;
        }
    }

    return result;
}

unsigned int FreeDVReporterDialog::FreeDVReporterDataModel::GetChildren (const wxDataViewItem &item, wxDataViewItemArray &children) const
{
    assert(wxThread::IsMain());
    if (item.IsOk())
    {
        // No children.
        return 0;
    }
    else
    {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        int count = 0;
        for (auto& row : allReporterData_)
        {
            if (row.second->isVisible)
            {
                count++;
                
                wxDataViewItem newItem(row.second);
                children.Add(newItem);
            }
        }

        return count;
    }
}

wxDataViewItem FreeDVReporterDialog::FreeDVReporterDataModel::GetParent (const wxDataViewItem &item) const
{
    // Return root item
    return wxDataViewItem(nullptr);
}

void FreeDVReporterDialog::FreeDVReporterDataModel::GetValue (wxVariant &variant, const wxDataViewItem &item, unsigned int col) const
{
    std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    assert(wxThread::IsMain());
    if (item.IsOk())
    {
        auto row = (ReporterData*)item.GetID();
        switch (col)
        {
            case CALLSIGN_COL:
                variant = wxVariant(row->callsign);
                break;
            case GRID_SQUARE_COL:
                variant = wxVariant(row->gridSquare);
                break;
            case DISTANCE_COL:
                variant = wxVariant(row->distance);
                break;
            case HEADING_COL:
                variant = wxVariant(row->heading);
                break;
            case VERSION_COL:
                variant = wxVariant(row->version);
                break;
            case FREQUENCY_COL:
                variant = wxVariant(row->freqString);
                break;
            case STATUS_COL:
                variant = wxVariant(row->status);
                break;
            case USER_MESSAGE_COL:
                variant = wxVariant(row->userMessage);
                break;
            case LAST_TX_DATE_COL:
                variant = wxVariant(row->lastTx);
                break;
            case LAST_RX_CALLSIGN_COL:
                variant = wxVariant(row->lastRxCallsign);
                break;
            case LAST_RX_MODE_COL:
                variant = wxVariant(row->lastRxMode);
                break;
            case SNR_COL:
                variant = wxVariant(row->snr);
                break;
            case LAST_UPDATE_DATE_COL:
                variant = wxVariant(row->lastUpdate);
                break;
            case TX_MODE_COL:
                variant = wxVariant(row->txMode);
                break;
            case RIGHTMOST_COL:
                variant = wxVariant("");
                break;
            default:
                variant = wxVariant("NOT VALID");
                break;
        }
    }
}

bool FreeDVReporterDialog::FreeDVReporterDataModel::IsContainer (const wxDataViewItem &item) const
{
    // Single-level (i.e. no children)
    return !item.IsOk();
}

bool FreeDVReporterDialog::FreeDVReporterDataModel::SetValue (const wxVariant &variant, const wxDataViewItem &item, unsigned int col)
{
    // I think this can just return false without changing anything as this is read-only (other than what comes from the server).
    return false;
}

void FreeDVReporterDialog::FreeDVReporterDataModel::refreshAllRows()
{
    std::unique_lock<std::recursive_mutex> lk(dataMtx_);
    
    for (auto& kvp : allReporterData_)
    {
        bool updated = false;
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
        
        updated |= kvp.second->freqString != frequencyString;
        kvp.second->freqString = frequencyString;

        // Refresh cardinal vs. degree directions.
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare != kvp.second->gridSquare)
        {
            wxString newHeading;
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
            {
                newHeading = GetCardinalDirection_(kvp.second->headingVal);
            }
            else
            {
                newHeading = wxString::Format("%.0f", kvp.second->headingVal);
            }

            updated |= kvp.second->heading != newHeading;
            kvp.second->heading = newHeading;
        }

        // Refresh filter state
        bool newVisibility = !isFiltered_(kvp.second->frequency);
        if (newVisibility != kvp.second->isVisible)
        {
            kvp.second->isVisible = newVisibility;
            if (newVisibility)
            {
                ItemAdded(wxDataViewItem(nullptr), wxDataViewItem(kvp.second));
                parent_->sortRequired_ = true;
            }
            else
            {
                ItemDeleted(wxDataViewItem(nullptr), wxDataViewItem(kvp.second));
            }
        }
        else if (updated && kvp.second->isVisible)
        {
            ItemChanged(wxDataViewItem(kvp.second));
            parent_->sortRequired_ = true;
        }
    }
}

void FreeDVReporterDialog::FreeDVReporterDataModel::requestQSY(wxDataViewItem selectedItem, uint64_t frequency, wxString customText)
{
    if (reporter_ && selectedItem.IsOk())
    {
        auto row = (ReporterData*)selectedItem.GetID();
        reporter_->requestQSY(row->sid, wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency, (const char*)customText.ToUTF8());
    }
}

// =================================================================================
// Note: these methods below do not run under the UI thread, so we need to make sure
// UI actions happen there.
// =================================================================================

void FreeDVReporterDialog::FreeDVReporterDataModel::onReporterConnect_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&]() {
        log_debug("Connected to server");
        filterSelfMessageUpdates_ = false;
        clearAllEntries_();
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onReporterDisconnect_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&]() {
        log_debug("Disconnected from server");
        isConnected_ = false;
        filterSelfMessageUpdates_ = false;
        clearAllEntries_();
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid, lastUpdate, callsign, gridSquare, version, rxOnly]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        assert(wxThread::IsMain());

        log_debug("User connected: %s (%s) with SID %s", callsign.c_str(), gridSquare.c_str(), sid.c_str());

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
        temp->connectTime = temp->lastUpdateDate;
        temp->isVisible = !isFiltered_(temp->frequency);
        
        if (allReporterData_.find(sid) != allReporterData_.end() && !isConnected_)
        {
            log_warn("Received duplicate user during connection process");
            return;
        }

        allReporterData_[sid] = temp;

        if (temp->isVisible)
        {
            ItemAdded(wxDataViewItem(nullptr), wxDataViewItem(temp));
            parent_->sortRequired_ = true;
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onConnectionSuccessfulFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));

        log_debug("Fully connected to server");

        // Enable highlighting now that we're fully connected.
        isConnected_ = true;
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onUserDisconnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        assert(wxThread::IsMain());

        log_debug("User with SID %s disconnected", sid.c_str());

        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            auto item = iter->second;
            wxDataViewItem dvi(item);
            if (item->isVisible)
            {
                item->isVisible = false;
                parent_->Unselect(dvi);
                ItemDeleted(wxDataViewItem(nullptr), dvi);
            }

            delete item;
            allReporterData_.erase(iter);
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, uint64_t frequencyHz)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid, frequencyHz, lastUpdate]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        
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

            wxDataViewItem dvi(iter->second);
            bool newVisibility = !isFiltered_(iter->second->frequency);
            if (newVisibility != iter->second->isVisible)
            {
                iter->second->isVisible = newVisibility;
                if (newVisibility)
                {
                    ItemAdded(wxDataViewItem(nullptr), dvi);
                    parent_->sortRequired_ = true;
                }
                else
                {
                    ItemDeleted(wxDataViewItem(nullptr), dvi);
                }
            }
            else if (newVisibility)
            {            
                ItemChanged(dvi);
                parent_->sortRequired_ = 
                    parent_->m_listSpots->GetColumn(FREQUENCY_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(LAST_UPDATE_DATE_COL)->IsSortKey();
            }
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string txMode, bool transmitting, std::string lastTxDate)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid, txMode, transmitting, lastTxDate, lastUpdate]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    
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

            if (iter->second->isVisible)
            { 
                wxDataViewItem dvi(iter->second);
                ItemChanged(dvi);
                parent_->sortRequired_ = 
                    parent_->m_listSpots->GetColumn(STATUS_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(TX_MODE_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(LAST_UPDATE_DATE_COL)->IsSortKey();
            }
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string receivedCallsign, float snr, std::string rxMode)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid, lastUpdate, receivedCallsign, snr, rxMode]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    
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
       
            if (iter->second->isVisible)
            { 
                wxDataViewItem dvi(iter->second);
                ItemChanged(dvi);
                parent_->sortRequired_ = 
                    parent_->m_listSpots->GetColumn(LAST_RX_CALLSIGN_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(LAST_RX_MODE_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(SNR_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(LAST_UPDATE_DATE_COL)->IsSortKey();
            }
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&, sid, lastUpdate, message]() {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
    
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
        
            if (message.size() > 0 && !filteringSelf)
            {
                iter->second->lastUpdateUserMessage = iter->second->lastUpdateDate;
            }
            else if (ourCallsign && filteringSelf)
            {
                // Filter only until we show up again, then return to normal behavior.
                filterSelfMessageUpdates_ = false;
            }
       
            if (iter->second->isVisible)
            {
                wxDataViewItem dvi(iter->second);
                ItemChanged(dvi);
                parent_->sortRequired_ = 
                    parent_->m_listSpots->GetColumn(USER_MESSAGE_COL)->IsSortKey() ||
                    parent_->m_listSpots->GetColumn(LAST_UPDATE_DATE_COL)->IsSortKey();
            }
        }
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onAboutToShowSelfFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    fnQueue_.push_back([&]() {
        filterSelfMessageUpdates_ = true;
    });

    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}
