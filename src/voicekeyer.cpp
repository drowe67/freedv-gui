/*
   voicekeyer.cpp
   
   Voice Keyer implementation
*/

#include "main.h"

void MainFrame::OnTogBtnVoiceKeyerClick (wxCommandEvent& event)
{
    if (vk_state == VK_IDLE)
        VoiceKeyerProcessEvent(VK_START);
    else
        VoiceKeyerProcessEvent(VK_SPACE_BAR);

    event.Skip();
}

extern SNDFILE *g_sfPlayFile;
extern bool g_playFileToMicIn;
extern bool g_loopPlayFileToMicIn;
extern FreeDVInterface freedvInterface;
extern int g_sfTxFs;

int MainFrame::VoiceKeyerStartTx(void)
{
    int next_state;

    // start playing wave file or die trying

    SF_INFO sfInfo;
    sfInfo.format = 0;

    SNDFILE* tmpPlayFile = sf_open(wxGetApp().appConfiguration.voiceKeyerWaveFile->mb_str(), SFM_READ, &sfInfo);
    if(tmpPlayFile == NULL) {
        wxString strErr = sf_strerror(NULL);
        wxMessageBox(strErr, wxT("Couldn't open:") + wxGetApp().appConfiguration.voiceKeyerWaveFile, wxOK);
        m_togBtnVoiceKeyer->SetValue(false);
        next_state = VK_IDLE;
    }
    else {
        g_sfTxFs = sfInfo.samplerate;
        g_sfPlayFile = tmpPlayFile;
        
        SetStatusText(wxT("Voice Keyer: Playing file ") + wxGetApp().appConfiguration.voiceKeyerWaveFile + wxT(" to mic input") , 0);
        g_loopPlayFileToMicIn = false;
        g_playFileToMicIn = true;

        m_btnTogPTT->SetValue(true); togglePTT();
        next_state = VK_TX;
    }

    return next_state;
}


void MainFrame::VoiceKeyerProcessEvent(int vk_event) {
    int next_state = vk_state;
    
    switch(vk_state) {

    case VK_IDLE:
        if (vk_event == VK_START) {
            // sample these puppies at start just in case they are changed while VK running
            vk_rx_pause = wxGetApp().appConfiguration.voiceKeyerRxPause;
            vk_repeats = wxGetApp().appConfiguration.voiceKeyerRepeats;
            if (g_verbose) fprintf(stderr, "vk_rx_pause: %d vk_repeats: %d\n", vk_rx_pause, vk_repeats);

            vk_repeat_counter = 0;
            next_state = VoiceKeyerStartTx();
        }
        break;

     case VK_TX:

        // In this state we are transmitting and playing a wave file
        // to Mic In

        if (vk_event == VK_SPACE_BAR) {
            m_btnTogPTT->SetValue(false); 
            m_btnTogPTT->SetBackgroundColour(wxNullColour);
            togglePTT();
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
            CallAfter([&]() { StopPlayFileToMicIn(); });
        }

        if (vk_event == VK_PLAY_FINISHED) {
            m_btnTogPTT->SetValue(false); 
            m_btnTogPTT->SetBackgroundColour(wxNullColour);
            togglePTT();
            vk_repeat_counter++;
            if (vk_repeat_counter > vk_repeats) {
                m_togBtnVoiceKeyer->SetValue(false);
                next_state = VK_IDLE;
            }
            else {
                vk_rx_time = 0.0;
                next_state = VK_RX;
            }
        }

        break;

     case VK_RX:

        // in this state we are receiving and waiting for
        // delay timer or valid sync

        if (vk_event == VK_DT) {
            if (freedvInterface.getSync() == 1) {
                // if we detect sync transition to SYNC_WAIT state
                next_state = VK_SYNC_WAIT;
                vk_rx_sync_time = 0.0;
            } else {
                vk_rx_time += DT;
                if (vk_rx_time >= vk_rx_pause) {
                    next_state = VoiceKeyerStartTx();
                }
            }
        }

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
        }

        break;

     case VK_SYNC_WAIT:

        // In this state we wait for valid sync to last
        // VK_SYNC_WAIT_TIME seconds

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
        }

        if (vk_event == VK_DT) {
            if (freedvInterface.getSync() == 0) {
                // if we lose sync transition to RX State
                next_state = VK_RX;
            } else {
                vk_rx_time += DT;
                vk_rx_sync_time += DT;
            }

            // drop out of voice keyer if we get a few seconds of valid sync

            if (vk_rx_sync_time >= VK_SYNC_WAIT_TIME) {
                m_togBtnVoiceKeyer->SetValue(false);
                next_state = VK_IDLE;
            }
        }
        break;

    default:
        // catch anything we missed

        m_btnTogPTT->SetValue(false); 
        m_btnTogPTT->SetBackgroundColour(wxNullColour);
        togglePTT();
        m_togBtnVoiceKeyer->SetValue(false);
        next_state = VK_IDLE;
    }

    //if ((vk_event != VK_DT) || (vk_state != next_state))
    //    fprintf(stderr, "VoiceKeyerProcessEvent: vk_state: %d vk_event: %d next_state: %d  vk_repeat_counter: %d\n", vk_state, vk_event, next_state, vk_repeat_counter);
    vk_state = next_state;
}

