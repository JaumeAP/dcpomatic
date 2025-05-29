/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#include "context.h"


using namespace grk_plugin;


static std::mutex launchMutex;


GrokContext::GrokContext(DcpomaticContext* dcpomatic_context)
	: _dcpomatic_context(dcpomatic_context)
{
	auto grok = Config::instance()->grok();
	if (!grok.enable) {
		return;
	}

	boost::filesystem::path folder(_dcpomatic_context->location);
	boost::filesystem::path binary_path = folder / "grk_compress";
	if (!boost::filesystem::exists(binary_path)) {
		getGrokLogger()->error(
			"Invalid binary location %s", _dcpomatic_context->location.c_str()
			);
		return;
	}

	auto proc = [this](const std::string& str) {
		try {
			Message msg(str);
			auto tag = msg.next();
			if (tag == GRK_MSGR_BATCH_SUBMIT_COMPRESSED) {
				auto clientFrameId = msg.nextUint();
				msg.nextUint(); // compressed frame ID
				auto compressedFrameLength = msg.nextUint();
				auto processor = [this](DCPVideo srcFrame, uint8_t* compressed, uint32_t compressedFrameLength) {
					auto compressed_data = std::make_shared<dcp::ArrayData>(compressed, compressedFrameLength);
					_dcpomatic_context->writer.write(compressed_data, srcFrame.index(), srcFrame.eyes());
					frame_done ();
				};

				int const minimum_size = 16384;

				/* Write the compressed data out and tidy up */
				bool needsRecompression = compressedFrameLength < minimum_size;
				_messenger->process_compressed(str, processor, needsRecompression);

				if (needsRecompression) {
					/* The JPEG2000 frame data was too small, so handle it with the CPU encoder */
					auto vf = _messenger->retrieve(clientFrameId);
					if (!vf) {
						return;
					}

					auto encoded = std::make_shared<dcp::ArrayData>(vf->encode_locally());
					_dcpomatic_context->writer.write(encoded, vf->index(), vf->eyes());
					frame_done ();
				}
			}
		} catch (std::exception& ex) {
			getGrokLogger()->error("%s",ex.what());
		}
	};

	_messenger = new Messenger(proc, std::thread::hardware_concurrency());
}


GrokContext::~GrokContext()
{
	if (!_messenger) {
		return;
	}

	std::unique_lock<std::mutex> lk_global(launchMutex);

	if (!_messenger) {
		return;
	}

	if (_launched) {
		_messenger->shutdown();
	}

	delete _messenger;
}


bool
GrokContext::launch(DCPVideo dcpv, int device)
{
	namespace fs = boost::filesystem;

	if (!_messenger) {
		return false;
	}
	if (_launched) {
		return true;
	}
	if (_launch_failed) {
		return false;
	}

	std::unique_lock<std::mutex> lk_global(launchMutex);

	if (!_messenger) {
		return false;
	}
	if (_launched) {
		return true;
	}
	if (_launch_failed) {
		return false;
	}

	if (!fs::exists(_dcpomatic_context->location) || !fs::is_directory(_dcpomatic_context->location)) {
		getGrokLogger()->error("Invalid directory %s", _dcpomatic_context->location.c_str());
		return false;
	}

	auto s = dcpv.get_size();
	_dcpomatic_context->set_dimensions(s.width, s.height);
	auto grok = Config::instance()->grok();
	if (!_messenger->launch_grok(
			_dcpomatic_context->location,
			_dcpomatic_context->width,
			_dcpomatic_context->width,
			_dcpomatic_context->height,
			3,
			12,
			device,
			_dcpomatic_context->film->resolution() == Resolution::FOUR_K,
			_dcpomatic_context->film->video_frame_rate(),
			_dcpomatic_context->film->video_bit_rate(VideoEncoding::JPEG2000),
			grok.licence_server,
			grok.licence)) {
		_launch_failed = true;
		return false;
	}

	_launched = _messenger->waitForClientInit();
	_launch_failed = _launched;

	return _launched;
}


bool
GrokContext::schedule_compress(DCPVideo const& vf)
{
	if (!_messenger) {
		return false;
	}

	auto cvt = [&vf](BufferSrc src) {
		vf.convert_to_xyz((uint16_t*) src.frame_ptr);
	};

	return _messenger->schedule_compress(vf, cvt);
}


void
GrokContext::frame_done()
{
	_dcpomatic_context->history.event();
}


GrokLogger* grk_plugin::sLogger = nullptr;


void
grk_plugin::setGrokLogger(grk_plugin::GrokLogger* logger)
{
 	delete sLogger;
 	sLogger = logger;
}


grk_plugin::GrokLogger*
grk_plugin::getGrokLogger()
{
	return sLogger;
}

