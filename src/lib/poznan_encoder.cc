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

#include "poznan_encoder.h"
#include "exceptions.h"
#include <dlfcn.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;

PoznanEncoder::PoznanEncoder ()
{
	void* config = open_library ("config");
	void* preprocessing = open_library ("preprocessing");
	void* dwt = open_library ("dwt");
	void* tier1 = open_library ("tier1");
	void* gpu_coeff_coder = open_library ("gpu_coeff_coder");
	void* tier2 = open_library ("tier2");
	
	_init_device = (void (*)(type_parameters *)) dlsym (config, "init_device");
	_mct = (void (*)(type_image *, type_parameters *)) dlsym (preprocessing, "mct");
	_fwt = (void (*)(type_tile *)) dlsym (dwt, "fwt");
	_quantize_tile = (void (*)(type_tile *)) dlsym (tier1, "quantize_tile");
	_encode_tile = (void (*)(type_tile *)) dlsym (gpu_coeff_coder, "encode_tile");
	_write_codestream = (void (*)(type_image *)) dlsym (tier2, "write_codestream");
	
	if (!_init_device || !_mct || !_fwt || !_quantize_tile || !_encode_tile || !_write_codestream) {
		throw JPEG2000EncoderUnavailableException (name(), "missing symbol");
	}
}

void *
PoznanEncoder::open_library (string library_name)
{
	/* XXX: need cross-platform implementation of dlopen etc. */
	
	library_name = "libdcpomatic-" + library_name + ".so";
	void* lib = dlopen (library_name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
	if (!lib) {
		throw JPEG2000EncoderUnavailableException (name(), "could not find " + library_name);
	}
	return lib;
}

void
PoznanEncoder::parameters_changed ()
{
	/* One tile which covers entire image */
	_param.param_tile_w = -1;
	_param.param_tile_h = -1;

	/* Wavelet decomposition levels */
	_param.param_tile_comp_dlvls = _resolution.get() == RESOLUTION_2K ? 5 : 6;

	/* Power of 2 for maximum codeblock size */
	_param.param_cblk_exp_w = 5;
	_param.param_cblk_exp_h = 5;

	/* DWT 9/7 transform */
	_param.param_wavelet_type = 1;

	/* Use MCT */
	_param.param_use_mct = 1;

	/* Device to run on */
	_param.param_device = 0;

	/* Target file size */
	_param.param_target_size = (_bandwidth.get() / _frame_rate.get()) / 8;
	if (_threed.get ()) {
		_param.param_target_size /= 2;
	}

	/* Bits per pixel per component */
	_param.param_bp = 12;

	/* Don't know about these: use the defaults */
	_param.param_use_part2_mct = 0;
	_param.param_mct_compression_method = 0;
	_param.param_mct_klt_iterations = 10000;
	_param.param_mct_klt_border_eigenvalue = 0.000001;
	_param.param_mct_klt_err = 1.0e-7;
	
	_init_device (&_param);
}

string
PoznanEncoder::name () const
{
	return _("CUDA (GPU) encoder (Poznan Supercomputing and Networking Center)");
}

shared_ptr<EncodedData>
PoznanEncoder::do_encode (shared_ptr<const dcp::XYZImage> input, dcp::NoteHandler note_handler)
{
	type_image img;
	_mct (&img, &_param);
	for (size_t i = 0; i < img.num_tiles; ++i) {
		type_tile* tile = &(img.tile[i]);
		_fwt (tile);
		_quantize_tile (tile);
		_encode_tile (tile);
	}
	_write_codestream (&img);
	return shared_ptr<EncodedData> ();
}
