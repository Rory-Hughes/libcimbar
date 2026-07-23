/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"
#include "TestHelpers.h"

#include "DeskewerPlus.h"
#include "image_hash/average_hash.h"
#include <string>

TEST_CASE( "DeskewerTest/testSimple", "[unit]" )
{
	Corners corners({312, 519}, {323, 2586}, {2405, 461}, {2425, 2594});
	DeskewerPlus de(0, {1024, 1024}, 30);

	cv::Mat actual = de.deskew(TestCimbar::getSample("6bit/4_30_f0_big.jpg"), corners);
	assertEquals(cv::Size(1024, 1024), actual.size());

	assertEquals( 0x6e483730782fee5c, image_hash::average_hash(actual) );
}

TEST_CASE( "DeskewerTest/testPadded", "[unit]" )
{
	Corners corners({312, 519}, {323, 2586}, {2405, 461}, {2425, 2594});
	DeskewerPlus de(8, {1024, 1024}, 30);

	cv::Mat actual = de.deskew(TestCimbar::getSample("6bit/4_30_f0_big.jpg"), corners);
	assertEquals(cv::Size(1040, 1040), actual.size());

	cv::Rect crop(8, 8, 1024, 1024);
	cv::Mat innerGrid = actual(crop);

	assertEquals( 0x6e483730782fee5c, image_hash::average_hash(innerGrid) );
}

TEST_CASE( "DeskewerTest/rejectsDegenerateGeometry", "[unit]" )
{
	cv::Mat img(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));

	Deskewer de(0, {32, 32}, 4);
	assertTrue(de.deskew(img, Corners({4, 4}, {4, 4}, {28, 28}, {28, 28})).empty());
	assertTrue(de.deskew(img, Corners({4, 4}, {12, 12}, {20, 20}, {28, 28})).empty());

	Deskewer invalid_output(0, {8, 8}, 16);
	assertTrue(invalid_output.deskew(img, Corners({4, 4}, {28, 4}, {4, 28}, {28, 28})).empty());
	assertTrue(de.deskew(cv::Mat(), Corners({4, 4}, {28, 4}, {4, 28}, {28, 28})).empty());
}


