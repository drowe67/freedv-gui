#ifndef NAME_OVERRIDE_ACCESSIBLE_H
#define NAME_OVERRIDE_ACCESSIBLE_H

#include <wx/setup.h>

#if wxUSE_ACCESSIBILITY

#include <functional>
#include <wx/access.h>

class NameOverrideAccessible : public wxAccessible
{
public:
    NameOverrideAccessible(std::function<wxString()> strValFn);
    virtual ~NameOverrideAccessible() = default;

    virtual wxAccStatus	GetName (int childId, wxString *strValue) override;
    virtual wxAccStatus GetHelpText(int	childId, wxString *helpText) override;

private:
    std::function<wxString()> strValFn_;
};

#endif // wxUSE_ACCESSIBILITY

#endif // NAME_OVERRIDE_ACCESSIBLE_H
