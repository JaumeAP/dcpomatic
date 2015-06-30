/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>
    Adapted from code Copyright (C) 2010-2011 Terrence Meiczinger

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

#include "openjpeg_encoder.h"
#include "data.h"
#include "exceptions.h"
#include <dcp/openjpeg_image.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;

string
OpenJPEGEncoder::name () const
{
	return _("OpenJPEG library");
}

Data
OpenJPEGEncoder::do_encode (shared_ptr<const dcp::OpenJPEGImage> input)
{
	/* Set the max image and component sizes based on frame_rate */
	int max_cs_len = ((float) _bandwidth.get()) / 8 / _frame_rate.get();
	if (_threed.get()) {
		/* In 3D we have only half the normal bandwidth per eye */
		max_cs_len /= 2;
	}
	int const max_comp_size = max_cs_len / 1.25;

	/* get a J2K compressor handle */
	opj_cinfo_t* cinfo = opj_create_compress (CODEC_J2K);
	if (cinfo == 0) {
		throw EncodeError (N_("could not create JPEG2000 encoder"));
	}

	/* Set encoding parameters to default values */
	opj_cparameters_t parameters;
	opj_set_default_encoder_parameters (&parameters);

	/* Set default cinema parameters */
	parameters.tile_size_on = false;
	parameters.cp_tdx = 1;
	parameters.cp_tdy = 1;

	/* Tile part */
	parameters.tp_flag = 'C';
	parameters.tp_on = 1;

	/* Tile and Image shall be at (0,0) */
	parameters.cp_tx0 = 0;
	parameters.cp_ty0 = 0;
	parameters.image_offset_x0 = 0;
	parameters.image_offset_y0 = 0;

	/* Codeblock size = 32x32 */
	parameters.cblockw_init = 32;
	parameters.cblockh_init = 32;
	parameters.csty |= 0x01;

	/* The progression order shall be CPRL */
	parameters.prog_order = CPRL;

	/* No ROI */
	parameters.roi_compno = -1;

	parameters.subsampling_dx = 1;
	parameters.subsampling_dy = 1;

	/* 9-7 transform */
	parameters.irreversible = 1;

	parameters.tcp_rates[0] = 0;
	parameters.tcp_numlayers++;
	parameters.cp_disto_alloc = 1;
	parameters.cp_rsiz = _resolution.get() == RESOLUTION_2K ? CINEMA2K : CINEMA4K;
	if (_resolution.get() == RESOLUTION_4K) {
		parameters.numpocs = 2;
		parameters.POC[0].tile = 1;
		parameters.POC[0].resno0 = 0;
		parameters.POC[0].compno0 = 0;
		parameters.POC[0].layno1 = 1;
		parameters.POC[0].resno1 = parameters.numresolution - 1;
		parameters.POC[0].compno1 = 3;
		parameters.POC[0].prg1 = CPRL;
		parameters.POC[1].tile = 1;
		parameters.POC[1].resno0 = parameters.numresolution - 1;
		parameters.POC[1].compno0 = 0;
		parameters.POC[1].layno1 = 1;
		parameters.POC[1].resno1 = parameters.numresolution;
		parameters.POC[1].compno1 = 3;
		parameters.POC[1].prg1 = CPRL;
	}

	parameters.cp_comment = strdup (N_("DCP-o-matic"));
	parameters.cp_cinema = _resolution.get() == RESOLUTION_2K ? CINEMA2K_24 : CINEMA4K_24;

	/* 3 components, so use MCT */
	parameters.tcp_mct = 1;

	/* set max image */
	parameters.max_comp_size = max_comp_size;
	parameters.tcp_rates[0] = ((float) (3 * input->size().width * input->size().height * 12)) / (max_cs_len * 8);

	/* Set event manager to null (openjpeg 1.3 bug) */
	cinfo->event_mgr = 0;

	/* Setup the encoder parameters using the current image and user parameters */
	opj_setup_encoder (cinfo, &parameters, input->opj_image ());

	opj_cio_t* cio = opj_cio_open ((opj_common_ptr) cinfo, 0, 0);
	if (cio == 0) {
		opj_destroy_compress (cinfo);
		throw EncodeError (N_("could not open JPEG2000 stream"));
	}

	int const r = opj_encode (cinfo, cio, input->opj_image(), 0);
	if (r == 0) {
		opj_cio_close (cio);
		opj_destroy_compress (cinfo);
		throw EncodeError (N_("JPEG2000 encoding failed"));
	}

	Data enc (cio->buffer, cio_tell (cio));

	opj_cio_close (cio);
	free (parameters.cp_comment);
	opj_destroy_compress (cinfo);

	return enc;
}

