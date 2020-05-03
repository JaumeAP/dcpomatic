#include "util.h"
#include "recipient_with_kdm.h"
#include <dcp/name_format.h>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

using std::list;
using std::cout;
using boost::shared_ptr;

int
write_kdm_files (
	list<shared_ptr<RecipientWithKDM> > kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (directory == "-") {
		/* Write KDMs to the stdout */
		BOOST_FOREACH (shared_ptr<RecipientWithKDM> i, kdms) {
			cout << i->kdm_as_xml ();
			++written;
		}

		return written;
	}

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	BOOST_FOREACH (shared_ptr<RecipientWithKDM> i, kdms) {
		i->add_name_values (name_values);
		boost::filesystem::path out = directory / careful_string_filter(name_format.get(name_values, ".xml"));
		if (!boost::filesystem::exists(out) || confirm_overwrite(out)) {
			i->kdm_as_xml_to_file (out);
			++written;
		}
	}

	return written;
}
