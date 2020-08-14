#ifndef DCPOMATIC_J2K_ENCODER_FASTVIDEO_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_FASTVIDEO_BACKEND_H


#include "j2k_encoder_backend.h"
#include <dcp/util.h>
#include <fastvideo_encoder_j2k.h>
#include <fastvideo_sdk.h>
#include <boost/thread.hpp>


class J2KEncoderFastvideoBackend : public J2KEncoderBackend
{
public:
	J2KEncoderFastvideoBackend ();
	~J2KEncoderFastvideoBackend ();

	std::vector<dcp::Data> encode (std::vector<boost::shared_ptr<DCPVideo> > video);

	int quantity () const {
		return 16;
	}

private:
	void setup (dcp::Size size);

	bool _setup_done;
	fastImportFromHostHandle_t _adapter;
	fastDeviceSurfaceBufferHandle_t _src_buffer;
	fastEncoderJ2kHandle_t _encoder;
	uint16_t* _xyz_buffer;
	int _xyz_buffer_stride;
};

#endif

