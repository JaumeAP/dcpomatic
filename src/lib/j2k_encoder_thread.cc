#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder.h"
#include "j2k_encoder_thread.h"
#include "scope_guard.h"


J2KEncoderThread::J2KEncoderThread(J2KEncoder& encoder)
	: _encoder(encoder)
{

}


void
J2KEncoderThread::start()
{
	_thread = boost::thread(boost::bind(&J2KEncoderThread::run, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np(_thread.native_handle(), "encode-worker");
#endif
}


void
J2KEncoderThread::stop()
{
	_thread.interrupt();
	try {
		_thread.join();
	} catch (std::exception& e) {
		LOG_ERROR("join() threw an exception: %1", e.what());
	} catch (...) {
		LOG_ERROR_NC("join() threw an exception");
	}
}


void
J2KEncoderThread::run()
try
{
	log_thread_start();

	while (true) {
		LOG_TIMING("encoder-sleep thread=%1", thread_id());
		auto frame = _encoder.pop();

		ScopeGuard frame_guard([this, &frame]() {
			_encoder.retry(frame);
		});

		LOG_TIMING("encoder-pop thread=%1 frame=%2 eyes=%3", thread_id(), frame.index(), static_cast<int>(frame.eyes()));

		auto encoded = encode(frame);

		if (encoded) {
			frame_guard.cancel();
			_encoder.write(encoded, frame.index(), frame.eyes());
		}
	}
} catch (boost::thread_interrupted& e) {
} catch (...) {
	store_current();
}

