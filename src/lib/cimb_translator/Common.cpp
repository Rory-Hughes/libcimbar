/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "Common.h"

#include "Config.h"
#include "base91/base.hpp"
#include "serialize/format.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <opencv2/opencv.hpp>

#include <limits>
#include <map>
#include <memory>
#include <string>
#include "bitmaps.h"

using cimbar::RGB;
using std::array;
using std::string;
using std::vector;

namespace {
	RGB getColor4(unsigned index)
	{
		// opencv uses BGR, but we don't have to conform to its tyranny
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0),
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0, 0xFF),
		};
		return colors[index];
	}

	RGB getColor4_enc(unsigned index)
	{
		// opencv uses BGR, but we don't have to conform to its tyranny
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0),
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0x55, 0xFF),
		};
		return colors[index];
	}

	RGB getColor4_old(unsigned index)
	{
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0, 0xFF),
			RGB(0, 0xFF, 0),
		};
		return colors[index];
	}

	RGB getColor8(unsigned index)
	{
		static constexpr array<RGB, 8> colors = {
			RGB(0, 0xFF, 0xFF), // cyan
			RGB(0xFF, 0xFF, 0), // yellow
			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
			RGB(0xFF, 0xFF, 0xFF), // white
			RGB(0, 0xFF, 0), // green
			RGB(0xFF, 0x9F, 0),  // orange
			RGB(0xFF, 0, 0xFF), // magenta
			RGB(0xFF, 65, 65), // red
		};
		return colors[index];
	}

	RGB getColor8_old(unsigned index)
	{
		static constexpr array<RGB, 8> colors = {
			RGB(0, 0xFF, 0xFF), // cyan
			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
			RGB(0xFF, 0, 0xFF), // magenta
			RGB(0xFF, 65, 65), // red
			RGB(0xFF, 0x9F, 0),  // orange
			RGB(0xFF, 0xFF, 0), // yellow
			RGB(0xFF, 0xFF, 0xFF),
			RGB(0, 0xFF, 0),
		};
		return colors[index];
	}

	RGB getBgColor4(unsigned index)
	{
		// opencv uses BGR, but we don't have to conform to its tyranny
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0x20, 0),
			RGB(0, 0, 0xFF),
			RGB(0x7F, 0, 0),
			RGB(0, 0, 0),
		};
		return colors[index];
	}
}

namespace cimbar {

namespace detail {

cv::Mat decode_image(const uint8_t* data, std::size_t size)
{
	static constexpr int maximum_dimension = 4096;
	static constexpr std::size_t maximum_pixels = 16U * 1024U * 1024U;
	static constexpr int output_channels = STBI_rgb_alpha;

	if (data == nullptr || size == 0U ||
	    size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		return cv::Mat();

	int width = 0;
	int height = 0;
	int source_channels = 0;
	const int encoded_size = static_cast<int>(size);
	if (!stbi_info_from_memory(data, encoded_size, &width, &height, &source_channels) ||
	    width <= 0 || height <= 0 ||
	    width > maximum_dimension || height > maximum_dimension ||
	    source_channels <= 0 || source_channels > STBI_rgb_alpha)
		return cv::Mat();

	const std::size_t pixel_count =
	    static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	if (pixel_count > maximum_pixels)
		return cv::Mat();

	using stbi_image_ptr = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
	stbi_image_ptr imgdata(
	    stbi_load_from_memory(
	        data,
	        encoded_size,
	        &width,
	        &height,
	        &source_channels,
	        output_channels
	    ),
	    stbi_image_free
	);
	if (!imgdata ||
	    width <= 0 || height <= 0 ||
	    width > maximum_dimension || height > maximum_dimension)
		return cv::Mat();

	cv::Mat rgba(height, width, CV_8UC4, imgdata.get());
	cv::Mat rgb;
	cv::cvtColor(rgba, rgb, cv::COLOR_RGBA2RGB);
	return rgb;
}

}

cv::Mat load_img(string path)
{
	auto it = cimbar::bitmaps.find(path);
	if (it == cimbar::bitmaps.end())
		return cv::Mat();

	string bytes = base91::decode(it->second);
	vector<unsigned char> data(bytes.data(), bytes.data() + bytes.size());
	return detail::decode_image(data.data(), data.size());
}

RGB getColor(unsigned index, unsigned num_colors, unsigned color_mode)
{
	if ((color_mode & 0xFF) == 0)
	{
		if (num_colors <= 4)
			return getColor4_old(index);
		else
			return getColor8_old(index);
	}

	if (num_colors > 4)
		return getColor8(index);
	else if (color_mode > 0x100)
		return getColor4_enc(index);
	else
		return getColor4(index);

}

RGB getBgColor(unsigned index, unsigned num_colors, unsigned color_mode)
{
	// >0x100 because color_mode=0 will use default bg
	if (color_mode > 0x100 and num_colors <= 4)
		return getBgColor4(index);
	else
		return RGB(0,0,0);
}

cv::Mat getTile(unsigned symbol_bits, unsigned symbol, bool dark, unsigned num_colors, unsigned color, unsigned color_mode)
{
	static cv::Vec3b background({0xFF, 0xFF, 0xFF});

	string imgPath = fmt::format("bitmap/{}/{:02x}.png", symbol_bits, symbol);
	cv::Mat tile = load_img(imgPath);

	uchar r, g, b;
	std::tie(r, g, b) = getColor(color, num_colors, color_mode);
	uchar bgr, bgg, bgb;
	std::tie(bgr, bgg, bgb) = getBgColor(color, num_colors, color_mode);
	cv::MatIterator_<cv::Vec3b> end = tile.end<cv::Vec3b>();
	for (cv::MatIterator_<cv::Vec3b> it = tile.begin<cv::Vec3b>(); it != end; ++it)
	{
		cv::Vec3b& c = *it;
		if (c != background)
			c = {r, g, b};
		else if (dark)
			c = {bgr, bgg, bgb};
	}
	return tile;
}

}
