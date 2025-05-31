//=========================================================================
// Name:            Win32COMObject.h
// Purpose:         Wrapper to ThreadedObject to guarantee COM initialization
//                  and teardown.
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

#ifndef WIN32_COM_OBJECT_H
#define WIN32_COM_OBJECT_H

#include "ThreadedObject.h"

class Win32COMObject : public ThreadedObject
{
public:
    virtual ~Win32COMObject();

protected:
    Win32COMObject();
    
private:
};

#endif // WIN32_COM_OBJECT_H
