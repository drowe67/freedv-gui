//=========================================================================
// Name:            HamlibRigController.h
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

#ifndef HAMLIB_RIG_CONTROLLER_H
#define HAMLIB_RIG_CONTROLLER_H

#include <string>
#include <vector>
#include <mutex>

#include "util/ThreadedObject.h"
#include "IRigFrequencyController.h"
#include "IRigPttController.h"

extern "C" 
{
    #include <hamlib/rig.h>
}

class HamlibRigController : public ThreadedObject, public IRigFrequencyController, public IRigPttController
{
public:
    enum PttType 
    {
        PTT_VIA_CAT = 0,
        PTT_VIA_RTS,
        PTT_VIA_DTR,
        PTT_VIA_NONE,
    };
    
    HamlibRigController(std::string rigName, std::string serialPort, const int serialRate, const int civHex = 0, const PttType pttType = PTT_VIA_CAT, std::string pttSerialPort = std::string(), bool restoreFreqModeOnDisconnect = false, bool freqOnly = false);
    HamlibRigController(int rigIndex, std::string serialPort, const int serialRate, const int civHex = 0, const PttType pttType = PTT_VIA_CAT, std::string pttSerialPort = std::string(), bool restoreFreqModeOnDisconnect = false, bool freqOnly = false);
    virtual ~HamlibRigController();
    
    virtual void connect() override;
    virtual void disconnect() override;
    virtual bool isConnected() override;
    virtual void ptt(bool state) override;
    virtual void setFrequency(uint64_t frequency) override;
    virtual void setMode(IRigFrequencyController::Mode mode) override;
    virtual void requestCurrentFrequencyMode() override;

    static void InitializeHamlibLibrary();
    static int RigNameToIndex(std::string rigName);
    static std::string RigIndexToName(unsigned int rigIndex);
    static int GetNumberSupportedRadios();

private:
    using RigList = std::vector<const struct rig_caps *>;
    
    std::string rigName_;
    std::string serialPort_;
    const int serialRate_;
    const int civHex_;
    const PttType pttType_;
    std::string pttSerialPort_;
    
    RIG* rig_;
    bool multipleVfos_;
    bool pttSet_;
    uint64_t currFreq_;
    rmode_t currMode_;
    bool restoreOnDisconnect_;
    uint64_t origFreq_;
    rmode_t origMode_;
    bool freqOnly_;
    
    vfo_t getCurrentVfo_();
    void setFrequencyHelper_(vfo_t currVfo, uint64_t frequencyHz);
    void setModeHelper_(vfo_t currVfo, rmode_t mode);
    
    void connectImpl_();
    void disconnectImpl_();
    void pttImpl_(bool state);
    void setFrequencyImpl_(uint64_t frequencyHz);
    void setModeImpl_(IRigFrequencyController::Mode mode);
    void requestCurrentFrequencyModeImpl_();
    
    static RigList RigList_;
    static std::mutex RigListMutex_;

    static bool RigCompare_(const struct rig_caps *rig1, const struct rig_caps *rig2);

#if RIGCAPS_NOT_CONST    
    static int BuildRigList_(struct rig_caps *rig, rig_ptr_t);
#else
    static int BuildRigList_(const struct rig_caps *rig, rig_ptr_t);
#endif // RIGCAPS_NOT_CONST
};

#endif // HAMLIB_RIG_CONTROLLER_H