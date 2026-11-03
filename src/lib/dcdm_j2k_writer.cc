/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "dcdm_j2k_writer.h"
#include "exceptions.h"
#include <dcp/openjpeg_image.h>
#include <openjpeg.h>
#include <cstring>
#include <string>


using std::shared_ptr;
using std::string;


static char const comment[] = "SMPTE DCDM X'Y'Z' image as defined in SMPTE ST 428-1";


void
write_lossless_j2k(boost::filesystem::path const& path, shared_ptr<const dcp::OpenJPEGImage> xyz)
{
	auto const size = xyz->size();

	opj_image_cmptparm_t components[3];
	memset(components, 0, sizeof(components));
	for (auto& c: components) {
		c.dx = 1;
		c.dy = 1;
		c.w = size.width;
		c.h = size.height;
		c.prec = 12;
		c.sgnd = 0;
	}

	auto image = opj_image_create(3, components, OPJ_CLRSPC_UNSPECIFIED);
	if (!image) {
		throw EncodeError("could not create OpenJPEG image");
	}
	image->x0 = 0;
	image->y0 = 0;
	image->x1 = size.width;
	image->y1 = size.height;
	for (int c = 0; c < 3; ++c) {
		memcpy(image->comps[c].data, xyz->data(c), sizeof(OPJ_INT32) * size.width * size.height);
	}

	opj_cparameters_t parameters;
	opj_set_default_encoder_parameters(&parameters);
	/* Lossless: one layer, no rate allocation, reversible 5/3 wavelet (irreversible = 0) */
	parameters.tcp_numlayers = 1;
	parameters.tcp_rates[0] = 0;
	parameters.cp_disto_alloc = 1;
	parameters.irreversible = 0;
	parameters.numresolution = 6;
	parameters.prog_order = OPJ_RPCL;
	parameters.cblockw_init = 128;
	parameters.cblockh_init = 32;
	/* Precinct sizes from the highest resolution down */
	parameters.csty |= 0x01;
	parameters.res_spec = 6;
	for (int i = 0; i < 6; ++i) {
		int const s = i == 5 ? 128 : 256;
		parameters.prcw_init[i] = s;
		parameters.prch_init[i] = s;
	}
	parameters.tp_on = 1;
	parameters.tp_flag = 'R';
	string comment_copy(comment);
	parameters.cp_comment = &comment_copy[0];

	auto codec = opj_create_compress(OPJ_CODEC_J2K);
	if (!codec) {
		opj_image_destroy(image);
		throw EncodeError("could not create OpenJPEG encoder");
	}

	auto cleanup = [&]() {
		opj_destroy_codec(codec);
		opj_image_destroy(image);
	};

	if (!opj_setup_encoder(codec, &parameters, image)) {
		cleanup();
		throw EncodeError("could not set up OpenJPEG encoder");
	}

	char const* options[] = { "TLM=YES", nullptr };
	if (!opj_encoder_set_extra_options(codec, options)) {
		cleanup();
		throw EncodeError("could not enable TLM markers in OpenJPEG");
	}

	auto stream = opj_stream_create_default_file_stream(path.string().c_str(), OPJ_FALSE);
	if (!stream) {
		cleanup();
		throw EncodeError("could not open JPEG 2000 file for writing: " + path.string());
	}

	bool const ok = opj_start_compress(codec, image, stream) && opj_encode(codec, stream) && opj_end_compress(codec, stream);
	opj_stream_destroy(stream);
	cleanup();

	if (!ok) {
		throw EncodeError("could not encode lossless JPEG 2000 image: " + path.string());
	}
}
