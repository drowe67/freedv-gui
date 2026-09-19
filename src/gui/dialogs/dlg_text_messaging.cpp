//=========================================================================
// Name:            dlg_text_messaging.cpp
// Purpose:         Chat window for FreeDV text messaging.
//
// Authors:         FreeDV text messaging contributors
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#include "dlg_text_messaging.h"

#include <algorithm>

#include <wx/datetime.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

#include "main.h"
#include "text_messaging/HeardStationList.h"
#include "text_messaging/MessageStore.h"
#include "text_messaging/TextMessagingSession.h"

using namespace TextMessaging;

namespace
{

// How often the station list ages out and the "last heard" column is redrawn.
constexpr int REFRESH_INTERVAL_MS = 1000;

enum
{
    ID_STATION_LIST = wxID_HIGHEST + 700,
    ID_PING,
    ID_SEND,
    ID_BROADCAST,
    ID_AUTO_REPLY,
    ID_ENTRY,
    ID_REFRESH_TIMER,
};

wxString escapeHtml(const std::string& text)
{
    wxString result;
    result.reserve(text.size() + 16);

    for (char c : text)
    {
        switch (c)
        {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\n': result += "<br>"; break;
            case '\r': break;
            default: result += (wxChar)(unsigned char)c; break;
        }
    }

    return result;
}

wxString formatTime(std::time_t when)
{
    return wxDateTime((time_t)when).Format("%H:%M:%S");
}

wxString formatSnr(float snr)
{
    return wxString::Format("%.1f dB", (double)snr);
}

// Relative time reads better than a clock for "is this station still here".
wxString formatAge(std::time_t lastHeard, std::time_t now)
{
    std::time_t age = now - lastHeard;
    if (age < 60) return _("just now");
    if (age < 3600) return wxString::Format(_("%d min ago"), (int)(age / 60));

    return wxString::Format(_("%d hr ago"), (int)(age / 3600));
}

// The colored chip on the right of a sent message.
wxString statusChip(const TextMessage& message)
{
    wxString label;
    wxString background;
    wxString foreground = "#FFFFFF";

    switch (message.status)
    {
        case MessageStatus::Queued:
            label = _("QUEUED");
            background = "#7F8C8D";
            break;
        case MessageStatus::Transmitting:
            label = _("SENDING");
            background = "#2980B9";
            break;
        case MessageStatus::AwaitingAck:
            label = _("SENT");
            background = "#7F8C8D";
            break;
        case MessageStatus::Retrying:
            label = wxString::Format(_("RETRY %d"), message.retryCount);
            background = "#F1C40F";
            foreground = "#000000";
            break;
        case MessageStatus::Acknowledged:
            label = _("OK");
            background = "#27AE60";
            break;
        case MessageStatus::Failed:
            label = _("FAILED");
            background = "#E74C3C";
            break;
        case MessageStatus::Sent:
            label = _("SENT");
            background = "#7F8C8D";
            break;
        case MessageStatus::Received:
            return "";
    }

    return "<table cellpadding=\"2\" cellspacing=\"0\" bgcolor=\"" + background +
           "\"><tr><td><font size=\"-2\" color=\"" + foreground + "\">" + label +
           "</font></td></tr></table>";
}

} // namespace

TextMessagingDialog::TextMessagingDialog(wxWindow* parent, wxWindowID id, const wxString& title,
                                         const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
    , m_stationList(nullptr)
    , m_btnPing(nullptr)
    , m_chatWindow(nullptr)
    , m_txtEntry(nullptr)
    , m_btnSend(nullptr)
    , m_btnBroadcast(nullptr)
    , m_chkAutoReply(nullptr)
    , m_txtStatus(nullptr)
    , m_refreshTimer(this, ID_REFRESH_TIMER)
{
    buildControls();

    Connect(ID_SEND, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(TextMessagingDialog::OnSend));
    Connect(ID_BROADCAST, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(TextMessagingDialog::OnBroadcast));
    Connect(ID_PING, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(TextMessagingDialog::OnPing));
    Connect(ID_AUTO_REPLY, wxEVT_COMMAND_CHECKBOX_CLICKED,
            wxCommandEventHandler(TextMessagingDialog::OnAutoReplyToggled));
    Connect(ID_STATION_LIST, wxEVT_COMMAND_LIST_ITEM_SELECTED,
            wxListEventHandler(TextMessagingDialog::OnStationSelected));
    Connect(ID_STATION_LIST, wxEVT_COMMAND_LIST_ITEM_DESELECTED,
            wxListEventHandler(TextMessagingDialog::OnStationDeselected));
    Connect(ID_REFRESH_TIMER, wxEVT_TIMER, wxTimerEventHandler(TextMessagingDialog::OnTimer));
    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TextMessagingDialog::OnClose));

    m_txtEntry->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(TextMessagingDialog::OnEntryKeyDown),
                        nullptr, this);

    TextMessagingSession::instance().protocol().setObserver(this);
    m_refreshTimer.Start(REFRESH_INTERVAL_MS);
}

TextMessagingDialog::~TextMessagingDialog()
{
    m_refreshTimer.Stop();
    TextMessagingSession::instance().protocol().setObserver(nullptr);

    m_txtEntry->Disconnect(wxEVT_KEY_DOWN, wxKeyEventHandler(TextMessagingDialog::OnEntryKeyDown),
                           nullptr, this);
}

TextMessagingDialog::Palette TextMessagingDialog::palette() const
{
    wxColour windowColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    bool dark = (windowColour.Red() + windowColour.Green() + windowColour.Blue()) / 3 < 128;

    Palette palette;
    if (dark)
    {
        palette.page = "#1E1E1E";
        palette.text = "#ECECEC";
        palette.sentBubble = "#1F4E66";
        palette.receivedBubble = "#333333";
        palette.subdued = "#9E9E9E";
    }
    else
    {
        palette.page = "#FFFFFF";
        palette.text = "#000000";
        palette.sentBubble = "#D6EAF8";
        palette.receivedBubble = "#EDEDED";
        palette.subdued = "#666666";
    }

    return palette;
}

void TextMessagingDialog::buildControls()
{
    Palette colors = palette();

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Heard stations on the left, conversation on the right.
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticBoxSizer* stationSizer =
        new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Heard Stations")), wxVERTICAL);

    m_stationList = new wxListCtrl(this, ID_STATION_LIST, wxDefaultPosition, wxSize(260, -1),
                                   wxLC_REPORT | wxLC_SINGLE_SEL);
    m_stationList->InsertColumn(0, _("Callsign"), wxLIST_FORMAT_LEFT, 100);
    m_stationList->InsertColumn(1, _("SNR"), wxLIST_FORMAT_RIGHT, 70);
    m_stationList->InsertColumn(2, _("Heard"), wxLIST_FORMAT_LEFT, 90);
    stationSizer->Add(m_stationList, 1, wxEXPAND | wxALL, 2);

    m_btnPing = new wxButton(this, ID_PING, _("Ping"));
    m_btnPing->SetToolTip(_("Ask the selected station to answer, to see whether you are being heard."));
    m_btnPing->Enable(false);
    stationSizer->Add(m_btnPing, 0, wxEXPAND | wxALL, 2);

    topSizer->Add(stationSizer, 0, wxEXPAND | wxALL, 4);

    wxStaticBoxSizer* chatSizer =
        new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Chat")), wxVERTICAL);
    m_chatWindow = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxHW_SCROLLBAR_AUTO | wxBORDER_SUNKEN);
    chatSizer->Add(m_chatWindow, 1, wxEXPAND | wxALL, 2);
    topSizer->Add(chatSizer, 1, wxEXPAND | wxALL, 4);

    mainSizer->Add(topSizer, 1, wxEXPAND);

    // Entry box with a send button beside it.
    wxBoxSizer* entrySizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtEntry = new wxTextCtrl(this, ID_ENTRY, wxEmptyString, wxDefaultPosition, wxSize(-1, 70),
                                wxTE_MULTILINE);
    m_txtEntry->SetBackgroundColour(wxColour(colors.sentBubble));
    m_txtEntry->SetToolTip(_("Enter sends the message; Shift+Enter starts a new line."));
    entrySizer->Add(m_txtEntry, 1, wxEXPAND | wxALL, 4);

    m_btnSend = new wxButton(this, ID_SEND, _("Send"), wxDefaultPosition, wxSize(140, 70));
    entrySizer->Add(m_btnSend, 0, wxEXPAND | wxALL, 4);
    mainSizer->Add(entrySizer, 0, wxEXPAND);

    m_btnBroadcast = new wxButton(this, ID_BROADCAST, _("Send as Broadcast"), wxDefaultPosition,
                                  wxSize(-1, 40));
    m_btnBroadcast->SetToolTip(
        _("Send to everybody listening. Nothing is expected back, so there is no delivery check."));
    mainSizer->Add(m_btnBroadcast, 0, wxEXPAND | wxALL, 4);

    wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    m_chkAutoReply = new wxCheckBox(this, ID_AUTO_REPLY,
                                    _("Automatically acknowledge messages and answer pings"));
    m_chkAutoReply->SetValue(TextMessagingSession::instance().protocol().autoReplyEnabled());
    m_chkAutoReply->SetToolTip(
        _("When checked, this station transmits on its own to confirm messages and answer pings."));
    bottomSizer->Add(m_chkAutoReply, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);

    m_txtStatus = new wxStaticText(this, wxID_ANY, wxEmptyString);
    bottomSizer->Add(m_txtStatus, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    mainSizer->Add(bottomSizer, 0, wxEXPAND);

    SetSizer(mainSizer);
    Layout();
}

void TextMessagingDialog::refreshFromSession()
{
    auto& session = TextMessagingSession::instance();

    // The chat callsign follows the reporting callsign, which is the one the
    // operator has already told FreeDV about.
    session.protocol().setMyCallsign(
        wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToStdString());

    m_messages = session.store().recentMessages(TextMessagingSession::MESSAGES_TO_RESTORE);
    m_chkAutoReply->SetValue(session.protocol().autoReplyEnabled());

    if (session.protocol().myCallsign().empty())
    {
        setStatus(_("Set your callsign in Tools/Options before sending anything."));
    }
    else
    {
        setStatus(wxEmptyString);
    }

    renderChat();
    refreshStations();
}

void TextMessagingDialog::setStatus(const wxString& status)
{
    m_txtStatus->SetLabel(status);
}

void TextMessagingDialog::appendMessage(const TextMessage& message)
{
    m_messages.push_back(message);

    int excess = (int)m_messages.size() - TextMessagingSession::MESSAGES_TO_RESTORE;
    if (excess > 0) m_messages.erase(m_messages.begin(), m_messages.begin() + excess);
}

void TextMessagingDialog::renderChat()
{
    Palette colors = palette();

    wxString html;
    html.reserve(4096);
    html += "<html><body bgcolor=\"" + colors.page + "\" text=\"" + colors.text + "\">";

    for (const TextMessage& message : m_messages)
    {
        if (message.kind == MessageKind::System)
        {
            html += "<table width=\"100%\"><tr><td align=\"center\"><font size=\"-2\" color=\"" +
                    colors.subdued + "\">" + escapeHtml(message.text) + " &middot; " +
                    formatTime(message.timestamp) + "</font></td></tr></table>";
            continue;
        }

        bool sent = message.direction == MessageDirection::Sent;
        wxString align = sent ? "right" : "left";
        wxString bubble = sent ? colors.sentBubble : colors.receivedBubble;

        wxString body;
        if (!sent) body += "<b>" + escapeHtml(message.originCallsign) + ":</b> ";
        body += escapeHtml(message.text);

        wxString tag;
        if (message.broadcast)
        {
            tag = " <font size=\"-2\" color=\"" + colors.subdued + "\">[BCAST]</font>";
        }

        html += "<table width=\"100%\" cellpadding=\"2\" cellspacing=\"0\"><tr><td align=\"" +
                align + "\">";
        html += "<table cellpadding=\"6\" cellspacing=\"0\" bgcolor=\"" + bubble +
                "\"><tr><td><font color=\"" + colors.text + "\">" + body + tag +
                "</font></td></tr></table>";
        html += "</td></tr>";

        // Timestamp on the left, delivery status on the right.
        wxString right;
        if (sent)
        {
            right = statusChip(message);
        }
        else if (message.snr != 0.0f)
        {
            right = "<font size=\"-2\" color=\"" + colors.subdued + "\">" +
                    formatSnr(message.snr) + "</font>";
        }

        html += "<tr><td><table width=\"100%\"><tr><td align=\"left\"><font size=\"-2\" color=\"" +
                colors.subdued + "\">" + formatTime(message.timestamp) +
                "</font></td><td align=\"right\">" + right + "</td></tr></table></td></tr>";
        html += "</table>";
    }

    html += "</body></html>";

    m_chatWindow->SetPage(html);

    // Keep the newest message in view, the way a chat window should.
    m_chatWindow->Scroll(0, m_chatWindow->GetScrollRange(wxVERTICAL));
}

void TextMessagingDialog::refreshStations()
{
    std::string previousSelection = selectedCallsign();

    auto& session = TextMessagingSession::instance();
    std::time_t now = std::time(nullptr);
    session.stations().prune(now);

    std::vector<HeardStation> stations = session.stations().stations();

    m_stationList->DeleteAllItems();
    long selectedIndex = -1;

    for (size_t index = 0; index < stations.size(); index++)
    {
        const HeardStation& station = stations[index];
        long item = m_stationList->InsertItem((long)index, wxString::FromUTF8(station.callsign));
        m_stationList->SetItem(item, 1, formatSnr(station.snr));
        m_stationList->SetItem(item, 2, formatAge(station.lastHeard, now));

        if (station.callsign == previousSelection) selectedIndex = item;
    }

    if (selectedIndex >= 0)
    {
        m_stationList->SetItemState(selectedIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    m_btnPing->Enable(selectedIndex >= 0);
}

std::string TextMessagingDialog::selectedCallsign() const
{
    long item = m_stationList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0) return "";

    return m_stationList->GetItemText(item).ToStdString();
}

void TextMessagingDialog::send(const std::string& destination)
{
    std::string text = m_txtEntry->GetValue().ToStdString();
    std::string error;

    if (TextMessagingSession::instance().protocol().sendMessage(text, destination, error))
    {
        m_txtEntry->Clear();
        setStatus(destination.empty()
                      ? _("Broadcast queued.")
                      : wxString::Format(_("Message to %s queued."),
                                         wxString::FromUTF8(destination)));
    }
    else
    {
        setStatus(wxString::FromUTF8(error));
    }
}

void TextMessagingDialog::OnSend(wxCommandEvent&)
{
    send(selectedCallsign());
}

void TextMessagingDialog::OnBroadcast(wxCommandEvent&)
{
    // A broadcast goes to nobody in particular, so the highlighted station is
    // cleared to make that obvious.
    long item = m_stationList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item >= 0) m_stationList->SetItemState(item, 0, wxLIST_STATE_SELECTED);
    m_btnPing->Enable(false);

    send("");
}

void TextMessagingDialog::OnPing(wxCommandEvent&)
{
    std::string destination = selectedCallsign();
    if (destination.empty()) return;

    std::string error;
    if (!TextMessagingSession::instance().protocol().sendPing(destination, error))
    {
        setStatus(wxString::FromUTF8(error));
    }
    else
    {
        setStatus(wxString::Format(_("Ping to %s queued."), wxString::FromUTF8(destination)));
    }
}

void TextMessagingDialog::OnStationSelected(wxListEvent& event)
{
    m_btnPing->Enable(true);
    event.Skip();
}

void TextMessagingDialog::OnStationDeselected(wxListEvent& event)
{
    m_btnPing->Enable(false);
    event.Skip();
}

void TextMessagingDialog::OnAutoReplyToggled(wxCommandEvent& event)
{
    TextMessagingSession::instance().protocol().setAutoReplyEnabled(m_chkAutoReply->GetValue());

    if (!m_chkAutoReply->GetValue())
    {
        setStatus(_("This station will no longer transmit on its own."));
    }
    else
    {
        setStatus(wxEmptyString);
    }

    event.Skip();
}

void TextMessagingDialog::OnEntryKeyDown(wxKeyEvent& event)
{
    bool isEnter = event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER;
    if (isEnter && !event.ShiftDown())
    {
        send(selectedCallsign());
        return;
    }

    event.Skip();
}

void TextMessagingDialog::OnTimer(wxTimerEvent&)
{
    refreshStations();
}

void TextMessagingDialog::OnClose(wxCloseEvent&)
{
    // Chat keeps running with the window closed, so this only hides it.
    Hide();
}

void TextMessagingDialog::onMessageAdded(const TextMessage& message)
{
    TextMessage copy = message;
    CallAfter([this, copy]()
    {
        appendMessage(copy);
        renderChat();
    });
}

void TextMessagingDialog::onMessageUpdated(const TextMessage& message)
{
    TextMessage copy = message;
    CallAfter([this, copy]()
    {
        for (TextMessage& existing : m_messages)
        {
            if (existing.id != copy.id) continue;

            existing = copy;
            renderChat();
            return;
        }
    });
}

void TextMessagingDialog::onStationsChanged()
{
    CallAfter([this]() { refreshStations(); });
}
