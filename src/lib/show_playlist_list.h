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


#ifndef DCPOMATIC_SHOW_PLAYLIST_LIST_H
#define DCPOMATIC_SHOW_PLAYLIST_LIST_H


#include "id.h"
#include "show_playlist.h"
#include "sqlite_database.h"
#include "sqlite_table.h"


/** @class ShowPlaylistID
 *
 *  @brief The SQLite ID (not the UUID) of a ShowPlaylist.
 */
class ShowPlaylistID : public ID
{
public:
	ShowPlaylistID(sqlite3_int64 id)
		: ID(id) {}

	bool operator<(ShowPlaylistID const& other) const {
		return get() < other.get();
	}
};


/** @class ShowPlaylistList
 *
 *  @brief A list of SPLs (show playlists) stored in a SQLite database.
 *
 *  A SPL (show playlist) is a list of content (and maybe later automation cues)
 *  that make up a "show" in a cinema/theater.  For example, a SPL might contain
 *  some adverts, some trailers, and a feature.
 */
class ShowPlaylistList
{
public:
	ShowPlaylistList();
	explicit ShowPlaylistList(boost::filesystem::path db_file);

	ShowPlaylistID add_show_playlist(ShowPlaylist const& show_playlist);
	void remove_show_playlist(ShowPlaylistID id);
	std::vector<std::pair<ShowPlaylistID, ShowPlaylist>> show_playlists() const;

	std::vector<std::string> entries(ShowPlaylistID show_playlist_id) const;
	std::vector<std::string> entries(std::string const& show_playlist_uuid) const;

	void add_entry(ShowPlaylistID, ShowPlaylistEntry entry);
	void move_entry_up(ShowPlaylistID, int index);

private:
	void setup_tables();
	void setup();

	SQLiteTable _show_playlists;
	SQLiteTable _entries;
	mutable SQLiteDatabase _db;
};


#endif

