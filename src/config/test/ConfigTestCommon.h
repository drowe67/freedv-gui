#ifndef CONFIG_TEST_COMMON_H
#define CONFIG_TEST_COMMON_H

// Helpers for testing configuration code against an in-memory configuration
// file (i.e. nothing here touches the user's real FreeDV configuration).

#include <map>
#include <memory>
#include <vector>

#include <wx/fileconf.h>
#include <wx/sstream.h>
#include <wx/string.h>

#include "../../test/UnitTestCommon.h"

// Creates a configuration with the given contents (in .ini format).
inline std::unique_ptr<wxFileConfig> makeConfig(wxString const& contents = "")
{
    wxStringInputStream stream(contents);
    return std::make_unique<wxFileConfig>(stream);
}

// The configuration's contents as they'd be written to disk.
inline wxString contentsOf(wxFileConfig& config)
{
    wxString contents;
    wxStringOutputStream stream(&contents);
    config.Save(stream);
    return contents;
}

// Equivalent to exiting and restarting FreeDV.
inline std::unique_ptr<wxFileConfig> reload(wxFileConfig& config)
{
    return makeConfig(contentsOf(config));
}

// Prints values in failure messages. (These are functions of our own rather than
// stream operators as whether wxString has one depends on how wxWidgets was built.)
inline void printValue(std::ostream& os, wxString const& str)
{
    os << '"' << (const char*)str.utf8_str() << '"';
}

template<typename T>
void printValue(std::ostream& os, T const& value)
{
    os << value;
}

inline void printValue(std::ostream& os, std::map<wxString, int> const& map)
{
    os << "{";
    for (auto const& kv : map)
    {
        os << " ";
        printValue(os, kv.first);
        os << "=" << kv.second;
    }
    os << " }";
}

template<typename T>
void printValue(std::ostream& os, std::vector<T> const& list)
{
    os << "{";
    for (size_t index = 0; index < list.size(); index++)
    {
        os << " ";
        printValue(os, (T)list[index]);
    }
    os << " }";
}

template<typename T>
bool check(const char* description, T const& actual, T const& expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was ";
    printValue(std::cout, actual);
    std::cout << ", expected ";
    printValue(std::cout, expected);
    std::cout << "] ";
    return false;
}

inline bool check(const char* description, wxString const& actual, const char* expected)
{
    return check(description, actual, wxString::FromUTF8(expected));
}

inline bool checkTrue(const char* description, bool value)
{
    if (!value) std::cout << "[" << description << "] ";
    return value;
}

#endif // CONFIG_TEST_COMMON_H
