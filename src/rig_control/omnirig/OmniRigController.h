//=========================================================================
// Name:            OmniRigController.h
// Purpose:         Interface for control via OmniRig.
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

#ifndef OMNIRIG_CONTROLLER_H
#define OMNIRIG_CONTROLLER_H

#include "util/ThreadedObject.h"
#include "../IRigFrequencyController.h"
#include "../IRigPttController.h"
#include "omnirig.h"

class OmniRigController : public ThreadedObject, public IRigFrequencyController, public IRigPttController
{
public:
    OmniRigController(int rigId, bool restoreFreqModeOnDisconnect = false, bool freqOnly = false);
    virtual ~OmniRigController();

    virtual void connect() override;
    virtual void disconnect() override;
    virtual bool isConnected() override;
    virtual void ptt(bool state) override;
    virtual void setFrequency(uint64_t frequency) override;
    virtual void setMode(IRigFrequencyController::Mode mode) override;
    virtual void requestCurrentFrequencyMode() override;
    
    virtual int getRigResponseTimeMicroseconds() override;

private:
    int rigId_; // can be either 0 or 1 (Rig 1 or 2)
    IOmniRigX* omniRig_;
    IRigX* rig_;
    uint64_t origFreq_;
    RigParamX origMode_;
    uint64_t currFreq_;
    IRigFrequencyController::Mode currMode_;
    bool restoreOnDisconnect_;
    long writableParams_; // used to help determine VFO
    bool freqOnly_;
    int rigResponseTime_;

    void connectImpl_();
    void disconnectImpl_();
    void pttImpl_(bool state);
    void setFrequencyImpl_(uint64_t frequencyHz);
    void setModeImpl_(IRigFrequencyController::Mode mode);
    void requestCurrentFrequencyModeImpl_();
};

#endif // OMNIRIG_CONTROLLER_H
