#include "audio_content.h"
#include "dcpomatic_time.h"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <vector>


class AudioContent;
class AudioDecoder;
class AudioStream;
class Resampler;


class ResamplerManager
{
public:
	ResamplerManager (std::weak_ptr<const Film> film)
		: _film (film)
	{}

	ResamplerManager (ResamplerManager const&) = delete;
	ResamplerManager& operator= (ResamplerManager const&) = delete;

	void add (dcpomatic::DCPTime start, dcpomatic::DCPTime end, std::shared_ptr<AudioContent> content, std::shared_ptr<AudioDecoder> decoder);
	std::shared_ptr<Resampler> get (AudioDecoder* decoder, AudioStreamPtr stream, bool fast);

	void flush (AudioDecoder* decoder)

private:
	friend struct resampler_manager_setup_test;

	class Group
	{
	public:
		Group (dcpomatic::DCPTime start_, dcpomatic::DCPTime end_, std::shared_ptr<AudioContent> content, std::shared_ptr<AudioDecoder> decoder)
			: start(start_)
			, end(end_)
		{
			contents.push_back (content);
			decoders.push_back (decoder);
			resamplers.resize (content->streams().size());
		}

		dcpomatic::DCPTime start;
		dcpomatic::DCPTime end;
		std::vector<std::shared_ptr<AudioContent>> contents;
		std::vector<std::shared_ptr<AudioDecoder>> decoders;
		std::vector<std::shared_ptr<Resampler>> resamplers;
	};

	bool can_share (Group const& a, Group const& b) const;
	void coalesce ();
	bool coalesce_pass ();

	std::weak_ptr<const Film> _film;
	std::vector<Group> _groups;
};

