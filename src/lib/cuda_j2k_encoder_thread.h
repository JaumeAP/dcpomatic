/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef DCPOMATIC_CUDA_J2K_ENCODER_THREAD
#define DCPOMATIC_CUDA_J2K_ENCODER_THREAD


#include "j2k_sync_encoder_thread.h"
#include <dcp/array_data.h>
#include <dcp/types.h>
#include <nvjpeg2k.h>


class DCPVideo;


/** @class CUDAJ2KEncoderThread
 *  @brief J2K encoder thread using CUDA via nvidia's nvjpeg2k library.
 *
 *  Each CUDAJ2KEncoderThread sends frames for encoding to a different CUDA
 *  stream.  The thread sends the image and blocks until the result is ready.
 *
 *  This is different to the Grok encoder (that also uses CUDA but via an
 *  additional paid-for tool called Grok).
 */
class CUDAJ2KEncoderThread : public J2KSyncEncoderThread
{
public:
	CUDAJ2KEncoderThread(J2KEncoder& encoder);
	~CUDAJ2KEncoderThread();

	std::shared_ptr<dcp::ArrayData> encode(DCPVideo const& frame) override;
	/* XXX */
	void log_thread_start() const override {}

private:
	void allocate_pixel_data_d(dcp::Size size);
	void free_pixel_data_d();

	nvjpeg2kEncoder_t _encoder_handle;
	nvjpeg2kEncodeState_t _encode_state;
	nvjpeg2kEncodeParams_t _encode_params;
	cudaStream_t _stream;

	uint8_t* _pixel_data_d[3] = { nullptr, nullptr, nullptr };
	size_t _pitch_in_bytes_d[3];
	dcp::Size _pixel_data_d_size;

	std::vector<int16_t> _xyz_x;
	std::vector<int16_t> _xyz_y;
	std::vector<int16_t> _xyz_z;
};


#endif

