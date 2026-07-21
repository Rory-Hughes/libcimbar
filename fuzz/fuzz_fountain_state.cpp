#include "fountain/FountainMetadata.h"
#include "fountain/fountain_decoder_sink.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr unsigned max_object_size = 64U * 1024U;
constexpr unsigned max_unique_blocks = 128U;
constexpr unsigned max_active_streams = 1U;
constexpr unsigned max_completed_transfers = 2U;

FountainDecoderLimits fuzz_limits()
{
	FountainDecoderLimits limits;
	limits.maximum_object_size = max_object_size;
	limits.maximum_unique_blocks = max_unique_blocks;
	limits.maximum_active_streams = max_active_streams;
	limits.maximum_completed_transfers = max_completed_transfers;
	return limits;
}

std::uint32_t read_identifier(const std::uint8_t* data, std::size_t size)
{
	std::uint32_t identifier = 0U;
	const std::size_t count = std::min(size, sizeof(identifier));
	std::memcpy(&identifier, data, count);
	return identifier;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (data == nullptr)
		return 0;

	fountain_decoder_sink sink(128U, nullptr, fuzz_limits());
	std::size_t offset = 0U;

	while (offset < size)
	{
		const std::uint8_t operation = data[offset++];
		const std::size_t remaining = size - offset;

		switch (operation % 4U)
		{
		case 0U:
		case 1U:
		{
			const std::size_t frame_size = std::min(remaining, static_cast<std::size_t>(512U));
			if (frame_size > 0U)
			{
				sink.decode_frame(
				    reinterpret_cast<const char*>(data + offset),
				    static_cast<unsigned>(frame_size)
				);
				offset += frame_size;
			}
			break;
		}
		case 2U:
		{
			const std::uint32_t identifier = read_identifier(data + offset, remaining);
			std::array<unsigned char, max_object_size> output{};
			sink.recover(identifier, output.data(), static_cast<unsigned>(output.size()));
			offset += std::min(remaining, sizeof(identifier));
			break;
		}
		case 3U:
			sink.reset();
			break;
		}

		if (sink.num_streams() > max_active_streams ||
		    sink.num_done() > max_completed_transfers)
		{
			__builtin_trap();
		}
	}

	return 0;
}
