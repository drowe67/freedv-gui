#include "NameOverrideAccessible.h"

#if wxUSE_ACCESSIBILITY

NameOverrideAccessible::NameOverrideAccessible(std::function<wxString()> strValFn)
    : strValFn_(strValFn)
{
    // empty
}

wxAccStatus NameOverrideAccessible::GetName (int childId, wxString *strValue)
{
    assert(strValue != nullptr);

    wxAccRole role;
    auto ret = GetRole(childId, &role);
    bool isTitleBar = ret == wxACC_OK && role == wxROLE_SYSTEM_TITLEBAR;

    if (childId == 0 || isTitleBar)
    {
        *strValue = strValFn_();
        return wxACC_OK;
    }
    else
    {
        return wxACC_NOT_IMPLEMENTED;
    }
}

wxAccStatus NameOverrideAccessible::GetHelpText(int childId, wxString *helpText)
{
    return GetName(childId, helpText);
}

#endif // wxUSE_ACCESSIBILITY
