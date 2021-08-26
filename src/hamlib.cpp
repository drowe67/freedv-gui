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

#include <wx/settings.h>

#define TOK_CIVADDR TOKEN_BACKEND(1)

extern int g_verbose;

using namespace std;

typedef std::vector<const struct rig_caps *> riglist_t;

static bool rig_cmp(const struct rig_caps *rig1, const struct rig_caps *rig2);
static int build_list(const struct rig_caps *rig, rig_ptr_t);

Hamlib::Hamlib() : 
    m_rig(NULL),
    m_modeBox(NULL),
    m_freqBox(NULL),
    m_currFreq(0),
    m_currMode(RIG_MODE_USB),
    m_vhfUhfMode(false)  {
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

bool Hamlib::connect(unsigned int rig_index, const char *serial_port, const int serial_rate, const int civ_hex) {

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

    m_rig = rig_init(m_rigList[rig_index]->rig_model);

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

#if defined(HAMLIB_FILPATHLEN)
    strncpy(m_rig->state.rigport.pathname, serial_port, HAMLIB_FILPATHLEN - 1);
#else 
    strncpy(m_rig->state.rigport.pathname, serial_port, FILPATHLEN - 1);
#endif // HAMLIB_FILPATHLEN

    if (serial_rate) {
        if (g_verbose) fprintf(stderr, "hamlib: setting serial rate: %d\n", serial_rate);
        m_rig->state.rigport.parm.serial.rate = serial_rate;
    }
    if (g_verbose) fprintf(stderr, "hamlib: serial rate: %d\n", m_rig->state.rigport.parm.serial.rate);
    if (g_verbose) fprintf(stderr, "hamlib: data_bits..: %d\n", m_rig->state.rigport.parm.serial.data_bits);
    if (g_verbose) fprintf(stderr, "hamlib: stop_bits..: %d\n", m_rig->state.rigport.parm.serial.stop_bits);

    if (rig_open(m_rig) == RIG_OK) {
        if (g_verbose) fprintf(stderr, "hamlib: rig_open() OK\n");
        return true;
    }
    if (g_verbose) fprintf(stderr, "hamlib: rig_open() failed ...\n");

    rig_cleanup(m_rig);
    m_rig = nullptr;
    
    return false;
}

int Hamlib::get_serial_rate(void) {
    return m_rig->state.rigport.parm.serial.rate;
}

int Hamlib::get_data_bits(void) {
    return m_rig->state.rigport.parm.serial.data_bits;
}

int Hamlib::get_stop_bits(void) {
    return m_rig->state.rigport.parm.serial.stop_bits;
}

bool Hamlib::ptt(bool press, wxString &hamlibError) {
    if (g_verbose) fprintf(stderr,"Hamlib::ptt: %d\n", press);
    hamlibError = "";

    if(!m_rig)
        return false;

    /* TODO(Joel): make ON_DATA and ON configurable. */

    ptt_t on = press ? RIG_PTT_ON : RIG_PTT_OFF;

    /* TODO(Joel): what should the VFO option be? */

    int retcode = rig_set_ptt(m_rig, RIG_VFO_CURR, on);
    if (g_verbose) fprintf(stderr,"Hamlib::ptt: rig_set_ptt returned: %d\n", retcode);
    if (retcode != RIG_OK ) {
        if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(retcode));
        hamlibError = rigerror(retcode);
    }

    return retcode == RIG_OK;
}

int Hamlib::hamlib_freq_cb(RIG* rig, vfo_t currVFO, freq_t currFreq, void* ptr)
{
    Hamlib* thisPtr = (Hamlib*)ptr;
    thisPtr->m_currFreq = currFreq;
    thisPtr->update_mode_status();
    return RIG_OK;
}

int Hamlib::hamlib_mode_cb(RIG* rig, vfo_t currVFO, rmode_t currMode, pbwidth_t passband, void* ptr)
{
    Hamlib* thisPtr = (Hamlib*)ptr;
    thisPtr->m_currMode = currMode;
    thisPtr->update_mode_status();
    return RIG_OK;
}

int Hamlib::update_frequency_and_mode(void)
{
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t passband = 0;
    int result = rig_get_mode(m_rig, RIG_VFO_CURR, &mode, &passband);
    if (result != RIG_OK)
    {
        if (g_verbose) fprintf(stderr, "rig_get_mode: error = %s \n", rigerror(result));
    }
    else
    {
        freq_t freq = 0;
        result = rig_get_freq(m_rig, RIG_VFO_CURR, &freq);
        if (result != RIG_OK)
        {
            if (g_verbose) fprintf(stderr, "rig_get_freq: error = %s \n", rigerror(result));
        }
        else
        {
            m_currMode = mode;
            m_currFreq = freq;
            update_mode_status();
        }
    }
    
    return result;
}

void Hamlib::enable_mode_detection(wxStaticText* statusBox, wxTextCtrl* freqBox, bool vhfUhfMode)
{
    // Set VHF/UHF mode. This governs whether FM is an acceptable mode for the detection display.
    m_vhfUhfMode = vhfUhfMode;
    
    m_freqBox = freqBox;
    
    // Enable control.
    m_modeBox = statusBox;
    m_modeBox->Enable(true);

    // Populate initial state.
    int result = update_frequency_and_mode();

    // If we couldn't get current mode/frequency for any reason, disable the UI for it.
    if (result != RIG_OK)
    {
        m_modeBox->SetLabel(wxT("unk"));
        m_modeBox->Enable(false);
        m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        m_modeBox = NULL;
    }
    else
    {
        // TBD: Due to hamlib not supporting polling on Windows, the bottom is temporarily
        // disabled. When/if that changes, re-enabling is a simple matter of removing
        // the #if/#endif below.
#if 0
        // Enable rig callbacks.
        rig_set_freq_callback(m_rig, &hamlib_freq_cb, this);
        rig_set_mode_callback(m_rig, &hamlib_mode_cb, this);
        rig_set_trn(m_rig, RIG_TRN_POLL);
#endif
    }
}

void Hamlib::disable_mode_detection()
{
    if (m_modeBox != NULL) 
    {
        // TBD: Due to hamlib not supporting polling on Windows, the bottom is temporarily
        // disabled. When/if that changes, re-enabling is a simple matter of removing
        // the #if/#endif below.
#if 0
        // Disable callbacks.
        rig_set_trn(m_rig, RIG_TRN_OFF);
        rig_set_freq_callback(m_rig, NULL, NULL);
        rig_set_mode_callback(m_rig, NULL, NULL);
#endif
    
        // Disable control.
        m_modeBox->SetLabel(wxT("unk"));
        m_modeBox->Enable(false);
        m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        m_modeBox = NULL;
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

    // Update color
    bool isMatchingMode = 
        (m_currFreq >= 10000000 && (m_currMode == RIG_MODE_USB || m_currMode == RIG_MODE_PKTUSB)) ||
        (m_currFreq < 10000000 && (m_currMode == RIG_MODE_LSB || m_currMode == RIG_MODE_PKTLSB)) ||
        (m_vhfUhfMode && m_currFreq >= 29510000 && (m_currMode == RIG_MODE_FM || m_currMode == RIG_MODE_PKTFM));
    if (isMatchingMode)
    {
        m_modeBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    else
    {
        m_modeBox->SetForegroundColour(wxColor(*wxRED));
    }

    // Update frequency box
    m_freqBox->SetValue(wxString::Format("%d", (unsigned int)m_currFreq/1000));
    
    // Refresh
    m_modeBox->Refresh();
}

void Hamlib::close(void) {
    if(m_rig) {
        rig_close(m_rig);
        rig_cleanup(m_rig);
        m_rig = NULL;
    }
}
