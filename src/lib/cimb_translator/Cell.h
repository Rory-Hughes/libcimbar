/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include <opencv2/opencv.hpp>

#include <cstddef>
#include <cstdint>
#include <tuple>

class Cell
{
public:
	static const bool SKIP = true;

public:
	Cell(const cv::Mat& img)
		: _img(img)
		, _cols(img.cols)
		, _rows(img.rows)
	{
	}

	Cell(const cv::Mat& img, int xstart, int ystart, int cols, int rows)
		: _img(img)
		, _xstart(xstart)
		, _ystart(ystart)
		, _cols(cols)
		, _rows(rows)
	{}

	std::tuple<uchar,uchar,uchar> mean_rgb_continuous(bool skip) const
	{
		return mean_rgb_region(skip);
	}

	std::tuple<uchar,uchar,uchar> mean_rgb(bool skip=false) const
	{
		return mean_rgb_region(skip);
	}

	uchar mean_grayscale_continuous() const
	{
		return mean_grayscale_region();
	}

	uchar mean_grayscale() const
	{
		return mean_grayscale_region();
	}

	void crop(int x, int y, int cols, int rows)
	{
		_xstart += x;
		_ystart += y;
		_cols = cols;
		_rows = rows;
	}

	int cols() const
	{
		return _cols;
	}

	int rows() const
	{
		return _rows;
	}

protected:
	bool valid_region(int minimum_channels, int maximum_channels) const
	{
		if (_img.empty() || _img.depth() != CV_8U ||
		    _img.channels() < minimum_channels || _img.channels() > maximum_channels ||
		    _xstart < 0 || _ystart < 0 || _cols <= 0 || _rows <= 0 ||
		    _xstart > _img.cols || _ystart > _img.rows)
			return false;

		return _cols <= _img.cols - _xstart && _rows <= _img.rows - _ystart;
	}

	std::tuple<uchar,uchar,uchar> mean_rgb_region(bool skip) const
	{
		if (!valid_region(3, CV_CN_MAX))
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);

		std::uint64_t first = 0U;
		std::uint64_t second = 0U;
		std::uint64_t third = 0U;
		std::size_t count = 0U;
		const int channels = _img.channels();
		const int row_increment = skip ? 2 : 1;
		const std::size_t start_offset =
		    static_cast<std::size_t>(_xstart) * static_cast<std::size_t>(channels);

		for (int row = 0; row < _rows; row += row_increment)
		{
			const uchar* pixels = _img.ptr<uchar>(_ystart + row) + start_offset;
			for (int column = 0; column < _cols; ++column, ++count)
			{
				const std::size_t offset =
				    static_cast<std::size_t>(column) * static_cast<std::size_t>(channels);
				first += pixels[offset];
				second += pixels[offset + 1U];
				third += pixels[offset + 2U];
			}
		}

		if (count == 0U)
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);
		return std::tuple<uchar,uchar,uchar>(
		    static_cast<uchar>(first / count),
		    static_cast<uchar>(second / count),
		    static_cast<uchar>(third / count)
		);
	}

	uchar mean_grayscale_region() const
	{
		if (!valid_region(1, 1))
			return 0;

		std::uint64_t total = 0U;
		std::size_t count = 0U;
		for (int row = 0; row < _rows; ++row)
		{
			const uchar* pixels = _img.ptr<uchar>(_ystart + row) + _xstart;
			for (int column = 0; column < _cols; ++column, ++count)
				total += pixels[column];
		}

		return count == 0U ? 0 : static_cast<uchar>(total / count);
	}

	const cv::Mat& _img;
	int _xstart = 0;
	int _ystart = 0;
	int _cols;
	int _rows;
};
