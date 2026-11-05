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


/** @file  src/tools/dcpomatic_script.cc
 *  @brief Command-line program to run a Lua script that makes or changes films.
 */


#include "lib/cross.h"
#include "lib/signal_manager.h"
#include "lib/state.h"
#include "lib/util.h"
#include "lua/lua_state.h"
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>


using std::cerr;
using std::string;
using boost::optional;


static int
usage(char const* program)
{
	cerr << "Syntax: " << program << " [--config <dir>] (-e <code> | <script.lua>)\n"
	     << "  -c, --config <dir>  directory containing config.xml and cinemas.xml\n"
	     << "  -e <code>           run code instead of a script file\n";
	return EXIT_FAILURE;
}


int
main(int argc, char* argv[])
{
	ArgFixer fixer(argc, argv);

	dcpomatic_setup_path_encoding();
	dcpomatic_setup();

	auto const program = fixer.argv()[0];
	optional<boost::filesystem::path> config_dir;
	optional<string> code;
	optional<boost::filesystem::path> script;

	for (int i = 1; i < fixer.argc(); ++i) {
		string const a = fixer.argv()[i];
		if ((a == "-c" || a == "--config") && i + 1 < fixer.argc()) {
			config_dir = fixer.argv()[++i];
		} else if (a == "-e" && i + 1 < fixer.argc() && !code && !script) {
			code = fixer.argv()[++i];
		} else if (!a.empty() && a[0] != '-' && !code && !script) {
			script = a;
		} else {
			return usage(program);
		}
	}

	if (!code && !script) {
		return usage(program);
	}

	if (config_dir) {
		State::override_path = *config_dir;
	}

	/* Made on the thread that runs the script: signals emitted there run at once, which the bindings rely on */
	signal_manager = new SignalManager();

	try {
		LuaState state;
		if (code) {
			state.run_string(*code);
		} else {
			state.run_file(*script);
		}
	} catch (std::exception& e) {
		cerr << e.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
