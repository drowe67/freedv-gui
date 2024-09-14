//=========================================================================
// Name:            IPipelineStep.cpp
// Purpose:         Describes a step in the audio pipeline.
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

#include <iostream>
#include "IPipelineStep.h"

IPipelineStep::~IPipelineStep()
{
    // empty
}

void IPipelineStep::dump(int indentLevel)
{
    std::cout << typeid(*this).name() << "[inputSR = " << getInputSampleRate() << ", outputSR = " << getOutputSampleRate() << "]" << std::endl;
}
