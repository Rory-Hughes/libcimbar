/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Corners.h"

#include "util/vec_xy.h"
#include <opencv2/opencv.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

class Deskewer
{
public:
	Deskewer(unsigned padding=0, cimbar::vec_xy image_size={}, unsigned anchor_size=0);

	template <typename MAT>
	MAT deskew(const MAT& img, const Corners& corners);

protected:
	cimbar::vec_xy _imageSize;
	unsigned _anchorSize;
	unsigned _padding;
};

namespace deskewer_detail
{
	inline bool perspective_transform_is_valid(const cv::Mat& transform)
	{
		if (transform.empty() || transform.rows != 3 || transform.cols != 3)
			return false;

		for (int row = 0; row < transform.rows; ++row)
			for (int col = 0; col < transform.cols; ++col)
				if (!std::isfinite(transform.at<double>(row, col)))
					return false;

		const double determinant = cv::determinant(transform);
		return std::isfinite(determinant) && std::fabs(determinant) > 1e-12;
	}
}

template <typename MAT>
inline MAT Deskewer::deskew(const MAT& img, const Corners& corners)
{
	if (img.empty() || _imageSize.width() == 0U || _imageSize.height() == 0U ||
	    _anchorSize > _imageSize.width() || _anchorSize > _imageSize.height())
	{
		return MAT();
	}

	const std::uint64_t output_width =
	    static_cast<std::uint64_t>(_imageSize.width()) + (static_cast<std::uint64_t>(_padding) * 2U);
	const std::uint64_t output_height =
	    static_cast<std::uint64_t>(_imageSize.height()) + (static_cast<std::uint64_t>(_padding) * 2U);
	if (output_width > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
	    output_height > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
	{
		return MAT();
	}

	std::vector<cv::Point2f> outputPoints;
	outputPoints.push_back(cv::Point2f(_anchorSize+_padding, _anchorSize+_padding));
	outputPoints.push_back(cv::Point2f(_imageSize.width() - _anchorSize+_padding, _anchorSize+_padding));
	outputPoints.push_back(cv::Point2f(_anchorSize+_padding, _imageSize.height() - _anchorSize+_padding));
	outputPoints.push_back(cv::Point2f(_imageSize.width() - _anchorSize+_padding, _imageSize.height() - _anchorSize+_padding));

	// + 2*padding ?
	cv::Mat transform = cv::getPerspectiveTransform(corners.all(), outputPoints);
	if (!deskewer_detail::perspective_transform_is_valid(transform))
		return MAT();

	MAT output(static_cast<int>(output_height), static_cast<int>(output_width), img.type());
	cv::warpPerspective(img, output, transform, output.size(), cv::INTER_LINEAR);
	return output;
}
