#include "cpu_player_video_preparer.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "player_video.h"
#include "timer.h"
#include <boost/thread.hpp>


using boost::bind;
using boost::shared_ptr;
using boost::weak_ptr;


CPUPlayerVideoPreparer::CPUPlayerVideoPreparer (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast)
	: _work (new boost::asio::io_service::work(_service))
	, _pixel_format (pixel_format)
	, _aligned (aligned)
	, _fast (fast)
{
	LOG_TIMING("start-prepare-threads %1", boost::thread::hardware_concurrency() * 2);

	for (size_t i = 0; i < boost::thread::hardware_concurrency() * 2; ++i) {
		_pool.create_thread (bind (&boost::asio::io_service::run, &_service));
	}
}


CPUPlayerVideoPreparer::~CPUPlayerVideoPreparer ()
{
	_work.reset ();
	_pool.join_all ();
	_service.stop ();
}


void
CPUPlayerVideoPreparer::prepare (weak_ptr<PlayerVideo> weak_video)
try
{
	shared_ptr<PlayerVideo> video = weak_video.lock ();
	/* If the weak_ptr cannot be locked the video obviously no longer requires any work */
	if (video) {
		LOG_TIMING("start-prepare in %1", thread_id());
		video->prepare (_pixel_format, _aligned, _fast);
		LOG_TIMING("finish-prepare in %1", thread_id());
		timestamped_printf("cpu finishes %d\n", video->time.frames_round(24));
	}
}
catch (...)
{
	store_current ();
}


void
CPUPlayerVideoPreparer::request (shared_ptr<PlayerVideo> pv)
{
	_service.post (bind(&CPUPlayerVideoPreparer::prepare, this, weak_ptr<PlayerVideo>(pv)));
}

