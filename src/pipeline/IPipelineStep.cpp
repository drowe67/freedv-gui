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

#include "IPipelineStep.h"

#if defined(SANITIZER_EMABLED)
#include <cstdlib>

#define codec2_malloc(sz) (malloc(sz))
#define codec2_free(ptr) (free(ptr))
#else
extern "C" {
    #include "debug_alloc.h"
}
#endif // defined(SANITIZER_EMABLED)

IPipelineStep::~IPipelineStep()
{
    // empty
}

void* IPipelineStep::operator new(std::size_t sz)
{
    return codec2_malloc(sz);
}

void* IPipelineStep::operator new[](std::size_t sz)
{
    return codec2_malloc(sz);
}

void IPipelineStep::operator delete(void* ptr) noexcept
{
    codec2_free(ptr);
}

void IPipelineStep::operator delete(void* ptr, std::size_t size) noexcept
{
    codec2_free(ptr);
}

void IPipelineStep::operator delete[](void* ptr) noexcept
{
    codec2_free(ptr);
}

void IPipelineStep::operator delete[](void* ptr, std::size_t size) noexcept
{
    codec2_free(ptr);
}

void* IPipelineStep::AllocRealtimeGeneric_(int size) noexcept
{
    return codec2_malloc(size);
}

void IPipelineStep::FreeRealtimeGeneric_(void* ptr) noexcept
{
    codec2_free(ptr);
}