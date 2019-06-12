/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

/* dcpomatic_dist_writer receives

DCP pathname\n
Internal name of drive to write to\n

   Then responds with one of the following.
*/

/** Write finished and everything was OK, e.g.

D\n

*/
#define DIST_WRITER_OK "D"

/** There was an error.  Following this will come

error message\n
error number\n

e.g.

E\n
Disc full\n
42\n

*/
#define DIST_WRITER_ERROR "E"

/** Some progress has been made.  Following this will come

progress as a float from 0 to 1\n

e.g.

P\n
0.3\n

*/
#define DIST_WRITER_PROGRESS "P"
