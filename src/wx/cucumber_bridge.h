/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "lib/nanomsg.h"
#include "lib/signaller.h"
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <wx/wx.h>


#define CUCUMBER_BRIDGE_READY "ready"
#define CUCUMBER_BRIDGE_CLICK_REGISTERED_BUTTON "click_registered_button"
#define CUCUMBER_BRIDGE_CLICK_BUTTON_ID "click_button_id"
#define CUCUMBER_BRIDGE_SELECT_MENU "select_menu"
#define CUCUMBER_BRIDGE_TYPE "type"
#define CUCUMBER_BRIDGE_CHARACTER "character"
#define CUCUMBER_BRIDGE_ADD_CONTENT_FILE "add_content_file"
#define CUCUMBER_BRIDGE_GET_CONTENT_LIST "get_content_list"

#define CUCUMBER_MENU_FILE_NEW "file_new"


class Nanomsg;


class CucumberBridge : public Signaller
{
public:
	CucumberBridge();

	void start();

	/* Emitted in the GUI thread */
	boost::signals2::signal<void (std::string)> ClickRegisteredButton;
	boost::signals2::signal<void (wxStandardID)> ClickButtonId;
	boost::signals2::signal<void (std::string)> SelectMenu;
	boost::signals2::signal<void (std::string)> Type;
	boost::signals2::signal<void (std::string)> AddContentFile;
	boost::signals2::signal<std::string ()> GetContentList;

private:
	void listener();
	void idle();

	struct Message
	{
		std::string id;
		boost::optional<std::string> parameter;
	};

	boost::thread _thread;
	boost::mutex _mutex;
	std::list<Message> _queue;

	Nanomsg _nanomsg;
};

