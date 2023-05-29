//==========================================================================
// Name:            hamlib.cpp
//
// Purpose:         Hamlib integration for FreeDV
// Created:         May 2013
// Authors:         Joel Stanley
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
#include <hamlib.h>

#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

#include <wx/settings.h>

#define TOK_CIVADDR TOKEN_BACKEND(1)

extern int g_verbose;

using namespace std;

typedef std::vector<const struct rig_caps *> riglist_t;

static bool rig_cmp(const struct rig_caps *rig1, const struct rig_caps *rig2);
static int build_list(const struct rig_caps *rig, rig_ptr_t);

Hamlib::Hamlib() : 
    m_rig(NULL),
    m_rig_model(0),
    m_modeBox(NULL),
    m_freqBox(NULL),
    m_currFreq(0),
    m_currMode(RIG_MODE_USB),
    m_vhfUhfMode(false),
    pttSet_(false),
    threadRunning_(false)  {
    /* Stop hamlib from spewing info to stderr. */
    rig_set_debug(RIG_DEBUG_NONE);

    /* Create sorted list of rigs. */
    rig_load_all_backends();
    rig_list_foreach(build_list, &m_rigList);
    sort(m_rigList.begin(), m_rigList.end(), rig_cmp);

    /* Reset debug output. */
    rig_set_debug(RIG_DEBUG_VERBOSE);

    m_rig = NULL;
}

Hamlib::~Hamlib() {
	if(m_rig)
		close();
}

static int build_list(const struct rig_caps *rig, rig_ptr_t rigList) {
    ((riglist_t *)rigList)->push_back(rig); 
    return 1;
}

static bool rig_cmp(const struct rig_caps *rig1, const struct rig_caps *rig2) {
    /* Compare manufacturer. */
    int r = strcasecmp(rig1->mfg_name, rig2->mfg_name);
    if (r != 0)
        return r < 0;

    /* Compare model. */
    r = strcasecmp(rig1->model_name, rig2->model_name);
    if (r != 0)
        return r < 0;

    /* Compare rig ID. */
    return rig1->rig_model < rig2->rig_model;
}

unsigned int Hamlib::rigNameToIndex(std::string rigName)
{
    unsigned int index = 0;
    for (auto& entry : m_rigList)
    {
        char name[128];
        snprintf(name, 128, "%s %s", entry->mfg_name, entry->model_name); 
        
        if (rigName == std::string(name))
        {
            return index;
        }
        
        index++;
    }
    
    return -1;
}

std::string Hamlib::rigIndexToName(unsigned int rigIndex)
{
    char name[128];
    snprintf(name, 128, "%s %s", m_rigList[rigIndex]->mfg_name, m_rigList[rigIndex]->model_name); 
    return name;
}

freq_t Hamlib::get_frequency(void) const
{
    return m_currFreq;
}

void Hamlib::populateComboBox(wxComboBox *cb) {

    riglist_t::const_iterator rig = m_rigList.begin();
    for (; rig !=m_rigList.end(); rig++) {
        char name[128];
        snprintf(name, 128, "%s %s", (*rig)->mfg_name, (*rig)->model_name); 
        cb->Append(name);
    }
}

bool Hamlib::connect(unsigned int rig_index, const char *serial_port, const int serial_rate, const int civ_hex, const PttType pttType) {

    /* Look up model from index. */

    if (rig_index >= m_rigList.size()) {
        if (g_verbose) fprintf(stderr, "rig_index invalid, returning\n");
        return false;
    }

    if (g_verbose) fprintf(stderr, "rig: %s %s (%d)\n", m_rigList[rig_index]->mfg_name,
            m_rigList[rig_index]->model_name, m_rigList[rig_index]->rig_model);

    if(m_rig) {
        if (g_verbose) fprintf(stderr, "Closing old hamlib instance!\n");
        close();
    }

    /* Initialise, configure and open. */

    if (m_rig == nullptr || m_rigList[rig_index]->rig_model != m_rig_model)
    {
        m_rig = rig_init(m_rigList[rig_index]->rig_model);
    }
    m_rig_model = m_rigList[rig_index]->rig_model;
    
    if (!m_rig) {
        if (g_verbose) fprintf(stderr, "rig_init() failed, returning\n");
        return false;
    }
    if (g_verbose) fprintf(stderr, "rig_init() OK ....\n");

    /* Set CI-V address if necessary. */
    if (!strncmp(m_rigList[rig_index]->mfg_name, "Icom", 4))
    {
        char civ_addr[5];
        snprintf(civ_addr, 5, "0x%0X", civ_hex);
        if (g_verbose) fprintf(stderr, "hamlib: setting CI-V address to: %s\n", civ_addr);
        rig_set_conf(m_rig, rig_token_lookup(m_rig, "civaddr"), civ_addr);
    }
    else
    {
        if (g_verbose) fprintf(stderr, "hamlib: ignoring CI-V configuration due to non-Icom radio\r\n");
    }

    rig_set_conf(m_rig, rig_token_lookup(m_rig, "rig_pathname"), serial_port);

    if (serial_rate) {
        if (g_verbose) fprintf(stderr, "hamlib: setting serial rate: %d\n", serial_rate);
        std::stringstream ss;
        ss << serial_rate;
        rig_set_conf(m_rig, rig_token_lookup(m_rig, "serial_speed"), ss.str().c_str());
    }
    if (g_verbose) fprintf(stderr, "hamlib: serial rate: %d\n", get_serial_rate());
    if (g_verbose) fprintf(stderr, "hamlib: data_bits..: %d\n", get_data_bits());
    if (g_verbose) fprintf(stderr, "hamlib: stop_bits..: %d\n", get_stop_bits());

    // Set PTT type as needed.
    switch(pttType)
    {
        case PTT_VIA_RTS:
            rig_set_conf(m_rig, rig_token_lookup(m_rig, "ptt_type"), "RTS");
            break;
        case PTT_VIA_DTR:
            rig_set_conf(m_rig, rig_token_lookup(m_rig, "ptt_type"), "DTR");
            break;
        case PTT_VIA_CAT:
        default:
            break;
    }
    
    if (rig_open(m_rig) == RIG_OK) {
        if (g_verbose) fprintf(stderr, "hamlib: rig_open() OK\n");
        
        // Determine whether we have multiple VFOs.
        multipleVfos_ = false;
        vfo_t vfo;
        if (rig_get_vfo (m_rig, &vfo) == RIG_OK && (m_rig->state.vfo_list & RIG_VFO_B))
        {
            multipleVfos_ = true;
        }
        
        return true;
    }
    if (g_verbose) fprintf(stderr, "hamlib: rig_open() failed ...\n");

    rig_cleanup(m_rig);
    m_rig = nullptr;
    
    return false;
}

int Hamlib::get_serial_rate(void) {
    char buf[128];
    rig_get_conf(m_rig, rig_token_lookup(m_rig, "serial_speed"), buf);
    return atoi(buf);
}

int Hamlib::get_data_bits(void) {
    char buf[128];
    rig_get_conf(m_rig, rig_token_lookup(m_rig, "data_bits"), buf);
    return atoi(buf);
}

int Hamlib::get_stop_bits(void) {
    char buf[128];
    rig_get_conf(m_rig, rig_token_lookup(m_rig, "stop_bits"), buf);
    return atoi(buf);
}

bool Hamlib::ptt(bool press, wxString &hamlibError) {
    std::unique_lock<std::mutex> lock(statusUpdateMutex_);
    
    if (g_verbose) fprintf(stderr,"Hamlib::ptt: %d\n", press);
    hamlibError = "";

    if(!m_rig)
        return false;

    ptt_t on = press ? RIG_PTT_ON : RIG_PTT_OFF;

    int result = rig_set_ptt(m_rig, RIG_VFO_CURR, on);
    if (result != RIG_OK) 
    {
        if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));
    }
    else
    {
        pttSet_ = press;
        if (!press)
        {
            update_frequency_and_mode();
        }
    }
    
    return result == RIG_OK;
}

int Hamlib::update_frequency_and_mode(void)
{
    if (m_rig == nullptr)
    {
        return RIG_EARG; // "NULL RIG handle or any invalid pointer parameter in get arg"
    }
    
    // Trigger update.
    statusUpdateCV_.notify_one();

    return 0;
}

void Hamlib::enable_mode_detection(wxStaticText* statusBox, wxTextCtrl* freqBox, bool vhfUhfMode)
{
    // Set VHF/UHF mode. This governs whether FM is an acceptable mode for the detection display.
    m_vhfUhfMode = vhfUhfMode;
    
    m_freqBox = freqBox;
    
    // Enable control.
    m_modeBox = statusBox;
    m_modeBox->Enable(true);

    // Start status update thread
    assert(!statusUpdateThread_.joinable());
    statusUpdateThread_ = std::thread(&Hamlib::statusUpdateThreadEntryFn_, this);
}

void Hamlib::disable_mode_detection()
{
    threadRunning_ = false;
    statusUpdateCV_.notify_one();
    
    if (statusUpdateThread_.joinable())
    {
        statusUpdateThread_.join();
    }
}


void Hamlib::statusUpdateThreadEntryFn_()
{
    threadRunning_ = true;
    
    // Populate initial state.
    int result = update_frequency_and_mode();

    // If we couldn't get current mode/frequency for any reason, disable the UI for it.
    if (result != RIG_OK)
    {
        m_modeBox->CallAfter([&]() {
            m_modeBox->SetLabel(wxT("unk"));
            m_modeBox->Enable(false);
            m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            m_modeBox = NULL;
        });
        
        threadRunning_ = false;
        return;
    }
    
    update_from_hamlib_();
    
    while (threadRunning_)
    {
        std::unique_lock<std::mutex> lock(statusUpdateMutex_);
        
        // Wait for refresh trigger from UI thread.
        statusUpdateCV_.wait_for(lock, 10s);
        if (!threadRunning_) break;
        
        update_from_hamlib_();
    }
    
    if (m_modeBox != NULL) 
    {
        // Disable control.
        m_modeBox->CallAfter([&]() {
            m_modeBox->SetLabel(wxT("unk"));
            m_modeBox->Enable(false);
            m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            m_modeBox = NULL;
        });
    }
}

void Hamlib::update_from_hamlib_()
{
    if (pttSet_)
    {
        // ignore Hamlib update when PTT active.
        return;
    }
    
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t passband = 0;
    vfo_t currVfo = getCurrentVfo_();    

modeAttempt:
    int result = rig_get_mode(m_rig, currVfo, &mode, &passband);
    if (result != RIG_OK && currVfo == RIG_VFO_CURR)
    {
        if (g_verbose) fprintf(stderr, "rig_get_mode: error = %s \n", rigerror(result));
    }
    else if (result != RIG_OK)
    {
        // We supposedly have multiple VFOs but ran into problems 
        // trying to get information about the supposed active VFO.
        // Make a last ditch effort using RIG_VFO_CURR before fully failing.
        currVfo = RIG_VFO_CURR;
        goto modeAttempt;
    }
    else
    {
freqAttempt:
        freq_t freq = 0;
        result = rig_get_freq(m_rig, currVfo, &freq);
        if (result != RIG_OK && currVfo == RIG_VFO_CURR)
        {
            if (g_verbose) fprintf(stderr, "rig_get_freq: error = %s \n", rigerror(result));
        }
        else if (result != RIG_OK)
        {
            // We supposedly have multiple VFOs but ran into problems 
            // trying to get information about the supposed active VFO.
            // Make a last ditch effort using RIG_VFO_CURR before fully failing.
            currVfo = RIG_VFO_CURR;
            goto freqAttempt;
        }
        else
        {
            m_currMode = mode;
            m_currFreq = freq;
            m_modeBox->CallAfter([&]() { update_mode_status(); });
        }
    }
}

void Hamlib::update_mode_status()
{
    // Update string value.
    if (m_currMode == RIG_MODE_USB || m_currMode == RIG_MODE_PKTUSB)
        m_modeBox->SetLabel(wxT("USB"));
    else if (m_currMode == RIG_MODE_LSB || m_currMode == RIG_MODE_PKTLSB)
        m_modeBox->SetLabel(wxT("LSB"));
    else if (m_currMode == RIG_MODE_FM || m_currMode == RIG_MODE_FM)
        m_modeBox->SetLabel(wxT("FM"));
    else
        m_modeBox->SetLabel(rig_strrmode(m_currMode));

    // Widest 60 meter allocation is 5.250-5.450 MHz per https://en.wikipedia.org/wiki/60-meter_band.
    bool is60MeterBand = m_currFreq >= 5250000 && m_currFreq <= 5450000;
    
    // Update color based on the mode and current frequency.
    bool isUsbFreq = m_currFreq >= 10000000 || is60MeterBand;
    bool isLsbFreq = m_currFreq < 10000000 && !is60MeterBand;
    bool isFmFreq = m_currFreq >= 29510000;
    
    bool isMatchingMode = 
        (isUsbFreq && (m_currMode == RIG_MODE_USB || m_currMode == RIG_MODE_PKTUSB)) ||
        (isLsbFreq && (m_currMode == RIG_MODE_LSB || m_currMode == RIG_MODE_PKTLSB)) ||
        (m_vhfUhfMode && isFmFreq && (m_currMode == RIG_MODE_FM || m_currMode == RIG_MODE_PKTFM));
    
    if (isMatchingMode)
    {
        m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    else
    {
        m_modeBox->SetForegroundColour(wxColor(*wxRED));
    }

    // Update frequency box
    m_freqBox->SetValue(wxString::Format("%.1f", m_currFreq/1000));
    
    // Refresh
    m_modeBox->Refresh();
}

void Hamlib::close(void) {
    if(m_rig) {
        // Turn off status thread if needed.
        disable_mode_detection();
        
        rig_close(m_rig);
        rig_cleanup(m_rig);
        m_rig = NULL;
    }
}

vfo_t Hamlib::getCurrentVfo_()
{
    vfo_t vfo = RIG_VFO_CURR;
    
    if (multipleVfos_)
    {
        int result = rig_get_vfo(m_rig, &vfo);
        if (result != RIG_OK && result != -RIG_ENAVAIL)
        {
            // Note: we'll still attempt operation using RIG_VFO_CURR just in case.
            if (g_verbose) fprintf(stderr, "rig_get_vfo: error = %s \n", rigerror(result));
            vfo = RIG_VFO_CURR;
        }
    }
    
    return vfo;
}
