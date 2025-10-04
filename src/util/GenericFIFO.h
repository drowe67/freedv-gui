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
    std::atomic<T*> pin;
    std::atomic<T*> pout;
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
    pin = buf;
    pout = buf;
}

template<typename T>
int GenericFIFO<T>::write(T* data, int len) noexcept
{
    assert(data != NULL);

    if (len > numFree()) {
      return -1;
    } else {
        auto copyLength = std::min(len, (int)((buf + nelem) - pin));
        memcpy(pin, data, copyLength * sizeof(T));
        len -= copyLength;
        if (len > 0)
        {
            memcpy(buf, data + copyLength, len * sizeof(T));
            pin = buf + len;
        }
        else
        {
            pin += copyLength;
        }
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
        auto copyLength = std::min(len, (int)((buf + nelem) - pout));
        memcpy(result, pout, copyLength * sizeof(T));
        len -= copyLength;
        if (len > 0)
        {
            memcpy(result + copyLength, buf, len * sizeof(T));
            pout = buf + len;
        }
        else
        {
            pout += copyLength;
        }
    }

    return 0;
}

template<typename T>
int GenericFIFO<T>::numUsed() const noexcept
{
    T *ppin = pin;
    T *ppout = pout;
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
