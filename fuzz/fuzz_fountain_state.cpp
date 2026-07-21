#include "fountain/FountainEncoder.h"
#include "fountain/FountainMetadata.h"
#include "fountain/fountain_decoder_sink.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr unsigned chunk_size = 128U;
constexpr unsigned packet_size = chunk_size - FountainMetadata::md_size;
constexpr unsigned max_object_size = 4096U;
constexpr unsigned max_unique_blocks = 64U;
constexpr unsigned max_active_streams = 1U;
constexpr unsigned max_completed_transfers = 2U;
constexpr unsigned max_cancelled_transfers = 2U;
constexpr unsigned max_packets_per_frame = 4U;
constexpr unsigned max_frames_per_transfer = 32U;
constexpr unsigned max_no_progress_frames = 4U;
constexpr auto max_transfer_duration = std::chrono::milliseconds(10);

using frame = std::array<std::uint8_t, chunk_size>;

[[noreturn]] void invariant_failure()
{
	__builtin_trap();
}

FountainDecoderLimits fuzz_limits()
{
	FountainDecoderLimits limits;
	limits.maximum_object_size = max_object_size;
	limits.maximum_active_object_bytes = max_object_size;
	limits.maximum_unique_blocks = max_unique_blocks;
	limits.maximum_active_streams = max_active_streams;
	limits.maximum_completed_transfers = max_completed_transfers;
	limits.maximum_cancelled_transfers = max_cancelled_transfers;
	limits.maximum_packets_per_frame = max_packets_per_frame;
	limits.maximum_frames_per_transfer = max_frames_per_transfer;
	limits.maximum_no_progress_frames = max_no_progress_frames;
	limits.maximum_transfer_duration = max_transfer_duration;
	return limits;
}

std::uint32_t read_identifier(const std::uint8_t* data, std::size_t size)
{
	std::uint32_t identifier = 0U;
	const std::size_t count = std::min(size, sizeof(identifier));
	std::memcpy(&identifier, data, count);
	return identifier;
}

std::uint16_t read_u16(const std::uint8_t* data, std::size_t size)
{
	std::uint16_t value = 0U;
	const std::size_t count = std::min(size, sizeof(value));
	std::memcpy(&value, data, count);
	return value;
}

frame make_frame(
	std::uint8_t encode_id,
	unsigned object_size,
	std::uint16_t block_id,
	const std::uint8_t* payload,
	std::size_t payload_size
)
{
	frame result{};
	FountainMetadata::to_uint8_arr(encode_id, object_size, block_id, result.data());
	const std::size_t copied = std::min(payload_size, static_cast<std::size_t>(packet_size));
	if (copied > 0U)
		std::copy(payload, payload + copied, result.begin() + FountainMetadata::md_size);
	return result;
}

std::uint8_t filename_encode_id(const std::string& filename)
{
	unsigned value = 0U;
	for (char character : filename)
	{
		if (character == '.')
			break;
		if (character < '0' || character > '9')
			invariant_failure();
		value = value * 10U + static_cast<unsigned>(character - '0');
	}
	if (value >= fountain_decoder_sink::encode_id_count)
		invariant_failure();
	return static_cast<std::uint8_t>(value);
}

void assert_bounds(const fountain_decoder_sink& sink)
{
	if (sink.num_streams() > max_active_streams ||
	    sink.num_done() > max_completed_transfers ||
	    sink.num_cancelled() > max_cancelled_transfers ||
	    sink.active_object_bytes() > max_object_size)
	{
		invariant_failure();
	}
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (data == nullptr)
		return 0;

	std::array<unsigned, fountain_decoder_sink::encode_id_count> completion_counts{};
	fountain_decoder_sink::time_point now{};
	auto on_store = [&completion_counts](
	    const std::string& filename,
	    const std::vector<std::uint8_t>&
	) {
		const std::uint8_t encode_id = filename_encode_id(filename);
		if (++completion_counts[encode_id] > 1U)
			invariant_failure();
		return filename;
	};
	fountain_decoder_sink sink(
	    chunk_size,
	    on_store,
	    fuzz_limits(),
	    [&now] { return now; }
	);
	std::vector<unsigned char> recovery(max_object_size);
	frame last_frame{};
	bool have_last_frame = false;
	std::size_t offset = 0U;

	while (offset < size)
	{
		const std::uint8_t operation = data[offset++];
		const std::size_t remaining = size - offset;

		switch (operation % 9U)
		{
		case 0U:
		{
			if (remaining == 0U)
				break;
			const std::size_t requested = 1U + data[offset++];
			const std::size_t frame_size = std::min(requested, size - offset);
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
		case 1U:
		{
			const std::uint8_t encode_id = remaining > 0U ? data[offset++] & 0x7FU : operation & 0x7FU;
			const std::size_t after_encode_id = size - offset;
			const std::uint16_t object_seed = read_u16(data + offset, after_encode_id);
			offset += std::min(after_encode_id, sizeof(object_seed));
			const unsigned object_size = 1U + object_seed % max_object_size;
			const std::size_t after_size = size - offset;
			const std::uint16_t block_id = read_u16(data + offset, after_size);
			offset += std::min(after_size, sizeof(block_id));
			last_frame = make_frame(encode_id, object_size, block_id, data + offset, size - offset);
			have_last_frame = true;
			sink.decode_frame(
			    reinterpret_cast<const char*>(last_frame.data()),
			    static_cast<unsigned>(last_frame.size())
			);
			const std::size_t consumed = std::min(size - offset, static_cast<std::size_t>(packet_size));
			offset += consumed;
			break;
		}
		case 2U:
			if (have_last_frame)
			{
				sink.decode_frame(
				    reinterpret_cast<const char*>(last_frame.data()),
				    static_cast<unsigned>(last_frame.size())
				);
			}
			break;
		case 3U:
			if (have_last_frame)
			{
				FountainMetadata previous(
				    reinterpret_cast<const char*>(last_frame.data()),
				    static_cast<unsigned>(last_frame.size())
				);
				const std::uint8_t conflicting_encode_id =
				    static_cast<std::uint8_t>((previous.encode_id() + 8U) & 0x7FU);
				frame conflicting = make_frame(
				    conflicting_encode_id,
				    previous.file_size(),
				    previous.block_id(),
				    last_frame.data() + FountainMetadata::md_size,
				    packet_size
				);
				sink.decode_frame(
				    reinterpret_cast<const char*>(conflicting.data()),
				    static_cast<unsigned>(conflicting.size())
				);
			}
			break;
		case 4U:
		{
			const std::uint32_t identifier = read_identifier(data + offset, remaining);
			sink.recover(identifier, recovery.data(), static_cast<unsigned>(recovery.size()));
			offset += std::min(remaining, sizeof(identifier));
			break;
		}
		case 5U:
			if (have_last_frame)
			{
				for (unsigned attempt = 0U; attempt < max_no_progress_frames; ++attempt)
				{
					sink.decode_frame(
					    reinterpret_cast<const char*>(last_frame.data()),
					    static_cast<unsigned>(last_frame.size())
					);
				}
			}
			break;
		case 6U:
			now += max_transfer_duration + std::chrono::milliseconds(1);
			sink.expire_transfers();
			break;
		case 7U:
			sink.reset();
			completion_counts.fill(0U);
			break;
		case 8U:
		{
			const std::uint8_t encode_id = remaining > 0U ? data[offset++] & 0x7FU : operation & 0x7FU;
			const unsigned extra = size > offset ? data[offset++] : operation;
			const std::size_t object_size = packet_size + 1U + extra;
			std::vector<std::uint8_t> payload(object_size);
			const std::size_t source_size = std::min(size - offset, static_cast<std::size_t>(32U));
			for (std::size_t index = 0U; index < payload.size(); ++index)
			{
				payload[index] = source_size > 0U
				    ? data[offset + index % source_size]
				    : static_cast<std::uint8_t>(operation + index);
			}
			offset += source_size;

			FountainEncoder encoder(payload.data(), payload.size(), packet_size);
			if (!encoder.good())
				break;
			const unsigned blocks_required = static_cast<unsigned>(
			    (payload.size() + packet_size - 1U) / packet_size
			);
			for (unsigned block_id = 0U; block_id < blocks_required + 16U; ++block_id)
			{
				last_frame.fill(0U);
				FountainMetadata::to_uint8_arr(
				    encode_id,
				    static_cast<unsigned>(payload.size()),
				    static_cast<std::uint16_t>(block_id),
				    last_frame.data()
				);
				encoder.encode(
				    block_id,
				    last_frame.data() + FountainMetadata::md_size,
				    packet_size
				);
				have_last_frame = true;
				sink.decode_frame(
				    reinterpret_cast<const char*>(last_frame.data()),
				    static_cast<unsigned>(last_frame.size())
				);
				FountainMetadata metadata(
				    reinterpret_cast<const char*>(last_frame.data()),
				    static_cast<unsigned>(last_frame.size())
				);
				if (sink.is_done(metadata.id()) || sink.is_cancelled(metadata.id()))
					break;
			}
			break;
		}
		}

		assert_bounds(sink);
	}

	return 0;
}
