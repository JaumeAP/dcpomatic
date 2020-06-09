#include "help.h"
#include <boost/bind.hpp>
#include <boost/ref.hpp>


Help* Help::_instance = 0;


Help*
Help::instance ()
{
	if (!_instance) {
		_instance = new Help ();
	}

	return _instance;
}


void
Help::event (EventType e)
{
	emit (boost::bind(boost::ref(Event), e));
}

