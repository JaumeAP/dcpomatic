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


#include "messenger.h"


using namespace grk_plugin;


Messenger::Messenger(MessengerInit init)
	: _running(true)
	, _init(init)
{

}


Messenger::~Messenger()
{
	_running = false;
	sendQueue.deactivate();
	receiveQueue.deactivate();

	if (outboundSynch_) {
		outboundSynch_->post(SYNCH_RECEIVE_READY);
		outbound.join();
	}

	if (inboundSynch_) {
		inboundSynch_->post(SYNCH_SENT);
		inbound.join();
	}

	for(auto& p: processors_) {
		p.join();
	}

	delete outboundSynch_;
	delete inboundSynch_;

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
		if (!receiveQueue.waitAndPop(message)) {
			break;
		}

		if (!_running) {
			break;
		}

		Msg msg(message);
		auto tag = msg.next();
		if (tag == GRK_MSGR_BATCH_COMPRESS_INIT) {
			auto width = msg.nextUint();
			msg.nextUint(); // stride
			auto height = msg.nextUint();
			auto samples_per_pixel = msg.nextUint();
			msg.nextUint(); // depth
			_init.uncompressedFrameSize_ = Messenger::uncompressedFrameSize(width, height, samples_per_pixel);
			auto compressed_frame_size = msg.nextUint();
			auto num_frames = msg.nextUint();
			initClient(compressed_frame_size, compressed_frame_size, num_frames);
		} else if (tag == GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED) {
			reclaimUncompressed(msg.nextUint());
		} else if (tag == GRK_MSGR_BATCH_PROCESSSED_COMPRESSED) {
			reclaimCompressed(msg.nextUint());
		}
		_init.processor_(message);
	}
}


void
Messenger::outbound_thread(const std::string &sendBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* send_buffer = nullptr;

	if (!SharedMemoryManager::initShm(sendBuf, messageBufferLen, &shm_fd, &send_buffer)) {
		return;
	}

	while (_running) {
		synch->wait(SYNCH_RECEIVE_READY);
		if (!_running) {
			break;
		}
		std::string message;
		if (!sendQueue.waitAndPop(message)) {
			break;
		}
		if (!_running) {
			break;
		}
		memcpy(send_buffer, message.c_str(), message.size() + 1);
		synch->post(SYNCH_SENT);
	}

	SharedMemoryManager::deinit_shm(sendBuf, messageBufferLen, shm_fd, &send_buffer);
}


void
Messenger::inbound_thread(const std::string &receiveBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* receive_buffer = nullptr;

	if (!SharedMemoryManager::initShm(receiveBuf, messageBufferLen, &shm_fd, &receive_buffer)) {
		return;
	}

	while (_running) {
		synch->wait(SYNCH_SENT);
		if (!_running) {
			break;
		}
		auto message = std::string(receive_buffer);
		synch->post(SYNCH_RECEIVE_READY);
		receiveQueue.push(message);
	}

	SharedMemoryManager::deinit_shm(receiveBuf, messageBufferLen, shm_fd, &receive_buffer);
}


void
Messenger::startThreads()
{
	outboundSynch_ = new Synch(_init.outboundSentSynch, _init.outboundReceiveReadySynch);
	outbound = std::thread(&Messenger::outbound_thread, this, _init.outboundMessageBuf, outboundSynch_);

	inboundSynch_ = new Synch(_init.inboundSentSynch, _init.inboundReceiveReadySynch);
	inbound = std::thread(&Messenger::inbound_thread, this, _init.inboundMessageBuf, inboundSynch_);

	for (size_t i = 0; i < _init.numProcessingThreads_; ++i) {
		processors_.push_back(std::thread(&Messenger::processor_thread, this));
	}
}


bool
Messenger::initBuffers()
{
	bool rc = true;
	if (_init.uncompressedFrameSize_) {
		rc = rc && SharedMemoryManager::initShm(
			grokUncompressedBuf,
			_init.uncompressedFrameSize_ * _init.numFrames_,
			&uncompressed_fd_, &uncompressed_buffer_
		);
	}

	if (_init.compressedFrameSize_) {
		rc = rc && SharedMemoryManager::initShm(
			grokCompressedBuf,
			_init.compressedFrameSize_ * _init.numFrames_,
			&compressed_fd_, &compressed_buffer_
		);
	}

	return rc;
}


bool
Messenger::deinit_shm()
{
	bool rc = SharedMemoryManager::deinit_shm(
		grokUncompressedBuf,
		_init.uncompressedFrameSize_ * _init.numFrames_,
		uncompressed_fd_, &uncompressed_buffer_
	);

	rc = rc && SharedMemoryManager::deinit_shm(
		grokCompressedBuf,
		_init.compressedFrameSize_ * _init.numFrames_,
		compressed_fd_, &compressed_buffer_
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
	std::unique_lock<std::mutex> lk(shutdownMutex_);
	if (async_result_.valid()) {
		return true;
	}
	_init.unlink();
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
	_init.uncompressedFrameSize_ = uncompressedFrameSize;
	_init.compressedFrameSize_ = compressedFrameSize;
	_init.numFrames_ = numFrames;
	initBuffers();
	auto ptr = uncompressed_buffer_;
	for(size_t i = 0; i < _init.numFrames_; ++i)
	{
		availableBuffers_.push(BufferSrc(0, i, (uint8_t*)ptr));
		ptr += _init.uncompressedFrameSize_;
	}

	std::unique_lock<std::mutex> lk(shutdownMutex_);
	_initialized = true;
	clientInitializedCondition_.notify_all();
}

bool
Messenger::waitForClientInit()
{
	if (_initialized) {
		return true;
	} else if (_shutdown) {
		return false;
	}

	std::unique_lock<std::mutex> lk(shutdownMutex_);

	if (_initialized) {
		return true;
	} else if (_shutdown) {
		return false;
	}

	while (true) {
		if (clientInitializedCondition_.wait_for(lk, std::chrono::seconds(1), [this]{ return _initialized || _shutdown; })) {
			break;
		}
		auto status = async_result_.wait_for(std::chrono::milliseconds(100));
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
	availableBuffers_.push(BufferSrc(0, frameId, getCompressedFrame(frameId)));
}


void
Messenger::reclaimUncompressed(size_t frameId)
{
	availableBuffers_.push(BufferSrc(0, frameId, getUncompressedFrame(frameId)));
}


uint8_t*
Messenger::getUncompressedFrame(size_t frameId)
{
	assert(frameId < _init.numFrames_);
	if(frameId >= _init.numFrames_)
		return nullptr;

	return (uint8_t*)(uncompressed_buffer_ + frameId * _init.uncompressedFrameSize_);
}


uint8_t*
Messenger::getCompressedFrame(size_t frameId)
{
	assert(frameId < _init.numFrames_);
	if(frameId >= _init.numFrames_)
		return nullptr;

	return (uint8_t*)(compressed_buffer_ + frameId * _init.compressedFrameSize_);
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
	cmd_ = cmd;
	getMessengerLogger()->info(cmd.c_str());
	async_result_ = std::async(std::launch::async, [this]() { return std::system(cmd_.c_str()); });
	bool success = async_result_.valid();
	if (!success)
		getMessengerLogger()->error("Grok launch failed");

	return success;
}
