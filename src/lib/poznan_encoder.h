/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "jpeg2000_encoder.h"
#include <poznan-jpeg2k-cth/config/parameters.h>
#include <poznan-jpeg2k-cth/types/image_types.h>
#include <string>

class PoznanEncoder : public JPEG2000Encoder
{
public:
	PoznanEncoder ();

	std::string name () const;

	std::string id () const {
		return "poznan";
	}

protected:

	dcp::Data do_encode (
		boost::shared_ptr<const dcp::OpenJPEGImage> input
		);

	void parameters_changed ();

private:
	void* open_library (std::string name);

	type_parameters _param;

	void (*_init_device) (type_parameters *);
	void (*_color_coder_lossy) (type_image *);
	void (*_fwt) (type_tile *);
	void (*_quantize_tile) (type_tile *);
	void (*_encode_tile) (type_tile *);
	void (*_set_coding_parameters) (type_image *, type_parameters *);
	void (*_init_tiles) (type_image **, type_parameters *);
	void (*_init_buffer) (type_buffer *);
	void (*_encode_codestream) (type_buffer *, type_image *);
	void (*_cuda_h_allocate_mem) (void **, uint64_t);
	void (*_cuda_memcpy_htd) (void *, void *, uint64_t);
	void (*_cuda_memcpy_dth) (void *, void *, uint64_t);
	void (*_cuda_h_free) (void *);
	void (*_check_cuda_error) (char const *);
};
