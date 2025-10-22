//=========================================================================
// Name:            GenericFIFO.h
// Purpose:         Generic version of codec2_fifo that can handle any type
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

#ifndef GENERIC_FIFO_H
#define GENERIC_FIFO_H

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <atomic>

template<typename T>
class GenericFIFO
{
public:
    GenericFIFO(int len, T* inBuf = nullptr);
    GenericFIFO(const GenericFIFO<T>& rhs) = delete;
    GenericFIFO(GenericFIFO<T>&& rhs);
    virtual ~GenericFIFO();
    
    int write(T* data, int len) noexcept;
    int read(T* result, int len) noexcept;
    
    int numUsed() const noexcept;
    int numFree() const noexcept;
    int capacity() const noexcept;

    void reset() noexcept;
    
private:
    T* buf;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winterference-size"
    alignas(std::hardware_destructive_interference_size) std::atomic<T*> pin;
    alignas(std::hardware_destructive_interference_size) std::atomic<T*> pout;
#pragma GCC diagnostic pop

    int nelem;
    bool ownBuffer_;
};

template<typename T>
GenericFIFO<T>::GenericFIFO(int len, T* inBuf)
    : buf(inBuf)
    , pin(nullptr)
    , pout(nullptr)
    , nelem(len)
    , ownBuffer_(inBuf == nullptr ? true : false)
{
    if (ownBuffer_)
    {
        buf = new T[nelem];
        assert(buf != nullptr);
    }
    
    pin = buf;
    pout = buf;
}

template<typename T>
GenericFIFO<T>::~GenericFIFO()
{
    if (buf != nullptr && ownBuffer_)
    {
        delete[] buf;
    }
}

template<typename T>
GenericFIFO<T>::GenericFIFO(GenericFIFO<T>&& rhs)
    : buf(rhs.buf)
    , pin(rhs.pin)
    , pout(rhs.pout)
    , nelem(rhs.nelem)
{
    rhs.buf = nullptr;
    rhs.pin = nullptr;
    rhs.pout = nullptr;
    rhs.nelem = 0;
}

template<typename T>
void GenericFIFO<T>::reset() noexcept
{
    pin.store(buf, std::memory_order_release);
    pout.store(buf, std::memory_order_release);
}

template<typename T>
int GenericFIFO<T>::write(T* data, int len) noexcept
{
    assert(data != NULL);

    if (len > numFree()) {
      return -1;
    } else {
        T* ppin = pin.load(std::memory_order_acquire);
        while (len > 0)
        {
            *ppin++ = *data++;
            if (ppin >= (buf + nelem))
            {
                ppin = buf;
            }
            len--;
        }
        pin.store(ppin, std::memory_order_release);
    }

    return 0;
}

template<typename T>
int GenericFIFO<T>::read(T* result, int len) noexcept
{
    assert(result != NULL);

    if (len > numUsed()) {
      return -1;
    } else {
        T* ppout = pout.load(std::memory_order_acquire);
        while (len > 0)
        {
            *result++ = *ppout++;
            if (ppout >= (buf + nelem))
            {
                ppout = buf;
            }
            len--;
        } 
        pout.store(ppout, std::memory_order_release);
    }

    return 0;
}

template<typename T>
int GenericFIFO<T>::numUsed() const noexcept
{
    T *ppin = pin.load(std::memory_order_acquire);
    T *ppout = pout.load(std::memory_order_acquire);
    unsigned int used;

    if (ppin >= ppout)
      used = ppin - ppout;
    else
      used = nelem + (unsigned int)(ppin - ppout);

    return used;
}

template<typename T>
int GenericFIFO<T>::numFree() const noexcept
{
    // available storage is one less than nshort as prd == pwr
    // is reserved for empty rather than full

    return nelem - numUsed() - 1;
}

template<typename T>
int GenericFIFO<T>::capacity() const noexcept
{
    return nelem - 1;
}

template<typename T, int StaticSize>
class PreAllocatedFIFO : public GenericFIFO<T>
{
public:
    PreAllocatedFIFO();
    virtual ~PreAllocatedFIFO() = default;
    
    // No move/copies possible.
    PreAllocatedFIFO(const PreAllocatedFIFO<T, StaticSize>& rhs) = delete;
    PreAllocatedFIFO(PreAllocatedFIFO<T, StaticSize>&& rhs) = delete;
    
private:
    T ourBuf[StaticSize];
};

template<typename T, int StaticSize>
PreAllocatedFIFO<T, StaticSize>::PreAllocatedFIFO()
    : GenericFIFO<T>(StaticSize, ourBuf)
{
    // empty
}

#endif // GENERIC_FIFO_H
