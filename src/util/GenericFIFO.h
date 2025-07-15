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

template<typename T>
class GenericFIFO
{
public:
    GenericFIFO(int len);
    GenericFIFO(const GenericFIFO<T>& rhs);
    GenericFIFO(GenericFIFO<T>&& rhs);
    virtual ~GenericFIFO();
    
    int write(T* data, int len) noexcept;
    int read(T* result, int len) noexcept;
    
    int numUsed() const noexcept;
    int numFree() const noexcept;
    
private:
    T *buf;
    T *pin;
    T *pout;
    int nelem;
};

template<typename T>
GenericFIFO<T>::GenericFIFO(int len)
    : buf(nullptr)
    , pin(nullptr)
    , pout(nullptr)
    , nelem(len)
{
    buf = new T[nelem];
    assert(buf != nullptr);
    
    pin = buf;
    pout = buf;
}

template<typename T>
GenericFIFO<T>::~GenericFIFO()
{
    if (buf != nullptr)
    {
        delete[] buf;
    }
}

template<typename T>
GenericFIFO<T>::GenericFIFO(const GenericFIFO<T>& rhs)
    : buf(nullptr)
    , pin(nullptr)
    , pout(nullptr)
    , nelem(rhs.nelem)
{
    buf = new T[nelem];
    assert(buf != nullptr);
    
    memcpy(buf, rhs.buf, sizeof(T) * nelem);
    pin = buf + (rhs.pin - rhs.buf);
    pout = buf + (rhs.pout - rhs.buf);
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
int GenericFIFO<T>::write(T* data, int len) noexcept
{
    int i;
    T *pdata;
    T *ppin = pin;

    assert(data != NULL);

    if (len > numFree()) {
      return -1;
    } else {
      /* This could be made more efficient with block copies
         using memcpy */

      pdata = data;
      for (i = 0; i < len; i++) {
        *ppin++ = *pdata++;
        if (ppin == (buf + nelem)) ppin = buf;
      }
      pin = ppin;
    }

    return 0;
}

template<typename T>
int GenericFIFO<T>::read(T* result, int len) noexcept
{
    int i;
    T *pdata;
    T *ppout = pout;

    assert(result != NULL);

    if (len > numUsed()) {
      return -1;
    } else {
      /* This could be made more efficient with block copies
         using memcpy */

      pdata = result;
      for (i = 0; i < len; i++) {
        *pdata++ = *ppout++;
        if (ppout == (buf + nelem)) ppout = buf;
      }
      pout = ppout;
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

#endif // GENERIC_FIFO_H