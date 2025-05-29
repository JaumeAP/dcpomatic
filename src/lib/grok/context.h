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


#include "../config.h"
#include "../dcp_video.h"
#include "../film.h"
#include "../log.h"
#include "../dcpomatic_log.h"
#include "../writer.h"
#include "messenger.h"
#include <dcp/array_data.h>
#include <boost/filesystem.hpp>


namespace grk_plugin
{

struct GrokLogger
{
	explicit GrokLogger(std::string const& preamble)
		: _preamble(preamble)
	{

	}

	void info(char const* fmt, ...)
	{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(_preamble + log_message(fmt, arg),LogEntry::TYPE_GENERAL);
		va_end(arg);
	}

	void warn(char const* fmt, ...)
	{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(_preamble + log_message(fmt, arg),LogEntry::TYPE_WARNING);
		va_end(arg);
	}

	void error(char const* fmt, ...)
	{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(_preamble + log_message(fmt, arg),LogEntry::TYPE_ERROR);
		va_end(arg);
	}

private:
	template<typename... Args>
	std::string log_message(char const* const format, Args&... args) noexcept
	{
		constexpr size_t message_size = 512;
		char message[message_size];

		std::snprintf(message, message_size, format, args...);
		return std::string(message);
	}

	std::string _preamble;
};


extern GrokLogger* sLogger;
void setGrokLogger(GrokLogger* logger);
GrokLogger* getGrokLogger();


struct DcpomaticContext
{
	DcpomaticContext(
		std::shared_ptr<const Film> film_,
		Writer& writer_,
		EventHistory& history_,
		boost::filesystem::path const& location_
		)
		: film(film_)
		, writer(writer_)
		, history(history_)
		, location(location_)
	{

	}

	void set_dimensions(uint32_t w, uint32_t h)
	{
		width = w;
		height = h;
	}

	std::shared_ptr<const Film> film;
	Writer& writer;
	EventHistory& history;
	boost::filesystem::path location;
	uint32_t width = 0;
	uint32_t height = 0;
};


class GrokContext
{
public:
	explicit GrokContext(DcpomaticContext* dcpomatic_context);
	~GrokContext();

	/** Launch a Grok encoder process, preparing it to work on images like @ref dcpv.
	 *  @return true if Grok is now running, false if not.
	 */
	bool launch(DCPVideo dcpv, int device);
	bool schedule_compress(DCPVideo const& vf);

private:
	void frame_done();

	DcpomaticContext* _dcpomatic_context;
	Messenger* _messenger = nullptr;
	bool _launched = false;
	bool _launch_failed = false;
};


}

