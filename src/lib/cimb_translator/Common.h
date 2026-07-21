/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include <opencv2/opencv.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cimbar
{
	using RGB = std::tuple<uchar,uchar,uchar>;

	namespace detail
	{
		cv::Mat decode_image(const uint8_t* data, std::size_t size);
	}

	cv::Mat load_img(std::string path);

	std::tuple<uchar,uchar,uchar> getColor(unsigned index, unsigned num_colors, unsigned color_mode);
	cv::Mat getTile(unsigned symbol_bits, unsigned symbol, bool dark=true, unsigned num_colors=4, unsigned color=0, unsigned color_mode=1);
}
