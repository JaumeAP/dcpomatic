/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include "types.h"


class AudioBuffers;


namespace dcp {
	class SoundFrame;
}


/** @return the samples of a frame of a PCM sound asset (24 or 16 bit) as floats, one buffer channel per channel */
std::shared_ptr<AudioBuffers> sound_mxf_frame_audio(std::shared_ptr<const dcp::SoundFrame> frame, int channels);


/** @class SoundMXFContent
 *  @brief A PCM sound MXF (a dcp::SoundAsset) that is added to a film on its own.
 */
class SoundMXFContent : public Content
{
public:
	SoundMXFContent(boost::filesystem::path path);
	SoundMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version);

	std::shared_ptr<SoundMXFContent> shared_from_this() {
		return std::dynamic_pointer_cast<SoundMXFContent>(Content::shared_from_this());
	}

	std::shared_ptr<const SoundMXFContent> shared_from_this() const {
		return std::dynamic_pointer_cast<const SoundMXFContent>(Content::shared_from_this());
	}

	void examine(std::shared_ptr<Job> job, bool tolerant) override;
	void prepare_for_add_to_film(std::shared_ptr<const Film> film) override;
	std::string summary() const override;
	std::string technical_summary() const override;

	void as_xml(
		xmlpp::Element* element,
		bool with_paths,
		PathBehaviour path_behaviour,
		boost::optional<boost::filesystem::path> film_directory
		) const override;

	dcpomatic::DCPTime full_length(std::shared_ptr<const Film> film) const override;
	dcpomatic::DCPTime approximate_length() const override;
	void add_properties(std::shared_ptr<const Film> film, std::list<UserProperty>& p) const override;

	/** @return the file as an asset that can go into the DCP as it is, with any trim as its entry point and duration, or
	 *  nothing if the film would not write the file's own sound
	 */
	std::vector<DCPAsset> assets(std::shared_ptr<const Film> film) const override;

	/** @return true if the frames of the file line up with the film's: its edit rate is the film's frame rate and the
	 *  picture it plays with does not change its speed.  Then a trim in frames of the file is a trim in frames of the DCP.
	 */
	bool frames_align_with(std::shared_ptr<const Film> film) const;

	/** @return the frames of the file before the start trim, through the audio mapping to the film's channels and with the
	 *  content's gain, or nothing if there is no start trim or the file's sampling rate is not the film's (a head resampled
	 *  on its own would not be a whole number of frames).  These are the sync pips at the head of the file.
	 */
	std::shared_ptr<const AudioBuffers> head_audio(std::shared_ptr<const Film> film) const;
	/** @return the length of head_audio() in frames of the file */
	Frame head_frames() const;
	/** @return the frames of the file after the end trim, as head_audio(), or nothing if there is no end trim */
	std::shared_ptr<const AudioBuffers> tail_audio(std::shared_ptr<const Film> film) const;

	/** @return an earlier content of the film that is a whole reel and whose sound, as the film writes it, is exactly the
	 *  same as this one's, so that this reel can refer to the sound asset of that reel instead of writing its own.  Null if
	 *  there is none, or if this content's sound is not written alone as a whole reel, or if the file goes into the DCP as it is.
	 */
	std::shared_ptr<const SoundMXFContent> shared_sound_source(std::shared_ptr<const Film> film) const;

	/** @return one entry for each channel of the file: true if the channel has sound (see SoundMXFExaminer) */
	std::vector<bool> channels_with_sound() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _channels_with_sound;
	}

	static bool valid_mxf(boost::filesystem::path path);

private:
	/** @return true if the film writes this content's sound on its own, as a function of the file and the settings of
	 *  this content only, and it fills one reel of the film
	 */
	bool written_alone_as_a_reel(std::shared_ptr<const Film> film) const;

	/** @return something that is the same for two contents if and only if the film writes the same sound for them */
	std::string sound_digest(std::shared_ptr<const Film> film) const;

	/** @return the sound of frames [from, to) of the file, mapped for the film and with the content's gain */
	std::shared_ptr<const AudioBuffers> frames_audio(std::shared_ptr<const Film> film, Frame from, Frame to) const;

	std::vector<bool> _channels_with_sound;
};
