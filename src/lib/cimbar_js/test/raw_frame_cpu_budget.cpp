/* This code is subject to the terms of the Mozilla Public License, v.2.0. */
#include "cimbar_js/cimbar_recv_js.h"

#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace
{
	constexpr unsigned default_dimension = 4096U;
	constexpr unsigned default_iterations = 3U;
	constexpr double default_budget_milliseconds = 250.0;

	enum class pattern
	{
		solid_black,
		solid_white,
		checkerboard,
		anchor_grid,
		noise,
	};

	struct options
	{
		unsigned dimension = default_dimension;
		unsigned iterations = default_iterations;
		double budget_milliseconds = default_budget_milliseconds;
	};

	const char* pattern_name(pattern value)
	{
		switch (value)
		{
		case pattern::solid_black:
			return "solid-black";
		case pattern::solid_white:
			return "solid-white";
		case pattern::checkerboard:
			return "checkerboard";
		case pattern::anchor_grid:
			return "anchor-grid";
		case pattern::noise:
			return "noise";
		}
		return "unknown";
	}

	bool parse_unsigned(const char* value, unsigned& parsed)
	{
		char* end = nullptr;
		const unsigned long candidate = std::strtoul(value, &end, 10);
		if (value[0] == '\0' || *end != '\0' || candidate > std::numeric_limits<unsigned>::max())
			return false;
		parsed = static_cast<unsigned>(candidate);
		return true;
	}

	bool parse_double(const char* value, double& parsed)
	{
		char* end = nullptr;
		const double candidate = std::strtod(value, &end);
		if (value[0] == '\0' || *end != '\0' || !std::isfinite(candidate) || candidate <= 0.0)
			return false;
		parsed = candidate;
		return true;
	}

	bool parse_options(int argc, char** argv, options& values)
	{
		for (int index = 1; index < argc; ++index)
		{
			const std::string_view argument(argv[index]);
			if (argument == "--help")
			{
				std::cout << "Usage: cimbar_raw_frame_cpu_budget [--dimension N] [--iterations N] [--max-ms N]\n";
				std::exit(0);
			}
			if (index + 1 >= argc)
				return false;

			if (argument == "--dimension")
			{
				if (!parse_unsigned(argv[++index], values.dimension))
					return false;
			}
			else if (argument == "--iterations")
			{
				if (!parse_unsigned(argv[++index], values.iterations))
					return false;
			}
			else if (argument == "--max-ms")
			{
				if (!parse_double(argv[++index], values.budget_milliseconds))
					return false;
			}
			else
			{
				return false;
			}
		}

		const std::uint64_t pixels = static_cast<std::uint64_t>(values.dimension) * values.dimension;
		return values.dimension > 0U && pixels <= CIMBARD_MAX_FRAME_PIXELS &&
		       values.iterations > 0U && values.iterations <= 10U;
	}

	bool grid_is_light(unsigned coordinate)
	{
		const unsigned phase = coordinate % 40U;
		return (phase >= 4U && phase < 8U) ||
		       (phase >= 12U && phase < 28U) ||
		       (phase >= 32U && phase < 36U);
	}

	std::vector<unsigned char> make_frame(pattern value, unsigned dimension)
	{
		const std::size_t pixels = static_cast<std::size_t>(dimension) * dimension;
		std::vector<unsigned char> frame(pixels * 3U);
		std::uint32_t random_state = 0x4d595df4U;

		for (unsigned y = 0U; y < dimension; ++y)
			for (unsigned x = 0U; x < dimension; ++x)
			{
				unsigned char intensity = 0U;
				switch (value)
				{
				case pattern::solid_black:
					break;
				case pattern::solid_white:
					intensity = 255U;
					break;
				case pattern::checkerboard:
					intensity = (((x / 4U) + (y / 4U)) & 1U) == 0U ? 0U : 255U;
					break;
				case pattern::anchor_grid:
					intensity = grid_is_light(x) && grid_is_light(y) ? 255U : 0U;
					break;
				case pattern::noise:
					random_state ^= random_state << 13U;
					random_state ^= random_state >> 17U;
					random_state ^= random_state << 5U;
					intensity = static_cast<unsigned char>(random_state >> 24U);
					break;
				}

				const std::size_t offset = (static_cast<std::size_t>(y) * dimension + x) * 3U;
				frame[offset] = intensity;
				frame[offset + 1U] = intensity;
				frame[offset + 2U] = intensity;
			}

		return frame;
	}

	double scan_once(const std::vector<unsigned char>& frame, unsigned dimension, std::vector<unsigned char>& output)
	{
		const auto started = std::chrono::steady_clock::now();
		const int result = cimbard_scan_extract_decode_checked(
			frame.data(),
			frame.size(),
			dimension,
			dimension,
			CIMBARD_PIXEL_FORMAT_RGB,
			output.data(),
			static_cast<unsigned>(output.size())
		);
		const auto finished = std::chrono::steady_clock::now();
		cimbard_reset_decode();

		if (result != CIMBARD_SCAN_EXTRACT_FAILED)
			return -1.0;
		return std::chrono::duration<double, std::milli>(finished - started).count();
	}
}

int main(int argc, char** argv)
{
	options values;
	if (!parse_options(argc, argv, values))
	{
		std::cerr << "Invalid arguments. Use --help for usage.\n";
		return 1;
	}

	cv::ocl::setUseOpenCL(false);
	cv::setNumThreads(1);
	cimbard_configure_decode(68);
	std::vector<unsigned char> output(static_cast<std::size_t>(cimbard_get_bufsize()));
	const std::array<pattern, 5> patterns = {
		pattern::solid_black,
		pattern::solid_white,
		pattern::checkerboard,
		pattern::anchor_grid,
		pattern::noise,
	};

	std::cout << std::fixed << std::setprecision(3);
	std::cout << "raw-frame CPU budget\n";
	std::cout << "opencv=" << CV_VERSION << " threads=" << cv::getNumThreads() << " opencl=disabled\n";
	std::cout << "frame=" << values.dimension << "x" << values.dimension << " rgb-bytes="
	          << static_cast<std::size_t>(values.dimension) * values.dimension * 3U
	          << " iterations=" << values.iterations << " budget-ms=" << values.budget_milliseconds << "\n";

	double overall_maximum = 0.0;
	for (const pattern value : patterns)
	{
		const std::vector<unsigned char> frame = make_frame(value, values.dimension);
		if (scan_once(frame, values.dimension, output) < 0.0)
		{
			std::cerr << "Unexpected scan result for " << pattern_name(value) << " warm-up.\n";
			return 1;
		}

		double maximum = 0.0;
		for (unsigned iteration = 0U; iteration < values.iterations; ++iteration)
		{
			const double elapsed = scan_once(frame, values.dimension, output);
			if (elapsed < 0.0)
			{
				std::cerr << "Unexpected scan result for " << pattern_name(value) << ".\n";
				return 1;
			}
			maximum = std::max(maximum, elapsed);
		}

		overall_maximum = std::max(overall_maximum, maximum);
		std::cout << "pattern=" << pattern_name(value) << " max-ms=" << maximum << "\n";
	}

	std::cout << "overall-max-ms=" << overall_maximum << "\n";
	if (overall_maximum > values.budget_milliseconds)
	{
		std::cout << "verdict=FAIL\n";
		return 2;
	}

	std::cout << "verdict=PASS\n";
	return 0;
}
