#ifndef DCPOMATIC_J2K_ENCODER_THREAD_H
#define DCPOMATIC_J2K_ENCODER_THREAD_H


#include "exception_store.h"
#include <dcp/array_data.h>
#include <boost/thread.hpp>


class DCPVideo;
class J2KEncoder;


class J2KEncoderThread : public ExceptionStore
{
public:
	J2KEncoderThread(J2KEncoder& encoder);

	J2KEncoderThread(J2KEncoderThread const&) = delete;
	J2KEncoderThread& operator=(J2KEncoderThread const&) = delete;

	void start();
	void stop();

	void run();

	virtual void log_thread_start() const = 0;
	virtual std::shared_ptr<dcp::ArrayData> encode(DCPVideo const& frame) = 0;

protected:
	J2KEncoder& _encoder;

private:
	boost::thread _thread;
};


#endif
