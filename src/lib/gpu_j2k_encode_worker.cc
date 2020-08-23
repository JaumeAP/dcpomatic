/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "gpu_j2k_encode_worker.h"
#include "dcp_video.h"
#include "cross.h"
#include "timer.h"
#include "dcpomatic_log.h"
#include <dcp/openjpeg_image.h>
extern "C" {
#include <poznanj2k/config/init_device.h>
#include <poznanj2k/types/image_types.h>
#include <poznanj2k/types/image.h>
#include <poznanj2k/preprocessing/mct.h>
#include <poznanj2k/dwt/dwt.h>
#include <poznanj2k/tier1/quantizer.h>
#include <poznanj2k/tier1/coeff_coder/gpu_coder.h>
#include <poznanj2k/tier2/codestream.h>
#include <poznanj2k/misc/memory_management.cuh>
#include <cuda_runtime_api.h>
}

#include "i18n.h"

using dcp::Data;
using boost::optional;
using boost::shared_ptr;

GPUJ2KEncodeWorker::GPUJ2KEncodeWorker ()
{
	init_device (0);
}

optional<Data>
GPUJ2KEncodeWorker::encode (shared_ptr<DCPVideo> vf)
{
	shared_ptr<dcp::OpenJPEGImage> image = DCPVideo::convert_to_xyz(vf->frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));
	int const width = image->size().width;
	int const height = image->size().height;

	type_image img;
	img.mct_compression_method = 0;
	img.width = width;
	img.height = height;
	img.num_components = 3;
	img.depth = 36;
	img.sign = UNSIGNED;
	/* XXX: 6 for 4K? */
	img.num_dlvls = 5;
	img.wavelet_type = 1;
	img.num_tiles = 1;
	img.tile_w = width;
	img.tile_h = height;
	img.coding_style = CODING_STYLE_PRECINCTS_DEFINED;
	img.prog_order = COMP_POS_RES_LY_PROG;
	img.num_layers = 1;
	img.num_range_bits = 12;
	img.use_mct = 1;
	img.use_part2_mct = 0;

	set_coding_parameters (&img, (vf->j2k_bandwidth() / 8) / vf->frames_per_second());

	init_tiles (&img, width, height, 5, 5);
	type_tile* tile = &(img.tile[0]);

	PeriodTimer encode("poz-encode");

	// XXX: it's a big shame about this int -> float conversion
	for (int i = 0; i < 3; ++i) {
		type_tile_comp* c = &tile->tile_comp[i];
		c->tile_comp_no = i;
		int const pixels = c->width * c->height;
		for (int j = 0; j < pixels; ++j) {
			c->img_data[j] = float (image->data(i)[j]);
		}
		cuda_memcpy_htd (c->img_data, c->img_data_d, pixels * sizeof(type_data));
	}

	mct (&img, 10000, 0.000001, 1.0e-7);
	fwt (tile);
	quantize_tile (tile);
	encode_tile (tile);

	type_buffer buffer;
	init_buffer (&buffer);
	encode_codestream (&buffer, &img);
	cudaThreadSynchronize ();

	image_destroy(&img);

	// XXX: remove this memcpy
	dcp::Data encoded (buffer.bytes_count);
	memcpy (encoded.data().get(), buffer.data, buffer.bytes_count);
	free (buffer.data);
	return encoded;
}

void
GPUJ2KEncodeWorker::log_thread_start ()
{
	LOG_TIMING ("start-encoder-thread thread=%1 GPU", thread_id());
}
