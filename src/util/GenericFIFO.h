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

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

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

#if !defined(__clang__) && defined(__cpp_lib_hardware_interference_size)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winterference-size"
#endif // !defined(__clang__)
    alignas(hardware_destructive_interference_size) std::atomic<T*> pin;
    T* poutCache;
    std::atomic<bool> resetOutCache;
    alignas(hardware_destructive_interference_size) std::atomic<T*> pout;
    T* pinCache;
    std::atomic<bool> resetInCache;
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif // !defined(__clang__) && defined(__cpp_lib_hardware_interference_size)

    int nelem;
    bool ownBuffer_;

    T* canWrite_(int len) noexcept;
    T* canRead_(int len) noexcept;
};

template<typename T>
GenericFIFO<T>::GenericFIFO(int len, T* inBuf)
    : buf(inBuf)
    , pin(nullptr)
    , poutCache(nullptr)
    , resetOutCache(false)
    , pout(nullptr)
    , pinCache(nullptr)
    , resetInCache(false)
    , nelem(len)
    , ownBuffer_(inBuf == nullptr ? true : false)
{
    if (ownBuffer_)
    {
        buf = new T[nelem];
        assert(buf != nullptr);
    }
    
    pin = buf;
    poutCache = buf;
    pout = buf;
    pinCache = buf;
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
    , poutCache(rhs.poutCache)
    , pout(rhs.pout)
    , pinCache(rhs.pinCache)
    , nelem(rhs.nelem)
{
    rhs.buf = nullptr;
    rhs.pin = nullptr;
    rhs.poutCache = nullptr;
    rhs.pout = nullptr;
    rhs.pinCache = nullptr;
    rhs.nelem = 0;
}

template<typename T>
void GenericFIFO<T>::reset() noexcept
{
    pin.store(buf, std::memory_order_release);
    resetOutCache.store(true, std::memory_order_release);
    pout.store(buf, std::memory_order_release);
    resetInCache.store(true, std::memory_order_release);
}

#define CALCULATE_ENTRIES_USED(in, out, used)     \
    if (in >= out)                                \
    {                                             \
        used = in - out;                          \
    }                                             \
    else                                          \
    {                                             \
        used = nelem + (unsigned int)(in - out);  \
    }

template<typename T>
T* GenericFIFO<T>::canWrite_(int len) noexcept
{
    T *ppin = pin.load(std::memory_order_acquire);
    unsigned int used;

    bool isResetting = resetOutCache.load(std::memory_order_relaxed);
    if (!isResetting)
    {
        CALCULATE_ENTRIES_USED(ppin, poutCache, used);
    }
    else
    {
        resetOutCache.store(false, std::memory_order_release);
    }
    if (isResetting || (nelem - used - 1) < (unsigned)len)
    {
        // reload poutCache and test again
        poutCache = pout.load(std::memory_order_acquire);
        CALCULATE_ENTRIES_USED(ppin, poutCache, used);
        if ((nelem - used - 1) < (unsigned)len)
        {
            return nullptr;
        }
    }

    return ppin;
}

template<typename T>
T* GenericFIFO<T>::canRead_(int len) noexcept
{
    T* ppout = pout.load(std::memory_order_acquire);
    unsigned int used;

    bool isResetting = resetInCache.load(std::memory_order_relaxed);
    if (!isResetting)
    {
        CALCULATE_ENTRIES_USED(pinCache, ppout, used);
    }
    else
    {
        resetInCache.store(false, std::memory_order_release);
    }

    if (isResetting || used < (unsigned)len)
    {
        pinCache = pin.load(std::memory_order_acquire);
        CALCULATE_ENTRIES_USED(pinCache, ppout, used);
        if (used < (unsigned)len)
        {
            return nullptr;
        }
    }

    return ppout;
}

template<typename T>
int GenericFIFO<T>::write(T* data, int len) noexcept
{
    assert(data != NULL);

    T* ppin = canWrite_(len);
    if (ppin == nullptr)
    {
        return -1;
    } 
    else 
    {
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

    T* ppout = canRead_(len);
    if (ppout == nullptr)
    {
        return -1;
    } 
    else 
    {
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
