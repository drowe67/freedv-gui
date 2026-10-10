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

inline std::ostream& operator<<(std::ostream& os, wxString const& str)
{
    return os << '"' << str.utf8_string() << '"';
}

template<typename T>
std::ostream& operator<<(std::ostream& os, std::vector<T> const& list)
{
    os << "{";
    for (auto const& item : list) os << " " << item;
    return os << " }";
}

inline std::ostream& operator<<(std::ostream& os, std::map<wxString, int> const& map)
{
    os << "{";
    for (auto const& kv : map) os << " " << kv.first << "=" << kv.second;
    return os << " }";
}

template<typename T>
bool check(const char* description, T const& actual, T const& expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
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
