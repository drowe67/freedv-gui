//=========================================================================
// Name:            HamlibRigController.cpp
// Purpose:         Controls radios using Hamlib library.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
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
//=========================================================================

#include <mutex>
#include <condition_variable>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <strings.h>

#include "HamlibRigController.h"

extern int g_verbose;

HamlibRigController::RigList HamlibRigController::RigList_;
std::mutex HamlibRigController::RigListMutex_;

#if RIGCAPS_NOT_CONST
int HamlibRigController::BuildRigList_(struct rig_caps *rig, rig_ptr_t rigList) {
#else
int HamlibRigController::BuildRigList_(const struct rig_caps *rig, rig_ptr_t rigList) {    
#endif // RIGCAPS_NOT_CONST
    ((HamlibRigController::RigList *)rigList)->push_back(rig); 
    return 1;
}

bool HamlibRigController::RigCompare_(const struct rig_caps *rig1, const struct rig_caps *rig2) {
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

HamlibRigController::HamlibRigController(std::string rigName, std::string serialPort, const int serialRate, const int civHex, const PttType pttType, std::string pttSerialPort, bool restoreFreqModeOnDisconnect)
    : rigName_(rigName)
    , serialPort_(serialPort)
    , serialRate_(serialRate)
    , civHex_(civHex)
    , pttType_(pttType)
    , pttSerialPort_(pttSerialPort)
    , rig_(nullptr)
    , multipleVfos_(false)
    , pttSet_(false)
    , currFreq_(0)
    , currMode_(RIG_MODE_NONE)
    , restoreOnDisconnect_(restoreFreqModeOnDisconnect)
    , origFreq_(0)
    , origMode_(RIG_MODE_NONE)
{
    // Perform initial load of rig list if this is our first time being created.
    InitializeHamlibLibrary();
}

HamlibRigController::HamlibRigController(int rigIndex, std::string serialPort, const int serialRate, const int civHex, const PttType pttType, std::string pttSerialPort, bool restoreFreqModeOnDisconnect)
    : rigName_(RigIndexToName(rigIndex))
    , serialPort_(serialPort)
    , serialRate_(serialRate)
    , civHex_(civHex)
    , pttType_(pttType)
    , pttSerialPort_(pttSerialPort)
    , rig_(nullptr)
    , multipleVfos_(false)
    , pttSet_(false)
    , currFreq_(0)
    , currMode_(RIG_MODE_NONE)
    , restoreOnDisconnect_(restoreFreqModeOnDisconnect)
    , origFreq_(0)
    , origMode_(RIG_MODE_NONE)
{
    // Perform initial load of rig list if this is our first time being created.
    InitializeHamlibLibrary();
}

HamlibRigController::~HamlibRigController()
{
    // Disconnect in a synchronous fashion before killing our thread.
    std::condition_variable cv;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk(mtx);
    
    enqueue_([&]() {
        std::unique_lock<std::mutex> innerLock(mtx);
        if (rig_ != nullptr)
        {
            disconnectImpl_();
        }
        cv.notify_one();
    });
    
    cv.wait(lk);
}

void HamlibRigController::InitializeHamlibLibrary()
{
    std::unique_lock<std::mutex> lk(RigListMutex_);
    if (RigList_.size() == 0)
    {
        /* Stop hamlib from spewing info to stderr. */
        rig_set_debug(RIG_DEBUG_NONE);

        /* Create sorted list of rigs. */
        rig_load_all_backends();
        rig_list_foreach(&HamlibRigController::BuildRigList_, &RigList_);
        std::sort(RigList_.begin(), RigList_.end(), &HamlibRigController::RigCompare_);

        /* Reset debug output. */
        rig_set_debug(RIG_DEBUG_VERBOSE);
    }
}

void HamlibRigController::connect()
{
    enqueue_(std::bind(&HamlibRigController::connectImpl_, this));
}

void HamlibRigController::disconnect()
{
    enqueue_(std::bind(&HamlibRigController::disconnectImpl_, this));
}

bool HamlibRigController::isConnected()
{
    return rig_ != nullptr;
}

void HamlibRigController::ptt(bool state)
{
    enqueue_(std::bind(&HamlibRigController::pttImpl_, this, state));
}

void HamlibRigController::setFrequency(uint64_t frequency)
{
    enqueue_(std::bind(&HamlibRigController::setFrequencyImpl_, this, frequency));
}

void HamlibRigController::setMode(IRigFrequencyController::Mode mode)
{
    enqueue_(std::bind(&HamlibRigController::setModeImpl_, this, mode));
}

void HamlibRigController::requestCurrentFrequencyMode()
{
    enqueue_(std::bind(&HamlibRigController::requestCurrentFrequencyModeImpl_, this));
}

int HamlibRigController::RigNameToIndex(std::string rigName)
{
    InitializeHamlibLibrary();

    int index = 0;
    for (auto& entry : RigList_)
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

std::string HamlibRigController::RigIndexToName(unsigned int rigIndex)
{
    InitializeHamlibLibrary();

    char name[128];
    snprintf(name, 128, "%s %s", RigList_[rigIndex]->mfg_name, RigList_[rigIndex]->model_name); 
    return name;
}

int HamlibRigController::GetNumberSupportedRadios()
{
    InitializeHamlibLibrary();
    return RigList_.size();
}

// Note: these execute in the thread created by ThreadedObject on object instantiation.

void HamlibRigController::connectImpl_()
{
    if (rig_ != nullptr)
    {
        std::string errMsg = "Already connected to radio.";
        onRigError(this, errMsg);
        return;
    }
    
    auto rigIndex = RigNameToIndex(rigName_);
    if (rigIndex == -1)
    {
        std::string errMsg = "Could not find rig " + rigName_ + " in Hamlib database.";
        onRigError(this, errMsg);
        return;
    }
    
    /* Initialise, configure and open. */
    origFreq_ = 0;
    origMode_ = RIG_MODE_NONE;
    
    rig_ = rig_init(RigList_[rigIndex]->rig_model);
    if (!rig_) 
    {
        if (g_verbose) fprintf(stderr, "rig_init() failed, returning\n");
        
        std::string errMsg = "Hamlib could not initialize rig " + rigName_;
        onRigError(this, errMsg);
        return;
    }
    if (g_verbose) fprintf(stderr, "rig_init() OK ....\n");

    /* Set CI-V address if necessary. */
    if (!strncmp(RigList_[rigIndex]->mfg_name, "Icom", 4))
    {
        char civAddr[5];
        snprintf(civAddr, 5, "0x%0X", civHex_);
        if (g_verbose) fprintf(stderr, "hamlib: setting CI-V address to: %s\n", civAddr);
        rig_set_conf(rig_, rig_token_lookup(rig_, "civaddr"), civAddr);
    }
    else
    {
        if (g_verbose) fprintf(stderr, "hamlib: ignoring CI-V configuration due to non-Icom radio\r\n");
    }

    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), serialPort_.c_str());

    if (pttSerialPort_.size() > 0)
    {
        rig_set_conf(rig_, rig_token_lookup(rig_, "ptt_pathname"), pttSerialPort_.c_str());
    }
    
    if (serialRate_ > 0) 
    {
        if (g_verbose) fprintf(stderr, "hamlib: setting serial rate: %d\n", serialRate_);
        std::stringstream ss;
        ss << serialRate_;
        rig_set_conf(rig_, rig_token_lookup(rig_, "serial_speed"), ss.str().c_str());
    }

    // Set PTT type as needed.
    switch(pttType_)
    {
        case PTT_VIA_RTS:
            rig_->state.pttport.type.ptt = RIG_PTT_SERIAL_RTS;
            break;
        case PTT_VIA_DTR:
            rig_->state.pttport.type.ptt = RIG_PTT_SERIAL_DTR;
            break;
        case PTT_VIA_NONE:
            rig_->state.pttport.type.ptt = RIG_PTT_NONE;
            break;
        case PTT_VIA_CAT:
        default:
            break;
    }
    
    rig_set_conf(rig_, rig_token_lookup(rig_, "timeout"), "1000");
    rig_set_conf(rig_, rig_token_lookup(rig_, "retry"), "1");
    rig_set_conf(rig_, rig_token_lookup(rig_, "timeout_retry"), "1");
    
    auto result = rig_open(rig_);
    if (result == RIG_OK) 
    {
        if (g_verbose) fprintf(stderr, "hamlib: rig_open() OK\n");
        onRigConnected(this);
        
        // Determine whether we have multiple VFOs.
        multipleVfos_ = false;
        vfo_t vfo;
        if (rig_get_vfo (rig_, &vfo) == RIG_OK && (rig_->state.vfo_list & RIG_VFO_B))
        {
            multipleVfos_ = true;
        }

        // Get current frequency and mode when we first connect so we can 
        // revert on close.
        requestCurrentFrequencyModeImpl_();
        return;
    }
    else
    {
        std::string errMsg = std::string("Could not connect to radio: ") + rigerror(result);
        onRigError(this, errMsg);
    }
    if (g_verbose) fprintf(stderr, "hamlib: rig_open() failed ...\n");

    rig_cleanup(rig_);
    rig_ = nullptr;
}

void HamlibRigController::disconnectImpl_()
{
    if (rig_) 
    {
        // Stop transmitting.
        if (pttSet_)
        {
            pttImpl_(false);
        }
 
        // If we're told to restore on disconnect, do so.
        if (restoreOnDisconnect_)
        {
            vfo_t currVfo = getCurrentVfo_(); 
            setFrequencyHelper_(currVfo, origFreq_);
            setModeHelper_(currVfo, origMode_);
        }
        
        origFreq_ = 0;
        origMode_ = RIG_MODE_NONE;
        
        rig_close(rig_);
        rig_cleanup(rig_);
        rig_ = nullptr;
    }
}

void HamlibRigController::pttImpl_(bool state)
{
    if (g_verbose) fprintf(stderr,"Hamlib::ptt: %d\n", state);

    if (rig_ == nullptr)
    {
        return;
    }

    ptt_t on = state ? RIG_PTT_ON : RIG_PTT_OFF;

    int result = RIG_OK;
    if (pttType_ != PTT_VIA_NONE)
    {
        result = rig_set_ptt(rig_, RIG_VFO_CURR, on);
    }
    if (result != RIG_OK) 
    {
        if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));
        
        std::string errMsg = "Cannot set PTT: " + std::string(rigerror(result));
        onRigError(this, errMsg);
    }
    else
    {
        if (pttSet_ != state)
        {
            onPttChange(this, state);
        }
        
        pttSet_ = state;
        
        if (!state)
        {
            requestCurrentFrequencyMode();
        }
    }
}

void HamlibRigController::setFrequencyImpl_(uint64_t frequencyHz)
{
    if (rig_ == nullptr)
    {
        return;
    }

    if (currFreq_ == frequencyHz)
    {
        return;
    }
    
    if (pttSet_)
    {
        // If transmitting, temporarily stop transmitting so we can change the mode.
        int result = rig_set_ptt(rig_, RIG_VFO_CURR, RIG_PTT_OFF);
        if (result != RIG_OK) 
        {
            // If we can't stop transmitting, we shouldn't try to change the mode
            // as it'll fail on some radios.
            if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));

            std::string errMsg = std::string("Could not disable PTT prior to frequency change: ") + rigerror(result);
            onRigError(this, errMsg);
            
            return;
        }
    }

    vfo_t currVfo = getCurrentVfo_(); 
    setFrequencyHelper_(currVfo, frequencyHz);

    if (pttSet_)
    {
        // If transmitting, temporarily stop transmitting so we can change the mode.
        int result = rig_set_ptt(rig_, RIG_VFO_CURR, RIG_PTT_ON);
        if (result != RIG_OK) 
        {
            // If we can't stop transmitting, we shouldn't try to change the mode
            // as it'll fail on some radios.
            if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));
            
            std::string errMsg = std::string("Could not enable PTT after frequency change: ") + rigerror(result);
            onRigError(this, errMsg);
        }
    }
}

void HamlibRigController::setModeImpl_(IRigFrequencyController::Mode mode)
{
    rmode_t hamlibMode;
    
    switch (mode)
    {
        case USB:
            hamlibMode = RIG_MODE_USB;
            break;
        case LSB:
            hamlibMode = RIG_MODE_LSB;
            break;
        case DIGU:
            hamlibMode = RIG_MODE_PKTUSB;
            break;
        case DIGL:
            hamlibMode = RIG_MODE_PKTLSB;
            break;
        case FM:
            hamlibMode = RIG_MODE_FM;
            break;
        case DIGFM:
            hamlibMode = RIG_MODE_PKTFM;
            break;
        case AM:
            hamlibMode = RIG_MODE_AM;
            break;
        default:
            onRigError(this, "Cannot set mode: mode not recognized");
            return;
    }
    
    if (rig_ == nullptr || currMode_ == hamlibMode)
    {
        return;
    }

    if (pttSet_)
    {
        // If transmitting, temporarily stop transmitting so we can change the mode.
        int result = rig_set_ptt(rig_, RIG_VFO_CURR, RIG_PTT_OFF);
        if (result != RIG_OK) 
        {
            // If we can't stop transmitting, we shouldn't try to change the mode
            // as it'll fail on some radios.
            if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));
            
            std::string errMsg = std::string("Could not disable PTT prior to mode change: ") + rigerror(result);
            onRigError(this, errMsg);

            return;
        }
    }

    vfo_t currVfo = getCurrentVfo_(); 
    setModeHelper_(currVfo, hamlibMode);

    if (pttSet_)
    {
        // If transmitting, temporarily stop transmitting so we can change the mode.
        int result = rig_set_ptt(rig_, RIG_VFO_CURR, RIG_PTT_ON);
        if (result != RIG_OK) 
        {
            // If we can't stop transmitting, we shouldn't try to change the mode
            // as it'll fail on some radios.
            if (g_verbose) fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(result));
            
            std::string errMsg = std::string("Could not enable PTT after mode change: ") + rigerror(result);
            onRigError(this, errMsg);
        }
    }
}

void HamlibRigController::requestCurrentFrequencyModeImpl_()
{
    if (rig_ == nullptr)
    {
        return;
    }
    
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t passband = 0;
    vfo_t currVfo = getCurrentVfo_();    

modeAttempt:
    int result = rig_get_mode(rig_, currVfo, &mode, &passband);
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
        result = rig_get_freq(rig_, currVfo, &freq);
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
            IRigFrequencyController::Mode currMode;
            
            switch (mode)
            {
                case RIG_MODE_USB:
                    currMode = USB;
                    break;
                case RIG_MODE_LSB:
                    currMode = LSB;
                    break;
                case RIG_MODE_PKTUSB:
                    currMode = DIGU;
                    break;
                case RIG_MODE_PKTLSB:
                    currMode = DIGL;
                    break;
                case RIG_MODE_FM:
                    currMode = FM;
                    break;
                case RIG_MODE_PKTFM:
                    currMode = DIGFM;
                    break;
                case RIG_MODE_AM:
                    currMode = AM;
                    break;
                default:
                    currMode = UNKNOWN;
                    break;
            }
            
            // If this is the first time we're retrieving the current frequency/mode,
            // store it for later restore on disconnect.
            if (origFreq_ == 0)
            {
                origFreq_ = freq;
                origMode_ = mode;
            }
            
            onFreqModeChange(this, freq, currMode);
        }
    }
}

vfo_t HamlibRigController::getCurrentVfo_()
{
    vfo_t vfo = RIG_VFO_CURR;
    
    if (multipleVfos_)
    {
        int result = rig_get_vfo(rig_, &vfo);
        if (result != RIG_OK && result != -RIG_ENAVAIL)
        {
            // Note: we'll still attempt operation using RIG_VFO_CURR just in case.
            if (g_verbose) fprintf(stderr, "rig_get_vfo: error = %s \n", rigerror(result));
            vfo = RIG_VFO_CURR;
        }
    }
    
    return vfo;
}

void HamlibRigController::setFrequencyHelper_(vfo_t currVfo, uint64_t frequencyHz)
{
    bool setOkay = false;

    if (currFreq_ == frequencyHz)
    {
        return;
    }
    
freqAttempt:
    int result = rig_set_freq(rig_, currVfo, frequencyHz);
    if (result != RIG_OK && currVfo == RIG_VFO_CURR)
    {
        if (g_verbose) fprintf(stderr, "rig_set_freq: error = %s \n", rigerror(result));
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
        setOkay = true;
    }
    
    // If we're able to set either frequency or mode, we should update the current state accordingly.
    // The actual configuration of the radio (in case of failure) will auto-update within
    // a few seconds after this update.
    if (setOkay)
    {
        currFreq_ = frequencyHz;
        requestCurrentFrequencyMode();
    }
}

void HamlibRigController::setModeHelper_(vfo_t currVfo, rmode_t mode)
{
    bool setOkay = false;
    
    if (currMode_ == mode)
    {
        return;
    }

modeAttempt:
    int result = rig_set_mode(rig_, currVfo, mode, RIG_PASSBAND_NOCHANGE);
    if (result != RIG_OK && currVfo == RIG_VFO_CURR)
    {
        if (g_verbose) fprintf(stderr, "rig_set_mode: error = %s \n", rigerror(result));
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
        setOkay = true;
    }

    // If we're able to set either frequency or mode, we should update the current state accordingly.
    // The actual configuration of the radio (in case of failure) will auto-update within
    // a few seconds after this update.
    if (setOkay)
    {
        currMode_ = mode;
        requestCurrentFrequencyMode();
    }
}
