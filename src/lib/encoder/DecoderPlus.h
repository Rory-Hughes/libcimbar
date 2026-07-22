/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Decoder.h"
#include "util/File.h"

#include <opencv2/opencv.hpp>
#include <limits>
#include <string>

class DecoderPlus : public Decoder
{
public:
	using Decoder::Decoder;
	using Decoder::decode;

	unsigned decode(std::string filename, std::string output);

	bool load_ccm(std::string filename);
	bool save_ccm(std::string filename);
};

inline unsigned DecoderPlus::decode(std::string filename, std::string output)
{
	cv::Mat img = cv::imread(filename);
	cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

	std::ofstream f(output, std::ios::binary);
	return Decoder::decode(img, f, false);
}

inline bool DecoderPlus::load_ccm(std::string filename)
{
	File f(filename);
	std::string data = f.read_all();
	if (data.size() < 3*3*4)
		return false;

	cv::Mat temp(3, 3, CV_32F, data.data());

	_decoder.update_color_correction(temp);
	return true;
}

inline bool DecoderPlus::save_ccm(std::string filename)
{
	if (not _decoder.get_ccm().active())
		return false;

	cv::Mat temp(_decoder.get_ccm().mat());
	if (temp.empty() || !temp.isContinuous())
		return false;

	const std::size_t element_size = temp.elemSize();
	if (element_size == 0U ||
	    temp.total() > std::numeric_limits<unsigned>::max() / element_size)
		return false;
	const unsigned byte_count = static_cast<unsigned>(temp.total() * element_size);

	File f(filename, true);
	return f.write(reinterpret_cast<const char*>(temp.data), byte_count) == byte_count;
}
