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


#include "../test.h"
#include "lib/util.h"
#include <dcp/scope_guard.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>

#include "lib/i18n.h"


using std::string;


#define translation_en BOOST_CHECK_EQUAL(string(_("3D denoiser")), "3D denoiser");
#define translation_de BOOST_CHECK_EQUAL(string(_("3D denoiser")), "3D Rauschunterdrückung");
#define fmt_en BOOST_CHECK_EQUAL(fmt::to_string("1.23"), "1.23");
#define numbers_en BOOST_CHECK_EQUAL(dcp::locale_convert<string>(1.23), "1.23");
#define numbers_de BOOST_CHECK_EQUAL(dcp::locale_convert<string>(1.23), "1,23");


/* From https://www.gnu.org/software/gettext/manual/gettext.html */
static void tell_gettext_about_language_change()
{
	extern int _nl_msg_cat_cntr;
	_nl_msg_cat_cntr++;
}


static void restore()
{
	dcpomatic_setup_c_and_gettext_i18n("C");
	tell_gettext_about_language_change();
}


#ifdef DCPOMATIC_POSIX
BOOST_AUTO_TEST_CASE(i18n_test)
{
	override_mo_path = boost::filesystem::current_path().parent_path().parent_path() / "share" / "locale";

	/* cscript sets LC_ALL=C.  Undo this first */
	unsetenv("LC_ALL");
	dcp::ScopeGuard sg = []() { restore(); };

	/* English language, German numbers */
	putenv(strdup("LANG=en_GB.UTF-8"));
	putenv(strdup("LC_NUMERIC=de_DE.UTF-8"));
	dcpomatic_setup_c_and_gettext_i18n("");
	tell_gettext_about_language_change();
	fmt_en

	translation_en
	numbers_de

	/* German language, German numbers */
	putenv(strdup("LANG=de_DE.UTF-8"));
	dcpomatic_setup_c_and_gettext_i18n("");
	tell_gettext_about_language_change();
	fmt_en

	translation_de
	numbers_de

	/* German language, German numbers, forced to English */
	dcpomatic_setup_c_and_gettext_i18n("en_GB");
	tell_gettext_about_language_change();
	fmt_en

	translation_en
	numbers_de
}
#endif


#ifdef DCPOMATIC_WINDOWS
/* This test is only run on a user account set to en_DE */
BOOST_AUTO_TEST_CASE(i18n_test_en_de)
{
	override_mo_path = boost::filesystem::current_path().parent_path().parent_path() / "share" / "locale";

	/* cscript sets LC_ALL=C.  Undo this first */
	putenv("LC_ALL=");
	dcp::ScopeGuard sg = []() { restore(); };

	/* English language, German numbers */
	dcpomatic_setup_c_and_gettext_i18n("");
	tell_gettext_about_language_change();
	fmt_en

	translation_en
	numbers_de

	/* English language, German numbers, forced to German */
	dcpomatic_setup_c_and_gettext_i18n("de_DE");
	tell_gettext_about_language_change();
	fmt_en

	translation_de
	numbers_de
}


/* This test is only run on a user account set to de_DE */
BOOST_AUTO_TEST_CASE(i18n_test_de_de)
{
	override_mo_path = boost::filesystem::current_path().parent_path().parent_path() / "share" / "locale";

	/* cscript sets LC_ALL=C.  Undo this first */
	putenv("LC_ALL=");
	dcp::ScopeGuard sg = []() { restore(); };

	/* German language, German numbers */
	dcpomatic_setup_c_and_gettext_i18n("");
	tell_gettext_about_language_change();
	fmt_en

	translation_de
	numbers_de

	/* German language, German numbers, forced back to English */
	dcpomatic_setup_c_and_gettext_i18n("en_GB");
	tell_gettext_about_language_change();
	fmt_en

	translation_en
	numbers_de
}
#endif
