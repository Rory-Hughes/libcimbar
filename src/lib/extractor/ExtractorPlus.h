/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Extractor.h"
#include "cimb_translator/Common.h"

#include <opencv2/opencv.hpp>
#include <string>

class ExtractorPlus : public Extractor
{
public:
	using Extractor::Extractor;
	using Extractor::extract;

	int extract(std::string read_path, cv::Mat& out);
	int extract(std::string read_path, std::string write_path);

protected:
};

inline int ExtractorPlus::extract(std::string read_path, cv::Mat& out)
{
	cv::Mat img = cimbar::load_img_file(read_path);
	if (img.empty())
		return Extractor::FAILURE;
	return Extractor::extract(img, out);
}

inline int ExtractorPlus::extract(std::string read_path, std::string write_path)
{
	cv::Mat decoded = cimbar::load_img_file(read_path);
	if (decoded.empty())
		return Extractor::FAILURE;
	cv::UMat img = decoded.getUMat(cv::ACCESS_FAST); // cv::USAGE_ALLOCATE_SHARED_MEMORY would be nice...;

	int res = Extractor::extract(img, img);

	cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
	cv::imwrite(write_path, img);
	return res;
}
