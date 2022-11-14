/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "audio_api.h"
#include "lib/dcpomatic_assert.h"
#include <boost/optional.hpp>
#include <vector>


using std::string;
using std::vector;
using boost::optional;


static AudioAPI wasapi(_("WASAPI"), "wasapi", RtAudio::WINDOWS_WASAPI);
static AudioAPI asio(_("ASIO"), "asio", RtAudio::WINDOWS_ASIO);
static AudioAPI pulse(_("PulseAudio"), "pulse", RtAudio::LINUX_PULSE);
static AudioAPI alsa(_("ALSA"), "alsa", RtAudio::LINUX_ALSA);
static AudioAPI core(_("CoreAudio"), "core", RtAudio::MACOSX_CORE);


vector<AudioAPI>
audio_apis()
{
#ifdef DCPOMATIC_WINDOWS
       return { wasapi, asio };
#endif
#ifdef DCPOMATIC_LINUX
       return { pulse, alsa };
#endif
#ifdef DCPOMATIC_OSX
       return { core };
#endif
}


AudioAPI
id_to_audio_api(string id)
{
       const auto apis = audio_apis();
       auto iter = std::find_if(apis.begin(), apis.end(), [&id](AudioAPI const& api) { return api.id() == id; });
       if (iter == apis.end()) {
               return apis[0];
       }

       return *iter;
}


AudioAPI
rtaudio_id_to_audio_api(RtAudio::Api id)
{
       const auto apis = audio_apis();
       auto iter = std::find_if(apis.begin(), apis.end(), [&id](AudioAPI const& api) { return api.rtaudio_id() == id; });
       DCPOMATIC_ASSERT(iter != apis.end());
       return *iter;
}

AudioAPI
id_to_audio_api(optional<string> id)
{
       const auto apis = audio_apis();
       if (!id) {
               return apis[0];
       }

       return id_to_audio_api(*id);
}


AudioAPI
index_to_audio_api(int index)
{
       const auto apis = audio_apis();
       if (index < 0 || index >= static_cast<int>(apis.size())) {
               return apis[0];
       }

       return apis[index];
}

