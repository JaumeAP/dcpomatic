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


#include "dcpomatic_time.h"
#include <dcp/raw_convert.h>
#include <boost/algorithm/string.hpp>
#include <inttypes.h>


using std::string;
using std::vector;
using namespace dcpomatic;


bool
dcpomatic::operator<=(HMSF const& a, HMSF const& b)
{
	if (a.h != b.h) {
		return a.h <= b.h;
	}

	if (a.m != b.m) {
		return a.m <= b.m;
	}

	if (a.s != b.s) {
		return a.s <= b.s;
	}

	return a.f <= b.f;
}


template <>
Time<DCPTimeDifferentiator, ContentTimeDifferentiator>::Time (ContentTime d, FrameRateChange f)
	: _t (llrint(d.get() / f.speed_up))
{

}


DCPTime
dcpomatic::min (DCPTime a, DCPTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}


DCPTime
dcpomatic::max (DCPTime a, DCPTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}


ContentTime
dcpomatic::min (ContentTime a, ContentTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}


ContentTime
dcpomatic::max (ContentTime a, ContentTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}


string
dcpomatic::to_string (ContentTime t)
{
	char buffer[64];
#ifdef DCPOMATIC_WINDOWS
	__mingw_snprintf (buffer, sizeof(buffer), "[CONT %" PRId64 " %fs]", t.get(), t.seconds());
#else
	snprintf (buffer, sizeof(buffer), "[CONT %" PRId64 " %fs]", t.get(), t.seconds());
#endif
	return buffer;
}


DCPTime::DCPTime(Type num)
	: _num(num)
	, _den(96000)
{

}


DCPTime::DCPTime(Type num, Type den)
	: _num(num)
	, _den(den)
{
	DCPOMATIC_ASSERT(_den);
}


DCPTime::DCPTime(ContentTime time, FrameRateChange frc)
	: _num(llrint(time.get() / frc.speed_up))
	, _den(ContentTime::HZ)
{

}


DCPTime::DCPTime(string const& serializable_string)
{
	vector<string> parts;
	boost::algorithm::split(parts, serializable_string, boost::is_any_of("_"));
	if (parts.size() == 1) {
		_num = dcp::raw_convert<int64_t>(parts[0]);
		_den = 96000;
	} else {
		_num = dcp::raw_convert<int64_t>(parts[0]);
		_den = dcp::raw_convert<int64_t>(parts[1]);
	}
}


DCPTime::DCPTime(HMSF const& hmsf, float fps)
{
	*this = from_seconds(hmsf.h * 3600)
		+ from_seconds(hmsf.m * 60)
		+ from_seconds(hmsf.s)
		+ DCPTime(hmsf.f * 1000, fps * 1000);
}


string
DCPTime::to_serialisable_string() const
{
	return fmt::format("{}_{}", _num, _den);
}


string
DCPTime::to_debug_string() const
{
	return fmt::format("[{}/{} {}]", _num, _den, seconds());
}


double
DCPTime::seconds() const
{
	return static_cast<double>(_num) / _den;
}

bool
DCPTime::operator<(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num < o._num;
}


bool
DCPTime::operator<=(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num <= o._num;
}


bool
DCPTime::operator==(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num == o._num;
}


bool
DCPTime::operator!=(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num != o._num;
}


bool
DCPTime::operator>=(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num >= o._num;
}


bool
DCPTime::operator>(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return _num > o._num;
}


int64_t
DCPTime::frames_floor(int r) const
{
	return (_num * r) / _den;
}


int64_t
DCPTime::frames_round(int r) const
{
	return ((_num * r) + (r / 2)) / _den;
}


int64_t
DCPTime::frames_ceil(int r) const
{
	return ((_num + 1) * r) / _den;
}


DCPTime
DCPTime::operator+(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return DCPTime(_num + o._num, _den);
}


DCPTime&
DCPTime::operator+=(DCPTime const& o)
{
	DCPOMATIC_ASSERT(_den == o._den);
	_num += o._num;
	return *this;
}


DCPTime
DCPTime::operator-(DCPTime const& o) const
{
	DCPOMATIC_ASSERT(_den == o._den);
	return DCPTime(_num - o._num, _den);
}


DCPTime
DCPTime::operator-() const
{
	return DCPTime(-_num, _den);
}


DCPTime&
DCPTime::operator-=(DCPTime const& o)
{
	DCPOMATIC_ASSERT(_den == o._den);
	_num -= o._num;
	return *this;
}


DCPTime
DCPTime::operator*(int o) const
{
	return DCPTime(_num * o, _den);
}


DCPTime
DCPTime::operator/(int o) const
{
	return DCPTime(_num, _den * o);
}


DCPTime::operator bool() const
{
	return _num != 0;
}


DCPTime
DCPTime::max() const
{
	return DCPTime(INT64_MAX, _den);
}


DCPTime
DCPTime::from_seconds(double s)
{
	return DCPTime(s * 96000, 96000);
}


DCPTime
DCPTime::floor(int r) const
{
	return DCPTime(frames_floor(r), r);
}


DCPTime
DCPTime::round(int r) const
{
	return DCPTime(frames_round(r), r);
}


DCPTime
DCPTime::ceil(int r) const
{
	return DCPTime(frames_ceil(r), r);
}


DCPTime
DCPTime::abs() const
{
	return DCPTime(std::abs(_num), _den);
}


HMSF
DCPTime::splitX(int r) const
{
	/* Do this calculation with frames so that we can round
	   to a frame boundary at the start rather than the end.
	*/
	auto ff = frames_round(r);
	HMSF hmsf;

	hmsf.h = ff / (3600 * r);
	ff -= static_cast<int64_t>(hmsf.h) * 3600 * r;
	hmsf.m = ff / (60 * r);
	ff -= static_cast<int64_t>(hmsf.m) * 60 * r;
	hmsf.s = ff / r;
	ff -= static_cast<int64_t>(hmsf.s) * r;

	hmsf.f = static_cast<int>(ff);
	return hmsf;
}


string
DCPTime::timecodeX(int r) const
{
	auto hmsf = splitX(r);

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d:%02d", hmsf.h, hmsf.m, hmsf.s, hmsf.f);
	return buffer;
}


ContentTime
DCPTime::content_time(FrameRateChange frc) const
{
	return ContentTime(frames_round(ContentTime::HZ) * frc.speed_up);
}

