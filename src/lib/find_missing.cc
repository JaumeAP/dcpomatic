/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "find_missing.h"
#include "util.h"
#include <boost/filesystem.hpp>


using std::map;
using std::shared_ptr;
using std::vector;


typedef map<shared_ptr<Content>, vector<boost::filesystem::path>> Replacements;


static
void
search (Replacements& replacement_paths, boost::filesystem::path directory, int depth = 0)
{
	boost::system::error_code ec;
	for (auto candidate: boost::filesystem::directory_iterator(directory, ec)) {
		LOG_GENERAL("Have candidate %1", candidate.path().string());
		if (boost::filesystem::is_regular_file(candidate.path())) {
			for (auto& replacement: replacement_paths) {
				for (auto& path: replacement.second) {
					if (!boost::filesystem::exists(path) && path.filename() == candidate.path().filename()) {
						LOG_GENERAL("Accept %1", candidate.path());
						path = candidate.path();
					} else {
						LOG_GENERAL("Reject %1", candidate.path());
					}
				}
			}
		} else if (boost::filesystem::is_directory(candidate.path()) && depth <= 2) {
			LOG_GENERAL("Recurse into %1", candidate);
			search (replacement_paths, candidate, depth + 1);
		} else {
			LOG_GENERAL("Candidate was ignored; depth=%1", candidate.path().string());
		}
	}

	/* Just ignore errors when creating the directory_iterator; they can be triggered by things like
	 * macOS' love of creating random directories (see #2291).
	 */
}


void
dcpomatic::find_missing (vector<shared_ptr<Content>> content_to_fix, boost::filesystem::path clue)
{
	using namespace boost::filesystem;

	LOG_GENERAL_NC("Find missing to fix:");
	for (auto content: content_to_fix) {
		for (auto file: content->paths()) {
			LOG_GENERAL("  %1", file.string());
		}
	}
	LOG_GENERAL("Clue is %1", clue);

	Replacements replacement_paths;
	for (auto content: content_to_fix) {
		replacement_paths[content] = content->paths();
	}

	search (replacement_paths, is_directory(clue) ? clue : clue.parent_path());

	for (auto content: content_to_fix) {
		auto const& repl = replacement_paths[content];
		bool const replacements_exist = std::find_if(repl.begin(), repl.end(), [](path p) { return !exists(p); }) == repl.end();
		LOG_GENERAL("replacements_exist? %1", replacements_exist);
		LOG_GENERAL("digest match? %1", simple_digest(replacement_paths[content]) == content->digest());
		if (replacements_exist && simple_digest(replacement_paths[content]) == content->digest()) {
			LOG_GENERAL_NC("Setting content paths:");
			for (auto const& i: repl) {
				LOG_GENERAL("  %1", i);
			}
			content->set_paths (repl);
		}
	}
}
