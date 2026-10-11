#ifndef SERIAL_PORT_LIST_H
#define SERIAL_PORT_LIST_H

#include <vector>

#include <wx/defs.h>
#include <wx/string.h>

// Returns the serial ports available on this machine, in the order they
// should be offered to the user.
std::vector<wxString> GetSerialPortList();

// Orders serial port names so that those differing only in a trailing
// number are in numeric (not text) order, e.g. COM9 before COM10.
bool SerialPortNameLess(const wxString& first, const wxString& second);

// Sorts the given list of serial port names using SerialPortNameLess().
void SortSerialPortNames(std::vector<wxString>& portList);

#if defined(__FreeBSD__) || defined(__WXOSX__)
// Returns true if the given path under /dev is a serial device that
// should be offered to the user (as opposed to e.g. a pseudo-terminal).
bool IsListableSerialDevice(const char* path);
#endif // defined(__FreeBSD__) || defined(__WXOSX__)

#endif // SERIAL_PORT_LIST_H
