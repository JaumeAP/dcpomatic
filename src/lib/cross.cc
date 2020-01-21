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
#ifdef DCPOMATIC_LINUX
#include <unistd.h>
#include <mntent.h>
#endif
#ifdef DCPOMATIC_WINDOWS
#include <windows.h>
#include <setupapi.h>
#undef DATADIR
#include <shlwapi.h>
#include <shellapi.h>
#include <fcntl.h>
#endif
#ifdef DCPOMATIC_OSX
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif
#ifdef DCPOMATIC_POSIX
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
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
#ifdef DCPOMATIC_POSIX
	sleep (s);
#endif
#ifdef DCPOMATIC_WINDOWS
	Sleep (s * 1000);
#endif
}

void
dcpomatic_sleep_milliseconds (int ms)
{
#ifdef DCPOMATIC_POSIX
	usleep (ms * 1000);
#endif
#ifdef DCPOMATIC_WINDOWS
	Sleep (ms);
#endif
}

/** @return A string of CPU information (model name etc.) */
string
cpu_info ()
{
	string info;

#ifdef DCPOMATIC_LINUX
	/* This use of ifstream is ok; the filename can never
	   be non-Latin
	*/
	ifstream f ("/proc/cpuinfo");
	while (f.good ()) {
		string l;
		getline (f, l);
		if (boost::algorithm::starts_with (l, "model name")) {
			string::size_type const c = l.find (':');
			if (c != string::npos) {
				info = l.substr (c + 2);
			}
		}
	}
#endif

#ifdef DCPOMATIC_OSX
	char buffer[64];
	size_t N = sizeof (buffer);
	if (sysctlbyname ("machdep.cpu.brand_string", buffer, &N, 0, 0) == 0) {
		info = buffer;
	}
#endif

#ifdef DCPOMATIC_WINDOWS
	HKEY key;
	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return info;
	}

	DWORD type;
	DWORD data;
	if (RegQueryValueEx (key, L"ProcessorNameString", 0, &type, 0, &data) != ERROR_SUCCESS) {
		return info;
	}

	if (type != REG_SZ) {
		return info;
	}

	wstring value (data / sizeof (wchar_t), L'\0');
	if (RegQueryValueEx (key, L"ProcessorNameString", 0, 0, reinterpret_cast<LPBYTE> (&value[0]), &data) != ERROR_SUCCESS) {
		RegCloseKey (key);
		return info;
	}

	info = string (value.begin(), value.end());

	RegCloseKey (key);

#endif

	return info;
}

#ifdef DCPOMATIC_OSX
/** @return Path of the Contents directory in the .app */
boost::filesystem::path
app_contents ()
{
	uint32_t size = 1024;
	char buffer[size];
	if (_NSGetExecutablePath (buffer, &size)) {
		throw runtime_error ("_NSGetExecutablePath failed");
	}

	boost::filesystem::path path (buffer);
	path = boost::filesystem::canonical (path);
	path = path.parent_path ();
	path = path.parent_path ();
	return path;
}
#endif

boost::filesystem::path
shared_path ()
{
#ifdef DCPOMATIC_LINUX
	char const * p = getenv ("DCPOMATIC_LINUX_SHARE_PREFIX");
	if (p) {
		return p;
	}
	return boost::filesystem::canonical (LINUX_SHARE_PREFIX);
#endif
#ifdef DCPOMATIC_WINDOWS
	wchar_t dir[512];
	GetModuleFileName (GetModuleHandle (0), dir, sizeof (dir));
	PathRemoveFileSpec (dir);
	boost::filesystem::path path = dir;
	return path.parent_path();
#endif
#ifdef DCPOMATIC_OSX
	return app_contents() / "Resources";
#endif
}

void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
#ifdef DCPOMATIC_WINDOWS
	SECURITY_ATTRIBUTES security;
	security.nLength = sizeof (security);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = 0;

	HANDLE child_stderr_read;
	HANDLE child_stderr_write;
	if (!CreatePipe (&child_stderr_read, &child_stderr_write, &security, 0)) {
		LOG_ERROR_NC ("ffprobe call failed (could not CreatePipe)");
		return;
	}

	wchar_t dir[512];
	GetModuleFileName (GetModuleHandle (0), dir, sizeof (dir));
	PathRemoveFileSpec (dir);
	SetCurrentDirectory (dir);

	STARTUPINFO startup_info;
	ZeroMemory (&startup_info, sizeof (startup_info));
	startup_info.cb = sizeof (startup_info);
	startup_info.hStdError = child_stderr_write;
	startup_info.dwFlags |= STARTF_USESTDHANDLES;

	wchar_t command[512];
	wcscpy (command, L"ffprobe.exe \"");

	wchar_t file[512];
	MultiByteToWideChar (CP_UTF8, 0, content.string().c_str(), -1, file, sizeof(file));
	wcscat (command, file);

	wcscat (command, L"\"");

	PROCESS_INFORMATION process_info;
	ZeroMemory (&process_info, sizeof (process_info));
	if (!CreateProcess (0, command, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &startup_info, &process_info)) {
		LOG_ERROR_NC (N_("ffprobe call failed (could not CreateProcess)"));
		return;
	}

	FILE* o = fopen_boost (out, "w");
	if (!o) {
		LOG_ERROR_NC (N_("ffprobe call failed (could not create output file)"));
		return;
	}

	CloseHandle (child_stderr_write);

	while (true) {
		char buffer[512];
		DWORD read;
		if (!ReadFile(child_stderr_read, buffer, sizeof(buffer), &read, 0) || read == 0) {
			break;
		}
		fwrite (buffer, read, 1, o);
	}

	fclose (o);

	WaitForSingleObject (process_info.hProcess, INFINITE);
	CloseHandle (process_info.hProcess);
	CloseHandle (process_info.hThread);
	CloseHandle (child_stderr_read);
#endif

#ifdef DCPOMATIC_LINUX
	string ffprobe = "ffprobe \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
        system (ffprobe.c_str ());
#endif

#ifdef DCPOMATIC_OSX
	boost::filesystem::path path = app_contents();
	path /= "MacOS";
	path /= "ffprobe";

	string ffprobe = "\"" + path.string() + "\" \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
	system (ffprobe.c_str ());
#endif
}

list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;

#ifdef DCPOMATIC_LINUX
	FILE* f = setmntent ("/etc/mtab", "r");
	if (!f) {
		return m;
	}

	while (true) {
		struct mntent* mnt = getmntent (f);
		if (!mnt) {
			break;
		}

		m.push_back (make_pair (mnt->mnt_dir, mnt->mnt_type));
	}

	endmntent (f);
#endif

	return m;
}

boost::filesystem::path
openssl_path ()
{
#ifdef DCPOMATIC_WINDOWS
	wchar_t dir[512];
	GetModuleFileName (GetModuleHandle (0), dir, sizeof (dir));
	PathRemoveFileSpec (dir);

	boost::filesystem::path path = dir;
	path /= "openssl.exe";
	return path;
#endif

#ifdef DCPOMATIC_OSX
	boost::filesystem::path path = app_contents();
	path /= "MacOS";
	path /= "openssl";
	return path;
#endif

#ifdef DCPOMATIC_LINUX
	return "dcpomatic2_openssl";
#endif

}

/* Apparently there is no way to create an ofstream using a UTF-8
   filename under Windows.  We are hence reduced to using fopen
   with this wrapper.
*/
FILE *
fopen_boost (boost::filesystem::path p, string t)
{
#ifdef DCPOMATIC_WINDOWS
        wstring w (t.begin(), t.end());
	/* c_str() here should give a UTF-16 string */
        return _wfopen (p.c_str(), w.c_str ());
#else
        return fopen (p.c_str(), t.c_str ());
#endif
}

int
dcpomatic_fseek (FILE* stream, int64_t offset, int whence)
{
#ifdef DCPOMATIC_WINDOWS
	return _fseeki64 (stream, offset, whence);
#else
	return fseek (stream, offset, whence);
#endif
}

void
Waker::nudge ()
{
#ifdef DCPOMATIC_WINDOWS
	boost::mutex::scoped_lock lm (_mutex);
	SetThreadExecutionState (ES_SYSTEM_REQUIRED);
#endif
}

Waker::Waker ()
{
#ifdef DCPOMATIC_OSX
	boost::mutex::scoped_lock lm (_mutex);
	/* We should use this */
        // IOPMAssertionCreateWithName (kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, CFSTR ("Encoding DCP"), &_assertion_id);
	/* but it's not available on 10.5, so we use this */
        IOPMAssertionCreate (kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &_assertion_id);
#endif
}

Waker::~Waker ()
{
#ifdef DCPOMATIC_OSX
	boost::mutex::scoped_lock lm (_mutex);
	IOPMAssertionRelease (_assertion_id);
#endif
}

void
start_tool (boost::filesystem::path dcpomatic, string executable,
#ifdef DCPOMATIC_OSX
	    string app
#else
	    string
#endif
	)
{
#if defined(DCPOMATIC_LINUX) || defined(DCPOMATIC_WINDOWS)
	boost::filesystem::path batch = dcpomatic.parent_path() / executable;
#endif

#ifdef DCPOMATIC_OSX
	boost::filesystem::path batch = dcpomatic.parent_path ();
	batch = batch.parent_path (); // MacOS
	batch = batch.parent_path (); // Contents
	batch = batch.parent_path (); // DCP-o-matic.app
	batch = batch.parent_path (); // Applications
	batch /= app;
	batch /= "Contents";
	batch /= "MacOS";
	batch /= executable;
#endif

#if defined(DCPOMATIC_LINUX) || defined(DCPOMATIC_OSX)
	pid_t pid = fork ();
	if (pid == 0) {
		int const r = system (batch.string().c_str());
		exit (WEXITSTATUS (r));
	}
#endif

#ifdef DCPOMATIC_WINDOWS
	STARTUPINFO startup_info;
	ZeroMemory (&startup_info, sizeof (startup_info));
	startup_info.cb = sizeof (startup_info);

	PROCESS_INFORMATION process_info;
	ZeroMemory (&process_info, sizeof (process_info));

	wchar_t cmd[512];
	MultiByteToWideChar (CP_UTF8, 0, batch.string().c_str(), -1, cmd, sizeof(cmd));
	CreateProcess (0, cmd, 0, 0, FALSE, 0, 0, 0, &startup_info, &process_info);
#endif
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
#ifdef DCPOMATIC_WINDOWS
	return (uint64_t) GetCurrentThreadId ();
#else
	return (uint64_t) pthread_self ();
#endif
}

int
avio_open_boost (AVIOContext** s, boost::filesystem::path file, int flags)
{
#ifdef DCPOMATIC_WINDOWS
	int const length = (file.string().length() + 1) * 2;
	char* utf8 = new char[length];
	WideCharToMultiByte (CP_UTF8, 0, file.c_str(), -1, utf8, length, 0, 0);
	int const r = avio_open (s, utf8, flags);
	delete[] utf8;
	return r;
#else
	return avio_open (s, file.c_str(), flags);
#endif
}

#ifdef DCPOMATIC_WINDOWS
void
maybe_open_console ()
{
	if (Config::instance()->win32_console ()) {
		AllocConsole();

		HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
		int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
		FILE* hf_out = _fdopen(hCrt, "w");
		setvbuf(hf_out, NULL, _IONBF, 1);
		*stdout = *hf_out;

		HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
		hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
		FILE* hf_in = _fdopen(hCrt, "r");
		setvbuf(hf_in, NULL, _IONBF, 128);
		*stdin = *hf_in;
	}
}
#endif

boost::filesystem::path
home_directory ()
{
#if defined(DCPOMATIC_LINUX) || defined(DCPOMATIC_OSX)
		return getenv("HOME");
#endif
#ifdef DCPOMATIC_WINDOWS
		return boost::filesystem::path(getenv("HOMEDRIVE")) / boost::filesystem::path(getenv("HOMEPATH"));
#endif
}

string
command_and_read (string cmd)
{
#ifdef DCPOMATIC_LINUX
	FILE* pipe = popen (cmd.c_str(), "r");
	if (!pipe) {
		throw runtime_error ("popen failed");
	}

	string result;
	char buffer[128];
	try {
		while (fgets(buffer, sizeof(buffer), pipe)) {
			result += buffer;
		}
	} catch (...) {
		pclose (pipe);
		throw;
	}

	pclose (pipe);
	return result;
#endif

	return "";
}

/** @return true if this process is a 32-bit one running on a 64-bit-capable OS */
bool
running_32_on_64 ()
{
#ifdef DCPOMATIC_WINDOWS
	BOOL p;
	IsWow64Process (GetCurrentProcess(), &p);
	return p;
#endif
	/* XXX: assuming nobody does this on Linux / OS X */
	return false;
}

#ifdef DCPOMATIC_OSX
static void
disk_appeared(DADiskRef disk, void* context)
{
	const char* name = DADiskGetBSDName (disk);
	if (name) {
		list<Drive>* drives = reinterpret_cast<list<Drive>*> (context);
		drives->push_back (name, 0, false, optional<string>(), optional<string>());
	}
}
#endif

vector<Drive>
get_drives ()
{
	vector<Drive> drives;

#ifdef DCPOMATIC_WINDOWS

	const GUID GUID_DEVICE_INTERFACE_DISK = {
		0x53F56307L, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B }
	};

	HDEVINFO device_info = SetupDiGetClassDevsA (&GUID_DEVICE_INTERFACE_DISK, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (device_info == INVALID_HANDLE_VALUE) {
		LOG_DIST_NC("SetupDiClassDevsA failed");
		return drives;
	}

	int i = 0;
	while (true) {
		SP_DEVINFO_DATA device_info_data;
		device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(device_info, i, &device_info_data)) {
			LOG_DIST ("SetupDiEnumDeviceInfo failed (%1)", GetLastError());
			return drives;
		}
		++i;

		wchar_t friendly_name_buffer[MAX_PATH];
		ZeroMemory (&friendly_name_buffer, sizeof(friendly_name_buffer));

		bool r = SetupDiGetDeviceRegistryPropertyW (
			device_info, &device_info_data, SPDRP_FRIENDLYNAME, 0, static_cast<PBYTE>(wbuffer), sizeof(wbuffer), 0
			);

		if (r) {
			int const length = (wcslen(friendly_name_buffer) + 1) * 2;
			char* utf8 = new char[length];
			/* XXX: this is used in a few places in this file; should be abstracted out */
			WideCharToMultiByte (CP_UTF8, 0, friendly_name_buffer, -1, utf8, length, 0, 0);
			/* XXX: utf8 contains a user-readable name */
			delete[] utf8;
		}

		int j = 0;
		while (true) {
			SP_DEVICE_INTERFACE_DATA device_interface_data;
			device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

			bool r = SetupDiEnumDeviceInterfaces (device_info, &device_info_data, &GUID_DEVICE_INTERFACE_DISK, j, &device_interface_data);
			if (!r) {
				DWORD e = GetLastError();
				if (e == ERROR_NO_MORE_ITEMS) {
					break;
				} else {
					LOG_DIST("SetupDiEnumDeviceInterfaces failed (%1)", e);
					return drives;
				}
			}

			DWORD size;
			r = SetupDiGetDeviceInterfaceDetailW(device_info, &device_interface_data, 0, 0, &size, 0);
			PSP_DEVICE_INTERFACE_DETAIL_DATA_W device_detail_data = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W> (malloc(size));
			if (!device_detail_data) {
				LOG_DIST_NC("malloc failed");
				return drives;
			}

			device_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

			r = SetupDiGetDeviceInterfaceDetailW (device_info, &device_interface_data, device_detail_data, size, &size, 0);
			if (!r) {
				LOG_DIST_NC("SetupDiGetDeviceInterfaceDetailW failed");
				return 1;
			}

			HANDLE device = CreateFileW (
				device_detail_data->DevicePath, 0,
				FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
				);

			if (device == INVALID_HANDLE_VALUE) {
				LOG_DIST_NC("CreateFileW failed");
				return drives;
			}

			VOLUME_DISK_EXTENTS disk_extents;
			r = DeviceIoControl (
				device, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0,
				&disk_extents, sizeof(VOLUME_DISK_EXTENTS), &size, 0
				);

			if (r && disk_extents.NumberOfDiskExtents > 0) {
				LOG_DIST("GET_VOLUME_DISK_EXTENTS gives %1", disk_extents.Extents[0].DiskNumber);
				/* Disk number for \\.\PHYSICALDRIVEx is disk disk_extents.Extents[0].DiskNumber */
			}

			STORAGE_DEVICE_NUMBER device_number;
			r = DeviceIoControl (
				device, IOCTL_STORAGE_GET_DEVICE_NUMBER, 0, 0,
				&device_number, sizeof(device_number), &size, 0
				);

			if (r) {
				LOG_DIST("GET_DEVICE_NUMBER gives %1", disk_number.DeviceNumber);
				/* Disk number for \\.\PHYSICALDRIVEx is device_number.DeviceNumber */
			}

			++j;
		}
	}
#endif

#ifdef DCPOMATIC_OSX
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
}
#endif

#ifdef DCPOMATIC_LINUX

	using namespace boost::filesystem;
	list<string> mounted_devices;
	std::ifstream f("/proc/mounts");
	string line;
	while (f.good()) {
		getline(f, line);
		vector<string> bits;
		boost::algorithm::split (bits, line, boost::is_any_of(" "));
		if (bits.size() > 0 && boost::algorithm::starts_with(bits[0], "/dev/")) {
			mounted_devices.push_back(bits[0]);
			LOG_DIST("Mounted device %1", bits[0]);
		}
	}

	for (directory_iterator i = directory_iterator("/sys/block"); i != directory_iterator(); ++i) {
		string const name = i->path().filename().string();
		path device_type_file("/sys/block/" + name + "/device/type");
		optional<string> device_type;
		if (exists(device_type_file)) {
			device_type = dcp::file_to_string (device_type_file);
			boost::trim(*device_type);
		}
		/* Device type 5 is "SCSI_TYPE_ROM" in blkdev.h; seems usually to be a CD/DVD drive */
		if (!boost::algorithm::starts_with(name, "loop") && (!device_type || *device_type != "5")) {
			uint64_t const size = dcp::raw_convert<uint64_t>(dcp::file_to_string(*i / "size")) * 512;
			if (size == 0) {
				continue;
			}
			bool mounted = false;
			optional<string> vendor;
			try {
				vendor = dcp::file_to_string("/sys/block/" + name + "/device/vendor");
				boost::trim(*vendor);
			} catch (...) {}
			optional<string> model;
			try {
				model = dcp::file_to_string("/sys/block/" + name + "/device/model");
				boost::trim(*model);
			} catch (...) {}
			BOOST_FOREACH (string j, mounted_devices) {
				if (boost::algorithm::starts_with(j, "/dev/" + name)) {
					mounted = true;
				}
			}
			drives.push_back(Drive(i->path().filename().string(), size, mounted, vendor, model));
			LOG_DIST("Block device %1 size %2 %3 vendor %4 model %5", name, size, mounted ? "mounted" : "not mounted", vendor.get_value_or("[none]"), model.get_value_or("[none]"));
		}
	}
#endif
	return drives;
}

string
Drive::description () const
{
	char gb[64];
	snprintf(gb, 64, "%.1f", _size / 1000000000.0);

	string name;
	if (_vendor) {
		name += *_vendor;
	}
	if (_model) {
		if (name.size() > 0) {
			name += " " + *_model;
		}
	}
	if (name.size() == 0) {
		name = _("Unknown");
	}

	return String::compose("%1 (%2 GB) [%3]", name, gb, _internal_name);
}

#ifdef DCPOMATIC_LINUX
void
unprivileged ()
{
	uid_t ruid, euid, suid;
	if (getresuid(&ruid, &euid, &suid) == -1) {
		cerr << "getresuid() failed.\n";
		exit (EXIT_FAILURE);
	}
	seteuid (ruid);
}

PrivilegeEscalator::~PrivilegeEscalator ()
{
	unprivileged ();
}

PrivilegeEscalator::PrivilegeEscalator ()
{
	seteuid (0);
}
#endif

boost::filesystem::path
config_path ()
{
	boost::filesystem::path p;
#ifdef DCPOMATIC_OSX
		p /= g_get_home_dir ();
		p /= "Library";
		p /= "Preferences";
		p /= "com.dcpomatic";
		p /= "2";
#else
		p /= g_get_user_config_dir ();
		p /= "dcpomatic2";
#endif
	return p;
}
