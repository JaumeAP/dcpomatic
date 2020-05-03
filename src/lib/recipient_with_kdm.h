#ifndef DCPOMATIC_RECIPIENT_WITH_KDM_H
#define DCPOMATIC_RECIPIENT_WITH_KDM_H

#include <dcp/name_format.h>
#include <boost/filesystem.hpp>

class RecipientWithKDM
{
public:
	virtual void add_name_values (dcp::NameFormat::Map& values) = 0;
	virtual std::string kdm_as_xml () const = 0;
	virtual void kdm_as_xml_to_file (boost::filesystem::path out) const = 0;
};

#endif
