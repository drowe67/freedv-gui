//==========================================================================
// Name:            test_agent.cpp
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

#include <cstdlib>

#include <wx/bookctrl.h>
#include <wx/tglbtn.h>

#include "test_agent.h"
#include "main.h"
#include "freedv_interface.h"

extern std::atomic<bool> g_tx;
extern std::atomic<bool> isModemRunning;
extern FreeDVInterface freedvInterface;

TestAgent* TestAgent::Instance_ = nullptr;

static std::string ToUtf8_(wxString const& str)
{
    return std::string(str.utf8_str());
}

static void AddString_(yyjson_mut_doc* doc, yyjson_mut_val* obj, const char* key, wxString const& str)
{
    yyjson_mut_obj_add_strcpy(doc, obj, key, ToUtf8_(str).c_str());
}

// Returns the string attached to the given item (e.g. the internal name of
// the audio device that the item displays), if it has one.
static bool GetItemData_(wxItemContainer* container, unsigned int index, wxString& data)
{
    if (!container->HasClientObjectData())
    {
        return false;
    }

    auto stringData = dynamic_cast<wxStringClientData*>(container->GetClientObject(index));
    if (stringData == nullptr)
    {
        return false;
    }

    data = stringData->GetData();
    return true;
}

TestAgent::TestAgent(int port)
    : closing_(false)
{
    Instance_ = this;

    g_wxMessageBoxTestHook = [this](const wxString& message, const wxString& caption, int style) {
        return onMessageBox_(message, caption, style);
    };

    log_info("Connecting to GUI test controller on port %d", port);
    connect("127.0.0.1", port, false);
}

TestAgent::~TestAgent()
{
    g_wxMessageBoxTestHook = nullptr;
    Instance_ = nullptr;

    // Must be done here rather than in the base class, which can no longer
    // call our handlers by the time its destructor runs.
    closing_.store(true, std::memory_order_release);
    auto fut = disconnect();
    fut.wait();
    waitForAllTasksComplete_();
}

void TestAgent::onConnect_()
{
    log_info("Connected to GUI test controller");
}

void TestAgent::onDisconnect_()
{
    if (!closing_.load(std::memory_order_acquire))
    {
        // The test script went away (or was never there). Nothing else is
        // going to close FreeDV, so don't linger.
        log_error("Lost connection to GUI test controller, exiting");
        std::_Exit(1);
    }
}

void TestAgent::onReceive_(char* buf, int length)
{
    lineBuffer_.append(buf, length);

    size_t pos = 0;
    while ((pos = lineBuffer_.find('\n')) != std::string::npos)
    {
        std::string line = lineBuffer_.substr(0, pos);
        lineBuffer_.erase(0, pos + 1);

        wxTheApp->CallAfter([line = std::move(line)]() {
            if (Instance_ != nullptr)
            {
                Instance_->handleLine_(line);
            }
        });
    }
}

int TestAgent::onMessageBox_(const wxString& message, const wxString& caption, int style)
{
    log_info("Message box suppressed for GUI test: %s", (const char*)message.ToUTF8());

    {
        std::unique_lock<std::mutex> lk(messageBoxMutex_);
        messageBoxes_.push_back({ message, caption, style });
    }

    // Answer as if the user had pressed the default button.
    if (style & wxYES_NO)
    {
        return (style & wxNO_DEFAULT) ? wxNO : wxYES;
    }
    return wxOK;
}

void TestAgent::reply_(yyjson_mut_doc* doc)
{
    size_t length = 0;
    char* json = yyjson_mut_write(doc, 0, &length);
    if (json != nullptr)
    {
        std::string line(json, length);
        line += '\n';
        send(line.c_str(), line.size());
        free(json);
    }
    yyjson_mut_doc_free(doc);
}

void TestAgent::replyOk_(yyjson_val* id, yyjson_mut_doc* doc, yyjson_mut_val* result)
{
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_val(doc, root, "id", (id != nullptr) ? yyjson_val_mut_copy(doc, id) : yyjson_mut_null(doc));
    yyjson_mut_obj_add_bool(doc, root, "ok", true);
    yyjson_mut_obj_add_val(doc, root, "result", (result != nullptr) ? result : yyjson_mut_null(doc));
    reply_(doc);
}

void TestAgent::replyError_(yyjson_val* id, wxString const& error)
{
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_val(doc, root, "id", (id != nullptr) ? yyjson_val_mut_copy(doc, id) : yyjson_mut_null(doc));
    yyjson_mut_obj_add_bool(doc, root, "ok", false);
    AddString_(doc, root, "error", error);
    reply_(doc);
}

void TestAgent::handleLine_(std::string const& line)
{
    yyjson_doc* requestDoc = yyjson_read(line.c_str(), line.size(), 0);
    yyjson_val* request = (requestDoc != nullptr) ? yyjson_doc_get_root(requestDoc) : nullptr;
    if (!yyjson_is_obj(request))
    {
        replyError_(nullptr, "request is not a JSON object");
        yyjson_doc_free(requestDoc);
        return;
    }

    yyjson_val* id = yyjson_obj_get(request, "id");
    const char* cmdStr = yyjson_get_str(yyjson_obj_get(request, "cmd"));
    std::string cmd = (cmdStr != nullptr) ? cmdStr : "";

    if (cmd == "ping")
    {
        replyOk_(id, yyjson_mut_doc_new(nullptr));
    }
    else if (cmd == "list_windows")
    {
        commandListWindows_(id);
    }
    else if (cmd == "list_widgets")
    {
        commandListWidgets_(id, request);
    }
    else if (cmd == "get")
    {
        commandGet_(id, request);
    }
    else if (cmd == "click")
    {
        commandClick_(id, request);
    }
    else if (cmd == "set_value")
    {
        commandSetValue_(id, request);
    }
    else if (cmd == "close")
    {
        commandClose_(id, request);
    }
    else if (cmd == "menu")
    {
        commandMenu_(id, request);
    }
    else if (cmd == "get_state")
    {
        commandGetState_(id);
    }
    else if (cmd == "message_boxes")
    {
        commandMessageBoxes_(id, request);
    }
    else if (cmd == "quit")
    {
        commandQuit_(id);
    }
    else
    {
        replyError_(id, wxString::Format("unknown command '%s'", wxString::FromUTF8(cmd.c_str())));
    }

    yyjson_doc_free(requestDoc);
}

// Looks up the window named by the request, replying with an error if there
// isn't one.
wxWindow* TestAgent::findWindow_(yyjson_val* id, yyjson_val* request)
{
    const char* name = yyjson_get_str(yyjson_obj_get(request, "name"));
    if (name == nullptr)
    {
        replyError_(id, "no window name given");
        return nullptr;
    }

    wxWindow* window = wxWindow::FindWindowByName(wxString::FromUTF8(name));
    if (window == nullptr)
    {
        replyError_(id, wxString::Format("no window named '%s'", wxString::FromUTF8(name)));
    }
    return window;
}

yyjson_mut_val* TestAgent::describeWindow_(yyjson_mut_doc* doc, wxWindow* window, bool includeItems)
{
    yyjson_mut_val* obj = yyjson_mut_obj(doc);
    AddString_(doc, obj, "name", window->GetName());
    AddString_(doc, obj, "class", window->GetClassInfo()->GetClassName());
    yyjson_mut_obj_add_bool(doc, obj, "enabled", window->IsEnabled());
    yyjson_mut_obj_add_bool(doc, obj, "shown", window->IsShownOnScreen());

    if (auto topLevel = wxDynamicCast(window, wxTopLevelWindow))
    {
        AddString_(doc, obj, "label", topLevel->GetTitle());

        auto dialog = wxDynamicCast(window, wxDialog);
        yyjson_mut_obj_add_bool(doc, obj, "modal", dialog != nullptr && dialog->IsModal());
        return obj;
    }

    AddString_(doc, obj, "label", window->GetLabel());

    wxItemContainer* container = nullptr;
    if (auto toggle = wxDynamicCast(window, wxToggleButton))
    {
        yyjson_mut_obj_add_bool(doc, obj, "value", toggle->GetValue());
    }
    else if (auto checkBox = wxDynamicCast(window, wxCheckBox))
    {
        yyjson_mut_obj_add_bool(doc, obj, "value", checkBox->GetValue());
    }
    else if (auto radioButton = wxDynamicCast(window, wxRadioButton))
    {
        yyjson_mut_obj_add_bool(doc, obj, "value", radioButton->GetValue());
    }
    else if (auto textCtrl = wxDynamicCast(window, wxTextCtrl))
    {
        AddString_(doc, obj, "value", textCtrl->GetValue());
    }
    else if (auto comboBox = wxDynamicCast(window, wxComboBox))
    {
        // Must be checked before wxChoice, which wxComboBox derives from
        // on some platforms.
        AddString_(doc, obj, "value", comboBox->GetValue());
        container = comboBox;
    }
    else if (auto choice = wxDynamicCast(window, wxChoice))
    {
        AddString_(doc, obj, "value", choice->GetStringSelection());
        container = choice;
    }
    else if (auto book = wxDynamicCast(window, wxBookCtrlBase))
    {
        yyjson_mut_obj_add_int(doc, obj, "value", book->GetSelection());
    }
    else if (auto gauge = wxDynamicCast(window, wxGauge))
    {
        yyjson_mut_obj_add_int(doc, obj, "value", gauge->GetValue());
    }
    else if (auto slider = wxDynamicCast(window, wxSlider))
    {
        yyjson_mut_obj_add_int(doc, obj, "value", slider->GetValue());
    }

    if (container != nullptr)
    {
        int selection = container->GetSelection();
        yyjson_mut_obj_add_int(doc, obj, "selection", selection);

        wxString data;
        if (selection != wxNOT_FOUND && GetItemData_(container, selection, data))
        {
            AddString_(doc, obj, "data", data);
        }

        if (includeItems)
        {
            yyjson_mut_val* items = yyjson_mut_arr(doc);
            for (unsigned int index = 0; index < container->GetCount(); index++)
            {
                yyjson_mut_val* item = yyjson_mut_obj(doc);
                AddString_(doc, item, "label", container->GetString(index));
                if (GetItemData_(container, index, data))
                {
                    AddString_(doc, item, "data", data);
                }
                yyjson_mut_arr_append(items, item);
            }
            yyjson_mut_obj_add_val(doc, obj, "items", items);
        }
    }

    return obj;
}

void TestAgent::commandListWindows_(yyjson_val* id)
{
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* windows = yyjson_mut_arr(doc);
    for (auto window : wxTopLevelWindows)
    {
        yyjson_mut_arr_append(windows, describeWindow_(doc, window, false));
    }
    replyOk_(id, doc, windows);
}

static void AppendDescendants_(std::vector<wxWindow*>& list, wxWindow* window)
{
    for (auto child : window->GetChildren())
    {
        list.push_back(child);
        AppendDescendants_(list, child);
    }
}

void TestAgent::commandListWidgets_(yyjson_val* id, yyjson_val* request)
{
    wxWindow* window = findWindow_(id, request);
    if (window == nullptr) return;

    std::vector<wxWindow*> descendants;
    AppendDescendants_(descendants, window);

    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* widgets = yyjson_mut_arr(doc);
    for (auto widget : descendants)
    {
        yyjson_mut_arr_append(widgets, describeWindow_(doc, widget, false));
    }
    replyOk_(id, doc, widgets);
}

void TestAgent::commandGet_(yyjson_val* id, yyjson_val* request)
{
    wxWindow* window = findWindow_(id, request);
    if (window == nullptr) return;

    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    replyOk_(id, doc, describeWindow_(doc, window, true));
}

// Returns the modal dialog that's keeping the user from interacting with the
// given window, if any.
static wxDialog* GetBlockingDialog_(wxWindow* window)
{
    auto ownDialog = wxDynamicCast(wxGetTopLevelParent(window), wxDialog);
    if (ownDialog != nullptr && ownDialog->IsModal())
    {
        return nullptr;
    }

    for (auto topLevel : wxTopLevelWindows)
    {
        auto dialog = wxDynamicCast(topLevel, wxDialog);
        if (dialog != nullptr && dialog->IsModal() && dialog->IsShown())
        {
            return dialog;
        }
    }
    return nullptr;
}

// Returns whether the user would currently be able to interact with the
// given window, providing the reason if not.
static bool CanInteract_(wxWindow* window, wxString& error)
{
    auto blockingDialog = GetBlockingDialog_(window);
    if (blockingDialog != nullptr)
    {
        error = wxString::Format(
            "'%s' cannot be used while '%s' is open", window->GetName(), blockingDialog->GetName());
        return false;
    }
    if (!window->IsShownOnScreen())
    {
        error = wxString::Format("'%s' is not visible", window->GetName());
        return false;
    }
    if (!window->IsEnabled())
    {
        error = wxString::Format("'%s' is disabled", window->GetName());
        return false;
    }
    return true;
}

static void SendCommandEvent_(wxWindow* window, wxEventType type, int intValue = 0, wxString const& stringValue = wxEmptyString)
{
    wxCommandEvent event(type, window->GetId());
    event.SetEventObject(window);
    event.SetInt(intValue);
    event.SetString(stringValue);
    window->ProcessWindowEvent(event);
}

// Does what a mouse click on the given window would do. Returns false if
// it's not something that can be clicked.
static bool Click_(wxWindow* window)
{
    if (auto toggle = wxDynamicCast(window, wxToggleButton))
    {
        toggle->SetValue(!toggle->GetValue());
        SendCommandEvent_(window, wxEVT_TOGGLEBUTTON, toggle->GetValue());
    }
    else if (auto checkBox = wxDynamicCast(window, wxCheckBox))
    {
        checkBox->SetValue(!checkBox->GetValue());
        SendCommandEvent_(window, wxEVT_CHECKBOX, checkBox->GetValue());
    }
    else if (auto radioButton = wxDynamicCast(window, wxRadioButton))
    {
        radioButton->SetValue(true);
        SendCommandEvent_(window, wxEVT_RADIOBUTTON, 1);
    }
    else if (wxDynamicCast(window, wxButton) != nullptr)
    {
        SendCommandEvent_(window, wxEVT_BUTTON);
    }
    else
    {
        return false;
    }
    return true;
}

static bool IsClickable_(wxWindow* window)
{
    return
        wxDynamicCast(window, wxToggleButton) != nullptr ||
        wxDynamicCast(window, wxCheckBox) != nullptr ||
        wxDynamicCast(window, wxRadioButton) != nullptr ||
        wxDynamicCast(window, wxButton) != nullptr;
}

void TestAgent::commandClick_(yyjson_val* id, yyjson_val* request)
{
    wxWindow* window = findWindow_(id, request);
    if (window == nullptr) return;

    wxString error;
    if (!CanInteract_(window, error))
    {
        replyError_(id, error);
        return;
    }
    if (!IsClickable_(window))
    {
        replyError_(id, wxString::Format("'%s' cannot be clicked", window->GetName()));
        return;
    }

    replyOk_(id, yyjson_mut_doc_new(nullptr));
    Click_(window);
}

void TestAgent::commandSetValue_(yyjson_val* id, yyjson_val* request)
{
    wxWindow* window = findWindow_(id, request);
    if (window == nullptr) return;

    wxString error;
    if (!CanInteract_(window, error))
    {
        replyError_(id, error);
        return;
    }

    yyjson_val* value = yyjson_obj_get(request, "value");
    yyjson_val* data = yyjson_obj_get(request, "data");
    yyjson_val* index = yyjson_obj_get(request, "index");

    bool isBoolControl =
        wxDynamicCast(window, wxToggleButton) != nullptr ||
        wxDynamicCast(window, wxCheckBox) != nullptr ||
        wxDynamicCast(window, wxRadioButton) != nullptr;
    auto comboBox = wxDynamicCast(window, wxComboBox);
    auto choice = wxDynamicCast(window, wxChoice);
    wxItemContainer* container = (comboBox != nullptr) ? (wxItemContainer*)comboBox : (wxItemContainer*)choice;

    if (isBoolControl)
    {
        if (!yyjson_is_bool(value))
        {
            replyError_(id, wxString::Format("'%s' needs a boolean value", window->GetName()));
            return;
        }

        bool current = false;
        if (auto toggle = wxDynamicCast(window, wxToggleButton)) current = toggle->GetValue();
        else if (auto checkBox = wxDynamicCast(window, wxCheckBox)) current = checkBox->GetValue();
        else if (auto radioButton = wxDynamicCast(window, wxRadioButton)) current = radioButton->GetValue();

        replyOk_(id, yyjson_mut_doc_new(nullptr));
        if (current != yyjson_get_bool(value))
        {
            Click_(window);
        }
    }
    else if (container != nullptr)
    {
        // Select an existing item by position, by its attached string or by
        // its label.
        int selection = wxNOT_FOUND;
        if (yyjson_is_int(index))
        {
            selection = yyjson_get_int(index);
            if (selection < 0 || selection >= (int)container->GetCount())
            {
                replyError_(id, wxString::Format("'%s' has no item %d", window->GetName(), selection));
                return;
            }
        }
        else if (yyjson_is_str(data))
        {
            wxString wanted = wxString::FromUTF8(yyjson_get_str(data));
            wxString itemData;
            for (unsigned int item = 0; item < container->GetCount(); item++)
            {
                if (GetItemData_(container, item, itemData) && itemData == wanted)
                {
                    selection = item;
                    break;
                }
            }
            if (selection == wxNOT_FOUND)
            {
                replyError_(id, wxString::Format("'%s' has no item with data '%s'", window->GetName(), wanted));
                return;
            }
        }
        else if (yyjson_is_str(value))
        {
            wxString wanted = wxString::FromUTF8(yyjson_get_str(value));
            selection = container->FindString(wanted, true);
            if (selection == wxNOT_FOUND)
            {
                if (comboBox == nullptr || !comboBox->IsEditable())
                {
                    replyError_(id, wxString::Format("'%s' has no item labeled '%s'", window->GetName(), wanted));
                    return;
                }

                // Not in the list, so type it in instead.
                replyOk_(id, yyjson_mut_doc_new(nullptr));
                comboBox->SetValue(wanted);
                return;
            }
        }
        else
        {
            replyError_(id, wxString::Format("'%s' needs a value, data or index", window->GetName()));
            return;
        }

        replyOk_(id, yyjson_mut_doc_new(nullptr));
        container->SetSelection(selection);
        SendCommandEvent_(
            window, (comboBox != nullptr) ? wxEVT_COMBOBOX : wxEVT_CHOICE,
            selection, container->GetString(selection));
    }
    else if (auto textCtrl = wxDynamicCast(window, wxTextCtrl))
    {
        if (!yyjson_is_str(value))
        {
            replyError_(id, wxString::Format("'%s' needs a string value", window->GetName()));
            return;
        }
        if (!textCtrl->IsEditable())
        {
            replyError_(id, wxString::Format("'%s' is read-only", window->GetName()));
            return;
        }

        // Generates the same event that typing does.
        replyOk_(id, yyjson_mut_doc_new(nullptr));
        textCtrl->SetValue(wxString::FromUTF8(yyjson_get_str(value)));
    }
    else
    {
        replyError_(id, wxString::Format("the value of '%s' cannot be set", window->GetName()));
    }
}

// Does what the close button in a window's title bar would do.
void TestAgent::commandClose_(yyjson_val* id, yyjson_val* request)
{
    wxWindow* window = findWindow_(id, request);
    if (window == nullptr) return;

    wxString error;
    if (wxDynamicCast(window, wxTopLevelWindow) == nullptr)
    {
        replyError_(id, wxString::Format("'%s' is not a top-level window", window->GetName()));
        return;
    }
    if (!CanInteract_(window, error))
    {
        replyError_(id, error);
        return;
    }

    replyOk_(id, yyjson_mut_doc_new(nullptr));
    window->Close();
}

void TestAgent::commandMenu_(yyjson_val* id, yyjson_val* request)
{
    // Menu items are given as "<menu>/<item>" using their labels without
    // mnemonics or shortcuts, e.g. "Settings/Setup Wizard...".
    const char* pathStr = yyjson_get_str(yyjson_obj_get(request, "path"));
    wxString path = (pathStr != nullptr) ? wxString::FromUTF8(pathStr) : wxString();
    wxString itemLabel;
    wxString menuLabel = path.BeforeFirst('/', &itemLabel);

    auto frame = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame);
    wxMenuBar* menuBar = (frame != nullptr) ? frame->GetMenuBar() : nullptr;
    if (menuBar == nullptr)
    {
        replyError_(id, "the main window has no menu bar");
        return;
    }

    int itemId = menuBar->FindMenuItem(menuLabel, itemLabel);
    wxMenu* menu = nullptr;
    wxMenuItem* item = (itemId != wxNOT_FOUND) ? menuBar->FindItem(itemId, &menu) : nullptr;
    if (item == nullptr || menu == nullptr)
    {
        replyError_(id, wxString::Format("no menu item '%s'", path));
        return;
    }

    wxString error;
    if (!CanInteract_(frame, error))
    {
        replyError_(id, error);
        return;
    }

    // Items are enabled and disabled just before their menu is displayed.
    menu->UpdateUI(frame);
    if (!item->IsEnabled())
    {
        replyError_(id, wxString::Format("menu item '%s' is disabled", path));
        return;
    }

    replyOk_(id, yyjson_mut_doc_new(nullptr));

    wxCommandEvent event(wxEVT_MENU, itemId);
    event.SetEventObject(menu);
    if (item->IsCheckable())
    {
        item->Check(!item->IsChecked());
        event.SetInt(item->IsChecked());
    }
    frame->ProcessWindowEvent(event);
}

template<typename SoundDeviceType>
static yyjson_mut_val* DescribeSoundDevice_(yyjson_mut_doc* doc, SoundDeviceType& device)
{
    yyjson_mut_val* obj = yyjson_mut_obj(doc);
    AddString_(doc, obj, "deviceName", device.deviceName.get());
    yyjson_mut_obj_add_int(doc, obj, "sampleRate", device.sampleRate.get());
    return obj;
}

void TestAgent::commandGetState_(yyjson_val* id)
{
    auto& app = wxGetApp();
    auto& config = app.appConfiguration;

    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* state = yyjson_mut_obj(doc);

    bool running = app.frame->m_RxRunning;
    yyjson_mut_obj_add_bool(doc, state, "running", running);
    yyjson_mut_obj_add_bool(doc, state, "modemRunning", isModemRunning.load(std::memory_order_acquire));
    yyjson_mut_obj_add_bool(doc, state, "transmitting", g_tx.load(std::memory_order_acquire));
    yyjson_mut_obj_add_int(doc, state, "numSoundCards", g_nSoundCards);
    yyjson_mut_obj_add_int(doc, state, "sync", running ? freedvInterface.getSync() : 0);

    yyjson_mut_val* audio = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, audio, "soundCard1In", DescribeSoundDevice_(doc, config.audioConfiguration.soundCard1In));
    yyjson_mut_obj_add_val(doc, audio, "soundCard1Out", DescribeSoundDevice_(doc, config.audioConfiguration.soundCard1Out));
    yyjson_mut_obj_add_val(doc, audio, "soundCard2In", DescribeSoundDevice_(doc, config.audioConfiguration.soundCard2In));
    yyjson_mut_obj_add_val(doc, audio, "soundCard2Out", DescribeSoundDevice_(doc, config.audioConfiguration.soundCard2Out));

    auto& rigConfig = config.rigControlConfiguration;
    yyjson_mut_val* rigControl = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_bool(doc, rigControl, "hamlibUseForPTT", rigConfig.hamlibUseForPTT.get());
    AddString_(doc, rigControl, "hamlibRigName", rigConfig.hamlibRigName.get());
    AddString_(doc, rigControl, "hamlibSerialPort", rigConfig.hamlibSerialPort.get());
    yyjson_mut_obj_add_uint(doc, rigControl, "hamlibSerialRate", rigConfig.hamlibSerialRate.get());
    yyjson_mut_obj_add_uint(doc, rigControl, "hamlibPTTType", rigConfig.hamlibPTTType.get());
    AddString_(doc, rigControl, "hamlibPttSerialPort", rigConfig.hamlibPttSerialPort.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "hamlibUseAnalogModes", rigConfig.hamlibUseAnalogModes.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "useSerialPTT", rigConfig.useSerialPTT.get());
    AddString_(doc, rigControl, "serialPTTPort", rigConfig.serialPTTPort.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "serialPTTUseRTS", rigConfig.serialPTTUseRTS.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "serialPTTPolarityRTS", rigConfig.serialPTTPolarityRTS.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "serialPTTUseDTR", rigConfig.serialPTTUseDTR.get());
    yyjson_mut_obj_add_bool(doc, rigControl, "serialPTTPolarityDTR", rigConfig.serialPTTPolarityDTR.get());

    auto& reportingConfig = config.reportingConfiguration;
    yyjson_mut_val* reporting = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_bool(doc, reporting, "reportingEnabled", reportingConfig.reportingEnabled.get());
    AddString_(doc, reporting, "reportingCallsign", reportingConfig.reportingCallsign.get());
    AddString_(doc, reporting, "reportingGridSquare", reportingConfig.reportingGridSquare.get());
    yyjson_mut_obj_add_bool(doc, reporting, "freedvReporterEnabled", reportingConfig.freedvReporterEnabled.get());
    yyjson_mut_obj_add_bool(doc, reporting, "pskReporterEnabled", reportingConfig.pskReporterEnabled.get());

    yyjson_mut_val* configObj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, configObj, "audio", audio);
    yyjson_mut_obj_add_val(doc, configObj, "rigControl", rigControl);
    yyjson_mut_obj_add_val(doc, configObj, "reporting", reporting);
    yyjson_mut_obj_add_val(doc, state, "config", configObj);

    replyOk_(id, doc, state);
}

void TestAgent::commandMessageBoxes_(yyjson_val* id, yyjson_val* request)
{
    std::vector<MessageBoxRecord> records;
    {
        std::unique_lock<std::mutex> lk(messageBoxMutex_);
        records = messageBoxes_;
        if (yyjson_get_bool(yyjson_obj_get(request, "clear")))
        {
            messageBoxes_.clear();
        }
    }

    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* list = yyjson_mut_arr(doc);
    for (auto& record : records)
    {
        yyjson_mut_val* obj = yyjson_mut_obj(doc);
        AddString_(doc, obj, "message", record.message);
        AddString_(doc, obj, "caption", record.caption);
        yyjson_mut_obj_add_int(doc, obj, "style", record.style);
        yyjson_mut_arr_append(list, obj);
    }
    replyOk_(id, doc, list);
}

void TestAgent::commandQuit_(yyjson_val* id)
{
    replyOk_(id, yyjson_mut_doc_new(nullptr));

    wxWindow* topWindow = wxTheApp->GetTopWindow();
    if (topWindow != nullptr)
    {
        topWindow->Close();
    }
}
