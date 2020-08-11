#include "cpu_player_video_preparer.h"
#include "player_video_preparer.h"
#include <dcp/data.h>
#include <fastvideo_decoder_j2k.h>
#include <fastvideo_sdk.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/weak_ptr.hpp>
#include <queue>


class J2KImageProxy;


class FastvideoPlayerVideoPreparer : public PlayerVideoPreparer
{
public:
	FastvideoPlayerVideoPreparer (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast);
	~FastvideoPlayerVideoPreparer ();

	void request (boost::shared_ptr<PlayerVideo> pv);

private:
	void thread ();
	void setup (dcp::Data sample);
	void send_to_gpu (boost::shared_ptr<PlayerVideo> pv, boost::shared_ptr<const J2KImageProxy> proxy);
	void transform_and_extract ();

	std::queue<boost::weak_ptr<PlayerVideo> > _queue;
	boost::mutex _mutex;
	boost::condition _work;

	boost::atomic<bool> _stop_thread;
	boost::thread _thread;

	fastDecoderJ2kHandle_t _decoder;
	fastDeviceSurfaceBufferHandle_t _buffer;
	fastExportToHostHandle_t _adapter;
	bool _setup_done;

	std::vector<boost::weak_ptr<PlayerVideo> > _batch;

	CPUPlayerVideoPreparer _cpu;

	static const int max_batch_size = 16;
	static const int max_stream_size = 1302083;
};

