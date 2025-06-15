//==========================================================================
// Name:            git_hash.h
//
// Purpose:         Exports the Git hash of the current version of the source code.
// Created:         June 14, 2025
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef GIT_HASH_H
#define GIT_HASH_H

#include <string>

extern const char* FREEDV_GIT_HASH;
std::string GetFreeDVVersion();

#endif // GIT_HASH_H
