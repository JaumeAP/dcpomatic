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


#include "constants.h"
#include "exceptions.h"
#include "image.h"
#include "ffmpeg_wrapper.h"
#include "vp9_encoder.h"
#include "util.h"
#include <dcp/raw_convert.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <boost/thread.hpp>


#include "i18n.h"


using std::shared_ptr;
using std::string;
using std::vector;
using dcp::raw_convert;
using namespace dcpomatic;


int
write_packet(void* opaque, uint8_t const* buffer, int size)
{
	/* Here comes the webm header from the format context */
	auto& header = reinterpret_cast<VP9Encoder*>(opaque)->_header;
	header.resize(size);
	memcpy(header.data(), buffer, size);
	return 0;
}


VP9Encoder::VP9Encoder()
{
	av_log_set_callback(ffmpeg_log_callback);

	auto codec = avcodec_find_encoder_by_name("libvpx-vp9");
	if (!codec) {
		throw EncodeError("avcodec_find_encoder_by_name failed for libvpx-vp9");
	}

	_codec_context = avcodec_alloc_context3(codec);
	if (!_codec_context) {
		throw std::bad_alloc();
	}

	_codec_context->width = SIGN_LANGUAGE_WIDTH;
	_codec_context->height = SIGN_LANGUAGE_HEIGHT;
	_codec_context->time_base = AVRational{1, SIGN_LANGUAGE_VIDEO_FRAME_RATE};
	_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
	// We need to take up the same bits as the equivalent length of audio
	// For some reason the ISDCF example script divides that bitrate by 2;
	// maybe to give some overhead?
	_codec_context->bit_rate = SIGN_LANGUAGE_AUDIO_FRAME_RATE * AUDIO_BYTES_PER_SAMPLE * 8 / 2;

	/* All these options are taken from
	 * https://github.com/ISDCF/Sign-Language-Video-Encoding/blob/master/encode-vp9-wav
	 */

	AVDictionary* options = nullptr;

	auto constexpr chunk_length_in_video_frames = SIGN_LANGUAGE_CHUNK_LENGTH_IN_SECONDS * SIGN_LANGUAGE_VIDEO_FRAME_RATE;

	av_dict_set_int(&options, "keyint_min", chunk_length_in_video_frames, 0);
	// GOP size
	av_dict_set_int(&options, "g", chunk_length_in_video_frames, 0);
	av_dict_set_int(&options, "speed", 6, 0);
	av_dict_set_int(&options, "tile-columns", 4, 0);
	av_dict_set_int(&options, "frame-parallel", 1, 0);
	av_dict_set_int(&options, "static-thresh", 0, 0);
	av_dict_set_int(&options, "max-intra-rate", 300, 0);
	av_dict_set(&options, "deadline", "realtime", 0);
	av_dict_set_int(&options, "lag-in-frames", 0, 0);
	av_dict_set_int(&options, "error-resilient", 1, 0);
	av_dict_set_int(&options, "minrate", _codec_context->bit_rate, 0);
	av_dict_set_int(&options, "maxrate", _codec_context->bit_rate, 0);

	int r = avcodec_open2(_codec_context, codec, &options);
	if (r < 0) {
		throw EncodeError(N_("avcodec_open2"), N_("VP9Encoder::VP9Encoder"));
	}

	vector<uint8_t> header_buffer(32768);
	auto avio_context = avio_alloc_context(header_buffer.data(), header_buffer.size(), 1, this, nullptr, write_packet, nullptr);
	if (!avio_context) {
		throw std::bad_alloc();
	}
	AVFormatContext* format_context;
	r = avformat_alloc_output_context2(&format_context, nullptr, "webm", nullptr);
	if (r < 0) {
		throw EncodeError(N_("avformat_alloc_output_context2"), N_("VP9Encoder::VP9Encoder"));
	}
	format_context->pb = avio_context;

	av_dict_set(&format_context->metadata, "MAJOR_BRAND", "isom", 0);
	av_dict_set(&format_context->metadata, "MINOR_VERSION", "512", 0);
	av_dict_set(&format_context->metadata, "COMPATIBLE_BRANDS", "isomiso2avc1mp41", 0);
	av_dict_set(&format_context->metadata, "ENCODER", "Lavf59.27.100", 0);

	auto stream = avformat_new_stream(format_context, codec);
	if (!stream) {
		throw EncodeError(N_("avformat_new_stream"), N_("VP9Encoder::VP9Encoder"));
	}

	stream->disposition |= AV_DISPOSITION_DEFAULT;
	r = avcodec_parameters_from_context(stream->codecpar, _codec_context);
	if (r < 0) {
		throw EncodeError(N_("avcodec_parameters_from_context"), N_("VP9Encoder::VP9Encoder"), r);
	}
	stream->avg_frame_rate = { 24, 1 };
	stream->codecpar->color_range = AVCOL_RANGE_MPEG;
	stream->codecpar->chroma_location = AVCHROMA_LOC_LEFT;
	stream->codecpar->field_order = AV_FIELD_PROGRESSIVE;

	av_dict_set(&stream->metadata, "ENCODER", "Lavc59.37.100 libvpx-vp9", 0);
	av_dict_set(&stream->metadata, "HANDLER_NAME", "VideoHandler", 0);
	av_dict_set(&stream->metadata, "VENDOR_ID", "[0][0][0][0]", 0);

	AVDictionary* format_options = nullptr;
	r = avformat_write_header(format_context, &format_options);
	if (r < 0) {
		throw EncodeError(N_("avformat_write_header"), N_("VP9Encoder::VP9Encoder"));
	}

	avformat_free_context(format_context);
}


VP9Encoder::~VP9Encoder()
{
	avcodec_free_context(&_codec_context);
}


void
VP9Encoder::encode(shared_ptr<const Image> image, DCPTime time)
{
	DCPOMATIC_ASSERT(image->pixel_format() == AV_PIX_FMT_YUV420P);
	DCPOMATIC_ASSERT(image->size() == dcp::Size(SIGN_LANGUAGE_WIDTH, SIGN_LANGUAGE_HEIGHT));

	auto frame = av_frame_alloc();
	if (!frame) {
		throw std::bad_alloc();
	}

	for (int i = 0; i < 3; ++i) {
		auto buffer = _pending_images.create_buffer(image, i);
		frame->buf[i] = av_buffer_ref(buffer);
		frame->data[i] = buffer->data;
		frame->linesize[i] = image->stride()[i];
		av_buffer_unref(&buffer);
	}

	frame->width = image->size().width;
	frame->height = image->size().height;
	frame->format = image->pixel_format();
	frame->pts = time.get() * SIGN_LANGUAGE_VIDEO_FRAME_RATE / DCPTime::HZ;

	int r = avcodec_send_frame(_codec_context, frame);
	av_frame_free(&frame);
	if (r < 0) {
		throw EncodeError(N_("avcodec_send_frame"), N_("VP9Encoder::video"), r);
	}

	ffmpeg::Packet packet;
	r = avcodec_receive_packet(_codec_context, packet.get());
	if (r < 0 && r != AVERROR(EAGAIN)) {
		throw EncodeError(N_("avcodec_receive_packet"), N_("VP9Encoder::video"), r);
	} else if (r >= 0) {
		_pcm_blocks.emplace_back(std::make_pair(time, VP9PCMBlock(packet.get()->data, packet.get()->size, _header)));
		_end = time + DCPTime::from_seconds(SIGN_LANGUAGE_CHUNK_LENGTH_IN_SECONDS);
	}
}

