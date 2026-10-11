#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <wx/defs.h>
#include <wx/regex.h>

#ifdef __WXMSW__
#include <wx/msw/registry.h>
#else
#include <glob.h>
#endif

#include "SerialPortList.h"

bool SerialPortNameLess(const wxString& first, const wxString& second)
{
    wxRegEx portRegex("^([^0-9]+)([0-9]+)$");
    wxString firstName = "";
    wxString firstNumber = "";
    wxString secondName = "";
    wxString secondNumber = "";
    int firstNumAsInt = 0;
    int secondNumAsInt = 0;
    
    if (portRegex.Matches(first))
    {
        firstName = portRegex.GetMatch(first, 1);
        firstNumber = portRegex.GetMatch(first, 2);
        firstNumAsInt = atoi(firstNumber.c_str());
    }
    else
    {
        firstName = first;
    }
    
    if (portRegex.Matches(second))
    {
        secondName = portRegex.GetMatch(second, 1);
        secondNumber = portRegex.GetMatch(second, 2);
        secondNumAsInt = atoi(secondNumber.c_str());
    }
    else
    {
        secondName = second;
    }
    
    return 
        (firstName < secondName) || 
        (firstName == secondName && firstNumAsInt < secondNumAsInt);
}

void SortSerialPortNames(std::vector<wxString>& portList)
{
    std::sort(portList.begin(), portList.end(), SerialPortNameLess);
}

#if defined(__FreeBSD__) || defined(__WXOSX__)
bool IsListableSerialDevice(const char* path)
{
    if(path[strlen(path)-1]=='/')
        return false;

#if defined(__FreeBSD__)
    /* Exclude pseudo TTYs */
    if(path[8] >= 'l' && path[8] <= 's')
        return false;
    if(path[8] >= 'L' && path[8] <= 'S')
        return false;

    /* Exclude virtual TTYs */
    if(path[8] == 'v')
        return false;
#else
    if (!strcmp("/dev/cu.wlan-debug", path))
        return false;
#endif // defined(__FreeBSD__)

    /* Exclude initial-state and lock-state devices */
#ifndef __WXOSX__
    if(strchr(path, '.') != NULL)
        return false;
#endif

    return true;
}
#endif // defined(__FreeBSD__) || defined(__WXOSX__)

std::vector<wxString> GetSerialPortList()
{
    std::vector<wxString> portList;
    
#ifdef __WXMSW__
    wxRegKey key(wxRegKey::HKLM, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"));
    if(!key.Exists())
    {
        return portList;
    }
    else
    {
        // Get the number of subkeys and enumerate them.
        if(!key.Open(wxRegKey::Read))
        {
            return portList;
        }    
        size_t subkeys;
        size_t values;
        if(!key.GetKeyInfo(&subkeys, NULL, &values, NULL))
        {
            return portList;
        }
        if(!key.HasValues())
        {
            return portList;
        }
        wxString key_name;
        long el = 1;
        key.GetFirstValue(key_name, el);
        wxString key_data;
        for(unsigned int i = 0; i < values; i++)
        {
            key.QueryValue(key_name, key_data);
            //wxPrintf("Value:  %s Data: %s\n", key_name, key_data);
            portList.push_back(key_data);
            key.GetNextValue(key_name, el);
        }
    }
#endif
#if defined(__WXGTK__) || defined(__WXOSX__)

#if defined(__FreeBSD__) || defined(__WXOSX__)
    glob_t    gl;
#if defined(__FreeBSD__)
    if(glob("/dev/tty*", GLOB_MARK, NULL, &gl)==0 ||
#else
    if(glob("/dev/tty.*", GLOB_MARK, NULL, &gl)==0 || // NOLINT
#endif // defined(__FreeBSD__)
       glob("/dev/cu.*", GLOB_MARK, NULL, &gl)==0) { // NOLINT
        for(unsigned int i=0; i<gl.gl_pathc; i++) {
            if (!IsListableSerialDevice(gl.gl_pathv[i]))
                continue;

            portList.push_back(gl.gl_pathv[i]);
        }
        globfree(&gl);
    }
#else
    glob_t    gl;
    if(glob("/sys/class/tty/*/device/driver", GLOB_MARK, NULL, &gl)==0)  // NOLINT
    {
        wxRegEx pathRegex("/sys/class/tty/([^/]+)");
        for(unsigned int i=0; i<gl.gl_pathc; i++) 
        {
            wxString path = gl.gl_pathv[i];
            if (pathRegex.Matches(path))
            {
                wxString name = "/dev/" + pathRegex.GetMatch(path, 1);
                portList.push_back(name);
            }
        }
        globfree(&gl);
    }

    // Support /dev/serial as well
    if (glob("/dev/serial/by-id/*", GLOB_MARK, NULL, &gl) == 0) // NOLINT
    {
        for(unsigned int i=0; i<gl.gl_pathc; i++)   
        {
            wxString path = gl.gl_pathv[i];
            portList.push_back(path);
        }
        globfree(&gl);
    }

    // Support /dev/rfcomm as well
    // linux usb over bluetooth
    if (glob("/dev/rfcomm*", GLOB_MARK, NULL, &gl) == 0) // NOLINT
    {
        for(unsigned int i=0; i<gl.gl_pathc; i++)
        {
            wxString path = gl.gl_pathv[i];
            portList.push_back(path);
        }
        globfree(&gl);
    }
#endif
#endif

    // Sort the list such that the port number is in numeric (not text) order.
    SortSerialPortNames(portList);

    return portList;
}
