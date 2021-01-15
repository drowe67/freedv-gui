/*
  ongui.cpp

  The simpler GUI event handlers.
*/

#include "main.h"

extern int g_mode;
extern bool  g_modal;

//-------------------------------------------------------------------------
// OnExitClick()
//-------------------------------------------------------------------------
void MainFrame::OnExitClick(wxCommandEvent& event)
{
    OnExit(event);
}

//-------------------------------------------------------------------------
// OnToolsAudio()
//-------------------------------------------------------------------------
void MainFrame::OnToolsAudio(wxCommandEvent& event)
{
    wxUnusedVar(event);
    int rv = 0;
    AudioOptsDialog *dlg = new AudioOptsDialog(NULL);
    rv = dlg->ShowModal();
    if(rv == wxID_OK)
    {
        dlg->ExchangeData(EXCHANGE_DATA_OUT);
    }
    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsAudioUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsAudioUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnToolsFilter()
//-------------------------------------------------------------------------
void MainFrame::OnToolsFilter(wxCommandEvent& event)
{
    wxUnusedVar(event);
    FilterDlg *dlg = new FilterDlg(NULL, m_RxRunning, &m_newMicInFilter, &m_newSpkOutFilter);
    dlg->ShowModal();
    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsOptions()
//-------------------------------------------------------------------------
void MainFrame::OnToolsOptions(wxCommandEvent& event)
{
    wxUnusedVar(event);
    g_modal = true;
    //fprintf(stderr,"g_modal: %d\n", g_modal);
    optionsDlg->Show();
}

//-------------------------------------------------------------------------
// OnToolsOptionsUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsOptionsUI(wxUpdateUIEvent& event)
{
}

//-------------------------------------------------------------------------
// OnToolsComCfg()
//-------------------------------------------------------------------------
void MainFrame::OnToolsComCfg(wxCommandEvent& event)
{
    wxUnusedVar(event);

    ComPortsDlg *dlg = new ComPortsDlg(NULL);

    dlg->ShowModal();

    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsComCfgUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsComCfgUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnHelpCheckUpdates()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdates(wxCommandEvent& event)
{
    wxMessageBox("Got Click!", "OnHelpCheckUpdates", wxOK);
    event.Skip();
}

//-------------------------------------------------------------------------
// OnHelpCheckUpdatesUI()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdatesUI(wxUpdateUIEvent& event)
{
    event.Enable(false);
}

//-------------------------------------------------------------------------
//OnHelpAbout()
//-------------------------------------------------------------------------
void MainFrame::OnHelpAbout(wxCommandEvent& event)
{
    wxUnusedVar(event);
    wxString msg;
    msg.Printf( wxT("FreeDV GUI %s\n\n")
                wxT("For Help and Support visit: http://freedv.org\n\n")

                wxT("GNU Public License V2.1\n\n")
                wxT("Created by David Witten KD0EAG and David Rowe VK5DGR in 2012.  ")
                wxT("Currently (2020) maintaned by Mooneer Salem K6AQ and David Rowe VK5DGR.\n\n")
                wxT("freedv-gui version: %s\n")
                wxT("freedv-gui git hash: %s\n")
                wxT("codec2 git hash: %s\n")
                wxT("lpcnet git hash: %s\n"),
                FREEDV_VERSION, FREEDV_VERSION, GIT_HASH, freedv_get_hash(), lpcnet_get_hash());

    wxMessageBox(msg, wxT("About"), wxOK | wxICON_INFORMATION, this);
}


// Attempt to talk to rig using Hamlib

bool MainFrame::OpenHamlibRig() {
    if (wxGetApp().m_boolHamlibUseForPTT != true)
       return false;
    if (wxGetApp().m_intHamlibRig == 0)
        return false;
    if (wxGetApp().m_hamlib == NULL)
        return false;

    int rig = wxGetApp().m_intHamlibRig;
    wxString port = wxGetApp().m_strHamlibSerialPort;
    int serial_rate = wxGetApp().m_intHamlibSerialRate;
    bool status = wxGetApp().m_hamlib->connect(rig, port.mb_str(wxConvUTF8), serial_rate, wxGetApp().m_intHamlibIcomCIVHex);
    if (status == false)
    {
        if (wxGetApp().m_psk_enable)
        {
            wxMessageBox("Couldn't connect to Radio with hamlib. PSK Reporter reporting will be disabled.", wxT("Error"), wxOK | wxICON_ERROR, this);
        }
        else
        {
            wxMessageBox("Couldn't connect to Radio with hamlib", wxT("Error"), wxOK | wxICON_ERROR, this);
        }
    }
    else
    {
        wxGetApp().m_hamlib->enable_mode_detection(m_txtModeStatus, g_mode == FREEDV_MODE_2400B);
    }

    // Initialize PSK Reporter reporting.
    if (status && wxGetApp().m_psk_enable)
    {
        std::string currentMode = "";
        switch (g_mode)
        {
            case FREEDV_MODE_1600:
                currentMode = "1600";
                break;
            case FREEDV_MODE_700C:
                currentMode = "700C";
                break;
            case FREEDV_MODE_700D:
                currentMode = "700D";
                break;
            case FREEDV_MODE_800XA:
                currentMode = "800XA";
                break;
            case FREEDV_MODE_2400B:
                currentMode = "2400B";
                break;
            case FREEDV_MODE_2020:
                currentMode = "2020";
                break;
            case FREEDV_MODE_700E:
                currentMode = "700E";
                break;
            default:
                currentMode = "unknown";
                break;
        }
        
        if (wxGetApp().m_psk_callsign.ToStdString() == "" || wxGetApp().m_psk_grid_square.ToStdString() == "")
        {
            wxMessageBox("PSK Reporter reporting requires a valid callsign and grid square in Tools->Options. Reporting will be disabled.", wxT("Error"), wxOK | wxICON_ERROR, this);
        }
        else
        {
            wxGetApp().m_callsignEncoder = new CallsignEncoder();
            wxGetApp().m_pskReporter = new PskReporter(
                wxGetApp().m_psk_callsign.ToStdString(), 
                wxGetApp().m_psk_grid_square.ToStdString(),
                std::string("FreeDV ") + FREEDV_VERSION + " " + currentMode);
            wxGetApp().m_pskPendingCallsign = "";
            wxGetApp().m_pskPendingSnr = 0;
        
            // Send empty packet to verify network connectivity.
            bool success = wxGetApp().m_pskReporter->send();
            if (success)
            {
                // Enable PSK Reporter timer (every 5 minutes).
                m_pskReporterTimer.Start(5 * 60 * 1000);
            }
            else
            {
                wxMessageBox("Couldn't connect to PSK Reporter server. Reporting functionality will be disabled.", wxT("Error"), wxOK | wxICON_ERROR, this);
                delete wxGetApp().m_pskReporter;
                delete wxGetApp().m_callsignEncoder;
                wxGetApp().m_pskReporter = NULL;
                wxGetApp().m_callsignEncoder = NULL;
            }
        }
    }
    else
    {
        wxGetApp().m_pskReporter = NULL;
        wxGetApp().m_callsignEncoder = NULL;
    }
    
    return status;
}
