#include "recipient_with_kdm.h"
#include <boost/function.hpp>
#include <list>

extern int write_kdm_files (
	std::list<boost::shared_ptr<RecipientWithKDM> > kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	);
