#ifndef LABEL_OVERRIDE_ACCESSIBLE_H
#define LABEL_OVERRIDE_ACCESSIBLE_H

#include <wx/setup.h>

#if wxUSE_ACCESSIBILITY

#include <functional>
#include <wx/access.h>

class LabelOverrideAccessible : public wxAccessible
{
public:
    LabelOverrideAccessible(std::function<wxString()> strValFn);
    virtual ~LabelOverrideAccessible() = default;

    virtual wxAccStatus	GetValue (int childId, wxString *strValue) override;

private:
    std::function<wxString()> strValFn_;
};

#endif // wxUSE_ACCESSIBILITY

#endif // LABEL_OVERRIDE_ACCESSIBLE_H
