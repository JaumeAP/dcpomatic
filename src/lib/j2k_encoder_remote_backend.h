#include "encode_server_description.h"
#include "j2k_encoder_backend.h"


class J2KEncoderRemoteBackend : public J2KEncoderBackend
{
public:
	J2KEncoderRemoteBackend (EncodeServerDescription server);
	boost::optional<dcp::Data> encode (boost::shared_ptr<DCPVideo> video);

private:
	EncodeServerDescription _server;
	int _backoff;
};


