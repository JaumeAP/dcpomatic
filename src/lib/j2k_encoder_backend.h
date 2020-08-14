#ifndef DCPOMATIC_J2K_ENCODER_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_BACKEND_H

#include <dcp/data.h>
#include <boost/foreach.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>


class DCPVideo;


class J2KEncoderBackend : public boost::noncopyable
{
public:
	virtual std::vector<dcp::Data> encode (std::vector<boost::shared_ptr<DCPVideo> > video) = 0;

	virtual int quantity () const {
		return 1;
	}
};

#endif
