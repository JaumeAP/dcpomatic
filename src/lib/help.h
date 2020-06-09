#ifndef DCPOMATIC_HELP_H
#define DCPOMATIC_HELP_H

#include "signaller.h"
#include <boost/signals2.hpp>

class Help : public Signaller
{
public:
	static Help* instance ();

	enum EventType {
		SUCCESSFUL_FILE_NEW,
		SUCCESSFUL_ADD_CONTENT,
	};

	void event (EventType e);

	boost::signals2::signal<void (EventType)> Event;

private:
	static Help* _instance;
};

#endif

