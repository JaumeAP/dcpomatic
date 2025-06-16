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


#include <AppKit/AppKit.h>


void enter_full_screen_mode(NSView* view, int* width, int* height)
{
	NSScreen* screen = [[NSScreen mainScreen] retain];
	[view enterFullScreenMode:screen withOptions:nil];
	*width = (int) view.frame.size.width;
	*height = (int) view.frame.size.height;
}


void exit_full_screen_mode(NSView* view)
{
	[view exitFullScreenModeWithOptions:nil];
}


