/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_J2K_ENCODER_FASTVIDEO_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_FASTVIDEO_BACKEND_H


#include "j2k_encoder_backend.h"
#include <dcp/util.h>
#include <fastvideo_encoder_j2k.h>
#include <fastvideo_sdk.h>
#include <boost/thread.hpp>


class Barrier;


class J2KEncoderFastvideoBackend : public J2KEncoderBackend
{
public:
	J2KEncoderFastvideoBackend (Barrier& barrier);
	~J2KEncoderFastvideoBackend ();

	std::vector<dcp::ArrayData> encode (std::vector<DCPVideo> const& video) override;

	int quantity () const override {
		return 1;
	}

private:
	void setup (dcp::Size size);

	Barrier& _barrier;
	bool _setup_done = false;
	fastImportFromHostHandle_t _adapter;
	fastDeviceSurfaceBufferHandle_t _src_buffer;
	fastEncoderJ2kHandle_t _encoder;
	uint16_t* _xyz_buffer;
	int _xyz_buffer_stride;
};


#endif


