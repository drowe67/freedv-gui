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

#include <sstream>
#include <math.h>
#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/clipbrd.h>
#include <wx/wrapsizer.h>
#include <wx/menuitem.h>
#include <wx/numdlg.h> 

#if wxCHECK_VERSION(3,2,0)
#include <wx/uilocale.h>
#endif // wxCHECK_VERSION(3,2,0)

#if defined(WIN32)
#include <wx/headerctrl.h>
#endif // defined(WIN32)

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

#define UNKNOWN_SNR_VAL (-99)

#define NUM_COLS (LAST_UPDATE_DATE_COL + 1)
#define RX_ONLY_STATUS "RX Only"
#define RX_COLORING_LONG_TIMEOUT_SEC (20)
#define RX_COLORING_SHORT_TIMEOUT_SEC (5)
#define MSG_COLORING_TIMEOUT_SEC (5)
#define STATUS_MESSAGE_MRU_MAX_SIZE (10)
#define MESSAGE_COLUMN_ID (6)

using namespace std::placeholders;

void FreeDVReporterDialog::createColumn_(int col, bool visible)
{
    wxDataViewColumn* colObj = nullptr;
    int minWidth = 0;
    int alignment = wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL;
    wxString colName = "";
    bool ellipsize = false;

    switch (col)
    {
        case CALLSIGN_COL:
            colName = wxT("Callsign");
            minWidth = 70;
            break;
        case GRID_SQUARE_COL:
            colName = wxT("Locator");
            minWidth = 65;
            break;
        case DISTANCE_COL:
            colName = wxT("km");
            minWidth = 60;
            alignment = wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL;
            break;
        case HEADING_COL:
            colName = wxT("Hdg");
            minWidth = 60;
            alignment = wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL;
            break;
        case VERSION_COL:
            colName = wxT("Version");
            minWidth = 70;
            break;
        case FREQUENCY_COL:
            colName = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz ? wxT("kHz") : wxT("MHz");
            minWidth = 60;
            alignment = wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL;
            break;
        case TX_MODE_COL:
            colName = wxT("Mode");
            minWidth = 65;
            break;
        case STATUS_COL:
            colName = wxT("Status");
            minWidth = 60;
            break;
        case USER_MESSAGE_COL:
            // Note: there's Windows specific logic here, so we create the column
            // here rather than farther down.
#if defined(WIN32)
            // Use ReportMessageRenderer only on Windows so that we can render emojis in color.
            colObj = new wxDataViewColumn(wxT("Msg"), new ReportMessageRenderer(), col, wxCOL_WIDTH_DEFAULT, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_REORDERABLE);
            m_listSpots->AppendColumn(colObj);
#else
            colObj = m_listSpots->AppendTextColumn(wxT("Msg"), col, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_DEFAULT, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_REORDERABLE);
#endif // defined(WIN32)
            colObj->SetWidth(wxGetApp().appConfiguration.reportingUserMsgColWidth);
            minWidth = 130;
            ellipsize = true;
            break;
        case LAST_TX_DATE_COL:
            colName = wxT("Last TX");
            minWidth = 60;
            break;
        case LAST_RX_CALLSIGN_COL:
            colName = wxT("RX Call");
            minWidth = 65;
            break;
        case LAST_RX_MODE_COL:
            colName = wxT("Mode");
            minWidth = 60;
            break;
        case SNR_COL:
            colName = wxT("SNR");
            minWidth = 60;
            break;
        case LAST_UPDATE_DATE_COL:
            colName = wxT("Last Update");
            minWidth = 100;
            break;
        default:
            return;
    }

    if (colObj == nullptr)
    {
        colObj = m_listSpots->AppendTextColumn(colName, col, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_REORDERABLE);
    }
    auto renderer = colObj->GetRenderer();
    renderer->SetAlignment(alignment);
    if (ellipsize)
    {
        renderer->EnableEllipsize(wxELLIPSIZE_END);
    }
    else
    {
        renderer->DisableEllipsize();
    }
    colObj->SetMinWidth(minWidth);
    if (col == wxGetApp().appConfiguration.reporterWindowCurrentSort)
    {
        colObj->SetSortOrder(wxGetApp().appConfiguration.reporterWindowCurrentSortDirection);
    }

    if (!visible)
    {
        colObj->SetHidden(true);
    }
}

FreeDVReporterDialog::FreeDVReporterDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxFrame(parent, id, title, pos, size, style)
    , tipWindow_(nullptr)
    , UNKNOWN_STR("")
    , SNR_FORMAT_STR("%.01f")
    , ALL_LETTERS_RGX("^[A-Z]{2}$")
    , MS_REMOVAL_RGX("\\.[^+-]+")
    , TIMEZONE_RGX("([+-])([0-9]+):([0-9]+)$")
    , TZ_OFFSET_STR("-")
    , EMPTY_STR("")
    , TIME_FORMAT_STR("%x %X")
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
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
    m_listSpots = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE);

    // Associate data model.
    spotsDataModel_ = new FreeDVReporterDataModel(this);
    m_listSpots->AssociateModel(spotsDataModel_.get());

    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->size() != NUM_COLS)
    {
        // Generate default column ordering
        log_info("Generating missing column ordering");
        auto iter = wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->begin();
        while (iter != wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->end())
        {
            if (*iter >= RIGHTMOST_COL)
            {
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->erase(iter);
                iter = wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->begin();
            }
            else
            {
                iter++;
            }
        }

        auto maxIndex = 
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->size() == 0 ? 
            -1 : 
            *std::max_element(
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->begin(),
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->end()
            );

        for (auto index = maxIndex + 1; index < NUM_COLS; index++)
        {
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder->push_back(index);
        }
    }

    while (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->size() < NUM_COLS)
    {
        // Generate default column visibility
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->push_back(true);
    }

    // Windows seems to have the model column ID equal to the actual column ID regardless of the 
    // actual ordering, so we just use wxHeaderCtrl to save/restore the column ordering.
#if defined(WIN32)
    for (auto col = 0; col < NUM_COLS; col++)
#else    
    for (auto& col : wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder.get())
#endif // defined(WIN32)
    {
        if (col < NUM_COLS)
        {
            log_info("Creating col %d", col);
            auto visible = (bool)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(col);

            // Hide RX Mode column if legacy modes aren't enabled
            if (col == LAST_RX_MODE_COL)
            {
                visible &= wxGetApp().appConfiguration.enableLegacyModes;
            }
            createColumn_(col, visible);
        }
    }
    m_listSpots->AppendTextColumn(wxT(" "), RIGHTMOST_COL, wxDATAVIEW_CELL_INERT, 1, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE);

#if defined(WIN32)
    auto headerCtrl = m_listSpots->GenericGetHeader();
    wxArrayInt wxColumnOrder;
    for (auto& col : wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder.get())
    {
        if (col < NUM_COLS)
        {
            wxColumnOrder.Add(col);
        }
    }
    wxColumnOrder.Add(RIGHTMOST_COL); // All columns need to be in the list to actually take effect.
    headerCtrl->SetColumnsOrder(wxColumnOrder);
#endif // defined(WIN32)
    
    sectionSizer->Add(m_listSpots, 0, wxALL | wxEXPAND, 2);
    
    // Bottom buttons
    // =============================
    auto reportingSettingsSizer = new wxWrapSizer(wxHORIZONTAL);

    wxBoxSizer* leftButtonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(this, wxID_OK, _("Close"));
    m_buttonOK->SetToolTip(_("Closes FreeDV Reporter window."));
    leftButtonSizer->Add(m_buttonOK, 0, wxALL, 5);

    m_buttonSendQSY = new wxButton(this, wxID_ANY, _("Request QSY"));
    m_buttonSendQSY->SetToolTip(_("Asks selected user to switch to your frequency."));
    m_buttonSendQSY->Enable(false); // disable by default unless we get a valid selection
    leftButtonSizer->Add(m_buttonSendQSY, 0, wxALL, 5);

    m_buttonDisplayWebpage = new wxButton(this, wxID_ANY, _("Website"));
    m_buttonDisplayWebpage->SetToolTip(_("Opens FreeDV Reporter in your Web browser."));
    leftButtonSizer->Add(m_buttonDisplayWebpage, 0, wxALL, 5);

    reportingSettingsSizer->Add(leftButtonSizer, 0, wxALL | wxEXPAND, 0);

    // Band filter list
    wxBoxSizer* bandFilterSizer = new wxBoxSizer(wxHORIZONTAL);

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
    
    bandFilterSizer->Add(new wxStaticText(this, wxID_ANY, _("Band:"), wxDefaultPosition, wxDefaultSize, 0), 
                          0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_bandFilter = new wxComboBox(
        this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 
        sizeof(bandList) / sizeof(wxString), bandList, wxCB_DROPDOWN | wxCB_READONLY);
    m_bandFilter->SetSelection(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter);    
    bandFilterSizer->Add(m_bandFilter, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    
    m_trackFrequency = new wxCheckBox(this, wxID_ANY, _("Track:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_trackFrequency->SetToolTip(_("Filters FreeDV Reporter list based on radio's current frequency or band."));
    bandFilterSizer->Add(m_trackFrequency, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    
    m_trackFreqBand = new wxRadioButton(this, wxID_ANY, _("band"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    bandFilterSizer->Add(m_trackFreqBand, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackFreqBand->Enable(false);
    
    m_trackExactFreq = new wxRadioButton(this, wxID_ANY, _("freq."), wxDefaultPosition, wxDefaultSize, 0);
    bandFilterSizer->Add(m_trackExactFreq, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    m_trackExactFreq->Enable(false);
    
    m_filterStatus = new wxStaticText(this, wxID_ANY, _("Idle Off"));
    bandFilterSizer->Add(m_filterStatus, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    
    m_trackFrequency->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency);
    reportingSettingsSizer->Add(bandFilterSizer, 0, wxALL | wxEXPAND, 0);
    
    setBandFilter((FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get());

    wxBoxSizer* statusMessageSizer = new wxBoxSizer(wxHORIZONTAL);

    auto statusMessageLabel = new wxStaticText(this, wxID_ANY, _("Message:"), wxDefaultPosition, wxDefaultSize);
    statusMessageSizer->Add(statusMessageLabel, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_statusMessage = new wxComboBox(this, wxID_ANY, _(""), wxDefaultPosition, wxSize(180, -1), 0, nullptr, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    statusMessageSizer->Add(m_statusMessage, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonSend = new wxButton(this, wxID_ANY, _("Send"));
    m_buttonSend->SetToolTip(_("Sends message to FreeDV Reporter. Right-click for additional options."));
    statusMessageSizer->Add(m_buttonSend, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonClear = new wxButton(this, wxID_ANY, _("Clear"));
    m_buttonClear->SetToolTip(_("Clears message from FreeDV Reporter. Right-click for additional options."));
    statusMessageSizer->Add(m_buttonClear, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    reportingSettingsSizer->Add(statusMessageSizer, 0, wxALL | wxEXPAND, 0);
    sectionSizer->Add(reportingSettingsSizer, 0, wxALL | wxEXPAND | wxFIXED_MINSIZE, 2);
    
    this->SetMinSize(GetBestSize());

    // Menu bar
    menuBar_ = new wxMenuBar();
    this->SetMenuBar(menuBar_);

    showMenu_ = new wxMenu();
    menuBar_->Append(showMenu_, _("Show"));

    std::vector<std::pair<int, wxString> > menus {
        {CALLSIGN_COL, _("Callsign")},
        {GRID_SQUARE_COL, _("Locator")},
        {DISTANCE_COL, _("Distance")},
        {VERSION_COL, _("Version")},
        {HEADING_COL, _("Heading")},
        {FREQUENCY_COL, _("Frequency")},
        {TX_MODE_COL, _("TX Mode")},
        {STATUS_COL, _("Status")},
        {USER_MESSAGE_COL, _("User Message")},
        {LAST_TX_DATE_COL, _("Last TX Date")},
        {LAST_RX_CALLSIGN_COL, _("Last RX Callsign")},
        {LAST_RX_MODE_COL, _("Last RX Mode")},
        {SNR_COL, _("SNR")},
        {LAST_UPDATE_DATE_COL, _("Last Update")},
    };

    for (auto& item : menus)
    {
        auto menuItem = showMenu_->Append(wxID_HIGHEST + 100 + item.first, item.second, wxEmptyString, wxITEM_CHECK);
        menuItem->Check(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(item.first));
        this->Connect(wxID_HIGHEST + 100 + item.first, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(FreeDVReporterDialog::OnShowColumn));

        if (item.first == LAST_RX_MODE_COL && !wxGetApp().appConfiguration.enableLegacyModes)
        {
            menuItem->Enable(false);
        }
    }

    std::vector<int> idleLongerThanMinutes { 30, 60, 90, 120 };
    filterMenu_ = new wxMenu();
    idleLongerThanMenu_ = new wxMenu();
    menuBar_->Append(filterMenu_, _("Filter"));
    filterMenu_->Append(wxID_ANY, _("Idle more than (minutes)..."), idleLongerThanMenu_);

    auto menuItem = idleLongerThanMenu_->Append(wxID_ANY, "Disabled", wxEmptyString, wxITEM_CHECK);
    menuItem->Check(!wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter);
    bool foundChecked = menuItem->IsChecked();
    this->Connect(menuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(FreeDVReporterDialog::OnIdleFilter));

    for (auto& item : idleLongerThanMinutes)
    {
        auto menuItem = idleLongerThanMenu_->Append(wxID_HIGHEST + 200 + item, wxString::Format("%d", item), wxEmptyString, wxITEM_CHECK);
        menuItem->Check(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter && wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes == item);
        foundChecked |= menuItem->IsChecked();
        this->Connect(wxID_HIGHEST + 200 + item, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(FreeDVReporterDialog::OnIdleFilter));
    }

    menuItem = idleLongerThanMenu_->Append(wxID_HIGHEST + 200, "Custom...", wxEmptyString, wxITEM_CHECK);
    menuItem->Check(!foundChecked);
    this->Connect(wxID_HIGHEST + 200, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(FreeDVReporterDialog::OnIdleFilter));

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
#if wxCHECK_VERSION(3,2,0)
    wxDisplay currentDisplay(this);
#else
    auto displayNo = wxDisplay::GetFromWindow(this);
    if (displayNo == wxNOT_FOUND)
    {
        displayNo = 0;
    }
    wxDisplay currentDisplay(displayNo);
#endif // wxCHECK_VERSION(3,2,0)
    
    auto displayGeometry = currentDisplay.GetClientArea();
    wxPoint actualPos = GetPosition();
    wxSize actualWindowSize = GetSize();
    if (actualPos.x < (displayGeometry.x - (0.9 * actualWindowSize.GetWidth())) ||
        actualPos.x > (displayGeometry.x + 0.9 * displayGeometry.width))
    {
        actualPos.x = displayGeometry.x;
    }
    if (actualPos.y < displayGeometry.y ||
        actualPos.y > (displayGeometry.y + 0.9 * displayGeometry.height))
    {
        actualPos.y = 0;
    }
    wxGetApp().appConfiguration.reporterWindowLeft = actualPos.x;
    wxGetApp().appConfiguration.reporterWindowTop = actualPos.y;
    SetPosition(actualPos);

    // Set up timers. Highlight clear timer has a slightly longer interval
    // to reduce CPU usage.
    m_highlightClearTimer = new wxTimer(this);
    m_highlightClearTimer->Start(250);
    m_resortTimer = new wxTimer(this);
    m_resortTimer->Start(100);
    m_deleteTimer = new wxTimer(this);
    m_deleteTimer->Start(1000);
    
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
        
    // Create popup menu for callsign lookups
    callsignPopupMenu_ = new wxMenu();
    assert(callsignPopupMenu_ != nullptr);
    
    auto qrzMenuItem = callsignPopupMenu_->Append(wxID_ANY, _("Lookup via QRZ"));
    callsignPopupMenu_->Connect(
        qrzMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnQRZLookup),
        NULL,
        this);
        
    auto hamQthMenuItem = callsignPopupMenu_->Append(wxID_ANY, _("Lookup via HamQTH"));
    callsignPopupMenu_->Connect(
        hamQthMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnHamQTHLookup),
        NULL,
        this);
        
    auto hamCallMenuItem = callsignPopupMenu_->Append(wxID_ANY, _("Lookup via HamCall"));
    callsignPopupMenu_->Connect(
        hamCallMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(FreeDVReporterDialog::OnHamCallLookup),
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
    m_listSpots->Connect(wxEVT_DATAVIEW_COLUMN_REORDERED, wxDataViewEventHandler(FreeDVReporterDialog::OnColumnReordered), NULL, this);
    m_listSpots->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(FreeDVReporterDialog::OnSetFocus), NULL, this);

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
    
    m_resortTimer->Stop();
    delete m_resortTimer;
    
    m_deleteTimer->Stop();
    delete m_deleteTimer;

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
    m_listSpots->Disconnect(wxEVT_DATAVIEW_COLUMN_REORDERED, wxDataViewEventHandler(FreeDVReporterDialog::OnColumnReordered), NULL, this);
    m_listSpots->Disconnect(wxEVT_SET_FOCUS, wxFocusEventHandler(FreeDVReporterDialog::OnSetFocus), NULL, this);

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

wxDataViewColumn* FreeDVReporterDialog::getColumnForModelColId_(unsigned int col)
{
    wxDataViewColumn* item = nullptr;

    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        item = m_listSpots->GetColumn(index);
        if (item->GetModelColumn() == col)
        {
            break;
        }
        item = nullptr;
    }
    assert(item != nullptr);
    return item;
}

void FreeDVReporterDialog::refreshLayout()
{
    // Update row colors
    msgRowBackgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowBackgroundColor);
    msgRowForegroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowForegroundColor);
    txRowBackgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor);
    txRowForegroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor);
    rxRowBackgroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor);
    rxRowForegroundColor = wxColour(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor);
 
    wxDataViewColumn* item = getColumnForModelColId_(DISTANCE_COL);

    if (wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances)
    {
        item->SetTitle("km ");
    }
    else
    {
        item->SetTitle("Miles");
    }
    
    // Refresh frequency units as appropriate.
    item = getColumnForModelColId_(FREQUENCY_COL);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        item->SetTitle("kHz");
    }
    else
    {
        item->SetTitle("MHz");
    }

    // Change direction/heading column label based on preferences
    item = getColumnForModelColId_(HEADING_COL);
    auto renderer = item->GetRenderer();
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal)
    {
        item->SetTitle("Dir");
        item->SetAlignment(wxALIGN_LEFT);
        renderer->SetAlignment(wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
    }
    else
    {
        item->SetTitle("Hdg");
        item->SetAlignment(wxALIGN_RIGHT);
        renderer->SetAlignment(wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
    }
    
    // Hide/show legacy columns
    item = getColumnForModelColId_(LAST_RX_MODE_COL);
    item->SetHidden(!wxGetApp().appConfiguration.enableLegacyModes || !wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(LAST_RX_MODE_COL));
    auto menuItem = showMenu_->FindChildItem(wxID_HIGHEST + 100 + LAST_RX_MODE_COL);
    menuItem->Enable(wxGetApp().appConfiguration.enableLegacyModes);

    // Update filter status in window
    updateFilterStatus_();

    // Refresh all data based on current settings and filters.
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->refreshAllRows();

    // Update status controls.
    wxCommandEvent tmpEvent;
    OnStatusTextChange(tmpEvent);
}

void FreeDVReporterDialog::updateFilterStatus_()
{
    if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter)
    {
        m_filterStatus->SetLabel(wxString::Format("Idle %d", (int)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes));
        
#if wxCHECK_VERSION(3,1,3)
        auto appearance = wxSystemSettings::GetAppearance();
        if (appearance.IsDark())
        {
            m_filterStatus->SetForegroundColour(wxTheColourDatabase->Find("SALMON"));
        }
        else
        {
            m_filterStatus->SetForegroundColour(wxTheColourDatabase->Find("FIREBRICK"));
        }
#endif // wxCHECK_VERSION(3,1,3)
    }
    else
    {
        m_filterStatus->SetLabel(_("Idle Off"));
        m_filterStatus->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
}

void FreeDVReporterDialog::setReporter(std::shared_ptr<FreeDVReporter> const& reporter)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->setReporter(reporter);
    
    if (reporter)
    {
        // Update status message
        auto statusMsg = m_statusMessage->GetValue();
        reporter->updateMessage((const char*)statusMsg.utf8_str());
    }
}

void FreeDVReporterDialog::OnShowColumn(wxCommandEvent& event)
{
    // Invert visibility value
    auto columnId = event.GetId() - wxID_HIGHEST - 100;
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(columnId) = 
        !wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(columnId);

    auto newColValue =
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnVisibility->at(columnId);

    wxMenuItem* menuItem = static_cast<wxMenuItem*>(event.GetEventObject());
    menuItem->Check(newColValue);

    // Set column visibility in wxDataViewCtl.
    auto col = getColumnForModelColId_(columnId);
    col->SetHidden(!newColValue);
}

void FreeDVReporterDialog::OnIdleFilter(wxCommandEvent& event)
{
    if (event.GetId() >= (wxID_HIGHEST + 200))
    {
        auto numMinutes = event.GetId() - wxID_HIGHEST - 200;
        if (numMinutes == 0)
        {
            // need to ask user for number of minutes
            numMinutes = 
                wxGetNumberFromUser(wxEmptyString, _("Enter number of minutes: "), _("Max Idle Time"), wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes, 1, 1440, this);
            if (numMinutes > -1)
            {
                auto menuItems = idleLongerThanMenu_->GetMenuItems();
                bool found = false;
                for (auto& item : menuItems)
                {
                    if (item->GetId() == (wxID_HIGHEST + 200 + numMinutes))
                    {
                        found = true;
                        item->Check(true);
                    }
                    else
                    {
                        item->Check(false);
                    }
                }
                if (!found)
                {
                    auto item = idleLongerThanMenu_->FindItem(event.GetId());
                    item->Check(true);
                }

                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes =
                    numMinutes;
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter = true;
            }
            else
            {
                // No changes needed.
                auto numMinutes = wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes;
                auto menuItems = idleLongerThanMenu_->GetMenuItems();
                bool found = false;
                for (auto& item : menuItems)
                {
                    if (item->GetId() == (wxID_HIGHEST + 200 + numMinutes))
                    {
                        item->Check(true);
                    }
                    else
                    {
                        item->Check(false);
                    }
                }
                if (!found)
                {
                    auto item = idleLongerThanMenu_->FindItem(event.GetId());
                    item->Check(true);
                }
                return;
            }
        }
        else
        {
            auto menuItems = idleLongerThanMenu_->GetMenuItems();
            for (auto& item : menuItems)
            {
                if (item->GetId() == (wxID_HIGHEST + 200 + numMinutes))
                {
                    item->Check(true);
                }
                else
                {
                    item->Check(false);
                }
            }

            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes =
                numMinutes;
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter = true;
        }
    }
    else
    {
        auto menuItems = idleLongerThanMenu_->GetMenuItems();
        for (auto& item : menuItems)
        {
            if (item->GetId() != event.GetId())
            {
                item->Check(false);
            }
            else
            {
                item->Check(true);
            }
        }

        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter = false;
    }

    // Update filter status in window
    updateFilterStatus_();

    // Allow changes to take effect.
    FreeDVReporterDataModel* model = static_cast<FreeDVReporterDataModel*>(m_listSpots->GetModel());
    model->refreshAllRows();
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
        wxGetApp().lastSelectedLoggingRow = MainApp::UNSELECTED;
        m_listSpots->Unselect(item);
    }
}

void FreeDVReporterDialog::OnSystemColorChanged(wxSysColourChangedEvent& event)
{
    // Works around issues on wxWidgets with certain controls not changing backgrounds
    // when the user switches between light and dark mode.

    // TBD - see if this workaround is still necessary
#if 0
    wxColour currentControlBackground = wxTransparentColour;
    m_listSpots->SetBackgroundColour(currentControlBackground);
#if !defined(WIN32)
    ((wxWindow*)m_listSpots->m_headerWin)->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif //!defined(WIN32)
#endif

    updateFilterStatus_();
    event.Skip();
}

void FreeDVReporterDialog::OnInitDialog(wxInitDialogEvent&)
{
    // TBD
}

void FreeDVReporterDialog::OnShow(wxShowEvent&)
{
    wxGetApp().appConfiguration.reporterWindowVisible = true;

    auto selected = m_listSpots->GetSelection();
    if (selected.IsOk())
    {
        m_listSpots->Unselect(selected);
    }
}

void FreeDVReporterDialog::OnSize(wxSizeEvent&)
{
    auto sz = GetSize();
    
    wxGetApp().appConfiguration.reporterWindowWidth = sz.GetWidth();
    wxGetApp().appConfiguration.reporterWindowHeight = sz.GetHeight();

    Layout();
    
#if defined(WIN32)
    // Only auto-resize columns on Windows due to known rendering bugs. Trying to do so on other
    // platforms causes excessive CPU usage for no benefit.
    autosizeColumns();
#endif // defined(WIN32)
}

void FreeDVReporterDialog::OnMove(wxMoveEvent& event)
{
    auto pos = event.GetPosition();
   
    wxGetApp().appConfiguration.reporterWindowLeft = pos.x;
    wxGetApp().appConfiguration.reporterWindowTop = pos.y;
    
    Layout();
    
#if defined(WIN32)
    // Only auto-resize columns on Windows due to known rendering bugs. Trying to do so on other
    // platforms causes excessive CPU usage for no benefit.
    autosizeColumns();
#endif // defined(WIN32)
}

void FreeDVReporterDialog::OnOK(wxCommandEvent&)
{
    // Preserve sort column/ordering
    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        auto colObj = getColumnForModelColId_(index);
        if (colObj != nullptr && colObj->IsSortKey())
        {
            wxGetApp().appConfiguration.reporterWindowCurrentSort = colObj->GetModelColumn();
            wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = colObj->IsSortOrderAscending();
            break;
        }
    }
   
    // Preserve column ordering
#if !defined(WIN32)
    wxDataViewEvent tmp;
    OnColumnReordered(tmp);
#endif // !defined(WIN32)
 
    // Preserve Msg column width
    auto userMsgCol = getColumnForModelColId_(USER_MESSAGE_COL);
    wxGetApp().appConfiguration.reportingUserMsgColWidth = userMsgCol->GetWidth();

    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnSendQSY(wxCommandEvent&)
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

void FreeDVReporterDialog::OnOpenWebsite(wxCommandEvent&)
{
    std::string url = std::string("https://") + (const char*)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->utf8_str() + "/";
    wxLaunchDefaultBrowser(url);
    DeselectItem();
}

void FreeDVReporterDialog::OnClose(wxCloseEvent&)
{
    // Preserve sort column/ordering
    bool found = false;
    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        auto colObj = getColumnForModelColId_(index);
        if (colObj != nullptr && colObj->IsSortKey())
        {
            found = true;
            wxGetApp().appConfiguration.reporterWindowCurrentSort = colObj->GetModelColumn();
            wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = colObj->IsSortOrderAscending();
            break;
        }
    }
   
    if (!found)
    {
        wxGetApp().appConfiguration.reporterWindowCurrentSort = -1;
        wxGetApp().appConfiguration.reporterWindowCurrentSortDirection = true;
    }

    // Preserve column ordering
#if !defined(WIN32)
    wxDataViewEvent tmp;
    OnColumnReordered(tmp);
#endif // !defined(WIN32)
 
    // Preserve Msg column width
    auto userMsgCol = getColumnForModelColId_(USER_MESSAGE_COL);
    wxGetApp().appConfiguration.reportingUserMsgColWidth = userMsgCol->GetWidth();
    
    wxGetApp().appConfiguration.reporterWindowVisible = false;
    Hide();
}

void FreeDVReporterDialog::OnItemSelectionChanged(wxDataViewEvent& event)
{
    if (event.GetItem().IsOk() 
#if !(defined(WIN32) || defined(__APPLE__))
&& isSelectionPossible_
#endif // !(defined(WIN32) || defined(__APPLE__)
        )
    {
        refreshQSYButtonState();

        // Bring up tooltip for longer reporting messages if the user happened to click on that column.
        wxMouseEvent dummyEvent;
        AdjustToolTip(dummyEvent);

        // For logging: make sure FreeDV Reporter information is used
        wxGetApp().lastSelectedLoggingRow = MainApp::FREEDV_REPORTER;
    }
    else
    {
        m_buttonSendQSY->Enable(false);
        DeselectItem();
    }
}

void FreeDVReporterDialog::OnBandFilterChange(wxCommandEvent&)
{
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter = 
        m_bandFilter->GetSelection();
    
    FilterFrequency freq = 
        (FilterFrequency)wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilter.get();
    setBandFilter(freq);

    // Defer deselection until after UI updates
    CallAfter([this]() { DeselectItem(); });
}

void FreeDVReporterDialog::FreeDVReporterDataModel::deallocateRemovedItems()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        std::unique_lock<std::recursive_mutex> lk(dataMtx_);
        
        std::vector<std::string> keysToRemove;
        auto curDate = wxDateTime::Now().ToUTC();
        
        for (auto& item : allReporterData_)
        {
            if (
                item.second->isPendingDelete &&
                !item.second->deleteTime.ToUTC().IsEqualUpTo(curDate, wxTimeSpan(0, 0, 1)))
            {
                keysToRemove.push_back(item.first);
                delete item.second;
            }
        }
        
        for (auto& key : keysToRemove)
        {
            allReporterData_.erase(key);
        }
    };

    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::triggerResort()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        Resort();
    };
    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::updateHighlights()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    
    handler.fn = [this](CallbackHandler&) {
#if defined(WIN32)
        bool doAutoSizeColumns = false;
#endif // define(WIN32)
    
        std::unique_lock<std::recursive_mutex> lk(dataMtx_);

        // Iterate across all visible rows. If a row is currently highlighted
        // green and it's been more than >20 seconds, clear coloring.
        auto curDate = wxDateTime::Now().ToUTC();

        wxDataViewItem currentSelection = parent_->m_listSpots->GetSelection();
        
        wxDataViewItemArray itemsAdded;
        wxDataViewItemArray itemsChanged;
        wxDataViewItemArray itemsDeleted;
        for (auto& item : allReporterData_)
        {
            if (item.second->isPendingDelete)
            {
                if (item.second->isVisible)
                {
                    wxDataViewItem dvi(item.second);
                    item.second->isVisible = false;
                    parent_->Unselect(dvi);
                    itemsDeleted.Add(dvi);
                    
                    if (currentSelection.IsOk() && dvi.GetID() == currentSelection.GetID())
                    {
                        // Selection is no longer valid.
                        currentSelection = wxDataViewItem(nullptr);
                    }
                }
                continue;
            }
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
            wxColour backgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
            wxColour foregroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

            if (isMessaging)
            {
                backgroundColor = parent_->msgRowBackgroundColor;
                foregroundColor = parent_->msgRowForegroundColor;
            }
            else if (isTransmitting)
            {
                backgroundColor = parent_->txRowBackgroundColor;
                foregroundColor = parent_->rxRowForegroundColor;
            }
            else if (isReceivingValidCallsign || isReceivingNotValidCallsign)
            {
                backgroundColor = parent_->rxRowBackgroundColor;
                foregroundColor = parent_->rxRowForegroundColor;
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
            }

            if (parent_->IsShownOnScreen())
            {
                bool newVisibility = !isFiltered_(reportData);
                if (newVisibility != reportData->isVisible)
                {
                    reportData->isVisible = newVisibility;
                    if (newVisibility)
                    {
                        itemsAdded.Add(wxDataViewItem(reportData));
#if defined(WIN32)
                        doAutoSizeColumns = true;
#endif // defined(WIN32)
                    }
                    else
                    {
                        wxDataViewItem dvi(reportData);
                        itemsDeleted.Add(dvi);
                        if (currentSelection.IsOk() && currentSelection.GetID() == dvi.GetID())
                        {
                            // Selection no longer valid
                            currentSelection = wxDataViewItem(nullptr);
                        }
                    }
                    sortOnNextTimerInterval = true;
                }
                else
                {
                    if (isHighlightUpdated || reportData->isPendingUpdate)
                    {
                        if (reportData->isVisible)
                        {
                            reportData->isPendingUpdate = false;

                            wxDataViewItem dvi(reportData);
                            itemsChanged.Add(dvi);
                        }
                    }
                }
            }
        }

        if (itemsDeleted.size() > 0 || itemsAdded.size() > 0)
        {
            Cleared(); // avoids spurious errors on macOS
            if (currentSelection.IsOk())
            {
                // Reselect after redraw
                parent_->CallAfter([&, currentSelection]() {
                    parent_->m_listSpots->Select(currentSelection);
                });
            }
        }
        else if (itemsChanged.size() > 0)
        {
            // Temporarily disable autosizing prior to item updates.
            // This is due to performance issues on macOS -- see https://github.com/wxWidgets/wxWidgets/issues/25972
            for (unsigned int index = 0; index < parent_->m_listSpots->GetColumnCount(); index++)
            {
                auto col = parent_->m_listSpots->GetColumn(index);
                col->SetWidth(col->GetWidth()); // GetWidth doesn't return AUTOSIZE
            }
            
            ItemsChanged(itemsChanged);

            // Re-enable autosizing
            for (unsigned int index = 0; index < parent_->m_listSpots->GetColumnCount(); index++)
            {
                auto col = parent_->m_listSpots->GetColumn(index);
                if (col->GetModelColumn() != USER_MESSAGE_COL && index != RIGHTMOST_COL)
                {
                    col->SetWidth(wxCOL_WIDTH_AUTOSIZE);
                }
            }
        }
        
#if defined(WIN32)
        // Only auto-resize columns on Windows due to known rendering bugs. Trying to do so on other
        // platforms causes excessive CPU usage for no benefit.
        if (itemsChanged.size() > 0 || doAutoSizeColumns)
        {
            parent_->autosizeColumns();
        }
#endif // defined(WIN32)
    
    };
    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::OnTimer(wxTimerEvent& event)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    if (event.GetTimer().GetId() == m_highlightClearTimer->GetId())
    {
        model->updateHighlights();
    }
    else if (event.GetTimer().GetId() == m_deleteTimer->GetId())
    {
        model->deallocateRemovedItems();
    }
    else
    {
        if (!IsShownOnScreen()) return;
        if (model->sortOnNextTimerInterval)
        {
            model->triggerResort();
            model->sortOnNextTimerInterval = false;
        }
    }
}

void FreeDVReporterDialog::OnFilterTrackingEnable(wxCommandEvent&)
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
    CallAfter([this]() { DeselectItem(); });
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

void FreeDVReporterDialog::AdjustToolTip(wxMouseEvent&)
{
    const wxPoint pt = wxGetMousePosition();
    int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
    int mouseY = pt.y - m_listSpots->GetScreenPosition().y;
    
    wxRect rect;
    wxDataViewItem item;
    wxDataViewColumn* col;
    m_listSpots->HitTest(wxPoint(mouseX, mouseY), item, col);
    if (item.IsOk() && IsActive())
    {
        // Linux workaround to avoid inadvertent selections.
        isSelectionPossible_ = true;

        // Show popup corresponding to the full message.
        FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
        tempUserMessage_ = _("");
        tempCallsign_ = _("");
    
        if (col->GetModelColumn() == USER_MESSAGE_COL)
        {
            tempUserMessage_ = model->getUserMessage(item);
            rect = m_listSpots->GetItemRect(item, col);
            if (tipWindow_ == nullptr && tempUserMessage_ != _(""))
            {
                // Use screen coordinates to determine bounds.
                auto pos = rect.GetPosition();
                rect.SetPosition(ClientToScreen(pos));
        
                tipWindow_ = new wxTipWindow(m_listSpots, tempUserMessage_, 1000, &tipWindow_, &rect);
                tipWindow_->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
                tipWindow_->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::SkipMouseEvent), NULL, this);
                tipWindow_->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::OnLeftClickTooltip), NULL, this);
            
                // Make sure we actually override behavior of needed events inside the tooltip.
                for (auto& child : tipWindow_->GetChildren())
                {
                    child->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::SkipMouseEvent), NULL, this);
                    child->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(FreeDVReporterDialog::OnRightClickSpotsList), NULL, this);
                    child->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(FreeDVReporterDialog::OnLeftClickTooltip), NULL, this);
                }
            }
        }
        else if (col->GetModelColumn() == CALLSIGN_COL)
        {
            tempCallsign_ = model->getCallsign(item);
        }
        else if (col->GetModelColumn() == LAST_RX_CALLSIGN_COL)
        {
            tempCallsign_ = model->getRxCallsign(item);
        }
    }
    else
    {
        tempUserMessage_ = _("");
        tempCallsign_ = _("");
    }
}

void FreeDVReporterDialog::OnRightClickSpotsList(wxContextMenuEvent&)
{
    wxDataViewEvent contextEvent;
    OnItemRightClick(contextEvent);
}

void FreeDVReporterDialog::SkipMouseEvent(wxMouseEvent&)
{
    wxDataViewEvent contextEvent;
    OnItemRightClick(contextEvent);
}

void FreeDVReporterDialog::OnLeftClickTooltip(wxMouseEvent& event)
{
    // Ensure that item is selected after tooltip closes
    CallAfter([this]() {
        const wxPoint pt = wxGetMousePosition();
        int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
        int mouseY = pt.y - m_listSpots->GetScreenPosition().y;
    
        wxDataViewItem item;
        wxDataViewColumn* col;
        m_listSpots->HitTest(wxPoint(mouseX, mouseY), item, col);
        if (item.IsOk() && IsActive())
        {
            m_listSpots->Select(item);
        }
    });
    
    // Allow tip window to handle event
    event.Skip();
}

void FreeDVReporterDialog::OnSetFocus(wxFocusEvent& event)
{
    CallAfter([this]() {
        const wxPoint pt = wxGetMousePosition();
        int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
        int mouseY = pt.y - m_listSpots->GetScreenPosition().y;

        wxDataViewItem item;
        wxDataViewColumn* col;
        m_listSpots->HitTest(wxPoint(mouseX, mouseY), item, col);
        if (item.IsOk())
        {
            m_listSpots->Select(item);
        }
    });
    
    event.Skip();
}

void FreeDVReporterDialog::OnColumnReordered(wxDataViewEvent&)
{
    // Preserve new column ordering
    // Note: Windows uses the same indices for model column and GetColumn()
    // so we need to use an alternate implementation for that platform.
#if defined(WIN32)
    CallAfter([this]() {
        std::vector<int> newColPositions;
        std::stringstream ss;
        auto headerCtrl = m_listSpots->GenericGetHeader();
        wxArrayInt wxColumnOrder = headerCtrl->GetColumnsOrder();
        for (unsigned int index = 0; index < wxColumnOrder.GetCount(); index++)
        {
            auto col = wxColumnOrder.Item(index);
            if (col < NUM_COLS)
            {
                newColPositions.push_back(col);
                ss << col << " ";
            }
        }

        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder = newColPositions;
        log_info("New column ordering: %s", ss.str().c_str());
    });
#else
    std::stringstream ss;
    std::vector<int> newColPositions;
    for (unsigned int index = 0; index < NUM_COLS; index++)
    {
        auto dvc = m_listSpots->GetColumn(index);
        newColPositions.push_back(dvc->GetModelColumn());
        ss << dvc->GetModelColumn() << " ";
    }
    
    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterColumnOrder = newColPositions;
    log_info("New column ordering: %s", ss.str().c_str());
#endif // defined(WIN32)
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

void FreeDVReporterDialog::OnItemRightClick(wxDataViewEvent&)
{
    // Make sure item's deselected as it should only be selected on
    // left-click.
    DeselectItem();

    if (tipWindow_ != nullptr)
    {
        tipWindow_->Close();
        tipWindow_ = nullptr;
    }

    // Only show the popup if we're already hovering over a message or callsign.
    const wxPoint pt = wxGetMousePosition();
    int mouseX = pt.x - m_listSpots->GetScreenPosition().x;
    int mouseY = pt.y - m_listSpots->GetScreenPosition().y;
    
    if (tempUserMessage_ != _(""))
    {
        // 170 here has been determined via experimentation to avoid an issue 
        // on some KDE installations where the popup menu immediately closes after
        // right-clicking. This in effect causes the popup to open to the left of
        // the mouse pointer.
        m_listSpots->PopupMenu(spotsPopupMenu_, wxPoint(mouseX - 170, mouseY));
    }
    else if (tempCallsign_ != _(""))
    {
        m_listSpots->PopupMenu(callsignPopupMenu_, wxPoint(mouseX, mouseY));
    }
}

void FreeDVReporterDialog::OnCopyUserMessage(wxCommandEvent&)
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

void FreeDVReporterDialog::OnQRZLookup(wxCommandEvent&)
{
    if (tempCallsign_ != _(""))
    {
        wxLaunchDefaultBrowser(wxString::Format("https://www.qrz.com/db/%s", tempCallsign_));
    }
}

void FreeDVReporterDialog::OnHamQTHLookup(wxCommandEvent&)
{
    if (tempCallsign_ != _(""))
    {
        wxLaunchDefaultBrowser(wxString::Format("https://www.hamqth.com/%s", tempCallsign_));
    }
}

void FreeDVReporterDialog::OnHamCallLookup(wxCommandEvent&)
{
    if (tempCallsign_ != _(""))
    {
        wxLaunchDefaultBrowser(wxString::Format("https://hamcall.net/call?callsign=%s", tempCallsign_));
    }
}

void FreeDVReporterDialog::OnStatusTextChange(wxCommandEvent&)
{
    auto statusMsg = m_statusMessage->GetValue();
    int insertPoint = m_statusMessage->GetInsertionPoint();

    log_debug("Status message currently %s", (const char*)statusMsg.ToUTF8());

    // Linux workaround: it's possible for some emoji flags to backspace
    // and not fully remove the flag. This logic scans the current value
    // of statusMsg for the appropriate Unicode sequence 
    // (U+1F3F4 + ... + U+E007F). If the beginning character is there
    // but not the end character, the intermediate tag characters are
    // removed along with the beginng.
    //
    // Note: it still takes a couple of backspaces before the delete process
    // begins so this is not a full workaround. Still, this significantly
    // reduces the number of times one must backspace in order to completely
    // remove the flag.
    int index = 0;
    while (index < (int)statusMsg.Length())
    {
        auto chr = statusMsg.GetChar(index);
        if (chr.GetValue() == 0x1F3F4)
        {
            log_debug("Found start char at index %d", index);
            auto endIndex = index + 1;
            bool foundEnd = false;
            while (endIndex < (int)statusMsg.Length())
            {
                auto endChar = statusMsg.GetChar(endIndex).GetValue();
                if (endChar == 0xE007F)
                {
                    foundEnd = true;
                    break;
                }
                else if (!(endChar >= 0xE0061 && endChar <= 0xE007A))
                {
                    // Not lowercase tag characters; missing end character.
                    endIndex--;
                    break;
                }
                endIndex++;
            }
            log_debug("Found end at %d", endIndex);

            if (!foundEnd && endIndex != index)
            {
                // Strip tag characters and beginning
                statusMsg = statusMsg.SubString(0, index - 1) + statusMsg.Mid(endIndex + 1);
                insertPoint = index;
                log_debug("status msg is now %s", (const char*)statusMsg.ToUTF8());
            }
            else
            {
                index = endIndex;
            }
        }
        else if (chr.GetValue() >= 0xE0000 && chr.GetValue() <= 0xE007F)
        {
            // Remove any stray tag characters that might still remain.
            statusMsg.Remove(index, 1);
            continue;
        }

        index++;
    }

    if (statusMsg != m_statusMessage->GetValue())
    {
        m_statusMessage->SetValue(statusMsg);

        // Reset cursor position to end of new character
        m_statusMessage->SetInsertionPoint(insertPoint);
    }
}

void FreeDVReporterDialog::OnStatusTextSend(wxCommandEvent&)
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

void FreeDVReporterDialog::OnStatusTextSendContextMenu(wxContextMenuEvent&)
{
    DeselectItem();
    m_buttonSend->PopupMenu(setPopupMenu_);
}

void FreeDVReporterDialog::OnStatusTextSendAndSaveMessage(wxCommandEvent& event)
{
    OnStatusTextSend(event);
    OnStatusTextSaveMessage(event);
}

void FreeDVReporterDialog::OnStatusTextSaveMessage(wxCommandEvent&)
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

void FreeDVReporterDialog::OnStatusTextClear(wxCommandEvent&)
{
    FreeDVReporterDataModel* model = (FreeDVReporterDataModel*)spotsDataModel_.get();
    model->updateMessage("");

    wxGetApp().appConfiguration.reportingConfiguration.freedvReporterStatusText = "";
    m_statusMessage->SetValue("");

    DeselectItem();
}

void FreeDVReporterDialog::OnStatusTextClearContextMenu(wxContextMenuEvent&)
{
    m_buttonClear->PopupMenu(clearPopupMenu_);
}

void FreeDVReporterDialog::OnStatusTextClearSelected(wxCommandEvent&)
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

void FreeDVReporterDialog::OnStatusTextClearAll(wxCommandEvent&)
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

    // Update filter status in window
    updateFilterStatus_();
}

#if defined(WIN32)
void FreeDVReporterDialog::autosizeColumns()
{
    for (unsigned int index = 0; index < m_listSpots->GetColumnCount(); index++)
    {
        if (index != USER_MESSAGE_COL)
        {
            // USER_MESSAGE_COL width is preserved and should not be messed with.
            auto col = getColumnForModelColId_(index);
            col->SetWidth(wxCOL_WIDTH_DEFAULT);
            col->SetWidth(wxCOL_WIDTH_AUTOSIZE);
        }
    }
}
#endif // defined(WIN32)

void FreeDVReporterDialog::FreeDVReporterDataModel::setBandFilter(FilterFrequency freq)
{
    filteredFrequency_ = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
    currentBandFilter_ = freq;

    refreshAllRows();
}

wxString FreeDVReporterDialog::FreeDVReporterDataModel::makeValidTime_(std::string const& timeStr, wxDateTime& timeObj)
{
    wxRegEx millisecondsRemoval(parent_->MS_REMOVAL_RGX);
    wxString tmp = timeStr;
    millisecondsRemoval.Replace(&tmp, parent_->EMPTY_STR);
    
    wxRegEx timezoneRgx(parent_->TIMEZONE_RGX);
    wxDateTime::TimeZone timeZone(0); // assume UTC by default
    if (timezoneRgx.Matches(tmp))
    {
        auto tzOffset = timezoneRgx.GetMatch(tmp, 1);
        auto hours = timezoneRgx.GetMatch(tmp, 2);
        auto minutes = timezoneRgx.GetMatch(tmp, 3);
        
        int tzMinutes = wxAtoi(hours) * 60;
        tzMinutes += wxAtoi(minutes);
        
        if (tzOffset == parent_->TZ_OFFSET_STR)
        {
            tzMinutes = -tzMinutes;
        }
        
        timezoneRgx.Replace(&tmp, parent_->EMPTY_STR);
        
        timeZone = wxDateTime::TimeZone(tzMinutes);
    }
    
    wxDateTime tmpDate(wxInvalidDateTime);
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
#if wxCHECK_VERSION(3,2,0)
        wxString sysLocale = wxUILocale::GetCurrent().GetName();
        auto sysLocaleT = newlocale(LC_TIME_MASK, (const char*)sysLocale.ToUTF8(), NULL);
        if (sysLocaleT != nullptr)
        {
            strftime_l(buf, sizeof(buf), (const char*)parent_->TIME_FORMAT_STR.ToUTF8(), &tmpTm, sysLocaleT);
            freelocale(sysLocaleT);
        }
        else
#endif // wxCHECK_VERSION(3,2,0)
        {
            strftime(buf, sizeof(buf), (const char*)parent_->TIME_FORMAT_STR.ToUTF8(), &tmpTm);
        }
        return buf;
#else
        return tmpDate.Format(parent_->TIME_FORMAT_STR, timeZone);
#endif // __APPLE__
    }
    else
    {
        timeObj = wxDateTime(wxInvalidDateTime);
        return parent_->UNKNOWN_STR;
    }
}

double FreeDVReporterDialog::FreeDVReporterDataModel::calculateDistance_(wxString gridSquare1, wxString gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;
    
    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare_(std::move(gridSquare1), lat1, lon1);
    calculateLatLonFromGridSquare_(std::move(gridSquare2), lat2, lon2);
    
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
    const wxRegEx allLetters(parent_->ALL_LETTERS_RGX);
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
    calculateLatLonFromGridSquare_(std::move(gridSquare1), lat1, lon1);
    calculateLatLonFromGridSquare_(std::move(gridSquare2), lat2, lon2);

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
    std::unique_lock<std::mutex> lk(fnQueueMtx_, std::defer_lock_t());
    lk.lock();
    auto size = fnQueue_.size();
    lk.unlock();

    while(size > 0)
    {
        lk.lock();
        auto handler = std::move(fnQueue_[0]);
        lk.unlock();

        handler.fn(handler);

        lk.lock();
        fnQueue_.pop_front();
        size = fnQueue_.size();
        lk.unlock();
    }
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

bool FreeDVReporterDialog::FreeDVReporterDataModel::filtersEnabled() const
{
    return 
        (currentBandFilter_ != FilterFrequency::BAND_ALL) ||
        (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency &&
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq) ||
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter;
}

bool FreeDVReporterDialog::FreeDVReporterDataModel::isFiltered_(ReporterData* data)
{
    bool isHidden = false;
    if (!parent_->IsShownOnScreen())
    {
        return true;
    }

    if (currentBandFilter_ != FilterFrequency::BAND_ALL)
    {
        auto freq = data->frequency;
        auto bandForFreq = parent_->getFilterForFrequency_(freq);

        isHidden =  
            (bandForFreq != currentBandFilter_) ||
            (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksFrequency &&
                wxGetApp().appConfiguration.reportingConfiguration.freedvReporterBandFilterTracksExactFreq &&
                freq != filteredFrequency_);
    }

    // If enabled, hide rows that exceed the max idle time.
    if (!isHidden && wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnableMaxIdleFilter)
    {
        // "Idle" is defined as someone who either:
        // * Connected more than X minutes ago and has yet to transmit, or
        // * Last transmitted more than X minutes ago.
        int maxIdleTimeMins = wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMaxIdleMinutes;
        wxDateTime maxIdleDate;
        wxDateTime origDate;
        if (data->lastTxDate.IsValid())
        {
            origDate = data->lastTxDate;
        }
        else
        {
            origDate = data->connectTime;
        }

        maxIdleDate = origDate.Add(wxTimeSpan(maxIdleTimeMins / 60, maxIdleTimeMins % 60));
        isHidden = maxIdleDate.IsEarlierThan(wxDateTime::Now());
    }

    return isHidden;
}

wxString FreeDVReporterDialog::FreeDVReporterDataModel::GetCardinalDirection_(int degrees)
{
    int cardinalDirectionNumber( static_cast<int>( ( ( degrees / 360.0 ) * 16 ) + 0.5 )  % 16 );
    const char* const cardinalDirectionTexts[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    return cardinalDirectionTexts[cardinalDirectionNumber];
}

FreeDVReporterDialog::FreeDVReporterDataModel::FreeDVReporterDataModel(FreeDVReporterDialog* parent)
    : sortOnNextTimerInterval(false)
    , isConnected_(false)
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
    
    for (auto& kvp : allReporterData_)
    {
        delete kvp.second;
    }
    allReporterData_.clear();
}

void FreeDVReporterDialog::FreeDVReporterDataModel::setReporter(std::shared_ptr<FreeDVReporter> const& reporter)
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
        
        reporter_->setRecvEndFn(FreeDVReporter::RecvEndFn());
    }
    
    auto isChanged = reporter_ != reporter;
    reporter_ = reporter;
    
    if (isChanged)
    {
        if (reporter_)
        {
            reporter_->setOnReporterConnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReporterConnect_, this));
            reporter_->setOnReporterDisconnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReporterDisconnect_, this));
    
            reporter_->setOnUserConnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onUserConnectFn_, this, _1, _2, _3, _4, _5, _6, _7));
            reporter_->setOnUserDisconnectFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onUserDisconnectFn_, this, _1, _2, _3, _4, _5, _6, _7));
            reporter_->setOnFrequencyChangeFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onFrequencyChangeFn_, this, _1, _2, _3, _4, _5));
            reporter_->setOnTransmitUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onTransmitUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));
            reporter_->setOnReceiveUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onReceiveUpdateFn_, this, _1, _2, _3, _4, _5, _6, _7));

            reporter_->setMessageUpdateFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onMessageUpdateFn_, this, _1, _2, _3));
            reporter_->setConnectionSuccessfulFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onConnectionSuccessfulFn_, this));
            reporter_->setAboutToShowSelfFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onAboutToShowSelfFn_, this));
            reporter_->setRecvEndFn(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::onRecvEndFn_, this));
        }
        else
        {
            // Spot list no longer valid, delete the items currently on there.
            // In case there's anything else on the queue prior to this, let those
            // actions fully execute before clearing entries.
            log_debug("Reporter object set to null");
            std::unique_lock<std::mutex> lk(fnQueueMtx_);
            CallbackHandler handler;
            handler.fn = [this](CallbackHandler&) {
                clearAllEntries_();
            };

            fnQueue_.push_back(std::move(handler));
            parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
        }
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

        row.second->isPendingDelete = true;
        row.second->deleteTime = wxDateTime::Now();
    }
    
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
        {
            // Blank entries should drop to the bottom when sorting in ascending order.
            wxString leftCopy(leftData->userMessage);
            wxString rightCopy(rightData->userMessage);
            leftCopy.Trim(false);
            leftCopy.Trim(true);
            rightCopy.Trim(false);
            rightCopy.Trim(true);

            if (leftCopy == "")
            {
                result = 1;
            }
            else if (rightCopy == "")
            {
                result = -1;
            }
            else
            {
                result = leftCopy.CmpNoCase(rightCopy);
            }
            break;
        }
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
            result = leftData->snrVal - rightData->snrVal;
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
        
        if (col < RIGHTMOST_COL)
        {
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
            if (row.second->isVisible && !row.second->isPendingDelete)
            {
                count++;
                
                wxDataViewItem newItem(row.second);
                children.Add(newItem);
            }
        }

        return count;
    }
}

wxDataViewItem FreeDVReporterDialog::FreeDVReporterDataModel::GetParent (const wxDataViewItem &) const
{
    // Return root item
    return wxDataViewItem(nullptr);
}

#if !wxCHECK_VERSION(3,2,0)
unsigned int FreeDVReporterDialog::FreeDVReporterDataModel::GetColumnCount () const
{
    return RIGHTMOST_COL + 1;
}

wxString FreeDVReporterDialog::FreeDVReporterDataModel::GetColumnType (unsigned int) const
{
    wxVariant tmp("");
    return tmp.GetType();   
}
#endif // !wxCHECK_VERSION(3,2,0)


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

bool FreeDVReporterDialog::FreeDVReporterDataModel::SetValue (const wxVariant &, const wxDataViewItem &, unsigned int)
{
    // I think this can just return false without changing anything as this is read-only (other than what comes from the server).
    return false;
}

void FreeDVReporterDialog::FreeDVReporterDataModel::refreshAllRows()
{
    std::unique_lock<std::recursive_mutex> lk(dataMtx_);

#if defined(WIN32)
    bool doAutoSizeColumns = false;
#endif // defined(WIN32)
    
    wxDataViewItemArray itemsAdded;
    wxDataViewItemArray itemsDeleted;
    wxDataViewItem currentSelection = parent_->m_listSpots->GetSelection();
    
    for (auto& kvp : allReporterData_)
    {
    	if (kvp.second->isPendingDelete)
    	{
    	    continue;
    	}

        bool updated = false;
        double frequencyUserReadable = kvp.second->frequency / 1000.0;
        wxString frequencyString;
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            frequencyString = wxNumberFormatter::ToString(frequencyUserReadable, 1);
        }
        else
        {
            frequencyUserReadable /= 1000.0;
            frequencyString = wxNumberFormatter::ToString(frequencyUserReadable, 4);
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
        bool newVisibility = !isFiltered_(kvp.second);
        if (newVisibility != kvp.second->isVisible)
        {
            kvp.second->isVisible = newVisibility;
            if (newVisibility)
            {
                itemsAdded.Add(wxDataViewItem(kvp.second));
#if defined(WIN32)
                doAutoSizeColumns = true;
#endif // defined(WIN32)
            }
            else
            {
                wxDataViewItem dvi(kvp.second);
                itemsDeleted.Add(dvi);
                
                if (currentSelection.IsOk() && currentSelection.GetID() == dvi.GetID())
                {
                    // Selection no longer valid
                    currentSelection = wxDataViewItem(nullptr);
                }
            }
            sortOnNextTimerInterval = true;
        }
        else if (updated && kvp.second->isVisible)
        {
            kvp.second->isPendingUpdate = true;
        }
    }
    
    if (itemsDeleted.size() > 0 || itemsAdded.size() > 0)
    {
        Cleared(); // avoids spurious errors on macOS
        if (currentSelection.IsOk())
        {
            // Reselect after redraw
            parent_->CallAfter([&, currentSelection]() {
                parent_->m_listSpots->Select(currentSelection);
            });
        }
    }
    
#if defined(WIN32)
    if (doAutoSizeColumns)
    {
        // Only auto-resize columns on Windows due to known rendering bugs. Trying to do so on other
        // platforms causes excessive CPU usage for no benefit.
        parent_->autosizeColumns();
    }
#endif // defined(WIN32)
}

void FreeDVReporterDialog::FreeDVReporterDataModel::requestQSY(wxDataViewItem selectedItem, uint64_t, wxString const& customText)
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
    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        log_debug("Connected to server");
        filterSelfMessageUpdates_ = false;
        clearAllEntries_();
    };

    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onReporterDisconnect_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        log_debug("Disconnected from server");
        isConnected_ = false;
        filterSelfMessageUpdates_ = false;
        clearAllEntries_();
    };

    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onUserConnectFn_(std::string sid, std::string lastUpdate, std::string callsign, std::string gridSquare, std::string version, bool rxOnly, std::string connectTime)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.lastUpdate = std::move(lastUpdate);
    handler.callsign = std::move(callsign);
    handler.gridSquare = std::move(gridSquare);
    handler.version = std::move(version);
    handler.rxOnly = rxOnly;
    handler.connectTime = std::move(connectTime);

    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        assert(wxThread::IsMain());

        std::string sid = std::move(handler.sid);
        std::string lastUpdate = std::move(handler.lastUpdate);
        std::string callsign = std::move(handler.callsign);
        std::string gridSquare = std::move(handler.gridSquare);
        std::string version = std::move(handler.version);
        std::string connectTime = std::move(handler.connectTime);
        bool rxOnly = handler.rxOnly;

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
            temp->distance = parent_->UNKNOWN_STR;
            temp->distanceVal = 0;
            temp->heading = parent_->UNKNOWN_STR;
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

            temp->distance = wxNumberFormatter::ToString(temp->distanceVal, 0);

            if (wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare == gridSquareWxString)
            {
                temp->headingVal = 0;
                temp->heading = parent_->UNKNOWN_STR;
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
        temp->freqString = parent_->UNKNOWN_STR;
        temp->userMessage = parent_->UNKNOWN_STR;
        temp->transmitting = false;
        
        if (rxOnly)
        {
            temp->status = RX_ONLY_STATUS;
            temp->txMode = parent_->UNKNOWN_STR;
            temp->lastTx = parent_->UNKNOWN_STR;
        }
        else
        {
            temp->status = parent_->UNKNOWN_STR;
            temp->txMode = parent_->UNKNOWN_STR;
            temp->lastTx = parent_->UNKNOWN_STR;
        }
        
        temp->lastRxCallsign = parent_->UNKNOWN_STR;
        temp->lastRxMode = parent_->UNKNOWN_STR;
        temp->snrVal = UNKNOWN_SNR_VAL;
        temp->snr = parent_->UNKNOWN_STR;
        
        // Default to sane colors for rows. If we need to highlight, the timer will change
        // these later. 
        //
#if defined(__APPLE__)
        // To ensure that the columns don't have a different color than the rest of the control.
        // Needed mainly for macOS.
        temp->backgroundColor = wxColour(wxTransparentColour);
#else
        temp->backgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
#endif // defined(__APPLE__)
        temp->foregroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

        auto lastUpdateTime = makeValidTime_(lastUpdate, temp->lastUpdateDate);
        temp->lastUpdate = lastUpdateTime;
        makeValidTime_(connectTime, temp->connectTime);
        // defer visibility until timer update
        temp->isVisible = false;
        temp->isPendingUpdate = false;

        if (allReporterData_.find(sid) != allReporterData_.end() && !isConnected_)
        {
            log_warn("Received duplicate user during connection process");
            return;
        }
        
        temp->isPendingDelete = false;
        
        auto existsIter = allReporterData_.find(sid);
        if (existsIter != allReporterData_.end())
        {
            // Pending deletion prior to reconnect, so go ahead and delete now.
            delete existsIter->second;
        }
        allReporterData_[sid] = temp;
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onConnectionSuccessfulFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);

    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));

        log_debug("Fully connected to server");

        // Enable highlighting now that we're fully connected.
        isConnected_ = true;
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onUserDisconnectFn_(std::string sid, std::string const&, std::string const&, std::string const&, std::string const&, bool, std::string const&)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);

    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
        assert(wxThread::IsMain());

        std::string sid = std::move(handler.sid);
        log_debug("User with SID %s disconnected", sid.c_str());

        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            auto item = iter->second;
            wxDataViewItem dvi(item);
            item->isPendingDelete = true;
            item->deleteTime = wxDateTime::Now();
        }
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onFrequencyChangeFn_(std::string sid, std::string lastUpdate, std::string const&, std::string const&, uint64_t frequencyHz)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.frequencyHz = frequencyHz;
    handler.lastUpdate = std::move(lastUpdate);
    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
       
        std::string sid = std::move(handler.sid);
        uint64_t frequencyHz = handler.frequencyHz;
        std::string lastUpdate = std::move(handler.lastUpdate);
 
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            if (iter->second->isPendingDelete)
            {
                log_warn("Got frequency change for user pending deletion (%s/%s, SID %s)", (const char*)iter->second->callsign.ToUTF8(), (const char*)iter->second->gridSquare.ToUTF8(), iter->second->sid.c_str());
                return;
            }

            double frequencyUserReadable = frequencyHz / 1000.0;
            wxString frequencyString;
            
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
            {
                frequencyString = wxNumberFormatter::ToString(frequencyUserReadable, 1);
            }
            else
            {
                frequencyUserReadable /= 1000.0;
                frequencyString = wxNumberFormatter::ToString(frequencyUserReadable, 4);
            }
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);

            auto sortingColumn = parent_->m_listSpots->GetSortingColumn();
            bool isChanged = 
                (sortingColumn == parent_->getColumnForModelColId_(FREQUENCY_COL) && iter->second->frequency != frequencyHz) ||
                (sortingColumn == parent_->getColumnForModelColId_(LAST_UPDATE_DATE_COL) && iter->second->lastUpdate != lastUpdateTime);
            bool isDataChanged = 
                (iter->second->frequency != frequencyHz ||
                 iter->second->lastUpdate != lastUpdateTime);
            
            iter->second->frequency = frequencyHz;
            iter->second->freqString = frequencyString;
            iter->second->lastUpdate = lastUpdateTime;

            if (iter->second->isVisible)
            {            
                iter->second->isPendingUpdate = isDataChanged;
                sortOnNextTimerInterval |= isChanged;
            }
        }
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onTransmitUpdateFn_(std::string sid, std::string lastUpdate, std::string const&, std::string const&, std::string txMode, bool transmitting, std::string lastTxDate)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.txMode = std::move(txMode);
    handler.transmitting = transmitting;
    handler.lastTxDate = std::move(lastTxDate);
    handler.lastUpdate = std::move(lastUpdate);
    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
   
        std::string sid = std::move(handler.sid);
        std::string txMode = std::move(handler.txMode);
        bool transmitting = handler.transmitting;
        std::string lastTxDate = std::move(handler.lastTxDate);
        std::string lastUpdate = std::move(handler.lastUpdate); 

        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {
            if (iter->second->isPendingDelete)
            {
                log_warn("Got TX change for user pending deletion (%s/%s, SID %s)", (const char*)iter->second->callsign.ToUTF8(), (const char*)iter->second->gridSquare.ToUTF8(), iter->second->sid.c_str());
                return;
            }

            bool isChanged = iter->second->transmitting != transmitting;
            bool isDataChanged = isChanged;
            
            iter->second->transmitting = transmitting;
        
            auto sortingColumn = parent_->m_listSpots->GetSortingColumn();
            std::string txStatus = "RX";
            if (transmitting)
            {
                txStatus = "TX";
            }
        
            if (iter->second->status != _(RX_ONLY_STATUS))
            {
                isChanged |=
                    (sortingColumn == parent_->getColumnForModelColId_(STATUS_COL) && iter->second->status != txStatus) ||
                    (sortingColumn == parent_->getColumnForModelColId_(TX_MODE_COL) && iter->second->txMode != txMode);
                isDataChanged |=
                    iter->second->status != txStatus ||
                    iter->second->txMode != txMode;
                
                iter->second->status = txStatus;
                iter->second->txMode = txMode;
            
                auto lastTxTime = makeValidTime_(lastTxDate, iter->second->lastTxDate);
                isChanged |= (sortingColumn == parent_->getColumnForModelColId_(LAST_TX_DATE_COL) && iter->second->lastTx != lastTxTime);
                isDataChanged |= iter->second->lastTx != lastTxTime;
                iter->second->lastTx = lastTxTime;
            }
        
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;

            if (iter->second->isVisible)
            { 
                iter->second->isPendingUpdate = isDataChanged;
                sortOnNextTimerInterval |= isChanged;
            }
        }
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onReceiveUpdateFn_(std::string sid, std::string lastUpdate, std::string const&, std::string const&, std::string receivedCallsign, float snr, std::string rxMode)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.lastUpdate = std::move(lastUpdate);
    handler.receivedCallsign = std::move(receivedCallsign);
    handler.snr = snr;
    handler.rxMode = std::move(rxMode);
    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
   
        std::string sid = std::move(handler.sid);
        std::string lastUpdate = std::move(handler.lastUpdate);
        std::string receivedCallsign = std::move(handler.receivedCallsign);
        float snr = handler.snr;
        std::string rxMode = std::move(handler.rxMode);
 
        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {            
            if (iter->second->isPendingDelete)
            {
                log_warn("Got RX change for user pending deletion (%s/%s, SID %s)", (const char*)iter->second->callsign.ToUTF8(), (const char*)iter->second->gridSquare.ToUTF8(), iter->second->sid.c_str());
                return;
            }

            wxString receivedCallsignWx(receivedCallsign);
            wxString rxModeWx(rxMode);

            auto sortingColumn = parent_->m_listSpots->GetSortingColumn();
            bool isChanged = 
                (sortingColumn == parent_->getColumnForModelColId_(LAST_RX_CALLSIGN_COL) && iter->second->lastRxCallsign != receivedCallsignWx) ||
                (sortingColumn == parent_->getColumnForModelColId_(LAST_RX_MODE_COL) && iter->second->lastRxMode != rxModeWx);
            bool isDataChanged =
                iter->second->lastRxCallsign != receivedCallsignWx ||
                iter->second->lastRxMode != rxModeWx;
            
            iter->second->lastRxCallsign = receivedCallsignWx;
            iter->second->lastRxMode = rxModeWx;

            wxString snrString = wxString::Format(parent_->SNR_FORMAT_STR, snr);
            if (receivedCallsign == "" && rxMode == "")
            {
                // Frequency change--blank out SNR too.
                isChanged |=
                    (sortingColumn == parent_->getColumnForModelColId_(LAST_RX_CALLSIGN_COL) && iter->second->lastRxCallsign != parent_->UNKNOWN_STR) ||
                    (sortingColumn == parent_->getColumnForModelColId_(LAST_RX_MODE_COL) && iter->second->lastRxMode != parent_->UNKNOWN_STR) ||
                    (sortingColumn == parent_->getColumnForModelColId_(SNR_COL) && iter->second->snr != parent_->UNKNOWN_STR) ||
                    iter->second->lastRxDate.IsValid();
                isDataChanged |=
                    iter->second->lastRxCallsign != parent_->UNKNOWN_STR ||
                    iter->second->lastRxMode != parent_->UNKNOWN_STR ||
                    iter->second->snr != parent_->UNKNOWN_STR ||
                    iter->second->lastRxDate.IsValid();
                
                iter->second->lastRxCallsign = parent_->UNKNOWN_STR;
                iter->second->lastRxMode = parent_->UNKNOWN_STR;
                iter->second->snrVal = UNKNOWN_SNR_VAL;
                iter->second->snr = parent_->UNKNOWN_STR;
                iter->second->lastRxDate = wxDateTime(wxInvalidDateTime);
            }
            else
            {
                isChanged |=
                    (sortingColumn == parent_->getColumnForModelColId_(SNR_COL) && iter->second->snr != snrString);
                isDataChanged |=
                    iter->second->snr != snrString;
                
                iter->second->snrVal = snr;
                iter->second->snr = snrString;
                iter->second->lastRxDate = wxDateTime::Now();
            }
       
            auto lastUpdateTime = makeValidTime_(lastUpdate, iter->second->lastUpdateDate);
            iter->second->lastUpdate = lastUpdateTime;
        
            if (iter->second->isVisible)
            { 
                iter->second->isPendingUpdate = isDataChanged;
                sortOnNextTimerInterval |= isChanged;
            }
        }
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onMessageUpdateFn_(std::string sid, std::string lastUpdate, std::string message)
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    CallbackHandler handler;
    handler.sid = std::move(sid);
    handler.lastUpdate = std::move(lastUpdate);
    handler.message = std::move(message);
    handler.fn = [this](CallbackHandler& handler) {
        std::unique_lock<std::recursive_mutex> lk(const_cast<std::recursive_mutex&>(dataMtx_));
   
        std::string sid = std::move(handler.sid);
        std::string lastUpdate = std::move(handler.lastUpdate); 
        std::string message = std::move(handler.message);

        auto iter = allReporterData_.find(sid);
        if (iter != allReporterData_.end())
        {        
            if (iter->second->isPendingDelete)
            {
                log_warn("Got message change for user pending deletion (%s/%s, SID %s)", (const char*)iter->second->callsign.ToUTF8(), (const char*)iter->second->gridSquare.ToUTF8(), iter->second->sid.c_str());
                return;
            }

            auto sortingColumn = parent_->m_listSpots->GetSortingColumn();
            bool isChanged = false;
            if (message.size() == 0)
            {
                isChanged |= (sortingColumn == parent_->getColumnForModelColId_(USER_MESSAGE_COL) && iter->second->userMessage != parent_->UNKNOWN_STR);
                iter->second->userMessage = parent_->UNKNOWN_STR;
            }
            else
            {
                auto msgAsWxString = wxString::FromUTF8(message.c_str());
                isChanged |= (sortingColumn == parent_->getColumnForModelColId_(USER_MESSAGE_COL) && iter->second->userMessage != msgAsWxString);
                iter->second->userMessage = msgAsWxString;
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
                iter->second->isPendingUpdate = true;
                sortOnNextTimerInterval |= isChanged;
            }
        }
    };

    fnQueue_.push_back(std::move(handler));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onAboutToShowSelfFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);

    CallbackHandler handler;
    handler.fn = [this](CallbackHandler&) {
        filterSelfMessageUpdates_ = true;
    };

    fnQueue_.push_back(std::move(handler));
    parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
}

void FreeDVReporterDialog::FreeDVReporterDataModel::onRecvEndFn_()
{
    std::unique_lock<std::mutex> lk(fnQueueMtx_);
    if (fnQueue_.size() > 0)
    {
        parent_->CallAfter(std::bind(&FreeDVReporterDialog::FreeDVReporterDataModel::execQueuedAction_, this));
    }
}
