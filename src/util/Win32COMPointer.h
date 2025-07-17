//=========================================================================
// Name:            Win32COMPointer.h
// Purpose:         Smart pointer implementation for COM objects.
//                  Original implementation at https://learn.microsoft.com/en-us/archive/msdn-magazine/2015/february/windows-with-c-com-smart-pointers-revisited.
//
// Authors:         Kenny Kerr with modifications by Mooneer Salem
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

#ifndef WIN32_COM_POINTER_H
#define WIN32_COM_POINTER_H

template <typename Interface>
class ComPtr
{
public:
    // Hide AddRef and Release to prevent overriding ComPtr counting
    class RemoveAddRefRelease : public Interface
    {
        ULONG __stdcall AddRef();
        ULONG __stdcall Release();
    };
    
    ComPtr() noexcept = default;
    ComPtr(ComPtr const & other) noexcept :
      m_ptr(other.m_ptr)
    {
      InternalAddRef();
    }
    
    ComPtr(ComPtr&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }
    
    template <typename T>
    ComPtr(ComPtr<T> const & other) noexcept :
      m_ptr(other.m_ptr)
    {
      InternalAddRef();
    }
    
    template <typename T>
    ComPtr(ComPtr<T> && other) noexcept :
      m_ptr(other.m_ptr)
    {
      other.m_ptr = nullptr;
    }
    
    ~ComPtr() noexcept
    {
      InternalRelease();
    }
  
    RemoveAddRefRelease<Interface> * operator->() const noexcept
    {
        return static_cast<RemoveAddRefRelease<Interface> *>(m_ptr);
    }
    
    ComPtr & operator=(ComPtr const & other) noexcept
    {
      InternalCopy(other.m_ptr);
      return *this;
    }
    
    template <typename T>
    ComPtr & operator=(ComPtr<T> const & other) noexcept
    {
      InternalCopy(other.m_ptr);
      return *this;
    }
    
    template <typename T>
    ComPtr & operator=(ComPtr<T> && other) noexcept
    {
      InternalMove(other);
      return *this;
    }
    
    void Swap(ComPtr & other) noexcept
    {
      Interface * temp = m_ptr;
      m_ptr = other.m_ptr;
      other.m_ptr = temp;
    }
    
    template <typename Interface>
    void swap(ComPtr<Interface> & left, 
      ComPtr<Interface> & right) noexcept
    {
      left.Swap(right);
    }
    
    explicit operator bool() const noexcept
    {
      return nullptr != m_ptr;
    }
    
    void Reset() noexcept
    {
      InternalRelease();
    }
    
    Interface * Get() const noexcept
    {
      return m_ptr;
    }
    
    Interface * Detach() noexcept
    {
      Interface * temp = m_ptr;
      m_ptr = nullptr;
      return temp;
    }
    
    void Copy(Interface * other) noexcept
    {
      InternalCopy(other);
    }
    
    void Attach(Interface * other) noexcept
    {
      InternalRelease();
      m_ptr = other;
    }
    
    Interface ** GetAddressOf() noexcept
    {
      ASSERT(m_ptr == nullptr);
      return &m_ptr;
    }
    
    void CopyTo(Interface ** other) const noexcept
    {
      InternalAddRef();
      *other = m_ptr;
    }
    
    template <typename T>
    ComPtr<T> As() const noexcept
    {
      ComPtr<T> temp;
      m_ptr->QueryInterface(temp.GetAddressOf());
      return temp;
    }
  
private:
  Interface * m_ptr = nullptr;
  
  void InternalAddRef() const noexcept
  {
    if (m_ptr)
    {
      m_ptr->AddRef();
    }
  }
  
  void InternalRelease() noexcept
  {
    Interface * temp = m_ptr;
    if (temp)
    {
      m_ptr = nullptr;
      temp->Release();
    }
  }
  
  void InternalCopy(Interface * other) noexcept
  {
    if (m_ptr != other)
    {
      InternalRelease();
      m_ptr = other;
      InternalAddRef();
    }
  }
  
  template <typename T>
  void InternalMove(ComPtr<T> & other) noexcept
  {
    if (m_ptr != other.m_ptr)
    {
      InternalRelease();
      m_ptr = other.m_ptr;
      other.m_ptr = nullptr;
    }
  }
};

#endif // WIN32_COM_POINTER_H