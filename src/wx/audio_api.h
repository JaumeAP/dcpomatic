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


#ifndef DCPOMATIC_AUDIO_API_H
#define DCPOMATIC_AUDIO_API_H


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
#include <RtAudio.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/optional.hpp>
#include <string>


class AudioAPI
{
public:
       AudioAPI(wxString name, std::string id, RtAudio::Api rtaudio_id)
               : _name(name)
               , _id(id)
               , _rtaudio_id(rtaudio_id)
       {}

       wxString name() const {
               return _name;
       }

       std::string id() const {
               return _id;
       }

       RtAudio::Api rtaudio_id() const {
               return _rtaudio_id;
       }

private:
       wxString _name;
       std::string _id;
       RtAudio::Api _rtaudio_id;
};


std::vector<AudioAPI> audio_apis();
AudioAPI id_to_audio_api(std::string id);
AudioAPI id_to_audio_api(boost::optional<std::string> id);
AudioAPI rtaudio_id_to_audio_api(RtAudio::Api api);
AudioAPI index_to_audio_api(int index);


#endif

