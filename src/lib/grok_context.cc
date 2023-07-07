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


#include "grok_context.h"


using namespace grk_plugin;


GrokLogger::GrokLogger(const std::string& preamble)
	: MessengerLogger(preamble)
{

}


void
GrokLogger::info(const char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	dcpomatic_log->log(preamble_ + log_message(fmt, arg), LogEntry::TYPE_GENERAL);
	va_end(arg);
}


void
GrokLogger::warn(const char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	dcpomatic_log->log(preamble_ + log_message(fmt, arg), LogEntry::TYPE_WARNING);
	va_end(arg);
}


void
GrokLogger::error(const char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	dcpomatic_log->log(preamble_ + log_message(fmt, arg), LogEntry::TYPE_ERROR);
	va_end(arg);
}


GrokInitializer::GrokInitializer()
{
	set_messenger_logger(new GrokLogger("[GROK] "));
}


FrameProxy::FrameProxy()
	: FrameProxy(0, Eyes::LEFT, DCPVideo())
{

}


FrameProxy::FrameProxy(int index, Eyes eyes, DCPVideo dcpv)
	: _index(index)
	, _eyes(eyes)
	, _vf(dcpv)
{

}


DcpomaticContext::DcpomaticContext(
	std::shared_ptr<const Film> film_,
	Writer& writer_,
	EventHistory& history_,
	const boost::optional<boost::filesystem::path>& location_
	)
	: film(film_)
	, writer(writer_)
	, history(history_)
	, location(location_)
	, width(0)
	, height(0)
{

}


void
DcpomaticContext::set_dimensions(uint32_t w, uint32_t h)
{
	width = w;
	height = h;
}


GrokContext::GrokContext(const DcpomaticContext& dcpomaticContext)
	: _dcpomatic_context(dcpomaticContext)
	, _messenger(nullptr)
	, _launched(false)
{
	struct CompressedData : public dcp::Data
	{
		explicit CompressedData(int dataLen)
			: data_(new uint8_t[dataLen])
			, dataLen_(dataLen)
		{
		}

		~CompressedData()
		{
			delete[] data_;
		}

		uint8_t const* data() const override
		{
			return data_;
		}

		uint8_t* data() override
		{
			return data_;
		}

		int size() const override
		{
			return dataLen_;
		}

		uint8_t* data_;
		int dataLen_;
	};

	if (Config::instance()->enable_gpu()) {
		DCPOMATIC_ASSERT(_dcpomatic_context.location);
		boost::filesystem::path folder(*_dcpomatic_context.location);
		boost::filesystem::path binaryPath = folder / "grk_compress";
		if (!boost::filesystem::exists(binaryPath)) {
			get_messenger_logger()->error("Invalid binary location %s",
						    _dcpomatic_context.location->c_str());
			return;
		}
		auto proc = [this](const std::string& str) {
			try {
				Msg msg(str);
				auto tag = msg.next();
				if (tag == GRK_MSGR_BATCH_SUBMIT_COMPRESSED) {
					auto clientFrameId = msg.next_uint();
					auto compressedFrameId = msg.next_uint();
					(void)compressedFrameId;
					auto compressedFrameLength = msg.next_uint();
					auto processor =
					    [this](FrameProxy srcFrame, uint8_t* compressed, uint32_t compressedFrameLength) {
						    auto compressedData = std::make_shared<CompressedData>(compressedFrameLength);
						    memcpy(compressedData->data_, compressed, compressedFrameLength);
						    _dcpomatic_context.writer.write(compressedData, srcFrame.index(), srcFrame.eyes());
						    frame_done();
					    };
					int const minimum_size = 16384;
					bool needsRecompression = compressedFrameLength < minimum_size;
					_messenger->process_compressed(str, processor, needsRecompression);
					if (needsRecompression) {
						bool success = false;
						auto fp = _messenger->retrieve(clientFrameId, success);
						if (!success) {
							return;
						}

						auto encoded = std::make_shared<dcp::ArrayData>(fp.vf().encode_locally());
						_dcpomatic_context.writer.write(encoded, fp.vf().index(), fp.vf().eyes());
						frame_done();
					}
				}
			} catch (std::exception& ex) {
				get_messenger_logger()->error("%s", ex.what());
			}
		};
		auto clientInit =
		    MessengerInit(clientToGrokMessageBuf, clientSentSynch, grokReceiveReadySynch,
				  grokToClientMessageBuf, grokSentSynch, clientReceiveReadySynch, proc,
				  std::thread::hardware_concurrency());
		_messenger = new ScheduledMessenger<FrameProxy>(clientInit);
	}
}


GrokContext::~GrokContext()
{
	shutdown();
}


bool
GrokContext::launch(DCPVideo dcpv, int device)
{
	if (!_messenger) {
		return false;
	}
	if (_launched) {
		return true;
	}
	std::unique_lock<std::mutex> lk_global(launchMutex);
	if (!_messenger) {
		return false;
	}
	if (_launched) {
		return true;
	}
	if (MessengerInit::first_launch(true)) {
		auto s = dcpv.get_size();
		_dcpomatic_context.set_dimensions(s.width, s.height);
		auto config = Config::instance();
		DCPOMATIC_ASSERT(_dcpomatic_context.location);
		DCPOMATIC_ASSERT(config->gpu_license_server());
		DCPOMATIC_ASSERT(config->gpu_license());
		_messenger->launch_grok(*_dcpomatic_context.location,
					_dcpomatic_context.width, _dcpomatic_context.width,
					_dcpomatic_context.height,
					3, 12, device,
					_dcpomatic_context.film->resolution() == Resolution::FOUR_K,
					_dcpomatic_context.film->video_frame_rate(),
					_dcpomatic_context.film->j2k_bandwidth(),
					*config->gpu_license_server(),
					config->gpu_license_port(),
					*config->gpu_license());
	}
	_launched = _messenger->wait_for_client_init();

	return _launched;
}


bool
GrokContext::schedule_compress(const DCPVideo& vf)
{
	if (!_messenger) {
		return false;
	}

	auto fp = FrameProxy(vf.index(), vf.eyes(), vf);
	auto cvt = [this, &fp](BufferSrc src) {
		// xyz conversion
		fp.vf().convert_to_xyz((uint16_t*)src.framePtr_);
	};
	return _messenger->schedule_compress(fp, cvt);
}

void
GrokContext::shutdown()
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
	_messenger = nullptr;
}


void
GrokContext::frame_done()
{
	_dcpomatic_context.history.event();
}


