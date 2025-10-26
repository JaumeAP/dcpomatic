/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "lib/signal.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


/** @class wxThreadWaker
 *  @brief Class to wake a thread so that it looks for incoming signals that it should deliver.
 */
class wxSignalManager2 : public SignalManager2
{
public:
	wxSignalManager2(wxEvtHandler* handler)
		: _handler(handler)
	{}

	void wake() override
	{
		/* This will result in an idle handler being called, which should process
		 * pending events.
		 */
		_handler->QueueEvent(new wxCommandEvent(wxEVT_IDLE));
	}

private:
	wxEvtHandler* _handler;
};
