#ifndef DCPOMATIC_PLAYER_VIDEO_PREPARER_H
#define DCPOMATIC_PLAYER_VIDEO_PREPARER_H

#include "exception_store.h"
#include <boost/shared_ptr.hpp>

class PlayerVideo;

class PlayerVideoPreparer : public ExceptionStore
{
public:
	virtual ~PlayerVideoPreparer () {}
	virtual void request (boost::shared_ptr<PlayerVideo> pv) = 0;
};

#endif

