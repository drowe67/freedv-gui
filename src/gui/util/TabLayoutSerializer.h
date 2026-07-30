#ifndef TAB_LAYOUT_SERIALIZER_H
#define TAB_LAYOUT_SERIALIZER_H

#include <wx/version.h>

// wxAuiNotebook::SaveLayout()/LoadLayout() and the wxAuiBookSerializer/
// wxAuiBookDeserializer interfaces they use were only added in wxWidgets 3.3,
// so tab layout persistence is unavailable when building against older
// wxWidgets (e.g. as still packaged by some Linux distributions).
#if wxCHECK_VERSION(3, 3, 0)

#include <vector>
#include <wx/aui/auibook.h>
#include <wx/aui/serializer.h>
#include <wx/string.h>

// Saves the layout of a wxAuiNotebook's tab controls (i.e. how the tabs are
// split and ordered) to a single wxString suitable for storing in the
// application configuration.
class TabLayoutSerializer : public wxAuiBookSerializer
{
public:
    wxString GetLayout() const { return m_layout; }

    virtual void BeforeSaveNotebook(const wxString& name) override;
    virtual void SaveNotebookTabControl(const wxAuiTabLayoutInfo& tab) override;

private:
    wxString m_layout;
};

// Restores a wxAuiNotebook tab layout previously saved by TabLayoutSerializer.
class TabLayoutDeserializer : public wxAuiBookDeserializer
{
public:
    explicit TabLayoutDeserializer(const wxString& layout);

    virtual std::vector<wxAuiTabLayoutInfo> LoadNotebookTabs(const wxString& name) override;

private:
    wxString m_layout;
};

#endif // wxCHECK_VERSION(3, 3, 0)

#endif // TAB_LAYOUT_SERIALIZER_H
