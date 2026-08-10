//==========================================================================
// Name:            tot_warning.h
// Purpose:         Dialog warning the user of impending Time-Out Timer expiry.
// Created:         June 2026
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

#ifndef TOT_WARNING_DIALOG_H
#define TOT_WARNING_DIALOG_H

#include <functional>
#include <wx/wx.h>

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class TotWarningDialog
// Shown when the Time-Out Timer is within 15 seconds of expiry.
// Closes automatically when the caller indicates the timer has fired.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class TotWarningDialog : public wxDialog
{
public:
    TotWarningDialog(wxWindow* parent, int initialRemainingMs,
                     std::function<void()> onExtend);
    virtual ~TotWarningDialog();

    void updateRemainingTime(int remainingMs);

private:
    wxStaticText* m_countdownText_;
    wxButton*     m_extendBtn_;

    std::function<void()> m_onExtend_;

    void OnExtend(wxCommandEvent&);
};

#endif // TOT_WARNING_DIALOG_H
