/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#pragma once


#include "config.h"
#include "dcp_video.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "grok_messenger.h"
#include "log.h"
#include "writer.h"


class Film;


static std::mutex launchMutex;


namespace grk_plugin {


struct GrokLogger : public MessengerLogger
{
	explicit GrokLogger(const std::string& preamble);
	virtual ~GrokLogger() = default;

	void info(const char* fmt, ...) override;
	void warn(const char* fmt, ...) override;
	void error(const char* fmt, ...) override;
};


struct GrokInitializer
{
	GrokInitializer();
	~GrokInitializer() = default;
};


class FrameProxy
{
public:
	FrameProxy();
	FrameProxy(int index, Eyes eyes, DCPVideo dcpv);

	int index() const {
		return _index;
	}

	Eyes eyes() const {
		return _eyes;
	}

	DCPVideo const& vf() const {
		return _vf;
	}

private:
	int _index;
	Eyes _eyes;
	DCPVideo _vf;
};


struct DcpomaticContext
{
	DcpomaticContext(
		std::shared_ptr<const Film> film_,
		Writer& writer_,
		EventHistory& history_,
		const boost::optional<boost::filesystem::path>& location_
		);

	void set_dimensions(uint32_t w, uint32_t h);

	std::shared_ptr<const Film> film;
	Writer& writer;
	EventHistory& history;
	boost::optional<boost::filesystem::path> location;
	uint32_t width;
	uint32_t height;
};


class GrokContext
{
public:
	explicit GrokContext(const DcpomaticContext& dcpomaticContext);
	~GrokContext();

	bool launch(DCPVideo dcpv, int device);
	bool schedule_compress(const DCPVideo& vf);
	void shutdown();
	void frame_done();

private:
	DcpomaticContext _dcpomatic_context;
	ScheduledMessenger<FrameProxy>* _messenger;
	bool _launched;
};

} // namespace grk_plugin
