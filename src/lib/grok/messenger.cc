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


#include "shm.h"
#include "messenger.h"


using namespace grk_plugin;


auto constexpr CLIENT_TO_GROK_MESSAGE = "Global\\client_to_grok_message";
auto constexpr CLIENT_SENT = "Global\\client_sent";
auto constexpr GROK_RECEIVE_READY = "Global\\grok_receive_ready";
auto constexpr GROK_TO_CLIENT_MESSAGE = "Global\\grok_to_client_message";
auto constexpr GROK_SENT = "Global\\grok_sent";
auto constexpr CLIENT_RECEIVE_READY = "Global\\client_receive_ready";
auto constexpr GROK_UNCOMPRESSED_BUFFER = "Global\\grok_uncompressed_buf";
auto constexpr GROK_COMPRESSED_BUFFER = "Global\\grok_compressed_buf";


Messenger::Messenger(std::function<void(std::string)> processor, size_t numProcessingThreads)
	: _running(true)
	, _frames_scheduled(0)
	, _frames_compressed(0)
	, _processor(processor)
	, _num_processing_threads(numProcessingThreads)
	, _uncompressed_frame_size(0)
	, _compressed_frame_size(0)
	, _num_frames(0)
{
	shm_unlink(GROK_TO_CLIENT_MESSAGE);
	shm_unlink(CLIENT_TO_GROK_MESSAGE);
}


Messenger::~Messenger()
{
	shutdown();

	_running = false;
	_send_queue.deactivate();
	_receive_queue.deactivate();

	if (_outbound_synch) {
		_outbound_synch->post(SYNCH_RECEIVE_READY);
		_outbound.join();
	}

	if (_inbound_synch) {
		_inbound_synch->post(SYNCH_SENT);
		_inbound.join();
	}

	for(auto& p: _processors) {
		p.join();
	}

	delete _outbound_synch;
	delete _inbound_synch;

	deinit_shm();
}


/* One of these is created for each core, to receive messages (e.g. "frame is encoded")
 * from the grok process and handle them.
 */
void
Messenger::processor_thread()
{
	while (_running) {
		std::string message;
		if (!_receive_queue.wait_and_pop(message)) {
			break;
		}

		if (!_running) {
			break;
		}

		Message msg(message);
		auto tag = msg.next();
		if (tag == GRK_MSGR_BATCH_COMPRESS_INIT) {
			auto width = msg.nextUint();
			msg.nextUint(); // stride
			auto height = msg.nextUint();
			auto samples_per_pixel = msg.nextUint();
			msg.nextUint(); // depth
			_uncompressed_frame_size = Messenger::uncompressedFrameSize(width, height, samples_per_pixel);
			auto compressed_frame_size = msg.nextUint();
			auto num_frames = msg.nextUint();
			initClient(_uncompressed_frame_size, compressed_frame_size, num_frames);
		} else if (tag == GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED) {
			reclaimUncompressed(msg.nextUint());
		} else if (tag == GRK_MSGR_BATCH_PROCESSSED_COMPRESSED) {
			reclaimCompressed(msg.nextUint());
		}

		/* Handle writing J2K data to disk */
		_processor(message);
	}
}


void
Messenger::outbound_thread()
{
	int shm_fd = 0;
	char* send_buffer = nullptr;

	if (!init_shm(CLIENT_TO_GROK_MESSAGE, messageBufferLen, &shm_fd, &send_buffer)) {
		return;
	}

	while (_running) {
		_outbound_synch->wait(SYNCH_RECEIVE_READY);
		if (!_running) {
			break;
		}
		std::string message;
		if (!_send_queue.wait_and_pop(message)) {
			break;
		}
		if (!_running) {
			break;
		}
		memcpy(send_buffer, message.c_str(), message.size() + 1);
		_outbound_synch->post(SYNCH_SENT);
	}

	::deinit_shm(CLIENT_TO_GROK_MESSAGE, messageBufferLen, shm_fd, &send_buffer);
}


void
Messenger::inbound_thread()
{
	int shm_fd = 0;
	char* receive_buffer = nullptr;

	if (!init_shm(GROK_TO_CLIENT_MESSAGE, messageBufferLen, &shm_fd, &receive_buffer)) {
		return;
	}

	while (_running) {
		_inbound_synch->wait(SYNCH_SENT);
		if (!_running) {
			break;
		}
		auto message = std::string(receive_buffer);
		_inbound_synch->post(SYNCH_RECEIVE_READY);
		_receive_queue.push(message);
	}

	::deinit_shm(GROK_TO_CLIENT_MESSAGE, messageBufferLen, shm_fd, &receive_buffer);
}


void
Messenger::startThreads()
{
	_outbound_synch = new Synch(CLIENT_SENT, GROK_RECEIVE_READY);
	_outbound = std::thread(&Messenger::outbound_thread, this);

	_inbound_synch = new Synch(GROK_SENT, CLIENT_RECEIVE_READY);
	_inbound = std::thread(&Messenger::inbound_thread, this);

	for (size_t i = 0; i < _num_processing_threads; ++i) {
		_processors.push_back(std::thread(&Messenger::processor_thread, this));
	}
}


bool
Messenger::initBuffers()
{
	bool rc = true;
	if (_uncompressed_frame_size) {
		rc = rc && init_shm(
			GROK_UNCOMPRESSED_BUFFER,
			_uncompressed_frame_size * _num_frames,
			&_uncompressed_fd, &_uncompressed_buffer
		);
	}

	if (_compressed_frame_size) {
		rc = rc && init_shm(
			GROK_COMPRESSED_BUFFER,
			_compressed_frame_size * _num_frames,
			&_compressed_fd, &_compressed_buffer
		);
	}

	return rc;
}


bool
Messenger::deinit_shm()
{
	bool rc = ::deinit_shm(
		GROK_UNCOMPRESSED_BUFFER,
		_uncompressed_frame_size * _num_frames,
		_uncompressed_fd, &_uncompressed_buffer
	);

	rc = rc && ::deinit_shm(
		GROK_COMPRESSED_BUFFER,
		_compressed_frame_size * _num_frames,
		_compressed_fd, &_compressed_buffer
	);

	return rc;
}


bool
Messenger::launch_grok(
	boost::filesystem::path const& dir,
	uint32_t width,
	uint32_t stride,
	uint32_t height,
	uint32_t samplesPerPixel,
	uint32_t depth,
	int device,
	bool is4K,
	uint32_t fps,
	uint32_t bandwidth,
	const std::string server,
	const std::string license
	)
{
	std::unique_lock<std::mutex> lk(_shutdown_mutex);
	if (_async_result.valid()) {
		return true;
	}

	shm_unlink(GROK_TO_CLIENT_MESSAGE);
	shm_unlink(CLIENT_TO_GROK_MESSAGE);

	startThreads();
	char cmd[4096];
	snprintf(cmd, sizeof(cmd),
			"./grk_compress -batch_src %s,%d,%d,%d,%d,%d -out_fmt j2k -k 1 "
			"-G %d -%s %d,%d -j %s -J %s -v",
			GRK_MSGR_BATCH_IMAGE.c_str(), width, stride, height, samplesPerPixel, depth,
			device, is4K ? "cinema4K" : "cinema2K", fps, bandwidth,
			license.c_str(), server.c_str());

	return launch(cmd, dir);
}


void
Messenger::initClient(size_t uncompressedFrameSize, size_t compressedFrameSize, size_t numFrames)
{
	// client fills queue with pending uncompressed buffers
	_uncompressed_frame_size = uncompressedFrameSize;
	_compressed_frame_size = compressedFrameSize;
	_num_frames = numFrames;
	initBuffers();
	auto ptr = _uncompressed_buffer;
	for(size_t i = 0; i < _num_frames; ++i)
	{
		_available_buffers.push(BufferSrc(i, (uint8_t*)ptr));
		ptr += _uncompressed_frame_size;
	}

	std::unique_lock<std::mutex> lk(_shutdown_mutex);
	_initialized = true;
	_client_initialized_condition.notify_all();
}

bool
Messenger::waitForClientInit()
{
	if (_initialized) {
		return true;
	} else if (_shutdown) {
		return false;
	}

	std::unique_lock<std::mutex> lk(_shutdown_mutex);

	if (_initialized) {
		return true;
	} else if (_shutdown) {
		return false;
	}

	while (true) {
		if (_client_initialized_condition.wait_for(lk, std::chrono::seconds(1), [this]{ return _initialized || _shutdown; })) {
			break;
		}
		auto status = _async_result.wait_for(std::chrono::milliseconds(100));
		if (status == std::future_status::ready) {
			getMessengerLogger()->error("Grok exited unexpectedly during initialization");
			return false;
		}
	}

	return _initialized && !_shutdown;
}


size_t
Messenger::uncompressedFrameSize(uint32_t w, uint32_t h, uint32_t samplesPerPixel)
{
	return sizeof(uint16_t) * w * h * samplesPerPixel;
}


void
Messenger::reclaimCompressed(size_t frameId)
{
	_available_buffers.push(BufferSrc(frameId, getCompressedFrame(frameId)));
}


void
Messenger::reclaimUncompressed(size_t frameId)
{
	_available_buffers.push(BufferSrc(frameId, getUncompressedFrame(frameId)));
}


uint8_t*
Messenger::getUncompressedFrame(size_t frameId)
{
	assert(frameId < _num_frames);
	if (frameId >= _num_frames) {
		return nullptr;
	}

	return (uint8_t*)(_uncompressed_buffer + frameId * _uncompressed_frame_size);
}


uint8_t*
Messenger::getCompressedFrame(size_t frameId)
{
	assert(frameId < _num_frames);
	if (frameId >= _num_frames) {
		return nullptr;
	}

	return (uint8_t*)(_compressed_buffer + frameId * _compressed_frame_size);
}

bool
Messenger::launch(std::string const& cmd, boost::filesystem::path const& dir)
{
	// Change the working directory
	if(!dir.empty())
	{
		boost::system::error_code ec;
		boost::filesystem::current_path(dir, ec);
		if (ec) {
			getMessengerLogger()->error("Error: failed to change the working directory");
			return false;
		}
	}
	// Execute the command using std::async and std::system
	getMessengerLogger()->info(cmd.c_str());
	_async_result = std::async(std::launch::async, [cmd]() { return std::system(cmd.c_str()); });
	bool success = _async_result.valid();
	if (!success)
		getMessengerLogger()->error("Grok launch failed");

	return success;
}


bool
Messenger::schedule_compress(DCPVideo const& proxy, std::function<void(BufferSrc const&)> converter)
{
	BufferSrc src;
	if (!_available_buffers.wait_and_pop(src)) {
		return false;
	}
	converter(src);
	_scheduled_frames.store(proxy);
	_frames_scheduled++;
	send(GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED, proxy.index(), src.frame_id);

	return true;
}


/** @param processor function taking compressed J2K data and sending it to the Writer */
void
Messenger::process_compressed(std::string const& message, std::function<void (DCPVideo, uint8_t*, uint32_t)> processor, bool needsRecompression)
{
	Message msg(message);
	msg.next();
	auto const clientFrameId = msg.nextUint();
	auto const compressedFrameId = msg.nextUint();
	auto const compressedFrameLength = msg.nextUint();
	if (!needsRecompression) {
		auto src_frame = _scheduled_frames.retrieve(clientFrameId);
		if (!src_frame) {
			return;
		}
		/* Write the compressed J2K data out */
		processor(*src_frame, getCompressedFrame(compressedFrameId), compressedFrameLength);
	}

	++_frames_compressed;
	send(GRK_MSGR_BATCH_PROCESSSED_COMPRESSED, compressedFrameId);

	if (_shutdown && _frames_compressed == _frames_scheduled) {
		_shutdown_condition.notify_all();
	}
}

void
Messenger::shutdown()
{
	try {
		std::unique_lock<std::mutex> lk(_shutdown_mutex);
		if (!_async_result.valid()) {
			return;
		}
		_shutdown = true;
		if (_frames_scheduled) {
			uint32_t scheduled = _frames_scheduled;
			send(GRK_MSGR_BATCH_FLUSH, scheduled);
			_shutdown_condition.wait(lk, [this] { return _frames_scheduled == _frames_compressed; });
		}
		_available_buffers.deactivate();
		send(GRK_MSGR_BATCH_SHUTDOWN);
		int result = _async_result.get();
		if(result != 0) {
			getMessengerLogger()->error("Accelerator failed with return code: %d\n",result);
		}
	} catch (std::exception &ex) {
		getMessengerLogger()->error("%s",ex.what());
	}
}
