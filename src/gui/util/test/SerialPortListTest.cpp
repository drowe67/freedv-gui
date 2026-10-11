#include <algorithm>
#include <vector>

#include <wx/init.h>

#include "../SerialPortList.h"
#include "../../../test/UnitTestCommon.h"

// The serial port lists in the PTT and Easy Setup windows are sorted so
// that ports whose names differ only by number appear in numeric order.

namespace {

using PortList = std::vector<wxString>;

bool sortsTo(PortList ports, PortList const& expected)
{
    SortSerialPortNames(ports);
    if (ports == expected) return true;

    std::cout << "[got";
    for (auto& port : ports) std::cout << " " << port.ToStdString();
    std::cout << ", expected";
    for (auto& port : expected) std::cout << " " << port.ToStdString();
    std::cout << "] ";
    return false;
}

bool numberedPortsSortNumerically()
{
    bool result = true;
    result &= sortsTo({ "COM10", "COM9", "COM1", "COM100", "COM2" }, { "COM1", "COM2", "COM9", "COM10", "COM100" });
    result &= sortsTo({ "/dev/ttyUSB10", "/dev/ttyUSB2", "/dev/ttyUSB0" }, { "/dev/ttyUSB0", "/dev/ttyUSB2", "/dev/ttyUSB10" });
    result &= sortsTo({ "/dev/rfcomm12", "/dev/rfcomm3" }, { "/dev/rfcomm3", "/dev/rfcomm12" });
    return result;
}

bool differentKindsOfPortAreGroupedByName()
{
    return sortsTo(
        { "/dev/ttyUSB1", "/dev/ttyS10", "/dev/ttyACM0", "/dev/ttyS2", "/dev/ttyUSB0", "/dev/ttyACM1" },
        { "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyS2", "/dev/ttyS10", "/dev/ttyUSB0", "/dev/ttyUSB1" });
}

bool portsWithoutTrailingNumberSortAsText()
{
    bool result = true;
    result &= sortsTo(
        { "/dev/cu.usbserial-B", "/dev/cu.Bluetooth-Incoming-Port", "/dev/cu.usbserial-A" },
        { "/dev/cu.Bluetooth-Incoming-Port", "/dev/cu.usbserial-A", "/dev/cu.usbserial-B" });

    // A number elsewhere in the name doesn't count; these compare as text,
    // where "10" comes before "9".
    result &= sortsTo(
        { "/dev/serial/by-id/usb-9-port", "/dev/serial/by-id/usb-10-port" },
        { "/dev/serial/by-id/usb-10-port", "/dev/serial/by-id/usb-9-port" });
    return result;
}

bool portWithoutNumberComesBeforeSameNameWithOne()
{
    // (It counts as number zero, so only higher numbers are certain to follow it.)
    return sortsTo({ "/dev/ttyAMA2", "/dev/ttyAMA", "/dev/ttyAMA1" }, { "/dev/ttyAMA", "/dev/ttyAMA1", "/dev/ttyAMA2" });
}

bool macSerialNumbersSortNumerically()
{
    // As named by macOS for USB serial adapters.
    return sortsTo(
        { "/dev/cu.usbmodem14201", "/dev/cu.usbmodem1101", "/dev/cu.usbmodem14101" },
        { "/dev/cu.usbmodem1101", "/dev/cu.usbmodem14101", "/dev/cu.usbmodem14201" });
}

bool emptyAndSingleEntryListsAreLeftAlone()
{
    bool result = true;
    result &= sortsTo({ }, { });
    result &= sortsTo({ "COM3" }, { "COM3" });
    return result;
}

bool noPortSortsBeforeItself()
{
    // std::sort requires this of the comparison it's given.
    bool result = true;
    for (const char* port : { "COM1", "/dev/ttyUSB0", "/dev/cu.usbserial-A", "" })
    {
        if (SerialPortNameLess(port, port))
        {
            std::cout << "[\"" << port << "\" sorts before itself] ";
            result = false;
        }
    }
    return result;
}

bool orderingIsConsistentInBothDirections()
{
    PortList ports = { "COM1", "COM2", "COM10", "/dev/ttyS0", "/dev/ttyUSB0", "/dev/ttyUSB10", "/dev/cu.usbserial-A", "/dev/ttyAMA" };

    bool result = true;
    for (auto& first : ports)
    {
        for (auto& second : ports)
        {
            if (first != second && SerialPortNameLess(first, second) == SerialPortNameLess(second, first))
            {
                std::cout << "[" << first.ToStdString() << " and " << second.ToStdString() << " have no definite order] ";
                result = false;
            }
        }
    }
    return result;
}

#if defined(__FreeBSD__) || defined(__WXOSX__)
bool listable(const char* path, bool expected)
{
    if (IsListableSerialDevice(path) == expected) return true;
    std::cout << "[" << path << " was " << (expected ? "left out" : "listed") << "] ";
    return false;
}

bool leavesOutDevicesThatAreNotSerialPorts()
{
    bool result = true;

    // Directories are marked with a trailing slash when looking through /dev.
    result &= listable("/dev/cu.something/", false);

#if defined(__FreeBSD__)
    result &= listable("/dev/ttyu0", true);
    result &= listable("/dev/ttyU0", true);
    result &= listable("/dev/ttyp0", false);  // pseudo-terminal
    result &= listable("/dev/ttyP0", false);
    result &= listable("/dev/ttyv0", false);  // virtual console
    result &= listable("/dev/ttyu0.init", false);
    result &= listable("/dev/ttyu0.lock", false);
#else
    result &= listable("/dev/cu.usbserial-1420", true);
    result &= listable("/dev/tty.usbmodem14201", true);
    result &= listable("/dev/cu.Bluetooth-Incoming-Port", true);
    result &= listable("/dev/cu.wlan-debug", false);
#endif // defined(__FreeBSD__)

    return result;
}
#endif // defined(__FreeBSD__) || defined(__WXOSX__)

bool portsOnThisMachineAreListedInSortedOrder()
{
    // What's attached to the machine running the test isn't known, so only
    // properties that hold regardless can be checked.
    auto ports = GetSerialPortList();

    bool result = true;
    if (!std::is_sorted(ports.begin(), ports.end(), SerialPortNameLess))
    {
        std::cout << "[list of " << ports.size() << " ports was not sorted] ";
        result = false;
    }

    for (auto& port : ports)
    {
        if (port.IsEmpty())
        {
            std::cout << "[list contains a port with no name] ";
            result = false;
        }
#if defined(__WXOSX__)
        if (port == "/dev/cu.wlan-debug")
        {
            std::cout << "[list contains /dev/cu.wlan-debug] ";
            result = false;
        }
#endif // defined(__WXOSX__)
    }
    return result;
}

} // namespace

int main()
{
    wxInitializer initializer;
    if (!initializer.IsOk())
    {
        std::cout << "Could not initialize wxWidgets" << std::endl;
        return -1;
    }

    executeTestCase("numberedPortsSortNumerically", numberedPortsSortNumerically);
    executeTestCase("differentKindsOfPortAreGroupedByName", differentKindsOfPortAreGroupedByName);
    executeTestCase("portsWithoutTrailingNumberSortAsText", portsWithoutTrailingNumberSortAsText);
    executeTestCase("portWithoutNumberComesBeforeSameNameWithOne", portWithoutNumberComesBeforeSameNameWithOne);
    executeTestCase("macSerialNumbersSortNumerically", macSerialNumbersSortNumerically);
    executeTestCase("emptyAndSingleEntryListsAreLeftAlone", emptyAndSingleEntryListsAreLeftAlone);
    executeTestCase("noPortSortsBeforeItself", noPortSortsBeforeItself);
    executeTestCase("orderingIsConsistentInBothDirections", orderingIsConsistentInBothDirections);
#if defined(__FreeBSD__) || defined(__WXOSX__)
    executeTestCase("leavesOutDevicesThatAreNotSerialPorts", leavesOutDevicesThatAreNotSerialPorts);
#endif // defined(__FreeBSD__) || defined(__WXOSX__)
    executeTestCase("portsOnThisMachineAreListedInSortedOrder", portsOnThisMachineAreListedInSortedOrder);

    return testResult();
}
