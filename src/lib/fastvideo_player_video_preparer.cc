#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "image.h"
#include "image_proxy.h"
#include "fastvideo_player_video_preparer.h"
#include "j2k_image_proxy.h"
#include "player_video.h"
#include "timer.h"
#include <fastvideo_decoder_j2k.h>
#include <fastvideo_sdk.h>
#include <boost/bind.hpp>


using boost::bind;
using boost::const_pointer_cast;
using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using boost::weak_ptr;


FastvideoPlayerVideoPreparer::FastvideoPlayerVideoPreparer (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast)
	: _stop_thread (false)
	, _decoder (0)
	, _setup_done (false)
	, _cpu (pixel_format, aligned, fast)
{
	fastSdkParametersHandle_t sdk_parameters;
	fastStatus_t r = fastGetSdkParametersHandle(&sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("GetSdkParametersHandle", r);
	}
	r = fastDecoderJ2kLibraryInit(sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("DecoderJ2kLibraryInit", r);
	}

	_thread = boost::thread (bind(&FastvideoPlayerVideoPreparer::thread, this));
}


FastvideoPlayerVideoPreparer::~FastvideoPlayerVideoPreparer ()
{
	_stop_thread = true;
	_work.notify_all ();
	try {
		_thread.join ();
	} catch (...) {}

	if (_setup_done) {
		fastDecoderJ2kDestroy(_decoder);
		fastExportToHostDestroy(_adapter);
	}
}


void
FastvideoPlayerVideoPreparer::request (shared_ptr<PlayerVideo> pv)
{
	boost::mutex::scoped_lock lm (_mutex);
	_queue.push (weak_ptr<PlayerVideo>(pv));
	_work.notify_all ();
}


void
FastvideoPlayerVideoPreparer::thread ()
{
	while (true) {
		boost::mutex::scoped_lock lm (_mutex);
		while (!_stop_thread && _queue.empty()) {
			_work.wait (lm);
		}

		if (_stop_thread) {
			return;
		}

		weak_ptr<PlayerVideo> weak = _queue.front ();
		_queue.pop ();

		lm.unlock ();
		shared_ptr<PlayerVideo> pv = weak.lock ();
		if (pv) {
			/* We're going to "prepare" just the image proxy here (if it interests us i.e. if it's J2K,
			 * then hand over the rest of the work to a CPUPlayerVideoPreparer.
			 */
			shared_ptr<const ImageProxy> ip = pv->image_proxy ();
			shared_ptr<const J2KImageProxy> jp = dynamic_pointer_cast<const J2KImageProxy> (ip);
			if (jp) {
				send_to_gpu (pv, jp);
			} else {
				_cpu.request (pv);
			}
		}
	}
}


void
FastvideoPlayerVideoPreparer::send_to_gpu (shared_ptr<PlayerVideo> pv, shared_ptr<const J2KImageProxy> proxy)
{
	if (!_setup_done) {
		setup (proxy->j2k());
		_setup_done = true;
	}

	//std::cout << "add to batch.\n";
	timestamped_printf("fv sends %d to gpu\n", pv->time.frames_round(24));
	fastDecoderJ2kAddImageToBatch(_decoder, proxy->j2k().data().get(), proxy->j2k().size());
	_batch.push_back (pv);

	int free_slots = 0;
	fastDecoderJ2kFreeSlotsInBatch(_decoder, &free_slots);
	timestamped_printf("now %d slots left.\n", free_slots);
	//std::cout << free_slots << " left in batch.\n";
	if (free_slots == 0) {
		/* Do some decoding */
		timestamped_printf("tx & extract\n");
		transform_and_extract ();
	}

	/* XXX: maybe should hoover up any left-overs at some point */
}


void
FastvideoPlayerVideoPreparer::transform_and_extract ()
{
	fastDecoderJ2kReport_t report;
	fastStatus_t r = fastDecoderJ2kTransformBatch(_decoder, &report);
	if (r != FAST_OK) {
		throw FastvideoError ("DecoderJ2kTransformBatch", r);
	}

	size_t index = 0;
	int images_left = 1;
	while (true) {
		DCPOMATIC_ASSERT (index <= _batch.size());
		shared_ptr<PlayerVideo> pv = _batch[index].lock();
		if (pv) {
			//std::cout << "got a pv for batch number " << index << "\n";
			/* XXX: this should be memlocked or whatever fastMalloc does */
			shared_ptr<J2KImageProxy> proxy = const_pointer_cast<J2KImageProxy>(dynamic_pointer_cast<const J2KImageProxy>(pv->image_proxy()));
			DCPOMATIC_ASSERT (proxy);
			dcp::Size const size = proxy->size();
			shared_ptr<dcpomatic::Image> image(new dcpomatic::Image(AV_PIX_FMT_RGB24, size, true));
			fastExportParameters_t export_parameters;
			export_parameters.convert = FAST_CONVERT_NONE;
			fastStatus_t r = fastExportToHostCopy(_adapter, image->data()[0], size.width, image->stride()[0], size.height, &export_parameters);
			if (r != FAST_OK) {
				throw FastvideoError ("ExportToHostCopy", r);
			}

			timestamped_printf("fv sets image for %d\n", pv->time.frames_round(24));
			proxy->set_image (image);

			_cpu.request (pv);
		}

		if (images_left == 0) {
			break;
		}
		fastStatus_t r = fastDecoderJ2kGetNextDecodedImage(_decoder, &report, &images_left);
		if (r != FAST_OK) {
			throw FastvideoError ("DecoderJ2kGetNextDecodedImage", r);
		}

		++index;
	};

	_batch.clear ();
}


void
FastvideoPlayerVideoPreparer::setup (dcp::Data sample)
{
	fastJ2kImageInfo_t info;
	fastStatus_t r = fastDecoderJ2kPredecode(&info, sample.data().get(), sample.size());
	if (r != FAST_OK) {
		throw FastvideoError ("DecoderJ2kPredecode", r);
	}

	fastDecoderJ2kStaticParameters_t parameters;
	memset(&parameters, 0, sizeof(fastDecoderJ2kStaticParameters_t));

	parameters.ResolutionLevels = 0;
	parameters.verboseLevel = 1;
	parameters.enableROI = 0;

	parameters.maxTileWidth = info.width;
	parameters.maxTileHeight = info.height;

	parameters.windowX0 = 0;
	parameters.windowY0 = 0;
	parameters.windowWidth = info.width;
	parameters.windowHeight = info.height;

	parameters.truncationLength = 0;
	parameters.truncationMode = 0;
	parameters.truncationRate = 0;

	parameters.DecodePasses = 0;
	parameters.imageInfo = &info;
	parameters.maxStreamSize = max_stream_size;

	r = fastDecoderJ2kCreate(
			&_decoder,
			&parameters,
			FAST_RGB8, info.width, info.height,
			max_batch_size,
			&_buffer
			);

	if (r != FAST_OK) {
		fastTraceClose ();
		throw FastvideoError ("J2kCreate", r);
	}

	fastSurfaceFormat_t surface_format = FAST_RGB8;
	r = fastExportToHostCreate(&_adapter, &surface_format, _buffer);
	if (r != FAST_OK) {
		throw FastvideoError ("ExportToHostCreate");
	}
}
