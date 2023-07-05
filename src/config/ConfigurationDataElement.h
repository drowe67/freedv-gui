//==========================================================================
// Name:            ConfigurationDataElement.h
// Purpose:         Implements the interface for a data item in the config.
// Created:         July 1, 2023
// Authors:         Mooneer Salem
// 
// License:
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
//==========================================================================

#ifndef CONFIGURATION_DATA_ELEMENT_H
#define CONFIGURATION_DATA_ELEMENT_H

#include <functional>

template<typename UnderlyingDataType>
class ConfigurationDataElement
{
public:
    ConfigurationDataElement(const char* elementName, UnderlyingDataType defaultVal);
    virtual ~ConfigurationDataElement() = default;
    
    const char* getElementName() const;
    
    void setDefaultVal(UnderlyingDataType& val); // useful for migration
    UnderlyingDataType& getDefaultVal();
    
    // Allows usage of class as though it were defined directly as UnderlyingDataType.
    ConfigurationDataElement<UnderlyingDataType>& operator=(UnderlyingDataType val);
    operator UnderlyingDataType();
    
    UnderlyingDataType* operator->();
    
    UnderlyingDataType get();
    UnderlyingDataType getWithoutProcessing();
    void setWithoutProcessing(UnderlyingDataType val);
    
    void setSaveProcessor(std::function<UnderlyingDataType(UnderlyingDataType)> fn);
    void setLoadProcessor(std::function<UnderlyingDataType(UnderlyingDataType)> fn);
private:
    const char* elementName_;
    UnderlyingDataType data_;
    UnderlyingDataType default_;
    
    std::function<UnderlyingDataType(UnderlyingDataType)> saveProcessor_;
    std::function<UnderlyingDataType(UnderlyingDataType)> loadProcessor_;
};

template<typename UnderlyingDataType>
ConfigurationDataElement<UnderlyingDataType>::ConfigurationDataElement(const char* elementName, UnderlyingDataType defaultVal)
    : elementName_(elementName)
    , data_(defaultVal)
    , default_(defaultVal)
{
    // empty
}

template<typename UnderlyingDataType>
const char* ConfigurationDataElement<UnderlyingDataType>::getElementName() const
{
    return elementName_;
}

template<typename UnderlyingDataType>
UnderlyingDataType& ConfigurationDataElement<UnderlyingDataType>::getDefaultVal()
{
    return default_;
}

template<typename UnderlyingDataType>
void ConfigurationDataElement<UnderlyingDataType>::setDefaultVal(UnderlyingDataType& val)
{
    default_ = val;
}

template<typename UnderlyingDataType>
ConfigurationDataElement<UnderlyingDataType>& ConfigurationDataElement<UnderlyingDataType>::operator=(UnderlyingDataType val)
{
    if (saveProcessor_)
    {
        val = saveProcessor_(val);
    }
    
    setWithoutProcessing(val);
    return *this;
}

template<typename UnderlyingDataType>
UnderlyingDataType* ConfigurationDataElement<UnderlyingDataType>::operator->()
{
    return &data_;
}

template<typename UnderlyingDataType>
void ConfigurationDataElement<UnderlyingDataType>::setWithoutProcessing(UnderlyingDataType val)
{
    data_ = val;
}

template<typename UnderlyingDataType>
UnderlyingDataType ConfigurationDataElement<UnderlyingDataType>::getWithoutProcessing()
{
    return data_;
}

template<typename UnderlyingDataType>
ConfigurationDataElement<UnderlyingDataType>::operator UnderlyingDataType()
{
    if (loadProcessor_)
    {
        return loadProcessor_(data_);
    }
    
    return data_;
}

template<typename UnderlyingDataType>
UnderlyingDataType ConfigurationDataElement<UnderlyingDataType>::get()
{
    if (loadProcessor_)
    {
        return loadProcessor_(data_);
    }
    
    return data_;
}

template<typename UnderlyingDataType>
void ConfigurationDataElement<UnderlyingDataType>::setSaveProcessor(std::function<UnderlyingDataType(UnderlyingDataType)> fn)
{
    saveProcessor_ = fn;
}

template<typename UnderlyingDataType>
void ConfigurationDataElement<UnderlyingDataType>::setLoadProcessor(std::function<UnderlyingDataType(UnderlyingDataType)> fn)
{
    loadProcessor_ = fn;
}

#endif // CONFIGURATION_DATA_ELEMENT_H