#include "dcpomatic_time.h"
#include "decoder.h"


class BlackContent;
class Image;
class RawImageProxy;


class BlackDecoder : public Decoder
{
public:
	BlackDecoder (std::shared_ptr<const Film> film, std::shared_ptr<const BlackContent> content);

	bool pass () override;
	void seek (dcpomatic::ContentTime time, bool accurate) override;

	dcpomatic::ContentTime position () const override {
		return dcpomatic::ContentTime(_position.get());
	}

private:
	std::shared_ptr<const BlackContent> _content;
	int _video_frame_rate;
	dcpomatic::DCPTime _position;
	std::shared_ptr<Image> _image;
	std::shared_ptr<const RawImageProxy> _proxy;
};
