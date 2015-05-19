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
#include "encoded_data.h"
#include "raw_convert.h"
#include <poznan/tier2/markers.h>
#include <dcp/xyz_image.h>
#include <dlfcn.h>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

PoznanEncoder::PoznanEncoder ()
{
	void* config = open_library ("config");
	void* preprocessing = open_library ("preprocessing");
	void* dwt = open_library ("dwt");
	void* tier1 = open_library ("tier1");
	void* gpu_coeff_coder = open_library ("gpu_coeff_coder");
	void* tier2 = open_library ("tier2");
	void* types = open_library ("types");
	void* misc = open_library ("misc");
	
	_init_device = (void (*)(type_parameters *)) dlsym (config, "init_device");
	_color_coder_lossy = (void (*)(type_image *)) dlsym (preprocessing, "color_coder_lossy");
	_fwt = (void (*)(type_tile *)) dlsym (dwt, "fwt");
	_quantize_tile = (void (*)(type_tile *)) dlsym (tier1, "quantize_tile");
	_encode_tile = (void (*)(type_tile *)) dlsym (gpu_coeff_coder, "encode_tile");
	_set_coding_parameters = (void (*)(type_image *, type_parameters *)) dlsym (types, "set_coding_parameters");
	_init_tiles = (void (*)(type_image **, type_parameters *)) dlsym (types, "init_tiles");
	_init_buffer = (void (*)(type_buffer *)) dlsym (types, "init_buffer");
	_encode_codestream = (void (*)(type_buffer *, type_image *)) dlsym (tier2, "encode_codestream");
	_cuda_h_allocate_mem = (void (*)(void **, uint64_t)) dlsym (misc, "cuda_h_allocate_mem");
	_cuda_memcpy_htd = (void (*)(void *, void *, uint64_t)) dlsym (misc, "cuda_memcpy_htd");
	_cuda_h_free = (void (*)(void *)) dlsym (misc, "cuda_h_free");
	
	if (
		!_init_device || !_color_coder_lossy || !_fwt || !_quantize_tile ||
		!_encode_tile || !_set_coding_parameters || !_init_tiles ||
		!_init_buffer || !_encode_codestream || !_cuda_h_allocate_mem ||
		!_cuda_memcpy_htd || !_cuda_h_free) {
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
		throw JPEG2000EncoderUnavailableException (name(), "could not find " + library_name + " (" + dlerror() + ")");
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
PoznanEncoder::do_encode (shared_ptr<const dcp::XYZImage> input)
{
	type_image* img = new type_image;

	img->width = input->size().width;
	img->height = input->size().height;
	img->depth = 12;
	img->num_components = 3;
	img->num_range_bits = 12;
	img->sign = UNSIGNED;
	img->num_dlvls = _param.param_tile_comp_dlvls;

	img->wavelet_type = _param.param_wavelet_type;
	img->use_mct = _param.param_use_mct;
	img->use_part2_mct = _param.param_use_part2_mct;
	img->mct_compression_method = _param.param_mct_compression_method;

	img->coding_style = CODING_STYLE;
	img->prog_order = LY_RES_COMP_POS_PROG;
	img->num_layers = NUM_LAYERS;

	_set_coding_parameters (img, &_param);
	_init_tiles (&img, &_param);

	_color_coder_lossy (img);

	type_tile* tile = &(img->tile[0]);

	/* XXX: it's a shame about this int -> float conversion */
	for (int i = 0; i < 3; ++i) {
		type_tile_comp* c = &tile->tile_comp[i];
		std::cout << "Tile comp " << i << ": " << c->width << "x" << c->height << "\n";
		int const pixels = c->width * c->height;
		_cuda_h_allocate_mem ((void **) &c->img_data, pixels * sizeof (type_data));
		for (int j = 0; j < pixels; ++j) {
			c->img_data[j] = float (input->data(i)[j]) / 4095;
			//c->img_data[j] = input->data(i)[j];
		}
		_cuda_memcpy_htd (c->img_data, c->img_data_d, pixels * sizeof (type_data));
		_cuda_h_free (c->img_data);
	}
	
	std::cout << "Tile " << tile->width << "x" << tile->height << "\n";
	_fwt (tile);
	_quantize_tile (tile);
	_encode_tile (tile);

	type_buffer buffer;
	_init_buffer (&buffer);
	_encode_codestream (&buffer, img);

	std::cout << img->num_tiles << " tiles.\n";
	std::cout << "got " << buffer.bytes_count << " bytes.\n";
	shared_ptr<EncodedData> encoded (new EncodedData (buffer.data, buffer.bytes_count));

	/* XXX! */
	static int shit = 0;
	{
		string name = raw_convert<string> (shit);
		FILE* f = fopen (name.c_str(), "wb");
		fwrite (buffer.data, 1, buffer.bytes_count, f);
		fclose (f);
		++shit;
	}

	free (buffer.data);
	delete img;

	return encoded;
}
