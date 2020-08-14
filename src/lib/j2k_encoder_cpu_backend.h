#ifndef DCPOMATIC_J2K_ENCODER_CPU_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_CPU_BACKEND_H


#include "j2k_encoder_backend.h"


class J2KEncoderCPUBackend : public J2KEncoderBackend
{
public:
	std::vector<dcp::Data> encode (std::vector<boost::shared_ptr<DCPVideo> > video);
};

#endif
