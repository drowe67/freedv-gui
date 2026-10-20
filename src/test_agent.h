//==========================================================================
// Name:            test_agent.h
//
// Purpose:         Lets a GUI test script drive FreeDV over a local socket.
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================
#ifndef TEST_AGENT_H
#define TEST_AGENT_H

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <wx/wx.h>
#include <wx/timer.h>

// On Windows, wxWidgets has already brought in wincrypt.h, some of whose
// names LibreSSL's headers (from TcpConnectionHandler.h) replace with their
// own. Nothing here uses wincrypt.h's, so that's fine.
#if defined(_WIN32) && !defined(LIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING)
#define LIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING
#endif // defined(_WIN32) && !defined(LIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING)
#include "util/TcpConnectionHandler.h"
#include "yyjson.h"

// Connects to a test script (see test/gui/freedvtest) listening on localhost
// and executes the commands it sends. Each command and each reply is a single
// line of JSON:
//
//     {"id": 1, "cmd": "click", "name": "main.btnOnOff"}
//     {"id": 1, "ok": true, "result": null}
//
// Widgets are addressed by the name given to them with wxWindow::SetName().
// Commands are executed on the UI thread. Those that perform an action reply
// before performing it, as the action may not return for some time (e.g. a
// button that opens a modal dialog); commands sent in the meantime are
// executed by that dialog's event loop.
class TestAgent : public TcpConnectionHandler
{
public:
    explicit TestAgent(int port);
    virtual ~TestAgent();

protected:
    virtual void onConnect_() override;
    virtual void onDisconnect_() override;
    virtual void onReceive_(char* buf, int length) override;

private:
    struct MessageBoxRecord
    {
        wxString message;
        wxString caption;
        int style;
    };

    // Only touched by the UI thread.
    static TestAgent* Instance_;

    std::atomic<bool> closing_;
    std::string lineBuffer_; // only touched by the connection's thread

    struct PopupItem
    {
        wxString label;
        bool enabled;
        bool checked;
    };

    // What became of the last attempt to display a popup menu.
    struct PopupRecord
    {
        bool shown = false;
        wxString window;
        std::vector<PopupItem> items;
        wxString chosen;
        wxString error;
    };

    std::mutex messageBoxMutex_;
    std::vector<MessageBoxRecord> messageBoxes_;
    std::deque<int> messageBoxAnswers_;

    // Only touched by the UI thread.
    bool popupChoiceArmed_;
    wxString popupChoice_;
    PopupRecord lastPopup_;

    // Titles of the file and folder choosers that would have been displayed,
    // and what to choose from the next ones (false to cancel).
    std::vector<wxString> pathDialogs_;
    struct PathDialogAnswer
    {
        bool choose;
        wxString path;
        std::map<wxString, bool> options;
    };
    std::deque<PathDialogAnswer> pathDialogAnswers_;

    // Addresses of the web pages that would have been opened.
    std::vector<wxString> browserUrls_;

    // For closing the main window (see commandQuit_()).
    wxTimer quitTimer_;
    int quitAttemptsLeft_;

    int onMessageBox_(const wxString& message, const wxString& caption, int style);
    bool onPathDialog_(const wxString& title, wxString& path);
    void onPopupMenu_(wxWindow* window, wxMenu* menu);
    void expectPopupMenu_(yyjson_val* request);
    void popupMenuExpected_();

    void handleLine_(std::string const& line);
    void reply_(yyjson_mut_doc* doc);
    void replyOk_(yyjson_val* id, yyjson_mut_doc* doc, yyjson_mut_val* result = nullptr);
    void replyError_(yyjson_val* id, wxString const& error);

    wxWindow* findWindow_(yyjson_val* id, yyjson_val* request);
    yyjson_mut_val* describeWindow_(yyjson_mut_doc* doc, wxWindow* window, bool includeItems);

    void commandListWindows_(yyjson_val* id);
    void commandListWidgets_(yyjson_val* id, yyjson_val* request);
    void commandGet_(yyjson_val* id, yyjson_val* request);
    void commandClick_(yyjson_val* id, yyjson_val* request);
    void commandSetValue_(yyjson_val* id, yyjson_val* request);
    void commandClose_(yyjson_val* id, yyjson_val* request);
    void commandActivate_(yyjson_val* id, yyjson_val* request);
    void commandMenu_(yyjson_val* id, yyjson_val* request);
    void commandContextMenu_(yyjson_val* id, yyjson_val* request);
    void commandGetPopupMenu_(yyjson_val* id);
    void commandComboPopup_(yyjson_val* id, yyjson_val* request);
    void commandDataView_(yyjson_val* id, yyjson_val* request);
    void commandGetState_(yyjson_val* id);
    void commandKey_(yyjson_val* id, yyjson_val* request);
    void commandPathDialogs_(yyjson_val* id, yyjson_val* request);
    void commandBrowserUrls_(yyjson_val* id, yyjson_val* request);
    void commandMouse_(yyjson_val* id, yyjson_val* request);
    void commandFocus_(yyjson_val* id, yyjson_val* request);
    void commandClipboard_(yyjson_val* id);
    void commandMessageBoxes_(yyjson_val* id, yyjson_val* request);
    void commandQuit_(yyjson_val* id);
};

#endif // TEST_AGENT_H
