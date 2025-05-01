//==========================================================================
// Name:            ReportMessageRenderer.h
// Purpose:         Renderer for wxDataViewCtrl that helps data render properly
//                  on all platforms.
// Created:         April 14, 2024
// Authors:         Mooneer Salem
// 
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef REPORT_MESSAGE_RENDERER_H
#define REPORT_MESSAGE_RENDERER_H

#include <wx/dataview.h>

class ReportMessageRenderer : public wxDataViewCustomRenderer
{
public:
    ReportMessageRenderer();
    virtual ~ReportMessageRenderer() = default;
    
    // Overrides from wxDataViewCustomRenderer
    virtual bool Render (wxRect cell, wxDC *dc, int state) override;
    virtual bool SetValue( const wxVariant &value ) override;
    virtual bool GetValue( wxVariant &WXUNUSED(value) ) const override;
    virtual wxSize GetSize() const override;

private:
    wxString m_value;
};

#endif // REPORT_MESSAGE_RENDERER_H