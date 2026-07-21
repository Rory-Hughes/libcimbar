/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "Common.h"

#include <limits>
#include <vector>

TEST_CASE( "CommonTest/decodeRgbImage", "[unit]" )
{
	cv::Mat source(2, 3, CV_8UC3, cv::Scalar(10, 20, 30));
	std::vector<uchar> encoded;
	REQUIRE(cv::imencode(".png", source, encoded));

	cv::Mat decoded = cimbar::detail::decode_image(encoded.data(), encoded.size());

	REQUIRE_FALSE(decoded.empty());
	assertEquals(3, decoded.cols);
	assertEquals(2, decoded.rows);
	assertEquals(3, decoded.channels());
}

TEST_CASE( "CommonTest/decodeRgbaImage", "[unit]" )
{
	cv::Mat source(2, 3, CV_8UC4, cv::Scalar(10, 20, 30, 40));
	std::vector<uchar> encoded;
	REQUIRE(cv::imencode(".png", source, encoded));

	cv::Mat decoded = cimbar::detail::decode_image(encoded.data(), encoded.size());

	REQUIRE_FALSE(decoded.empty());
	assertEquals(3, decoded.channels());
}

TEST_CASE( "CommonTest/rejectInvalidInput", "[unit]" )
{
	const uint8_t invalid[] = {0x00, 0x01, 0x02, 0x03};

	REQUIRE(cimbar::detail::decode_image(nullptr, sizeof(invalid)).empty());
	REQUIRE(cimbar::detail::decode_image(invalid, 0U).empty());
	REQUIRE(cimbar::detail::decode_image(invalid, sizeof(invalid)).empty());
	REQUIRE(cimbar::detail::decode_image(
	    invalid,
	    static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1U
	).empty());
}
