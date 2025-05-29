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


#include "../dcp_video.h"
#include "scheduled_frames.h"
#include <future>
#include <queue>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>


namespace grk_plugin {

static std::string grokToClientMessageBuf = "Global\\grok_to_client_message";
static std::string grokSentSynch = "Global\\grok_sent";
static std::string clientReceiveReadySynch = "Global\\client_receive_ready";
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


enum SynchDirection
{
	SYNCH_SENT,
	SYNCH_RECEIVE_READY
};


class Synch
{
public:
	Synch(std::string const& sent_sem_name, std::string const& receive_ready_sem_name);
	~Synch();

	void post(SynchDirection dir);
	void wait(SynchDirection dir);

	void open();
	void close();

	void unlink();

private:
	sem_t* _sent_sem;
	sem_t* _receive_ready_sem;

	std::string _sent_sem_name;
	std::string _receive_ready_sem_name;
};



template<typename Data>
class MessengerBlockingQueue
{
public:
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
			rc = push_unlocked(value);
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
			rc = pop_unlocked(value);
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
			rc = pop_unlocked(value);
		}
		if (rc) {
			_can_push.notify_one();
		}

		return rc;
	}

private:
	bool push_unlocked(Data const& value)
	{
		if (_queue.size() == _max_size || !_active) {
			return false;
		}

		_queue.push(value);

		return true;
	}

	bool pop_unlocked(Data& value)
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
	bool _active = true;
	size_t _max_size = UINT_MAX;
};


struct BufferSrc
{
public:
	BufferSrc() = default;

	BufferSrc(size_t frame_id_, uint8_t* frame_ptr_)
		: frame_id(frame_id_)
		, frame_ptr(frame_ptr_)
	{}

	size_t frame_id = 0;
	uint8_t* frame_ptr = nullptr;
};


class Message
{
public:
	explicit Message(std::string const& msg)
	{
		std::stringstream ss(msg);
		while (ss.good()) {
			std::string substr;
			std::getline(ss, substr, ',');
			_cs.push_back(substr);
		}
	}

	std::string next()
	{
		if (_ct == _cs.size()) {
			getMessengerLogger()->error("Message: comma separated list exhausted. returning empty.");
			return "";
		}
		return _cs[_ct++];
	}

	uint32_t nextUint()
	{
		return (uint32_t)std::stoi(next());
	}

private:
	std::vector<std::string> _cs;
	size_t _ct = 0;
};


class Messenger
{
public:
	Messenger(
		std::string const& outSent,
		std::string const& outReceiveReady,
		std::string const& inBuf,
		std::string const& inSent,
		std::string const& inReceiveReady,
		std::function<void(std::string)> processor,
		size_t numProcessingThreads
	);

	~Messenger();

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

		_send_queue.push(oss.str());
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

	bool schedule_compress(DCPVideo const& proxy, std::function<void(BufferSrc const&)> converter);

	/** @param processor function taking compressed J2K data and sending it to the Writer */
	void process_compressed(std::string const& message, std::function<void (DCPVideo, uint8_t*, uint32_t)> processor, bool needsRecompression);

	void shutdown();

	boost::optional<DCPVideo> retrieve(size_t index) {
		return _scheduled_frames.retrieve(index);
	}

	void store(DCPVideo& val) {
		_scheduled_frames.store(val);
	}

private:
	bool launch(std::string const& cmd, boost::filesystem::path const& dir);
	void processor_thread();
	void outbound_thread();
	void inbound_thread();

	std::atomic_bool _running;
	bool _initialized = false;
	/** a queue of messages sent (by outbound_thread) via shared memory to the Grok process */
	MessengerBlockingQueue<std::string> _send_queue;
	/** a queue of messages received (by inbound_thread) via shared memory from the Grok process */
	MessengerBlockingQueue<std::string> _receive_queue;

	std::thread _outbound;
	Synch* _outbound_synch = nullptr;

	std::thread _inbound;
	Synch* _inbound_synch = nullptr;

	std::vector<std::thread> _processors;
	/** Shared memory buffer for passing input (uncompressed) image data.
	  * Allocated to _init._num_frames BufferSrc objects in _available_buffers
	  */
	char* _uncompressed_buffer = nullptr;
	/** Shared memory buffer for passing output (compressed) image data.
	 *  Contains multiple frames at a spacing of _init._compressed_frame_size bytes
	 */
	char* _compressed_buffer = nullptr;

	int _uncompressed_fd = 0;
	int _compressed_fd = 0;

	std::condition_variable _client_initialized_condition;
	MessengerBlockingQueue<BufferSrc> _available_buffers;
	std::future<int> _async_result;
	std::condition_variable _shutdown_condition;
	bool _shutdown = false;
	std::mutex _shutdown_mutex;

	ScheduledFrames _scheduled_frames;
	std::atomic<uint32_t> _frames_scheduled;
	std::atomic<uint32_t> _frames_compressed;

	std::string _outbound_sent_synch;
	std::string _outbound_receive_ready_synch;

	std::string _inbound_message_buf;
	std::string _inbound_sent_synch;
	std::string _inbound_receive_ready_synch;

	std::function<void(std::string)> _processor;
	size_t _num_processing_threads;

	size_t _uncompressed_frame_size;
	size_t _compressed_frame_size;
	size_t _num_frames;
};

} // namespace grk_plugin
