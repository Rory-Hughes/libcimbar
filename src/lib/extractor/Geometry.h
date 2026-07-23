/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Corners.h"
#include "Midpoints.h"
#include "Point.h"
#include <cmath>
#include <optional>
#include <vector>
#include <utility>

namespace Geometry
{
	using floating_point = point<double>;

	inline bool is_finite(const floating_point& p)
	{
		return std::isfinite(p.x()) && std::isfinite(p.y());
	}

	inline floating_point line_intersection(const std::pair<floating_point, floating_point>& lineA, const std::pair<floating_point, floating_point>& lineB)
	{
		if (!is_finite(lineA.first) || !is_finite(lineA.second) ||
		    !is_finite(lineB.first) || !is_finite(lineB.second))
		{
			return floating_point::NONE();
		}

		auto compute = [] (const floating_point& p, const floating_point& q) {
			double xdiff = q.x() - p.x();
			double ydiff = p.y() - q.y();
			double determinant = q.x() * p.y() - p.x() * q.y();
			return std::tuple<double, double, double>(xdiff, ydiff, determinant);
		};

		auto [ax, ay, adet] = compute(lineA.first, lineA.second);
		auto [bx, by, bdet] = compute(lineB.first, lineB.second);

		double D = ay * bx - ax * by;
		if (!std::isfinite(D) || std::fabs(D) < 1e-8)
		    return floating_point::NONE();

		double Dx = adet * bx - ax * bdet;
		double Dy = ay * bdet - adet * by;
		floating_point result({Dx / D, Dy / D});
		if (!is_finite(result))
			return floating_point::NONE();
		return result;
	}

	inline Midpoints calculate_midpoints(const Corners& sq)
	{
		std::vector<floating_point> mids;
		using line = std::pair<floating_point, floating_point>;

		line cross1 = {sq.top_left().to_float(), sq.bottom_right().to_float()};
		line cross2 = {sq.top_right().to_float(), sq.bottom_left().to_float()};
		auto center = line_intersection(cross1, cross2);
		if (!center)
			return mids;

		line right = {sq.top_right().to_float(), sq.bottom_right().to_float()};
		line left = {sq.top_left().to_float(), sq.bottom_left().to_float()};
		auto leftRightInf = line_intersection(right, left);
		if (!leftRightInf)
			return mids;
		line vertical = {center, leftRightInf};

		line top = {sq.top_left().to_float(), sq.top_right().to_float()};
		line bottom = {sq.bottom_left().to_float(), sq.bottom_right().to_float()};
		auto topBottomInf = line_intersection(top, bottom);
		if (!topBottomInf)
			return mids;
		line horizontal = {center, topBottomInf};

		// there are some corner cases that need to be handled here...
		auto tmid = line_intersection(top, vertical);
		if (!tmid)
			return {};
		mids.push_back(tmid);

		auto rmid = line_intersection(right, horizontal);
		if (!rmid)
			return {};
		mids.push_back(rmid);

		auto bmid = line_intersection(bottom, vertical);
		if (!bmid)
			return {};
		mids.push_back(bmid);

		auto lmid = line_intersection(left, horizontal);
		if (!lmid)
			return {};
		mids.push_back(lmid);

		return mids;
	}
}
