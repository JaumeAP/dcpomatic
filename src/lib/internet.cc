/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "exceptions.h"
#include "scoped_temporary.h"
#include "util.h"
#include <dcp/file.h>
#include <curl/curl.h>
#include <zip.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <string>

#include "i18n.h"


using std::function;
using std::list;
using std::string;
using boost::algorithm::trim;
using boost::optional;


static size_t
ls_url_data(void* buffer, size_t size, size_t nmemb, void* output)
{
	string* s = reinterpret_cast<string*>(output);
	char* c = reinterpret_cast<char*>(buffer);
	for (size_t i = 0; i < (size * nmemb); ++i) {
		*s += c[i];
	}
	return nmemb;
}


list<string>
ls_url(string url)
{
	auto curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1);

	string ls;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ls_url_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ls);
	auto const cr = curl_easy_perform(curl);

	if (cr != CURLE_OK) {
		return {};
	}

	list<string> result;
	result.push_back("");
	for (size_t i = 0; i < ls.size(); ++i) {
		if (ls[i] == '\n') {
			result.push_back("");
		} else if (ls[i] != '\r') {
			result.back() += ls[i];
		}
	}

	result.pop_back();
	return result;
}


static size_t
get_from_url_data(void* buffer, size_t size, size_t nmemb, void* data_void)
{
	auto data = reinterpret_cast<dcp::ArrayData*>(data_void);
	int const old_size = data->size();
	data->set_size(old_size + size * nmemb);
	memcpy(data->data() + old_size, buffer, size * nmemb);
	return size;
}


dcp::ArrayData
get_from_url(string url, bool pasv, bool skip_pasv_ip)
{
	dcp::ArrayData data;

	auto curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_from_url_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0);
	curl_easy_setopt(curl, CURLOPT_FTP_USE_EPRT, 0);
	if (skip_pasv_ip) {
		curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1);
	}
	if (!pasv) {
		curl_easy_setopt(curl, CURLOPT_FTPPORT, "-");
	}
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Maximum time is 20s */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);

	auto const cr = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	if (cr != CURLE_OK) {
		throw DownloadError(fmt::format(_("Download failed ({} error {})"), url, (int) cr));
	}

	return data;
}


/** @param url URL of ZIP file.
 *  @param file Filename within ZIP file.
 *  @return File's data.
 *
 *  DownloadError is thrown in case of error.
 */
dcp::ArrayData
get_from_zip_url(string url, string file, bool pasv, bool skip_pasv_ip)
{
	/* Download the ZIP file and write it to temp_zip */
	auto const zip_data = get_from_url(url, pasv, skip_pasv_ip);
	ScopedTemporary temp_zip;
	zip_data.write(temp_zip.path());

	/* Open the ZIP file and read `file' out of it */

#ifdef DCPOMATIC_HAVE_ZIP_SOURCE_T
	/* This is the way to do it with newer versions of libzip, and is required on Windows.
	   The zip_source_t API is missing in the libzip versions shipped with Ubuntu 14.04,
	   Centos 6, Centos 7, Debian 7 and Debian 8.
	*/

	auto& zip_file = temp_zip.open("rb");
	if (!zip_file) {
		throw DownloadError(_("Could not open downloaded ZIP file"));
	}

	auto zip_source = zip_source_filep_create(zip_file.take(), 0, -1, 0);
	if (!zip_source) {
		throw DownloadError(_("Could not open downloaded ZIP file"));
	}

	zip_error_t error;
	zip_error_init(&error);
	auto zip = zip_open_from_source(zip_source, ZIP_RDONLY, &error);
	if (!zip) {
		throw DownloadError(fmt::format(_("Could not open downloaded ZIP file ({}:{}: {})"), error.zip_err, error.sys_err, error.str ? error.str : ""));
	}

#else
	struct zip* zip = zip_open(temp_zip.c_str(), 0, 0);
#endif

	struct zip_file* file_in_zip = zip_fopen(zip, file.c_str(), 0);
	if (!file_in_zip) {
		throw DownloadError(_("Unexpected ZIP file contents"));
	}

	int constexpr block_size = 4096;
	int offset = 0;
	dcp::ArrayData data(block_size);
	while (true) {
		int const N = zip_fread(file_in_zip, data.data() + offset, block_size);
		if (N < block_size) {
			data.set_size(offset + N);
			break;
		}
		offset += block_size;
	}
	zip_fclose(file_in_zip);
	zip_close(zip);

	return data;
}
