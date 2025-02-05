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
#include "show_playlist.h"
#include "show_playlist.h"
#include "show_playlist_entry.h"
#include "show_playlist_list.h"
#include "sqlite_statement.h"
#include "sqlite_transaction.h"


using std::make_pair;
using std::pair;
using std::string;
using std::vector;
using boost::optional;


ShowPlaylistList::ShowPlaylistList()
	: _show_playlists("show_playlists")
	, _entries("entries")
	, _db(Config::instance()->show_playlists_file())
{
	setup_tables();
	setup();
}


ShowPlaylistList::ShowPlaylistList(boost::filesystem::path db_file)
	: _show_playlists("show_playlists")
	, _entries("entries")
	, _db(db_file)
{
	setup_tables();
	setup();
}


void
ShowPlaylistList::setup_tables()
{
	_show_playlists.add_column("uuid", "TEXT");
	_show_playlists.add_column("name", "TEXT");

	_entries.add_column("show_playlist", "INTEGER");
	_entries.add_column("uuid", "TEXT");
	_entries.add_column("sort_index", "INTEGER");
}


void
ShowPlaylistList::setup()
{
	SQLiteStatement show_playlists(_db, _show_playlists.create());
	show_playlists.execute();

	SQLiteStatement entries(_db, _entries.create());
	entries.execute();
}


ShowPlaylistID
ShowPlaylistList::add_show_playlist(ShowPlaylist const& playlist)
{
	SQLiteStatement statement(_db, _show_playlists.insert());

	statement.bind_text(1, playlist.uuid());
	statement.bind_text(2, playlist.name());

	statement.execute();

	return sqlite3_last_insert_rowid(_db.db());
}


void
ShowPlaylistList::remove_show_playlist(ShowPlaylistID id)
{
	SQLiteStatement statement(_db, "DELETE FROM show_playlists WHERE ID=?");
	statement.bind_int64(1, id.get());
	statement.execute();
}


static
vector<pair<ShowPlaylistID, ShowPlaylist>>
show_playlists_from_result(SQLiteStatement& statement)
{
	vector<pair<ShowPlaylistID, ShowPlaylist>> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 3);
		ShowPlaylistID const id = statement.column_int64(0);
		auto const uuid = statement.column_text(1);
		auto const name = statement.column_text(2);
		output.push_back(make_pair(id, ShowPlaylist(uuid, name)));
	});

	return output;
}


vector<pair<ShowPlaylistID, ShowPlaylist>>
ShowPlaylistList::show_playlists() const
{
	SQLiteStatement statement(_db, _show_playlists.select("ORDER BY name COLLATE unicode ASC"));
	return show_playlists_from_result(statement);
}



vector<string>
ShowPlaylistList::entries(ShowPlaylistID show_playlist_id) const
{
	SQLiteStatement statement(_db, "SELECT entries.uuid FROM entries JOIN show_playlists ON entries.show_playlist=show_playlists.id WHERE show_playlists.id=? ORDER BY entries.sort_index");
	statement.bind_int64(1, show_playlist_id.get());

	vector<string> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		output.push_back(statement.column_text(0));
	});

	return output;
}


vector<string>
ShowPlaylistList::entries(string const& show_playlist_uuid) const
{
	SQLiteStatement statement(_db, "SELECT entries.uuid FROM entries JOIN show_playlists ON entries.show_playlist=show_playlists.id WHERE show_playlists.uuid=? ORDER BY entries.sort_index");
	statement.bind_text(1, show_playlist_uuid);

	vector<string> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		output.push_back(statement.column_text(0));
	});

	return output;
}


void
ShowPlaylistList::add_entry(ShowPlaylistID playlist_id, ShowPlaylistEntry entry)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement find_last_screen(_db, "SELECT MAX(sort_index) FROM entries WHERE show_playlist=?");
	find_last_screen.bind_int64(1, playlist_id.get());

	optional<int> highest_index;
	find_last_screen.execute([&highest_index](SQLiteStatement& statement) {
		if (statement.data_count() == 1) {
			highest_index = statement.column_int64(0);
		}
	});

	SQLiteStatement add_entry(_db, _entries.insert());

	add_entry.bind_int64(1, playlist_id.get());
	add_entry.bind_text(2, entry.uuid());
	add_entry.bind_int64(3, highest_index ? *highest_index + 1 : 0);

	add_entry.execute();

	transaction.commit();
}


void
ShowPlaylistList::move_entry_up(ShowPlaylistID playlist_id, int index)
{
	DCPOMATIC_ASSERT(index >= 1);

	SQLiteTransaction transaction(_db);

	SQLiteStatement find(_db, "SELECT id,sort_index FROM entries WHERE show_playlist=? ORDER BY sort_index LIMIT 2 OFFSET ?");
	find.bind_int64(1, playlist_id.get());
	find.bind_int64(2, index - 1);

	vector<pair<int64_t, int64_t>> rows;
	find.execute([&rows](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 2);
		rows.push_back({statement.column_int64(0), statement.column_int64(1)});
	});

	DCPOMATIC_ASSERT(rows.size() == 2);

	SQLiteStatement swap1(_db, "UPDATE entries SET sort_index=? WHERE id=?");
	swap1.bind_int64(1, rows[0].second);
	swap1.bind_int64(2, rows[1].first);
	swap1.execute();

	SQLiteStatement swap2(_db, "UPDATE entries SET sort_index=? WHERE id=?");
	swap2.bind_int64(1, rows[1].second);
	swap2.bind_int64(2, rows[0].first);
	swap2.execute();

	transaction.commit();
}

