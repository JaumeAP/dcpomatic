#ifndef DCPOMATIC_J2K_ENCODER_CPU_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_CPU_BACKEND_H


#include "j2k_encoder_backend.h"


class J2KEncoderCPUBackend : public J2KEncoderBackend
{
public:
	boost::optional<dcp::Data> encode (boost::shared_ptr<DCPVideo> video);
};

#endif
