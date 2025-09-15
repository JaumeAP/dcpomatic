/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "info_file_checker.h"
#include <dcp/filesystem.h>

#include "i18n.h"


using std::shared_ptr;


InfoFileChecker::InfoFileChecker(dcp::File& file)
	: _file(file)
	, _next(_file)
{

}


void
InfoFileChecker::check(int64_t offset, uint8_t const* data, int length)
{
	while (_next && offset <= static_cast<int64_t>(_next->offset) && _next->offset < static_cast<uint64_t>(offset + length)) {
		auto hash_start_offset = _next->offset - offset;
		auto hash_length = std::min(_next->size, length - hash_start_offset);
		_digester.add(data + hash_start_offset, hash_length);
		_next->offset += hash_length;
		_next->size -= hash_length;
		if (_next->size == 0) {
			if (_digester.get_vector() != _next->hash) {
				throw EncodeError(_("File on disk is corrupted"));
			}

			_next = boost::none;
			try {
				_next = J2KFrameInfo(_file);
			} catch (...) {}

			_digester = Digester();
		}
	}
}

