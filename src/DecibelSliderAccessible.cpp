#include "DecibelSliderAccessible.h"

DecibelSliderAccessible::DecibelSliderAccessible(std::function<wxString()> strValFn)
    : strValFn_(strValFn)
{
    // empty
}

wxAccStatus DecibelSliderAccessible::GetValue (int childId, wxString *strValue)
{
    assert(strValue != nullptr);
    *strValue = strValFn_();
    return wxACC_OK;
}
