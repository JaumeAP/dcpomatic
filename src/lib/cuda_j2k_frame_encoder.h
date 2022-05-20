/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CUDA_J2K_FRAME_ENCODER
#define DCPOMATIC_CUDA_J2K_FRAME_ENCODER


#include "j2k_frame_encoder.h"
#include <dcp/types.h>
#include <nvjpeg2k.h>
#include <vector>


class CUDAJ2KFrameEncoder : public J2KFrameEncoder
{
public:
	CUDAJ2KFrameEncoder();
	~CUDAJ2KFrameEncoder();

	boost::optional<dcp::ArrayData> encode(DCPVideo const &) override;
	void log_thread_start() override;

private:
	void allocate(dcp::Size size);
	void free();

	nvjpeg2kEncoder_t _encoder_handle;
	nvjpeg2kEncodeState_t _encoder_state;
	nvjpeg2kEncodeParams_t _encoder_params;
	uint8_t* _pixel_data_h[3];
	uint8_t* _pixel_data_d[3];
	size_t _pitch_in_bytes[3];
	nvjpeg2kImageComponentInfo_t _image_component_info[3];

	dcp::Size _allocated_size;
};


#endif
