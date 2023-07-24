#ifndef DECIBEL_SLIDER_ACCESSIBLE_H
#define DECIBEL_SLIDER_ACCESSIBLE_H

#include <wx/setup.h>

#if wxUSE_ACCESSIBILITY

#include <functional>
#include <wx/access.h>

class DecibelSliderAccessible : public wxAccessible
{
public:
    DecibelSliderAccessible(std::function<wxString()> strValFn);
    virtual ~DecibelSliderAccessible() = default;

    virtual wxAccStatus	GetValue (int childId, wxString *strValue) override;

private:
    std::function<wxString()> strValFn_;
};

#endif // wxUSE_ACCESSIBILITY

#endif // DECIBEL_SLIDER_ACCESSIBLE_H
