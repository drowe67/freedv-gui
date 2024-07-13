//==========================================================================
// Name:            monitor_volume_adj.h
// Purpose:         Popup to allow adjustment of monitor volume
// Created:         Jul 12, 2024
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

#ifndef __MONITOR_VOLUME_ADJ__
#define __MONITOR_VOLUME_ADJ__

#include <wx/popupwin.h>
#include <wx/slider.h>

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class MontiorVolumeAdjPopup
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class MontiorVolumeAdjPopup : public wxPopupTransientWindow
{
    public:        
        MontiorVolumeAdjPopup( wxWindow* parent );
        ~MontiorVolumeAdjPopup();
        
    protected:
        
    private:
        wxSlider* volumeSlider_;
};

#endif // __MONITOR_VOLUME_ADJ__
