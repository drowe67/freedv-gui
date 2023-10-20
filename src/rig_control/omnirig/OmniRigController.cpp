//=========================================================================
// Name:            OmniRigController.cpp
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

#include <cassert>
#include "OmniRigController.h"

OmniRigController::OmniRigController(int rigId)
    : rigId_(rigId)
    , omniRig_(nullptr)
    , rig_(nullptr)
{
    // empty
}

OmniRigController::~OmniRigController()
{
    disconnect();
}

void OmniRigController::connect()
{
    enqueue_(std::bind(&OmniRigController::connectImpl_, this));
}

void OmniRigController::disconnect()
{
    enqueue_(std::bind(&OmniRigController::disconnectImpl_, this));
}

bool OmniRigController::isConnected()
{
    return rig_ != nullptr;
}

void OmniRigController::ptt(bool state)
{
    enqueue_(std::bind(&OmniRigController::pttImpl_, this, state));
}

void OmniRigController::setFrequency(uint64_t frequency)
{
    enqueue_(std::bind(&OmniRigController::setFrequencyImpl_, this, frequency));
}

void OmniRigController::setMode(IRigFrequencyController::Mode mode)
{
    enqueue_(std::bind(&OmniRigController::setModeImpl_, this, mode));
}

void OmniRigController::requestCurrentFrequencyMode()
{
    enqueue_(std::bind(&OmniRigController::requestCurrentFrequencyModeImpl_, this));
}

void OmniRigController::connectImpl_()
{
    // Ensure that COM is properly initialized.
    CoInitializeEx (nullptr, 0 /*COINIT_APARTMENTTHREADED*/);

    // Attempt to create OmniRig COM object.
    auto rv = CoCreateInstance(
        CLSID_OmniRigX,
        NULL,
        CLSCTX_LOCAL_SERVER,
        IID_IOmniRigX,
        (LPVOID*)&omniRig_);
    if (rv == S_OK)
    {
        if (rigId_ == 0)
        {
            omniRig_->get_Rig1(&rig_);
        } 
        else if (rigId_ == 1)
        {
            omniRig_->get_Rig2(&rig_);
        }
        else
        {
            assert(0);
        }

        onRigConnected(this);

        // Get current frequency and mode when we first connect so we can
        // revert on close.
        requestCurrentFrequencyModeImpl_();
    }
    else
    {
        omniRig_ = nullptr;
        onRigError(this, "Could not create COM object for OmniRig.");
    }
}

void OmniRigController::disconnectImpl_()
{
    if (omniRig_ != nullptr)
    {
        omniRig_->Release();
        omniRig_ = nullptr;
        rig_ = nullptr;

        onRigDisconnected(this);
    }

    // Uninitialize COM.
    CoUninitialize ();
}

void OmniRigController::pttImpl_(bool state)
{
    if (rig_ != nullptr)
    {
        rig_->put_Tx(state ? PM_TX : PM_RX);
        onPttChange(this, state);
    }
}

void OmniRigController::setFrequencyImpl_(uint64_t frequencyHz)
{
    if (rig_ != nullptr)
    {
        rig_->put_Freq(frequencyHz);
        requestCurrentFrequencyMode();
    }
}

void OmniRigController::setModeImpl_(IRigFrequencyController::Mode mode)
{
    RigParamX omniRigMode;

    switch(mode)
    {
        case IRigFrequencyController::USB:
            omniRigMode = PM_SSB_U;
            break;
        case IRigFrequencyController::DIGU:
            omniRigMode = PM_DIG_U;
            break;
        case IRigFrequencyController::LSB:
            omniRigMode = PM_SSB_L;
            break;
        case IRigFrequencyController::DIGL:
            omniRigMode = PM_DIG_L;
            break;
        case IRigFrequencyController::FM:
        case IRigFrequencyController::DIGFM:
            omniRigMode = PM_FM;
            break;
        case IRigFrequencyController::AM:
            omniRigMode = PM_AM;
            break;
        default:
            onRigError(this, "Unknown mode passed into OmniRig.");
            return;
    }

    if (rig_ != nullptr)
    {
        rig_->put_Mode(omniRigMode);
        requestCurrentFrequencyMode();
    }
}

void OmniRigController::requestCurrentFrequencyModeImpl_()
{
    RigParamX omniRigMode;
    long freq;

    if (rig_ != nullptr)
    {
        rig_->get_Freq(&freq);
        rig_->get_Mode(&omniRigMode);

        // Convert OmniRig mode to our mode
        IRigFrequencyController::Mode mode;
        switch (omniRigMode)
        {
            case PM_DIG_U:
                mode = IRigFrequencyController::DIGU;
                break;
            case PM_DIG_L:
                mode = IRigFrequencyController::DIGL;
                break;
            case PM_SSB_U:
                mode = IRigFrequencyController::USB;
                break;
            case PM_SSB_L:
                mode = IRigFrequencyController::LSB;
                break;
            case PM_AM:
                mode = IRigFrequencyController::AM;
                break;
            case PM_FM:
                mode = IRigFrequencyController::FM;
                break;
            default:
                mode = IRigFrequencyController::UNKNOWN;
                break;
        }

        // Publish frequency/mode to subscribers
        onFreqModeChange(this, freq, mode);
    }
}
