#ifndef DCPOMATIC_CPU_PLAYER_VIDEO_PREPARER_H
#define DCPOMATIC_CPU_PLAYER_VIDEO_PREPARER_H

#include "player_video_preparer.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/weak_ptr.hpp>


class CPUPlayerVideoPreparer : public PlayerVideoPreparer
{
public:
	CPUPlayerVideoPreparer (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast);
	~CPUPlayerVideoPreparer ();

	void request (boost::shared_ptr<PlayerVideo> pv);

private:
	void prepare (boost::weak_ptr<PlayerVideo> pv);

	boost::thread_group _pool;
	boost::asio::io_service _service;
	boost::shared_ptr<boost::asio::io_service::work> _work;

	boost::function<AVPixelFormat (AVPixelFormat)> _pixel_format;
	bool _aligned;
	bool _fast;
};

#endif
