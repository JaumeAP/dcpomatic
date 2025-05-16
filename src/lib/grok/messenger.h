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

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <iostream>
#include <string>
#include <cstring>
#include <atomic>
#include <functional>
#include <sstream>
#include <future>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cassert>
#include <cstdarg>
#include <climits>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <tlhelp32.h>
#pragma warning(disable : 4100)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#endif

namespace grk_plugin
{
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
	virtual ~IMessengerLogger(void) = default;
	virtual void info(const char* fmt, ...) = 0;
	virtual void warn(const char* fmt, ...) = 0;
	virtual void error(const char* fmt, ...) = 0;

  protected:
	template<typename... Args>
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
	explicit MessengerLogger(const std::string &preamble) : preamble_(preamble) {}
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

extern IMessengerLogger* sLogger;
void setMessengerLogger(IMessengerLogger* logger);
IMessengerLogger* getMessengerLogger(void);


struct MessengerInit
{
	MessengerInit(
		std::string const& outBuf,
		std::string const& outSent,
		std::string const& outReceiveReady,
		std::string const& inBuf,
		std::string const& inSent,
		std::string const& inReceiveReady,
		std::function<void(std::string)> processor,
		size_t numProcessingThreads
		)
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
		unlink();
	}

	void unlink()
	{
#ifndef _WIN32
		shm_unlink(grokToClientMessageBuf.c_str());
		shm_unlink(clientToGrokMessageBuf.c_str());
#endif
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
	Synch(const std::string &sentSemName, const std::string &receiveReadySemName)
		: sentSemName_(sentSemName), receiveReadySemName_(receiveReadySemName)
	{
		// unlink semaphores in case of previous crash
		unlink();
		open();
	}
	~Synch()
	{
		close();
		unlink();
	}
	void post(SynchDirection dir)
	{
		auto sem = (dir == SYNCH_SENT ? sentSem_ : receiveReadySem_);
		int rc = sem_post(sem);
		if(rc)
			getMessengerLogger()->error("Error posting to semaphore: %s", strerror(errno));
	}
	void wait(SynchDirection dir)
	{
		auto sem = dir == SYNCH_SENT ? sentSem_ : receiveReadySem_;
		int rc = sem_wait(sem);
		if(rc)
			getMessengerLogger()->error("Error waiting for semaphore: %s", strerror(errno));
	}
	void open(void)
	{
		sentSem_ = sem_open(sentSemName_.c_str(), O_CREAT, 0666, 0);
		if(!sentSem_)
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
		receiveReadySem_ = sem_open(receiveReadySemName_.c_str(), O_CREAT, 0666, 1);
		if(!receiveReadySem_)
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
	}
	void close(void)
	{
		int rc = sem_close(sentSem_);
		if(rc)
			getMessengerLogger()->error("Error closing semaphore %s: %s", sentSemName_.c_str(),
										strerror(errno));
		rc = sem_close(receiveReadySem_);
		if(rc)
			getMessengerLogger()->error("Error closing semaphore %s: %s",
										receiveReadySemName_.c_str(), strerror(errno));
	}
	void unlink(void)
	{
		int rc = sem_unlink(sentSemName_.c_str());
		if(rc == -1 && errno != ENOENT)
			getMessengerLogger()->error("Error unlinking semaphore %s: %s", sentSemName_.c_str(),
										strerror(errno));
		rc = sem_unlink(receiveReadySemName_.c_str());
		if(rc == -1 && errno != ENOENT)
			getMessengerLogger()->error("Error unlinking semaphore %s: %s",
										receiveReadySemName_.c_str(), strerror(errno));
	}
	sem_t* sentSem_;
	sem_t* receiveReadySem_;

  private:
	std::string sentSemName_;
	std::string receiveReadySemName_;
};
struct SharedMemoryManager
{
	static bool initShm(const std::string &name, size_t len, grk_handle* shm_fd, char** buffer)
	{
		*shm_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
		if(*shm_fd < 0)
		{
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
			return false;
		}
		int rc = ftruncate(*shm_fd, sizeof(char) * len);
		if(rc)
		{
			getMessengerLogger()->error("Error truncating shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if(rc)
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
			rc = shm_unlink(name.c_str());
			// 2 == No such file or directory
			if(rc && errno != 2)
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
			return false;
		}
		*buffer = static_cast<char*>(mmap(0, len, PROT_WRITE, MAP_SHARED, *shm_fd, 0));
		if(!*buffer)
		{
			getMessengerLogger()->error("Error mapping shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if(rc)
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
			rc = shm_unlink(name.c_str());
			// 2 == No such file or directory
			if(rc && errno != 2)
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
		}

		return *buffer != nullptr;
	}
	static bool deinit_shm(const std::string &name, size_t len, grk_handle &shm_fd, char** buffer)
	{
		if (!*buffer || !shm_fd)
			return true;

		int rc = munmap(*buffer, len);
		*buffer = nullptr;
		if(rc)
			getMessengerLogger()->error("Error unmapping shared memory %s: %s", name.c_str(), strerror(errno));
		rc = close(shm_fd);
		shm_fd = 0;
		if(rc)
			getMessengerLogger()->error("Error closing shared memory %s: %s", name.c_str(), strerror(errno));
		rc = shm_unlink(name.c_str());
		// 2 == No such file or directory
		if(rc && errno != 2)
			fprintf(stderr,"Error unlinking shared memory %s : %s\n", name.c_str(), strerror(errno));

		return true;
	}
};

template<typename Data>
class MessengerBlockingQueue
{
  public:
	explicit MessengerBlockingQueue(size_t max) : active_(true), max_size_(max) {}
	MessengerBlockingQueue() : MessengerBlockingQueue(UINT_MAX) {}
	size_t size() const
	{
		return queue_.size();
	}
	// deactivate and clear queue
	void deactivate()
	{
		{
			std::lock_guard<std::mutex> lk(mutex_);
			active_ = false;
			while(!queue_.empty())
				queue_.pop();
		}

		// release all waiting threads
		can_pop_.notify_all();
		can_push_.notify_all();
	}
	void activate()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		active_ = true;
	}
	bool push(Data const& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			rc = push_(value);
		}
		if(rc)
			can_pop_.notify_one();

		return rc;
	}
	bool pop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			rc = pop_(value);
		}
		if(rc)
			can_push_.notify_one();

		return rc;
	}
	bool waitAndPop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			if(!active_)
				return false;
			// in case of spurious wakeup, loop until predicate in lambda
			// is satisfied.
			can_pop_.wait(lk, [this] { return !queue_.empty() || !active_; });
			rc = pop_(value);
		}
		if(rc)
			can_push_.notify_one();

		return rc;
	}

  private:
	bool push_(Data const& value)
	{
		if(queue_.size() == max_size_ || !active_)
			return false;
		queue_.push(value);

		return true;
	}
	bool pop_(Data& value)
	{
		if(queue_.empty() || !active_)
			return false;
		value = queue_.front();
		queue_.pop();

		return true;
	}
	std::queue<Data> queue_;
	mutable std::mutex mutex_;
	std::condition_variable can_pop_;
	std::condition_variable can_push_;
	bool active_;
	size_t max_size_;
};
struct BufferSrc
{
	BufferSrc(void) : BufferSrc("") {}
	explicit BufferSrc(const std::string &file) : file_(file), clientFrameId_(0), frameId_(0), framePtr_(nullptr)
	{}
	BufferSrc(size_t clientFrameId, size_t frameId, uint8_t* framePtr)
		: file_(""), clientFrameId_(clientFrameId), frameId_(frameId), framePtr_(framePtr)
	{}
	bool fromDisk(void)
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


class Messenger
{
public:
	explicit Messenger(MessengerInit init);
	virtual ~Messenger();

	void startThreads();
	bool initBuffers();
	bool deinit_shm();

	template<typename... Args>
	void send(const std::string& str, Args... args)
	{
		std::ostringstream oss;
		oss << str;
		int dummy[] = {0, ((void)(oss << ',' << args), 0)...};
		static_cast<void>(dummy);

		sendQueue.push(oss.str());
	}

	bool launch_grok(
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
		);

	void initClient(size_t uncompressedFrameSize, size_t compressedFrameSize, size_t numFrames);
	bool waitForClientInit();

	void reclaimCompressed(size_t frameId);
	void reclaimUncompressed(size_t frameId);
	uint8_t* getUncompressedFrame(size_t frameId);
	uint8_t* getCompressedFrame(size_t frameId);

	static size_t uncompressedFrameSize(uint32_t w, uint32_t h, uint32_t samplesPerPixel);

	std::atomic_bool running;
	bool _initialized = false;
	bool _shutdown = false;
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
	bool launch(std::string const& cmd, boost::filesystem::path const& dir);

	std::thread outbound;
	Synch* outboundSynch_ = nullptr;

	std::thread inbound;
	Synch* inboundSynch_ = nullptr;

	std::vector<std::thread> processors_;
	char* uncompressed_buffer_ = nullptr;
	char* compressed_buffer_ = nullptr;

	grk_handle uncompressed_fd_ = 0;
	grk_handle compressed_fd_ = 0;
};


struct Msg
{
	explicit Msg(const std::string &msg) : ct_(0)
	{
		std::stringstream ss(msg);
		while(ss.good())
		{
			std::string substr;
			std::getline(ss, substr, ',');
			cs_.push_back(substr);
		}
	}
	std::string next()
	{
		if(ct_ == cs_.size())
		{
			getMessengerLogger()->error("Msg: comma separated list exhausted. returning empty.");
			return "";
		}
		return cs_[ct_++];
	}

	uint32_t nextUint(void)
	{
		return (uint32_t)std::stoi(next());
	}

	std::vector<std::string> cs_;
	size_t ct_;
};


template<typename F>
struct ScheduledFrames
{
	void store(F const& val)
	{
		std::unique_lock<std::mutex> lk(mapMutex_);
		auto it = map_.find(val.index());
		if (it == map_.end())
			map_.emplace(std::make_pair(val.index(), val));
	}
	boost::optional<F> retrieve(size_t index)
	{
		std::unique_lock<std::mutex> lk(mapMutex_);
		auto it = map_.find(index);
		if(it == map_.end())
			return {};

		F val = it->second;
		map_.erase(index);

		return val;
	}

 private:
	std::mutex mapMutex_;
	std::map<size_t, F> map_;
};

template<typename F>
struct ScheduledMessenger : public Messenger
{
	explicit ScheduledMessenger(MessengerInit init)
		: Messenger(init)
		, _frames_scheduled(0)
		, _frames_compressed(0)
	{}

	~ScheduledMessenger()
	{
		shutdown();
	}

	bool schedule_compress(F const& proxy, std::function<void(BufferSrc const&)> converter)
	{
		BufferSrc src;
		if (!availableBuffers_.waitAndPop(src)) {
			return false;
		}
		converter(src);
		_scheduled_frames.store(proxy);
		_frames_scheduled++;
		send(GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED, proxy.index(), src.frameId_);

		return true;
	}

	void process_compressed(std::string const& message, std::function<void (F, uint8_t*, uint32_t)> processor, bool needsRecompression)
	{
		Msg msg(message);
		msg.next();
		auto const clientFrameId = msg.nextUint();
		auto const compressedFrameId = msg.nextUint();
		auto const compressedFrameLength = msg.nextUint();
		if (!needsRecompression) {
			auto src_frame = _scheduled_frames.retrieve(clientFrameId);
			if (!src_frame) {
				return;
			}
			processor(*src_frame, getCompressedFrame(compressedFrameId),compressedFrameLength);
		}

		++_frames_compressed;
		send(GRK_MSGR_BATCH_PROCESSSED_COMPRESSED, compressedFrameId);

		if (_shutdown && _frames_compressed == _frames_scheduled) {
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
			_shutdown = true;
			if (_frames_scheduled) {
				uint32_t scheduled = _frames_scheduled;
				send(GRK_MSGR_BATCH_FLUSH, scheduled);
				shutdownCondition_.wait(lk, [this] { return _frames_scheduled == _frames_compressed; });
			}
			availableBuffers_.deactivate();
			send(GRK_MSGR_BATCH_SHUTDOWN);
			int result = async_result_.get();
			if(result != 0) {
				getMessengerLogger()->error("Accelerator failed with return code: %d\n",result);
			}
		} catch (std::exception &ex) {
			getMessengerLogger()->error("%s",ex.what());
		}
	}

	boost::optional<F> retrieve(size_t index) {
		return _scheduled_frames.retrieve(index);
	}

	void store(F& val) {
		_scheduled_frames.store(val);
	}

private:
	ScheduledFrames<F> _scheduled_frames;
	std::atomic<uint32_t> _frames_scheduled;
	std::atomic<uint32_t> _frames_compressed;
};

} // namespace grk_plugin
