#include "lib/help.h"
#include <wx/wx.h>
#include <boost/optional.hpp>
#include <map>
#include <string>
#include <vector>

class HelpGUI
{
public:
	static HelpGUI* instance ();

	void start ();

	enum Landmark {
		ADD_FOLDER_BUTTON,
		PLAY_BUTTON
	};

	void landmark (wxWindow* landmark, Landmark id);

private:
	class Message
	{
	public:
		Message (std::string id_, wxPoint anchor_, Help::EventType next_trigger_)
			: id (id_)
			, anchor_point (anchor_)
			, next_trigger (next_trigger_)
		{}

		Message (std::string id_, Landmark anchor_, wxSize offset_)
			: id (id_)
			, anchor_landmark (anchor_)
			, offset (offset_)
		{}

		Message (std::string id_, Landmark anchor_, wxSize offset_, Help::EventType next_trigger_)
			: id (id_)
			, anchor_landmark (anchor_)
			, offset (offset_)
			, next_trigger (next_trigger_)
		{}

		std::string id;
		boost::optional<wxPoint> anchor_point;
		boost::optional<Landmark> anchor_landmark;
		wxSize offset;
		boost::optional<Help::EventType> next_trigger;
	};

	HelpGUI ();
	void event (Help::EventType e);
	void show (int index);
	void next ();
	void stop ();

	static HelpGUI* _instance;

	int _current_index;
	wxFrame* _current_frame;
	std::vector<Message> _messages;
	std::map<Landmark, wxWindow*> _landmarks;
};


