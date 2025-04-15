//==========================================================================
// Name:            ReportMessageRenderer.cpp
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

#include "ReportMessageRenderer.h"

ReportMessageRenderer::ReportMessageRenderer()
    : wxDataViewCustomRenderer("string", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT) { }

bool ReportMessageRenderer::Render(wxRect cell, wxDC *dc, int state)
{
#if defined(WIN32)
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDirect2DRenderer();
    wxGraphicsContext* context = renderer->CreateContextFromUnknownDC(*dc);
    if (context != nullptr)
    {
        context->SetFont(dc->GetFont(), dc->GetTextForeground());
        context->DrawText(m_value, cell.x, cell.y);
        delete context;
    }
    else
#endif // defined(WIN32)
    {
        RenderText(m_value, 0, cell, dc, state);
    }

    return true;
}

bool ReportMessageRenderer::SetValue( const wxVariant &value )
{
    m_value = value.GetString();
    return true;
}

bool ReportMessageRenderer::GetValue( wxVariant &WXUNUSED(value) ) const 
{ 
    return true; 
}

wxSize ReportMessageRenderer::GetSize() const 
{
    // Basically just renders text.
    return GetTextExtent(m_value);
}