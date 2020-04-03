/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "compose.hpp"
#include "log.h"
#include "dcpomatic_log.h"
#include "config.h"
#include "exceptions.h"
#include <dcp/raw_convert.h>
#include <glib.h>
extern "C" {
#include <libavformat/avio.h>
}
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/regex.hpp>
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <DiskArbitration/DADisk.h>
#include <DiskArbitration/DiskArbitration.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>

#include "i18n.h"

using std::pair;
using std::list;
using std::ifstream;
using std::string;
using std::wstring;
using std::make_pair;
using std::vector;
using std::cerr;
using std::cout;
using std::runtime_error;
using boost::shared_ptr;
using boost::optional;

/** @param s Number of seconds to sleep for */
void
dcpomatic_sleep_seconds (int s)
{
	sleep (s);
}

void
dcpomatic_sleep_milliseconds (int ms)
{
	usleep (ms * 1000);
}

/** @return A string of CPU information (model name etc.) */
string
cpu_info ()
{
	string info;

	char buffer[64];
	size_t N = sizeof (buffer);
	if (sysctlbyname ("machdep.cpu.brand_string", buffer, &N, 0, 0) == 0) {
		info = buffer;
	}

	return info;
}

/** @return Path of the Contents directory in the .app */
boost::filesystem::path
app_contents ()
{
	std::cout << "program_location=" << boost::dll::program_location() << "\n";
	return boost::dll::program_location().parent_path().parent_path();
}

boost::filesystem::path
shared_path ()
{
	return app_contents() / "Resources";
}

void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
	boost::filesystem::path path = app_contents();
	path /= "MacOS";
	path /= "ffprobe";

	string ffprobe = "\"" + path.string() + "\" \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
	system (ffprobe.c_str ());
}

list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;
	return m;
}

boost::filesystem::path
openssl_path ()
{
	boost::filesystem::path path = app_contents();
	path /= "MacOS";
	path /= "openssl";
	return path;
}

boost::filesystem::path
disk_writer_path ()
{
	return "/Users/carl/dcpomatic/src/dcpomatic/build/src/tools/dcpomatic2_disk_writer";
	boost::filesystem::path path = app_contents();
	path /= "MacOS";
	path /= "dcpomatic2_disk_writer";
	return path;
}

/* Apparently there is no way to create an ofstream using a UTF-8
   filename under Windows.  We are hence reduced to using fopen
   with this wrapper.
*/
FILE *
fopen_boost (boost::filesystem::path p, string t)
{
        return fopen (p.c_str(), t.c_str ());
}

int
dcpomatic_fseek (FILE* stream, int64_t offset, int whence)
{
	return fseek (stream, offset, whence);
}

void
Waker::nudge ()
{

}

Waker::Waker ()
{
	boost::mutex::scoped_lock lm (_mutex);
	/* We should use this */
        // IOPMAssertionCreateWithName (kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, CFSTR ("Encoding DCP"), &_assertion_id);
	/* but it's not available on 10.5, so we use this */
        IOPMAssertionCreate (kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &_assertion_id);
}

Waker::~Waker ()
{
	boost::mutex::scoped_lock lm (_mutex);
	IOPMAssertionRelease (_assertion_id);
}

void
start_tool (boost::filesystem::path dcpomatic, string executable, string app)
{
	boost::filesystem::path batch = dcpomatic.parent_path ();
	batch = batch.parent_path (); // MacOS
	batch = batch.parent_path (); // Contents
	batch = batch.parent_path (); // DCP-o-matic.app
	batch = batch.parent_path (); // Applications
	batch /= app;
	batch /= "Contents";
	batch /= "MacOS";
	batch /= executable;

	pid_t pid = fork ();
	if (pid == 0) {
		int const r = system (batch.string().c_str());
		exit (WEXITSTATUS (r));
	}
}

void
start_batch_converter (boost::filesystem::path dcpomatic)
{
	start_tool (dcpomatic, "dcpomatic2_batch", "DCP-o-matic\\ 2\\ Batch\\ Converter.app");
}

void
start_player (boost::filesystem::path dcpomatic)
{
	start_tool (dcpomatic, "dcpomatic2_player", "DCP-o-matic\\ 2\\ Player.app");
}

uint64_t
thread_id ()
{
	return (uint64_t) pthread_self ();
}

int
avio_open_boost (AVIOContext** s, boost::filesystem::path file, int flags)
{
	return avio_open (s, file.c_str(), flags);
}

boost::filesystem::path
home_directory ()
{
		return getenv("HOME");
}

string
command_and_read (string cmd)
{
	return "";
}

/** @return true if this process is a 32-bit one running on a 64-bit-capable OS */
bool
running_32_on_64 ()
{
	/* I'm assuming nobody does this on OS X */
	return false;
}

static void
disk_appeared (DADiskRef disk, void* context)
{
	const char* name = DADiskGetBSDName (disk);
	if (!name) {
		return;
	}

	vector<Drive>* drives = reinterpret_cast<vector<Drive>*> (context);

	CFDictionaryRef description = DADiskCopyDescription (disk);

	optional<string> vendor;
	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceVendorKey);
	if (str) {
		vendor = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	}

	optional<string> model;
	str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceModelKey);
	if (str) {
		model = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	}

	str = CFDictionaryGetValue (description, kDADiskDescriptionMediaPathKey);
	if (str) {
		if (strncmp(str, "IoDeviceTree:", 13) != 0) {
			return;
		}
	}

	io_service_t service = DADiskCopyIOMedia (disk);
        CFTypeRef whole_media_ref = IORegistryEntryCreateCFProperty (service, CFSTR(kIOMediaWholeKey), kCFAllocatorDefault, 0);
	bool whole_media = false;
        if (whole_media_ref) {
		whole_media = CFBooleanGetValue((CFBooleanRef) whole_media);
                CFRelease (whole_media_ref);
        }
	IOObjectRelease (service);
	if (!whole_media) {
		return;
	}

	unsigned long size;
	CFNumberGetValue ((CFNumberRef) CFDictionaryGetValue (description, kDADiskDescriptionMediaSizeKey), kCFNumberLongType, &size);

	CFRelease (description);

	drives->push_back (Drive(name, size, false, vendor, model));
}

vector<Drive>
get_drives ()
{
std::cout << "get_drives!\n";
	vector<Drive> drives;

	DASessionRef session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return drives;
	}

	DARegisterDiskAppearedCallback (session, NULL, disk_appeared, &drives);
	CFRunLoopRef run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, 0);
	DAUnregisterCallback(session, (void *) disk_appeared, &drives);
	CFRelease(session);

	return drives;
}

boost::filesystem::path
config_path ()
{
	boost::filesystem::path p;
	p /= g_get_home_dir ();
	p /= "Library";
	p /= "Preferences";
	p /= "com.dcpomatic";
	p /= "2";
	return p;
}
