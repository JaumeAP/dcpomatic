/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/data.h>
#include <functional>


class VideoMXFContent : public Content
{
public:
	VideoMXFContent(boost::filesystem::path path);
	VideoMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version);

	std::shared_ptr<VideoMXFContent> shared_from_this() {
		return std::dynamic_pointer_cast<VideoMXFContent>(Content::shared_from_this());
	}

	std::shared_ptr<const VideoMXFContent> shared_from_this() const {
		return std::dynamic_pointer_cast<const VideoMXFContent>(Content::shared_from_this());
	}

	void examine(std::shared_ptr<Job> job, bool tolerant) override;
	std::string summary() const override;
	std::string technical_summary() const override;
	std::string identifier() const override;

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
	 *  nothing if the film would not write the file's own image
	 */
	std::vector<DCPAsset> assets(std::shared_ptr<const Film> film) const override;

	/** @return an earlier content of the film that is a whole reel and whose image, as the film writes it, is exactly
	 *  the same as this one's, so that this reel can refer to the picture asset of that reel instead of writing its
	 *  own.  Null if there is none, or if this content's image is not written alone as a whole reel, or if the file
	 *  goes into the DCP as it is.
	 */
	std::shared_ptr<const VideoMXFContent> shared_picture_source(std::shared_ptr<const Film> film) const;

	/** @return true if the film writes this content's image again and the frames that its trims cut off are written too,
	 *  so that the picture asset has the file's whole length and the CPL plays only the part inside the trims
	 */
	bool keeps_whole_length(std::shared_ptr<const Film> film) const;

	/** @return the length of the file in frames, ignoring any trims */
	Frame whole_length() const;

	/** Encode frames [from, to) of the file with the changes that the content's settings make, and pass each to @param write */
	void encode_frames(
		std::shared_ptr<const Film> film, Frame from, Frame to, std::function<void (std::shared_ptr<const dcp::Data>)> write
		) const;

	static bool valid_mxf(boost::filesystem::path path);

private:
	/** @return true if the film writes this content's image on its own, as a function of the file and the settings of
	 *  this content only, and it fills one reel of the film
	 */
	bool written_alone_as_a_reel(std::shared_ptr<const Film> film) const;

	/** @return something that is the same for two contents if and only if the film writes the same image for them */
	std::string picture_digest(std::shared_ptr<const Film> film) const;
};
