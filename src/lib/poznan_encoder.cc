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
#include "poznan/config/parameters.h"
#include <dlfcn.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;

PoznanEncoder::PoznanEncoder ()
{
	/* XXX: need cross-platform implementation of dlopen etc. */
	
	void* config = dlopen ("libdcpomatic-config.so", RTLD_LAZY | RTLD_GLOBAL);
	if (!config) {
		throw JPEG2000EncoderUnavailableException (name(), "could not find libdcpomatic-config.so");
	}

	void* misc = dlopen ("libdcpomatic-misc.so", RTLD_LAZY | RTLD_GLOBAL);
	if (!misc) {
		throw JPEG2000EncoderUnavailableException (name(), "could not find libdcpomatic-misc.so");
	}
	
	void (*init_device) (type_parameters *);
	init_device = (void (*)(type_parameters *)) dlsym (config, "init_device");
	if (!init_device) {
		throw JPEG2000EncoderUnavailableException (name(), "missing symbol");
	}
		
	type_parameters param;

	/* One tile which covers entire image */
	param.param_tile_w = -1;
	param.param_tile_h = -1;

	/* XXX */
	/*
	uint8_t param_tile_comp_dlvls;
	uint8_t param_cblk_exp_w; ///Maximum codeblock size is 2^6 x 2^6 ( 64 x 64 ).
	uint8_t param_cblk_exp_h; ///Maximum codeblock size is 2^6 x 2^6 ( 64 x 64 ).
	uint8_t param_wavelet_type; ///Lossy encoding
	uint8_t param_use_mct;//Multi-component transform
	*/
	param.param_device = 0;
	/*
	uint32_t param_target_size;//Target size of output file
	float param_bp;//Bits per pixel per component
	uint8_t param_use_part2_mct; // Multiple component transform as in 15444-2
	uint8_t param_mct_compression_method; // 0 klt 2 wavelet
	uint32_t param_mct_klt_iterations; // max number of iterations of Gram-Schmidt algorithm
	float param_mct_klt_border_eigenvalue; // cut-off for dumping components 
	float param_mct_klt_err; // error sufficient for Gram-Schmit algorithm to end iteration
	*/
	
	init_device (&param);
}

string
PoznanEncoder::name () const
{
	return _("CUDA (GPU) encoder (Poznan Supercomputing and Networking Center)");
}

void
PoznanEncoder::set_bandwidth (int bandwidth)
{
	/* XXX */
}

void
PoznanEncoder::set_frame_rate (int frame_rate)
{
	/* XXX */
}

void
PoznanEncoder::set_resolution (Resolution resolution)
{
	/* XXX */
}

void
PoznanEncoder::set_threed (bool threed)
{
	/* XXX */
}

shared_ptr<EncodedData>
PoznanEncoder::do_encode (shared_ptr<const dcp::XYZImage> input, dcp::NoteHandler note_handler)
{
	return shared_ptr<EncodedData> ();
}
