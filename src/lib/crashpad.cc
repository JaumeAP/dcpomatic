#include <client/crashpad_client.h>


void
setup_crashpad()
{
	crashpad::CrashpadClient client;
	client.StartHandler(
		base::FilePath("/home/carl/src/crashpad/crashpad/out/Default/crashpad_handler"),
		base::FilePath("/home/carl/.config/dcpomatic2/crashpad"),
		base::FilePath("/home/carl/.config/dcpomatic2/crashpad"),
		"",
		{},
		{},
		true,
		false,
		{}
	);
}

