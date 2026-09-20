//=========================================================================
// Name:            dlg_text_messaging.h
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

#ifndef __FDV_TEXT_MESSAGING_DIALOG__
#define __FDV_TEXT_MESSAGING_DIALOG__

#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/html/htmlwin.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

#include "text_messaging/TextMessagingTypes.h"
#include "text_messaging/TextMessagingProtocol.h"

// The chat window: who has been heard, what has been said, and a place to say
// something back. All protocol work happens in the session, which keeps
// running when this window is closed; the dialog only observes it.
class TextMessagingDialog : public wxDialog, public TextMessaging::ITextMessagingObserver
{
public:
    TextMessagingDialog(wxWindow* parent, wxWindowID id = wxID_ANY,
                        const wxString& title = _("FreeDV Text Chat"),
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxSize(900, 620),
                        long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~TextMessagingDialog();

    // Reloads history and the operator's callsign; called before showing.
    void refreshFromSession();

    // ITextMessagingObserver. These arrive on the session's thread and hand
    // the work to the GUI thread.
    virtual void onMessageAdded(const TextMessaging::TextMessage& message) override;
    virtual void onMessageUpdated(const TextMessaging::TextMessage& message) override;
    virtual void onStationsChanged() override;

private:
    // Colors that work on both a light and a dark desktop.
    struct Palette
    {
        wxString page;
        wxString text;
        wxString sentBubble;
        wxString receivedBubble;
        wxString subdued;
    };

    void buildControls();
    Palette palette() const;
    void renderChat();
    void refreshStations();
    void setStatus(const wxString& status);
    std::string selectedCallsign() const;
    void send(const std::string& destination);
    void appendMessage(const TextMessaging::TextMessage& message);
    void updateTransmitControls();

    void OnSend(wxCommandEvent& event);
    void OnBroadcast(wxCommandEvent& event);
    void OnPing(wxCommandEvent& event);
    void OnStationSelected(wxListEvent& event);
    void OnStationDeselected(wxListEvent& event);
    void OnAutoReplyToggled(wxCommandEvent& event);
    void OnEntryKeyDown(wxKeyEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnClose(wxCloseEvent& event);

    wxListCtrl* m_stationList;
    wxButton* m_btnPing;
    wxHtmlWindow* m_chatWindow;
    wxTextCtrl* m_txtEntry;
    wxButton* m_btnSend;
    wxButton* m_btnBroadcast;
    wxCheckBox* m_chkAutoReply;
    wxStaticText* m_txtStatus;
    wxTimer m_refreshTimer;

    // Remembered so the one second timer only touches the buttons when the
    // transmitter's state actually changes, rather than on every tick.
    bool m_transmitControlsDisabled;

    std::vector<TextMessaging::TextMessage> m_messages;
};

#endif // __FDV_TEXT_MESSAGING_DIALOG__
