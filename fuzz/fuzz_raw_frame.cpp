#include "cimbar_js/cimbar_recv_js.h"
#include "extractor/Deskewer.h"
#include "extractor/Geometry.h"
#include "extractor/Scanner.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

constexpr unsigned max_fuzz_dimension = 96U;
constexpr std::size_t max_fuzz_frame_bytes =
    static_cast<std::size_t>(max_fuzz_dimension) * max_fuzz_dimension * 4U;

[[noreturn]] void invariant_failure()
{
	__builtin_trap();
}

int select_format(std::uint8_t selector)
{
	switch (selector & 0x07U)
	{
	case 0U:
		return 0;
	case 1U:
		return CIMBARD_PIXEL_FORMAT_RGB;
	case 2U:
		return CIMBARD_PIXEL_FORMAT_RGBA;
	case 3U:
		return CIMBARD_PIXEL_FORMAT_NV12;
	case 4U:
		return CIMBARD_PIXEL_FORMAT_I420;
	case 5U:
		return 99;
	default:
		return CIMBARD_PIXEL_FORMAT_RGB;
	}
}

unsigned select_dimension(std::uint8_t seed)
{
	switch (seed & 0x0FU)
	{
	case 0U:
		return 0U;
	case 1U:
		return 4097U;
	default:
		return 1U + (seed % max_fuzz_dimension);
	}
}

bool expected_size_for_frame(unsigned width, unsigned height, int format, std::size_t& expected_size)
{
	if (format <= 0)
		format = CIMBARD_PIXEL_FORMAT_RGB;
	if (width == 0U || height == 0U)
		return false;

	const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
	if (pixels > CIMBARD_MAX_FRAME_PIXELS)
		return false;

	std::uint64_t bytes = 0U;
	switch (format)
	{
	case CIMBARD_PIXEL_FORMAT_RGB:
		bytes = pixels * 3U;
		break;
	case CIMBARD_PIXEL_FORMAT_RGBA:
		bytes = pixels * 4U;
		break;
	case CIMBARD_PIXEL_FORMAT_NV12:
	case CIMBARD_PIXEL_FORMAT_I420:
		if ((width & 1U) != 0U || (height & 1U) != 0U)
			return false;
		bytes = pixels + pixels / 2U;
		break;
	default:
		return false;
	}

	if (bytes > max_fuzz_frame_bytes)
		return false;
	expected_size = static_cast<std::size_t>(bytes);
	return true;
}

std::uint8_t payload_byte(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::size_t index,
    std::uint8_t mode
)
{
	switch (mode % 6U)
	{
	case 0U:
		return 0U;
	case 1U:
		return 0xFFU;
	case 2U:
		return static_cast<std::uint8_t>((index * 37U + mode) & 0xFFU);
	case 3U:
		return ((index + mode) & 1U) == 0U ? 0U : 0xFFU;
	default:
		if (payload_size == 0U)
			return static_cast<std::uint8_t>(index & 0xFFU);
		return payload[index % payload_size];
	}
}

std::vector<std::uint8_t> make_frame_bytes(
    std::size_t frame_size,
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::uint8_t mode
)
{
	std::vector<std::uint8_t> frame(frame_size);
	for (std::size_t index = 0U; index < frame.size(); ++index)
		frame[index] = payload_byte(payload, payload_size, index, mode);
	return frame;
}

bool accepted_scan_result(int result, unsigned output_size)
{
	switch (result)
	{
	case CIMBARD_SCAN_INVALID_DIMENSIONS:
	case CIMBARD_SCAN_OUTPUT_BUFFER_TOO_SMALL:
	case CIMBARD_SCAN_EXTRACT_FAILED:
	case CIMBARD_SCAN_NULL_POINTER:
	case CIMBARD_SCAN_UNSUPPORTED_FORMAT:
	case CIMBARD_SCAN_FRAME_TOO_LARGE:
	case CIMBARD_SCAN_INVALID_BUFFER_SIZE:
	case CIMBARD_SCAN_PROCESSING_ERROR:
		return true;
	default:
		return result >= 0 && static_cast<unsigned>(result) <= output_size;
	}
}

double read_double(const std::uint8_t* data, std::size_t size, std::size_t offset)
{
	double value = 0.0;
	if (offset < size)
	{
		const std::size_t copied = std::min(sizeof(value), size - offset);
		std::memcpy(&value, data + offset, copied);
	}
	return value;
}

int bounded_coordinate(std::uint8_t lo, std::uint8_t hi)
{
	const int value = static_cast<int>(static_cast<unsigned>(lo) | (static_cast<unsigned>(hi) << 8U));
	return (value % 4097) - 2048;
}

Corners make_corners(const std::uint8_t* data, std::size_t size)
{
	std::array<int, 8> values{};
	for (std::size_t index = 0U; index < values.size(); ++index)
	{
		const std::size_t offset = 4U + index * 2U;
		const std::uint8_t lo = offset < size ? data[offset] : static_cast<std::uint8_t>(index * 17U);
		const std::uint8_t hi = offset + 1U < size ? data[offset + 1U] : static_cast<std::uint8_t>(index * 31U);
		values[index] = bounded_coordinate(lo, hi);
	}
	return Corners(
	    {values[0], values[1]},
	    {values[2], values[3]},
	    {values[4], values[5]},
	    {values[6], values[7]}
	);
}

void exercise_line_geometry(const std::uint8_t* data, std::size_t size)
{
	const Geometry::floating_point a{read_double(data, size, 0U), read_double(data, size, 8U)};
	const Geometry::floating_point b{read_double(data, size, 16U), read_double(data, size, 24U)};
	const Geometry::floating_point c{read_double(data, size, 32U), read_double(data, size, 40U)};
	const Geometry::floating_point d{read_double(data, size, 48U), read_double(data, size, 56U)};

	const Geometry::floating_point intersection = Geometry::line_intersection({a, b}, {c, d});
	if (intersection && (!std::isfinite(intersection.x()) || !std::isfinite(intersection.y())))
		invariant_failure();
}

void exercise_geometry_stack(
    const std::uint8_t* data,
    std::size_t size,
    const std::vector<std::uint8_t>& frame,
    unsigned width,
    unsigned height
)
{
	exercise_line_geometry(data, size);

	if (width == 0U || height == 0U || frame.empty() ||
	    width > max_fuzz_dimension || height > max_fuzz_dimension)
	{
		return;
	}

	cv::Mat img(static_cast<int>(height), static_cast<int>(width), CV_8UC1);
	for (int row = 0; row < img.rows; ++row)
		for (int col = 0; col < img.cols; ++col)
			img.at<uchar>(row, col) = frame[static_cast<std::size_t>(row * img.cols + col) % frame.size()];

	Scanner scanner(img, (data[0] & 0x01U) != 0U, (data[0] & 0x02U) != 0U, static_cast<int>(data[0] & 0x0FU) - 4);
	const std::vector<Anchor> anchors = scanner.scan();
	if (anchors.size() > 4U)
		invariant_failure();

	Midpoints midpoints;
	const Corners corners = make_corners(data, size);
	const std::vector<point<int>> edges = scanner.scan_edges(corners, midpoints);
	if (!edges.empty() && edges.size() != 4U)
		invariant_failure();

	Deskewer deskewer(0U, {std::max(1U, width), std::max(1U, height)}, std::min(4U, std::max(1U, std::min(width, height))));
	const cv::Mat deskewed = deskewer.deskew(img, corners);
	if (!deskewed.empty() && (deskewed.cols != static_cast<int>(width) || deskewed.rows != static_cast<int>(height)))
		invariant_failure();

	Deskewer invalid(0U, {1U, 1U}, 8U);
	if (!invalid.deskew(img, corners).empty())
		invariant_failure();
}

void exercise_known_rejections()
{
	std::array<std::uint8_t, 12> rgb{};
	std::vector<unsigned char> output(static_cast<std::size_t>(cimbard_get_bufsize()));
	if (cimbard_scan_extract_decode_checked(nullptr, rgb.size(), 2U, 2U, CIMBARD_PIXEL_FORMAT_RGB, output.data(), static_cast<unsigned>(output.size())) != CIMBARD_SCAN_NULL_POINTER)
		invariant_failure();
	if (cimbard_scan_extract_decode_checked(rgb.data(), rgb.size(), 2U, 2U, CIMBARD_PIXEL_FORMAT_RGB, nullptr, static_cast<unsigned>(output.size())) != CIMBARD_SCAN_NULL_POINTER)
		invariant_failure();
	if (cimbard_scan_extract_decode_checked(rgb.data(), rgb.size(), 0U, 2U, CIMBARD_PIXEL_FORMAT_RGB, output.data(), static_cast<unsigned>(output.size())) != CIMBARD_SCAN_INVALID_DIMENSIONS)
		invariant_failure();
	if (cimbard_scan_extract_decode_checked(rgb.data(), rgb.size(), 2U, 2U, 99, output.data(), static_cast<unsigned>(output.size())) != CIMBARD_SCAN_UNSUPPORTED_FORMAT)
		invariant_failure();
	if (cimbard_scan_extract_decode_checked(rgb.data(), rgb.size() - 1U, 2U, 2U, CIMBARD_PIXEL_FORMAT_RGB, output.data(), static_cast<unsigned>(output.size())) != CIMBARD_SCAN_INVALID_BUFFER_SIZE)
		invariant_failure();
	if (cimbard_scan_extract_decode_checked(rgb.data(), rgb.size(), 3U, 2U, CIMBARD_PIXEL_FORMAT_NV12, output.data(), static_cast<unsigned>(output.size())) != CIMBARD_SCAN_INVALID_BUFFER_SIZE)
		invariant_failure();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (data == nullptr)
		return 0;

	exercise_known_rejections();

	if (size < 4U)
		return 0;

	const int format = select_format(data[0] >> 4U);
	const unsigned width = select_dimension(data[1]);
	const unsigned height = select_dimension(data[2]);

	std::size_t expected_size = 0U;
	const bool has_expected_size = expected_size_for_frame(width, height, format, expected_size);
	std::size_t frame_size = std::min(size - 4U, max_fuzz_frame_bytes);
	if (has_expected_size)
	{
		switch (data[3] & 0x03U)
		{
		case 0U:
			frame_size = expected_size;
			break;
		case 1U:
			frame_size = expected_size == 0U ? 0U : expected_size - 1U;
			break;
		case 2U:
			frame_size = std::min(expected_size + 1U, max_fuzz_frame_bytes);
			break;
		default:
			frame_size = std::min(frame_size, expected_size);
			break;
		}
	}

	const std::uint8_t* payload = data + 4U;
	const std::size_t payload_size = size - 4U;
	std::vector<std::uint8_t> frame = make_frame_bytes(frame_size, payload, payload_size, data[3] >> 2U);

	std::vector<unsigned char> output(static_cast<std::size_t>(cimbard_get_bufsize()));
	const bool null_input = (data[0] & 0x80U) != 0U;
	const unsigned output_size = (data[3] & 0x80U) != 0U ? 0U : static_cast<unsigned>(output.size());

	cimbard_configure_decode(68);
	const int result = cimbard_scan_extract_decode_checked(
	    null_input ? nullptr : frame.data(),
	    frame.size(),
	    width,
	    height,
	    format,
	    output.data(),
	    output_size
	);
	if (!accepted_scan_result(result, output_size))
		invariant_failure();
	cimbard_reset_decode();

	exercise_geometry_stack(data, size, frame, width, height);
	return 0;
}
