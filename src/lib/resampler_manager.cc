#include "audio_content.h"
#include "audio_decoder.h"
#include "dcpomatic_log.h"
#include "resampler.h"
#include "resampler_manager.h"
#include <boost/foreach.hpp>
#include <vector>


using std::vector;
using boost::shared_ptr;
using namespace dcpomatic;


void
ResamplerManager::add (DCPTime start, DCPTime end, shared_ptr<AudioContent> content, shared_ptr<AudioDecoder> decoder)
{
	_groups.push_back (Group(start, end, content, decoder));
	coalesce ();
}


void
ResamplerManager::coalesce ()
{
	if (_groups.size() < 2) {
		return;
	}

	while (coalesce_pass()) {}
}


bool
ResamplerManager::can_share (Group const& a, Group const& b) const
{
	vector<AudioStreamPtr> as = a.contents[0]->streams();
	vector<AudioStreamPtr> bs = b.contents[0]->streams();

	if (a.contents[0]->streams().size() != b.contents[0]->streams().size()) {
		return false;
	}

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	if (a.contents[0]->resampled_frame_rate(film) != b.contents[0]->resampled_frame_rate(film)) {
		return false;
	}

	/* The first, second, etc. streams in a and b must be the same */

	vector<AudioStreamPtr>::const_iterator i = as.begin();
	vector<AudioStreamPtr>::const_iterator j = bs.begin();

	while (i != as.end()) {
		if (
			(*i)->channels() != (*j)->channels() ||
			(*i)->mapping().mapped_channels() != (*j)->mapping().mapped_channels() ||
			(*i)->frame_rate() != (*j)->frame_rate()) {
			return false;
		}
		++i;
		++j;
	}

	return true;
}


bool
ResamplerManager::coalesce_pass ()
{
	for (size_t i = 0; i < _groups.size(); ++i) {
		for (size_t j = 0; j < _groups.size(); ++j) {
			if (i == j) {
				continue;
			}

			if (_groups[i].start == _groups[j].end && can_share(_groups[i], _groups[j])) {
				copy (_groups[i].decoders.begin(), _groups[i].decoders.end(), back_inserter(_groups[j].decoders));
				copy (_groups[i].contents.begin(), _groups[i].contents.end(), back_inserter(_groups[j].contents));
				_groups[j].end = _groups[i].end;
				_groups.erase (_groups.begin() + i);
				return true;
			}
		}
	}

	return false;
}


shared_ptr<Resampler>
ResamplerManager::get (AudioDecoder* decoder, AudioStreamPtr stream, bool fast)
{
	shared_ptr<const Film> film = _film.lock();
	DCPOMATIC_ASSERT (film);

	BOOST_FOREACH (Group& g, _groups) {
		for (size_t i = 0; i < g.decoders.size(); ++i) {
			if (g.decoders[i].get() == decoder) {
				vector<AudioStreamPtr> content_streams = g.contents[i]->streams();
				ptrdiff_t index = find(content_streams.begin(), content_streams.end(), stream) - content_streams.begin();
				DCPOMATIC_ASSERT (index < static_cast<ptrdiff_t>(content_streams.size()));
				if (!g.resamplers[index] && stream->frame_rate() != g.contents[i]->resampled_frame_rate(film)) {
					LOG_GENERAL (
						"Creating new resampler from %1 to %2 with %3 channels",
						stream->frame_rate(),
						g.contents[i]->resampled_frame_rate(film),
						stream->channels()
						);

					g.resamplers[index].reset (new Resampler(stream->frame_rate(), g.contents[i]->resampled_frame_rate(film), stream->channels()));
					if (fast) {
						g.resamplers[index]->set_fast ();
					}
				}
				return g.resamplers[index];
			}
		}
	}

	DCPOMATIC_ASSERT (false);
	return shared_ptr<Resampler>();
}


void
ResamplerManager::maybe_flush (AudioDecoder* decoder)
{
	BOOST_FOREACH (Group& g, _groups) {
		BOOST_FOREACH (shared_ptr<AudioDecoder> d, g.decoders) {
			if (d.get() == decoder) {
				BOOST_FOREACH (shared_ptr<Resampler> r, g.resamplers) {
					shared_ptr<const AudioBuffers> ro = r->flush ();
					/* XXX: err... which content do these samples belong to?  Won't
					 * the player throw the samples away if they appear to come from the
					 * wrong place?
					 */
			}
		}
	}
}

