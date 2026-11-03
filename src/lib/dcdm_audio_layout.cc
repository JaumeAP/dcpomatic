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


#include "audio_buffers.h"
#include "dcdm_audio_layout.h"
#include <stdexcept>


using std::make_shared;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;


DCDMLayout
dcdm_layout(set<dcp::Channel> used)
{
	std::pair<dcp::Channel, string> const not_allowed[] = {
		{ dcp::Channel::HI, "HI" },
		{ dcp::Channel::VI, "VI" },
		{ dcp::Channel::LC, "Lc" },
		{ dcp::Channel::RC, "Rc" }
	};

	vector<string> bad;
	for (auto const& channel: not_allowed) {
		if (used.count(channel.first)) {
			bad.push_back(channel.second);
		}
	}

	if (!bad.empty()) {
		string names;
		for (auto const& name: bad) {
			names += (names.empty() ? "" : ", ") + name;
		}
		throw std::runtime_error("A DCDM can only carry 5.1 or 7.1 audio, but these channels are used: " + names);
	}

	if (used.count(dcp::Channel::BSL) || used.count(dcp::Channel::BSR)) {
		return DCDMLayout::SEVEN_ONE;
	}

	return DCDMLayout::FIVE_ONE;
}


vector<string>
dcdm_channel_labels(DCDMLayout layout)
{
	vector<string> labels = { "L", "R", "C", "LFE", "Ls", "Rs" };
	if (layout == DCDMLayout::SEVEN_ONE) {
		labels.push_back("Lrs");
		labels.push_back("Rrs");
	}
	return labels;
}


vector<dcp::Channel>
dcdm_channels(DCDMLayout layout)
{
	vector<dcp::Channel> channels = {
		dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE, dcp::Channel::LS, dcp::Channel::RS
	};
	if (layout == DCDMLayout::SEVEN_ONE) {
		channels.push_back(dcp::Channel::BSL);
		channels.push_back(dcp::Channel::BSR);
	}
	return channels;
}


shared_ptr<AudioBuffers>
dcdm_layout_channels(shared_ptr<const AudioBuffers> in, DCDMLayout layout)
{
	auto const channels = dcdm_channels(layout);
	auto out = make_shared<AudioBuffers>(static_cast<int>(channels.size()), in->frames());
	out->make_silent();
	for (size_t i = 0; i < channels.size(); ++i) {
		auto const from = static_cast<int>(channels[i]);
		if (from < in->channels()) {
			out->copy_channel_from(in.get(), from, static_cast<int>(i));
		}
	}
	return out;
}
