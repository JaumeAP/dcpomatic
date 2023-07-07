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
#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#include <tlhelp32.h>
#include <windows.h>
#pragma warning(disable : 4100)
#else
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace grk_plugin {
static std::string grokToClientMessageBuf = "Global\\grok_to_client_message";
static std::string grokSentSynch = "Global\\grok_sent";
static std::string clientReceiveReadySynch = "Global\\client_receive_ready";
static std::string clientToGrokMessageBuf = "Global\\client_to_grok_message";
static std::string clientSentSynch = "Global\\client_sent";
static std::string grokReceiveReadySynch = "Global\\grok_receive_ready";
static std::string grokUncompressedBuf = "Global\\grok_uncompressed_buf";
static std::string grokCompressedBuf = "Global\\grok_compressed_buf";
static const std::string GRK_MSGR_BATCH_IMAGE = "GRK_MSGR_BATCH_IMAGE";
static const std::string GRK_MSGR_BATCH_COMPRESS_INIT = "GRK_MSGR_BATCH_COMPRESS_INIT";
static const std::string GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED = "GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED";
static const std::string GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED =
    "GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED";
static const std::string GRK_MSGR_BATCH_SUBMIT_COMPRESSED = "GRK_MSGR_BATCH_SUBMIT_COMPRESSED";
static const std::string GRK_MSGR_BATCH_PROCESSSED_COMPRESSED =
    "GRK_MSGR_BATCH_PROCESSSED_COMPRESSED";
static const std::string GRK_MSGR_BATCH_SHUTDOWN = "GRK_MSGR_BATCH_SHUTDOWN";
static const std::string GRK_MSGR_BATCH_FLUSH = "GRK_MSGR_BATCH_FLUSH";
static const size_t messageBufferLen = 256;

struct IMessengerLogger
{
	virtual ~IMessengerLogger() = default;
	virtual void info(const char* fmt, ...) = 0;
	virtual void warn(const char* fmt, ...) = 0;
	virtual void error(const char* fmt, ...) = 0;

protected:
	template <typename... Args>
	std::string log_message(char const* const format, Args&... args) noexcept
	{
		constexpr size_t message_size = 512;
		char message[message_size];

		std::snprintf(message, message_size, format, args...);
		return std::string(message);
	}
};

struct MessengerLogger : public IMessengerLogger
{
	explicit MessengerLogger(const std::string& preamble)
		: preamble_(preamble)
	{
	}

	virtual ~MessengerLogger() = default;

	virtual void info(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stdout, new_fmt.c_str(), args);
		va_end(args);
	}

	virtual void warn(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stdout, new_fmt.c_str(), args);
		va_end(args);
	}

	virtual void error(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stderr, new_fmt.c_str(), args);
		va_end(args);
	}

protected:
	std::string preamble_;
};

static IMessengerLogger* sLogger = nullptr;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
static void
setMessengerLogger(IMessengerLogger* logger)
{
	delete sLogger;
	sLogger = logger;
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
static IMessengerLogger*
getMessengerLogger()
{
	return sLogger;
}

struct MessengerInit
{
	MessengerInit(const std::string& outBuf, const std::string& outSent,
	              const std::string& outReceiveReady, const std::string& inBuf,
	              const std::string& inSent,
	              const std::string& inReceiveReady,
	              std::function<void(std::string)> processor,
	              size_t numProcessingThreads)
		: outboundMessageBuf(outBuf)
		, outboundSentSynch(outSent)
		, outboundReceiveReadySynch(outReceiveReady)
		, inboundMessageBuf(inBuf)
		, inboundSentSynch(inSent)
		, inboundReceiveReadySynch(inReceiveReady)
		, processor_(processor)
		, numProcessingThreads_(numProcessingThreads)
		, uncompressedFrameSize_(0)
		, compressedFrameSize_(0)
		, numFrames_(0)
	{
		if (first_launch(true)) {
			unlink();
}
	}

	void unlink()
	{
#ifndef _WIN32
		shm_unlink(grokToClientMessageBuf.c_str());
		shm_unlink(clientToGrokMessageBuf.c_str());
#endif
	}

	static bool first_launch(bool isClient)
	{
		bool debugGrok = false;
		return debugGrok != isClient;
	}

	std::string outboundMessageBuf;
	std::string outboundSentSynch;
	std::string outboundReceiveReadySynch;

	std::string inboundMessageBuf;
	std::string inboundSentSynch;
	std::string inboundReceiveReadySynch;

	std::function<void(std::string)> processor_;
	size_t numProcessingThreads_;

	size_t uncompressedFrameSize_;
	size_t compressedFrameSize_;
	size_t numFrames_;
};

/*************************** Synchronization *******************************/
enum SynchDirection
{
	SYNCH_SENT,
	SYNCH_RECEIVE_READY
};

typedef int grk_handle;

struct Synch
{
	Synch(const std::string& sentSemName, const std::string& receiveReadySemName)
		: _sent_sem_name(sentSemName)
		, _receive_ready_sem_name(receiveReadySemName)
	{
		// unlink semaphores in case of previous crash
		if (MessengerInit::first_launch(true)) {
			unlink();
}
		open();
	}

	~Synch()
	{
		close();
		if (MessengerInit::first_launch(true)) {
			unlink();
}
	}

	void post(SynchDirection dir)
	{
		auto sem = (dir == SYNCH_SENT ? sentSem_ : receiveReadySem_);
		int rc = sem_post(sem);
		if (rc) {
			getMessengerLogger()->error("Error posting to semaphore: %s", strerror(errno));
}
	}

	void wait(SynchDirection dir)
	{
		auto sem = dir == SYNCH_SENT ? sentSem_ : receiveReadySem_;
		int rc = sem_wait(sem);
		if (rc) {
			getMessengerLogger()->error("Error waiting for semaphore: %s", strerror(errno));
}
	}

	void open()
	{
		sentSem_ = sem_open(_sent_sem_name.c_str(), O_CREAT, 0666, 0);
		if (!sentSem_) {
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
}
		receiveReadySem_ = sem_open(_receive_ready_sem_name.c_str(), O_CREAT, 0666, 1);
		if (!receiveReadySem_) {
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
}
	}

	void close()
	{
		int rc = sem_close(sentSem_);
		if (rc) {
			getMessengerLogger()->error("Error closing semaphore %s: %s", _sent_sem_name.c_str(),
			                            strerror(errno));
}
		rc = sem_close(receiveReadySem_);
		if (rc) {
			getMessengerLogger()->error("Error closing semaphore %s: %s",
			                            _receive_ready_sem_name.c_str(), strerror(errno));
}
	}

	void unlink()
	{
		int rc = sem_unlink(_sent_sem_name.c_str());
		if (rc == -1 && errno != ENOENT) {
			getMessengerLogger()->error("Error unlinking semaphore %s: %s", _sent_sem_name.c_str(),
			                            strerror(errno));
}
		rc = sem_unlink(_receive_ready_sem_name.c_str());
		if (rc == -1 && errno != ENOENT) {
			getMessengerLogger()->error("Error unlinking semaphore %s: %s",
			                            _receive_ready_sem_name.c_str(), strerror(errno));
}
	}

	sem_t* sentSem_;
	sem_t* receiveReadySem_;

private:
	std::string _sent_sem_name;
	std::string _receive_ready_sem_name;
};

struct SharedMemoryManager
{
	static bool init_shm(const std::string& name, size_t len, grk_handle* shm_fd, char** buffer)
	{
		*shm_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
		if (*shm_fd < 0) {
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
			return false;
		}
		int rc = ftruncate(*shm_fd, sizeof(char) * len);
		if (rc) {
			getMessengerLogger()->error("Error truncating shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if (rc) {
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
}
			rc = shm_unlink(name.c_str());
			if (rc) {
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
}
			return false;
		}
		*buffer = static_cast<char*>(mmap(0, len, PROT_WRITE, MAP_SHARED, *shm_fd, 0));
		if (!*buffer) {
			getMessengerLogger()->error("Error mapping shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if (rc) {
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
}
			rc = shm_unlink(name.c_str());
			if (rc) {
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
}
		}

		return *buffer != nullptr;
	}

	static bool deinit_shm(const std::string& name, size_t len, grk_handle& shm_fd, char** buffer)
	{
		if (!*buffer || !shm_fd) {
			return true;
}

		int rc = munmap(*buffer, len);
		*buffer = nullptr;
		if (rc) {
			getMessengerLogger()->error("Error unmapping shared memory %s: %s", name.c_str(), strerror(errno));
}
		rc = close(shm_fd);
		shm_fd = 0;
		if (rc) {
			getMessengerLogger()->error("Error closing shared memory %s: %s", name.c_str(), strerror(errno));
}
		rc = shm_unlink(name.c_str());
		if (rc) {
			fprintf(stderr, "Error unlinking shared memory %s : %s\n", name.c_str(), strerror(errno));
}

		return true;
	}
};

template <typename Data>
class MessengerBlockingQueue
{
public:
	explicit MessengerBlockingQueue(size_t max)
		: _active(true)
		, _max_size(max)
	{
	}

	MessengerBlockingQueue()
		: MessengerBlockingQueue(UINT_MAX)
	{
	}

	size_t size() const
	{
		return _queue.size();
	}

	// deactivate and clear queue
	void deactivate()
	{
		{
			std::lock_guard<std::mutex> lk(_mutex);
			_active = false;
			while (!_queue.empty()) {
				_queue.pop();
}
		}

		// release all waiting threads
		_can_pop.notify_all();
		_can_push.notify_all();
	}

	void activate()
	{
		std::lock_guard<std::mutex> lk(_mutex);
		_active = true;
	}

	bool push(Data const& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(_mutex);
			rc = _push(value);
		}
		if (rc) {
			_can_pop.notify_one();
}

		return rc;
	}

	bool wait_and_push(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(_mutex);
			if (!_active) {
				return false;
}
			// in case of spurious wakeup, loop until predicate in lambda
			// is satisfied.
			_can_push.wait(lk, [this] { return _queue.size() < _max_size || !_active; });
			rc = _push(value);
		}
		if (rc) {
			_can_pop.notify_one();
}

		return rc;
	}

	bool pop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(_mutex);
			rc = _pop(value);
		}
		if (rc) {
			_can_push.notify_one();
}

		return rc;
	}

	bool wait_and_pop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(_mutex);
			if (!_active) {
				return false;
}
			// in case of spurious wakeup, loop until predicate in lambda
			// is satisfied.
			_can_pop.wait(lk, [this] { return !_queue.empty() || !_active; });
			rc = _pop(value);
		}
		if (rc) {
			_can_push.notify_one();
}

		return rc;
	}

private:
	bool _push(Data const& value)
	{
		if (_queue.size() == _max_size || !_active) {
			return false;
}
		_queue.push(value);

		return true;
	}

	bool _pop(Data& value)
	{
		if (_queue.empty() || !_active) {
			return false;
}
		value = _queue.front();
		_queue.pop();

		return true;
	}

	std::queue<Data> _queue;
	mutable std::mutex _mutex;
	std::condition_variable _can_pop;
	std::condition_variable _can_push;
	bool _active;
	size_t _max_size;
};

struct BufferSrc
{
	BufferSrc()
		: BufferSrc("")
	{
	}

	explicit BufferSrc(const std::string& file)
		: file_(file)
		, clientFrameId_(0)
		, frameId_(0)
		, framePtr_(nullptr)
	{
	}

	BufferSrc(size_t clientFrameId, size_t frameId, uint8_t* framePtr)
		: file_("")
		, clientFrameId_(clientFrameId)
		, frameId_(frameId)
		, framePtr_(framePtr)
	{
	}

	bool from_disk()
	{
		return !file_.empty() && framePtr_ == nullptr;
	}

	size_t index() const
	{
		return clientFrameId_;
	}

	std::string file_;
	size_t clientFrameId_;
	size_t frameId_;
	uint8_t* framePtr_;
};

struct Messenger;
static void
outboundThread(Messenger* messenger, const std::string& sendBuf, Synch* synch);
static void
inboundThread(Messenger* messenger, const std::string& receiveBuf, Synch* synch);
static void
processorThread(Messenger* messenger, std::function<void(std::string)> processor);

struct Messenger
{
	explicit Messenger(MessengerInit init)
		: running(true)
		, initialized_(false)
		, shutdown_(false)
		, init_(init)
		, _outbound_synch(nullptr)
		, _inbound_synch(nullptr)
		, _uncompressed_buffer(nullptr)
		, _compressed_buffer(nullptr)
		, _uncompressed_fd(0)
		, _compressed_fd(0)
	{
	}

	virtual ~Messenger()
	{
		running = false;
		sendQueue.deactivate();
		receiveQueue.deactivate();

		if (_outbound_synch) {
			_outbound_synch->post(SYNCH_RECEIVE_READY);
			_outbound.join();
		}

		if (_inbound_synch) {
			_inbound_synch->post(SYNCH_SENT);
			_inbound.join();
		}

		for (auto& p : _processors) {
			p.join();
}

		delete _outbound_synch;
		delete _inbound_synch;

		deinit_shm();
	}

	void start_threads()
	{
		_outbound_synch =
		    new Synch(init_.outboundSentSynch, init_.outboundReceiveReadySynch);
		_outbound = std::thread(outboundThread, this, init_.outboundMessageBuf, _outbound_synch);

		_inbound_synch =
		    new Synch(init_.inboundSentSynch, init_.inboundReceiveReadySynch);
		_inbound = std::thread(inboundThread, this, init_.inboundMessageBuf, _inbound_synch);

		for (size_t i = 0; i < init_.numProcessingThreads_; ++i) {
			_processors.push_back(std::thread(processorThread, this, init_.processor_));
}
	}

	size_t serialize(const std::string& dir, size_t clientFrameId, uint8_t* compressedPtr,
	                 size_t compressedLength)
	{
		char fname[512];
		if (!compressedPtr || !compressedLength) {
			return 0;
}
		sprintf(fname, "%s/test_%d.j2k", dir.c_str(), (int)clientFrameId);
		auto fp = fopen(fname, "wb");
		if (!fp) {
			return 0;
}
		size_t written = fwrite(compressedPtr, 1, compressedLength, fp);
		if (written != compressedLength) {
			fclose(fp);
			return 0;
		}
		fflush(fp);
		fclose(fp);

		return written;
	}

	bool init_buffers()
	{
		bool rc = true;
		if (init_.uncompressedFrameSize_) {
			rc = rc && SharedMemoryManager::init_shm(grokUncompressedBuf,
			                                        init_.uncompressedFrameSize_ * init_.numFrames_,
			                                        &_uncompressed_fd, &_uncompressed_buffer);
		}
		if (init_.compressedFrameSize_) {
			rc = rc && SharedMemoryManager::init_shm(grokCompressedBuf,
			                                        init_.compressedFrameSize_ * init_.numFrames_,
			                                        &_compressed_fd, &_compressed_buffer);
		}

		return rc;
	}

	bool deinit_shm()
	{
		bool rc = SharedMemoryManager::deinit_shm(grokUncompressedBuf,
		                                         init_.uncompressedFrameSize_ * init_.numFrames_,
		                                         _uncompressed_fd, &_uncompressed_buffer);
		rc = rc && SharedMemoryManager::deinit_shm(grokCompressedBuf,
		                                          init_.compressedFrameSize_ * init_.numFrames_,
		                                          _compressed_fd, &_compressed_buffer);

		return rc;
	}

	template <typename... Args>
	void send(const std::string& str, Args... args)
	{
		std::ostringstream oss;
		oss << str;
		int dummy[] = {0, ((void)(oss << ',' << args), 0)...};
		static_cast<void>(dummy);

		sendQueue.push(oss.str());
	}

	static pid_t get_pid_by_process_name(const char* name)
	{
		char command[256];
		snprintf(command, sizeof(command), "pgrep %s", name);
		auto pgrep = popen(command, "r");
		if (!pgrep) {
			return -1;
}
		pid_t pid;
		if (fscanf(pgrep, "%d", &pid) != 1) {
			pid = -1;
}
		pclose(pgrep);

		return pid;
	}

	static bool terminate_process(const char* name)
	{
		auto pid = get_pid_by_process_name(name);

		return (pid != -1 && kill(pid, SIGTERM) != -1);
	}

	static bool kill_process(const char* name)
	{
		auto pid = get_pid_by_process_name(name);

		return (pid != -1 && kill(pid, SIGKILL) != -1);
	}

	void launch_grok(const std::string& dir, uint32_t width, uint32_t stride,
	                uint32_t height, uint32_t samplesPerPixel, uint32_t depth,
	                int device, bool is4K, uint32_t fps, uint32_t bandwidth,
	                const std::string server, uint32_t port,
	                const std::string license)
	{

		std::unique_lock<std::mutex> lk(shutdownMutex_);
		if (async_result_.valid()) {
			return;
}
		if (MessengerInit::first_launch(true)) {
			init_.unlink();
}
		start_threads();
		char _cmd[4096];
		auto fullServer = server + ":" + std::to_string(port);
		sprintf(_cmd,
		        "./grk_compress -batch_src %s,%d,%d,%d,%d,%d -out_fmt j2k -out_dir - -k 1 "
		        "-G %d -%s %d,%d -j %s -J %s",
		        GRK_MSGR_BATCH_IMAGE.c_str(), width, stride, height, samplesPerPixel, depth,
		        device, is4K ? "cinema4K" : "cinema2K", fps, bandwidth,
		        license.c_str(), fullServer.c_str());
		launch(_cmd, dir);
	}

	void init_client(size_t uncompressedFrameSize, size_t compressedFrameSize, size_t numFrames)
	{
		// client fills queue with pending uncompressed buffers
		init_.uncompressedFrameSize_ = uncompressedFrameSize;
		init_.compressedFrameSize_ = compressedFrameSize;
		init_.numFrames_ = numFrames;
		init_buffers();
		auto ptr = _uncompressed_buffer;
		for (size_t i = 0; i < init_.numFrames_; ++i) {
			availableBuffers_.push(BufferSrc(0, i, (uint8_t*)ptr));
			ptr += init_.uncompressedFrameSize_;
		}

		std::unique_lock<std::mutex> lk(shutdownMutex_);
		initialized_ = true;
		clientInitializedCondition_.notify_all();
	}

	bool wait_for_client_init()
	{
		if (initialized_) {
			return true;
}

		std::unique_lock<std::mutex> lk(shutdownMutex_);
		if (initialized_) {
			return true;
		} else if (shutdown_) {
			return false;
}
		clientInitializedCondition_.wait(lk, [this] { return initialized_ || shutdown_; });

		return initialized_ && !shutdown_;
	}

	static size_t uncompressed_frame_size(uint32_t w, uint32_t h, uint32_t samplesPerPixel)
	{
		return sizeof(uint16_t) * w * h * samplesPerPixel;
	}

	void reclaim_compressed(size_t frameId)
	{
		availableBuffers_.push(BufferSrc(0, frameId, get_compressed_frame(frameId)));
	}

	void reclaim_uncompressed(size_t frameId)
	{
		availableBuffers_.push(BufferSrc(0, frameId, get_uncompressed_frame(frameId)));
	}

	uint8_t* get_uncompressed_frame(size_t frameId)
	{
		assert(frameId < init_.numFrames_);
		if (frameId >= init_.numFrames_) {
			return nullptr;
}

		return (uint8_t*)(_uncompressed_buffer + frameId * init_.uncompressedFrameSize_);
	}

	uint8_t* get_compressed_frame(size_t frameId)
	{
		assert(frameId < init_.numFrames_);
		if (frameId >= init_.numFrames_) {
			return nullptr;
}

		return (uint8_t*)(_compressed_buffer + frameId * init_.compressedFrameSize_);
	}

	std::atomic_bool running;
	bool initialized_;
	bool shutdown_;
	MessengerBlockingQueue<std::string> sendQueue;
	MessengerBlockingQueue<std::string> receiveQueue;
	MessengerBlockingQueue<BufferSrc> availableBuffers_;
	MessengerInit init_;
	std::string cmd_;
	std::future<int> async_result_;
	std::mutex shutdownMutex_;
	std::condition_variable shutdownCondition_;

protected:
	std::condition_variable clientInitializedCondition_;

private:
	void launch(const std::string& cmd, const std::string& dir)
	{
		// Change the working directory
		if (!dir.empty()) {
			if (chdir(dir.c_str()) != 0) {
				getMessengerLogger()->error("Error: failed to change the working directory");
				return;
			}
		}
		// Execute the command using std::async and std::system
		cmd_ = cmd;
		async_result_ = std::async(std::launch::async, [this]() { return std::system(cmd_.c_str()); });
	}

	std::thread _outbound;
	Synch* _outbound_synch;

	std::thread _inbound;
	Synch* _inbound_synch;

	std::vector<std::thread> _processors;
	char* _uncompressed_buffer;
	char* _compressed_buffer;

	grk_handle _uncompressed_fd;
	grk_handle _compressed_fd;
};

static void
outboundThread(Messenger* messenger, const std::string& sendBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* send_buffer = nullptr;

	if (!SharedMemoryManager::init_shm(sendBuf, messageBufferLen, &shm_fd, &send_buffer)) {
		return;
}
	while (messenger->running) {
		synch->wait(SYNCH_RECEIVE_READY);
		if (!messenger->running) {
			break;
}
		std::string message;
		if (!messenger->sendQueue.wait_and_pop(message)) {
			break;
}
		if (!messenger->running) {
			break;
}
		memcpy(send_buffer, message.c_str(), message.size() + 1);
		synch->post(SYNCH_SENT);
	}
	SharedMemoryManager::deinit_shm(sendBuf, messageBufferLen, shm_fd, &send_buffer);
}

static void
inboundThread(Messenger* messenger, const std::string& receiveBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* receive_buffer = nullptr;

	if (!SharedMemoryManager::init_shm(receiveBuf, messageBufferLen, &shm_fd, &receive_buffer)) {
		return;
}
	while (messenger->running) {
		synch->wait(SYNCH_SENT);
		if (!messenger->running) {
			break;
}
		auto message = std::string(receive_buffer);
		synch->post(SYNCH_RECEIVE_READY);
		messenger->receiveQueue.push(message);
	}
	SharedMemoryManager::deinit_shm(receiveBuf, messageBufferLen, shm_fd, &receive_buffer);
}

struct Msg
{
	explicit Msg(const std::string& msg)
		: ct_(0)
	{
		std::stringstream ss(msg);
		while (ss.good()) {
			std::string substr;
			std::getline(ss, substr, ',');
			cs_.push_back(substr);
		}
	}

	std::string next()
	{
		if (ct_ == cs_.size()) {
			getMessengerLogger()->error("Msg: comma separated list exhausted. returning empty.");
			return "";
		}
		return cs_[ct_++];
	}

	uint32_t next_uint()
	{
		return (uint32_t)std::stoi(next());
	}

	std::vector<std::string> cs_;
	size_t ct_;
};

static void
processorThread(Messenger* messenger, std::function<void(std::string)> processor)
{
	while (messenger->running) {
		std::string message;
		if (!messenger->receiveQueue.wait_and_pop(message)) {
			break;
}
		if (!messenger->running) {
			break;
}
		Msg msg(message);
		auto tag = msg.next();
		if (tag == GRK_MSGR_BATCH_COMPRESS_INIT) {
			auto width = msg.next_uint();
			auto stride = msg.next_uint();
			(void)stride;
			auto height = msg.next_uint();
			auto samplesPerPixel = msg.next_uint();
			auto depth = msg.next_uint();
			(void)depth;
			messenger->init_.uncompressedFrameSize_ =
			    Messenger::uncompressed_frame_size(width, height, samplesPerPixel);
			auto compressedFrameSize = msg.next_uint();
			auto numFrames = msg.next_uint();
			messenger->init_client(compressedFrameSize, compressedFrameSize, numFrames);
		} else if (tag == GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED) {
			messenger->reclaim_uncompressed(msg.next_uint());
		} else if (tag == GRK_MSGR_BATCH_PROCESSSED_COMPRESSED) {
			messenger->reclaim_compressed(msg.next_uint());
		}
		processor(message);
	}
}

template <typename F>
struct ScheduledFrames
{
	void store(F& val)
	{
		std::unique_lock<std::mutex> lk(_map_mutex);
		auto it = _map.find(val.index());
		if (it == _map.end()) {
			_map[val.index()] = val;
}
	}

	F retrieve(size_t index, bool& success)
	{
		std::unique_lock<std::mutex> lk(_map_mutex);
		success = false;
		auto it = _map.find(index);
		if (it == _map.end()) {
			return F();
}

		success = true;
		F val = it->second;
		_map.erase(index);

		return val;
	}

private:
	std::mutex _map_mutex;
	std::map<size_t, F> _map;
};

template <typename F>
struct ScheduledMessenger : public Messenger
{
	explicit ScheduledMessenger(MessengerInit init)
		: Messenger(init)
		, _frames_scheduled(0)
		, _frames_compressed(0)
	{
	}

	~ScheduledMessenger()
	{
		shutdown();
	}

	bool schedule_compress(F proxy, std::function<void(BufferSrc)> converter)
	{
		size_t frameSize = init_.uncompressedFrameSize_;
		assert(frameSize >= init_.uncompressedFrameSize_);
		BufferSrc src;
		if (!availableBuffers_.wait_and_pop(src)) {
			return false;
}
		converter(src);
		_scheduled_frames.store(proxy);
		_frames_scheduled++;
		send(GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED, proxy.index(), src.frameId_);

		return true;
	}

	void process_compressed(const std::string& message, std::function<void(F, uint8_t*, uint32_t)> processor, bool needsRecompression)
	{
		Msg msg(message);
		msg.next();
		auto clientFrameId = msg.next_uint();
		auto compressedFrameId = msg.next_uint();
		auto compressedFrameLength = msg.next_uint();
		if (!needsRecompression) {
			bool success = false;
			auto srcFrame = _scheduled_frames.retrieve(clientFrameId, success);
			if (!success) {
				return;
}
			processor(srcFrame, get_compressed_frame(compressedFrameId), compressedFrameLength);
		}
		++_frames_compressed;
		send(GRK_MSGR_BATCH_PROCESSSED_COMPRESSED, compressedFrameId);
		if (shutdown_ && _frames_compressed == _frames_scheduled) {
			shutdownCondition_.notify_all();
}
	}

	void shutdown()
	{
		try {
			std::unique_lock<std::mutex> lk(shutdownMutex_);
			if (!async_result_.valid()) {
				return;
}
			shutdown_ = true;
			if (_frames_scheduled) {
				uint32_t scheduled = _frames_scheduled;
				send(GRK_MSGR_BATCH_FLUSH, scheduled);
				shutdownCondition_.wait(lk, [this] { return _frames_scheduled == _frames_compressed; });
			}
			availableBuffers_.deactivate();
			send(GRK_MSGR_BATCH_SHUTDOWN);
			int result = async_result_.get();
			if (result != 0) {
				getMessengerLogger()->error("Accelerator failed with return code: %d\n", result);
}
		} catch (std::exception& ex) {
			getMessengerLogger()->error("%s", ex.what());
		}
	}

	F retrieve(size_t index, bool& success)
	{
		return _scheduled_frames.retrieve(index, success);
	}

	void store(F& val)
	{
		_scheduled_frames.store(val);
	}

private:
	ScheduledFrames<F> _scheduled_frames;
	std::atomic<uint32_t> _frames_scheduled;
	std::atomic<uint32_t> _frames_compressed;
};

} // namespace grk_plugin
