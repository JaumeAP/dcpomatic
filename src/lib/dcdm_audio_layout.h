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


#ifndef DCPOMATIC_DCDM_AUDIO_LAYOUT_H
#define DCPOMATIC_DCDM_AUDIO_LAYOUT_H


#include <dcp/types.h>
#include <memory>
#include <set>
#include <string>
#include <vector>


class AudioBuffers;


/** The only audio layouts a DCDM export writes */
enum class DCDMLayout
{
	FIVE_ONE,
	SEVEN_ONE
};


/** @param used DCP channels that carry programme audio.
 *  @return SEVEN_ONE if BSL or BSR is used, otherwise FIVE_ONE.
 *  Channels that are not programme audio (motion data, sync signal, sign language) are ignored.
 *  @throw std::runtime_error naming the channels if HI, VI, LC or RC is used.
 */
DCDMLayout dcdm_layout(std::set<dcp::Channel> used);

/** @return file labels for a layout, in output order: L R C LFE Ls Rs, plus Lrs Rrs for 7.1 */
std::vector<std::string> dcdm_channel_labels(DCDMLayout layout);

/** @return the DCP channels written for a layout, in the same order as dcdm_channel_labels() */
std::vector<dcp::Channel> dcdm_channels(DCDMLayout layout);

/** Pick the layout's channels out of buffers whose channel index is the DCP channel index.
 *  Channels that the input does not have are silent.
 */
std::shared_ptr<AudioBuffers> dcdm_layout_channels(std::shared_ptr<const AudioBuffers> in, DCDMLayout layout);


#endif
