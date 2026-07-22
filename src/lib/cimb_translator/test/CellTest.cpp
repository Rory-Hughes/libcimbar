/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"
#include "TestHelpers.h"

#include "Cell.h"
#include "Common.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
using std::string;


TEST_CASE( "CellTest/testRgbMatchesOpenCV", "[unit]" )
{
	cv::Mat cell = TestCimbar::loadSample("mycell.png");
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(cell).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (unsigned)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (unsigned)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (unsigned)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	cv::Rect crop(125, 8, 8, 8);
	cv::Mat cell = img(crop);
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(cell).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Contiguous", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	cv::Rect crop(125, 8, 8, 8);
	cv::Mat cell = img(crop);
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(img, 125, 8, 8, 8).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Asymmetric", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	cv::Rect crop(125, 8, 4, 6);
	cv::Mat cell = img(crop);
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(cell).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Asymmetric.Contiguous", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	cv::Rect crop(126, 9, 6, 6);
	cv::Mat cell = img(crop);
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(img, 126, 9, 6, 6).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
		assertEquals( 198, (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
		assertEquals( 198, (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
		assertEquals( 0, (int)b );
	}
}

/*TEST_CASE( "CellTest/testRgbCellOffsets.WideCanvas", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("bm/ecc35.png");

	cv::Rect crop(127, 10, 6, 6);
	cv::Mat cell = img(crop);
	cv::Scalar expectedColor = cv::mean(cell);

	auto [r, g, b] = Cell(img, 127, 10, 6, 6).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
		assertEquals( 148, (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
		assertEquals( 0, (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
		assertEquals( 148, (int)b );
	}
}*/

TEST_CASE( "CellTest/rectangularNonContiguousRgbMatchesOpenCV", "[unit][security]" )
{
	cv::Mat parent(9, 13, CV_8UC3);
	for (int row = 0; row < parent.rows; ++row)
	{
		for (int column = 0; column < parent.cols; ++column)
			parent.at<cv::Vec3b>(row, column) = cv::Vec3b(
			    static_cast<uchar>(row * 10 + column),
			    static_cast<uchar>(row + column * 3),
			    static_cast<uchar>(200 - row * 2 - column)
			);
	}

	cv::Mat region = parent(cv::Rect(3, 2, 7, 4));
	assertFalse(region.isContinuous());
	const cv::Scalar expected = cv::mean(region);
	const auto [first, second, third] = Cell(region).mean_rgb();
	assertEquals(static_cast<int>(expected[0]), static_cast<int>(first));
	assertEquals(static_cast<int>(expected[1]), static_cast<int>(second));
	assertEquals(static_cast<int>(expected[2]), static_cast<int>(third));
}

TEST_CASE( "CellTest/largeContinuousRgbDoesNotOverflowAccumulator", "[unit][security]" )
{
	cv::Mat image(300, 300, CV_8UC3, cv::Scalar(255, 254, 253));
	const auto [first, second, third] = Cell(image).mean_rgb();
	assertEquals(255, static_cast<int>(first));
	assertEquals(254, static_cast<int>(second));
	assertEquals(253, static_cast<int>(third));
}

TEST_CASE( "CellTest/grayscaleRegionUsesRowsAndColumns", "[unit][security]" )
{
	cv::Mat image(7, 11, CV_8UC1);
	for (int row = 0; row < image.rows; ++row)
		for (int column = 0; column < image.cols; ++column)
			image.at<uchar>(row, column) = static_cast<uchar>(row * 11 + column);

	const cv::Rect bounds(4, 1, 5, 3);
	const cv::Scalar expected = cv::mean(image(bounds));
	const uchar actual = Cell(image, bounds.x, bounds.y, bounds.width, bounds.height).mean_grayscale();
	assertEquals(static_cast<int>(expected[0]), static_cast<int>(actual));
}

TEST_CASE( "CellTest/rejectsOutOfBoundsRegions", "[unit][security]" )
{
	cv::Mat image(4, 4, CV_8UC3, cv::Scalar(1, 2, 3));
	const auto [negative_first, negative_second, negative_third] =
	    Cell(image, -1, 0, 2, 2).mean_rgb();
	assertEquals(0, static_cast<int>(negative_first));
	assertEquals(0, static_cast<int>(negative_second));
	assertEquals(0, static_cast<int>(negative_third));

	const auto [large_first, large_second, large_third] =
	    Cell(image, 3, 3, 2, 2).mean_rgb();
	assertEquals(0, static_cast<int>(large_first));
	assertEquals(0, static_cast<int>(large_second));
	assertEquals(0, static_cast<int>(large_third));
}
