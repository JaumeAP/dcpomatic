/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "dcpomatic_sentry.h"
#include "version.h"
#include <sentry.h>
#include <fmt/format.h>


void
maybe_setup_sentry()
{
	if (!Config::instance()->enable_crash_reporting()) {
		return;
	}

	auto *options = sentry_options_new();
	sentry_options_set_dsn(options, "https://aa14795ff538105836cfc663e1079fb1@o4510456286347264.ingest.de.sentry.io/4510456326651984");
	// This is also the default-path. For further information and recommendations:
	// https://docs.sentry.io/platforms/native/configuration/options/#database-path
	sentry_options_set_database_path(options, State::write_path("sentry-database").string().c_str());
	sentry_options_set_release(options, fmt::format("dcpomatic@{}", dcpomatic_version).c_str());
#ifdef DCPOMATIC_DEBUG
	sentry_options_set_debug(options, 1);
#else
	sentry_options_set_debug(options, 0);
#endif
	sentry_init(options);

	sentry_close();
}


void
closedown_sentry()
{
	sentry_close();
}

