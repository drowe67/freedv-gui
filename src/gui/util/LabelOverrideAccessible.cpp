#include "LabelOverrideAccessible.h"

#if wxUSE_ACCESSIBILITY

LabelOverrideAccessible::LabelOverrideAccessible(std::function<wxString()> strValFn)
    : strValFn_(strValFn)
{
    // empty
}

wxAccStatus LabelOverrideAccessible::GetValue (int childId, wxString *strValue)
{
    assert(strValue != nullptr);
    
    if (childId == 0)
    {
        *strValue = strValFn_();
        return wxACC_OK;
    }
    else
    {
        return wxACC_NOT_IMPLEMENTED;
    }
}

#endif // wxUSE_ACCESSIBILITY
