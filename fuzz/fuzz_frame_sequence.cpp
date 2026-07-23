#include "fountain/FountainDecoder.h"
#include "fountain/FountainEncoder.h"
#include "fountain/fountain_decoder_session.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

constexpr unsigned chunk_size = 128U;
constexpr unsigned packet_size = chunk_size - FountainMetadata::md_size;
constexpr unsigned max_object_size = 1536U;
constexpr unsigned max_sequence_packets = 48U;
constexpr unsigned max_block_id = 96U;
constexpr unsigned max_packets_per_frame = 4U;
constexpr unsigned max_frames_per_transfer = 64U;
constexpr unsigned max_no_progress_frames = 16U;
constexpr unsigned max_operations = 96U;
constexpr auto max_transfer_duration = std::chrono::milliseconds(10);

using packet = std::array<std::uint8_t, chunk_size>;

[[noreturn]] void invariant_failure()
{
	__builtin_trap();
}

std::uint16_t read_u16(const std::uint8_t* data, std::size_t size)
{
	std::uint16_t value = 0U;
	const std::size_t count = std::min(size, sizeof(value));
	for (std::size_t index = 0U; index < count; ++index)
		value |= static_cast<std::uint16_t>(data[index]) << (index * 8U);
	return value;
}

std::uint8_t read_byte(
    const std::uint8_t* data,
    std::size_t size,
    std::size_t& offset,
    std::uint8_t fallback)
{
	if (offset >= size)
		return fallback;
	return data[offset++];
}

std::size_t max_codec_memory()
{
	static const std::size_t value = [] {
		const auto required = FountainDecoder::decoder_memory_required(max_object_size, packet_size);
		return required.value_or(0U);
	}();
	if (value == 0U)
		invariant_failure();
	return value;
}

FountainTransferPolicy sequence_policy()
{
	FountainTransferPolicy policy;
	policy.object_class = FountainObjectClass::message;
	policy.decoder_limits.maximum_object_size = max_object_size;
	policy.decoder_limits.maximum_active_object_bytes = max_object_size;
	policy.decoder_limits.maximum_codec_memory_bytes = max_codec_memory();
	policy.decoder_limits.maximum_active_codec_memory_bytes = max_codec_memory();
	policy.decoder_limits.maximum_active_streams = 1U;
	policy.decoder_limits.maximum_completed_transfers = 0U;
	policy.decoder_limits.maximum_unique_blocks = max_sequence_packets;
	policy.decoder_limits.maximum_block_id = max_block_id;
	policy.decoder_limits.maximum_packets_per_frame = max_packets_per_frame;
	policy.decoder_limits.maximum_frames_per_transfer = max_frames_per_transfer;
	policy.decoder_limits.maximum_no_progress_frames = max_no_progress_frames;
	policy.decoder_limits.maximum_cancelled_transfers = 2U;
	policy.decoder_limits.maximum_transfer_duration = max_transfer_duration;
	return policy;
}

std::vector<std::uint8_t> make_source(
    const std::uint8_t* data,
    std::size_t size,
    std::uint8_t encode_id,
    unsigned object_size)
{
	std::vector<std::uint8_t> source(object_size);
	for (std::size_t index = 0U; index < source.size(); ++index)
	{
		const std::uint8_t seed = size > 0U
		    ? data[index % size]
		    : static_cast<std::uint8_t>(encode_id);
		const unsigned mixed =
		    static_cast<unsigned>(seed) ^
		    ((static_cast<unsigned>(index & 0xFFU) * 31U) & 0xFFU) ^
		    static_cast<unsigned>(object_size & 0xFFU);
		source[index] = static_cast<std::uint8_t>(mixed & 0xFFU);
	}
	return source;
}

std::vector<packet> encode_packets(
    const std::vector<std::uint8_t>& source,
    std::uint8_t encode_id)
{
	FountainEncoder encoder(source.data(), source.size(), packet_size);
	if (!encoder.good())
		return {};

	const unsigned blocks_required = static_cast<unsigned>(
	    (source.size() + packet_size - 1U) / packet_size
	);
	const unsigned wanted_packets = std::min(
	    max_sequence_packets,
	    blocks_required + 24U
	);

	std::vector<packet> packets;
	packets.reserve(wanted_packets);
	for (unsigned block_id = 0U;
	     block_id <= max_block_id && packets.size() < wanted_packets;
	     ++block_id)
	{
		packet next{};
		const std::size_t written = encoder.encode(
		    block_id,
		    next.data() + FountainMetadata::md_size,
		    packet_size
		);
		if (written != packet_size)
			continue;
		FountainMetadata::to_uint8_arr(
		    encode_id,
		    static_cast<unsigned>(source.size()),
		    static_cast<std::uint16_t>(block_id),
		    next.data()
		);
		packets.push_back(next);
	}
	return packets;
}

std::vector<std::uint8_t> single_packet_frame(const packet& source)
{
	return std::vector<std::uint8_t>(source.begin(), source.end());
}

std::vector<std::uint8_t> batch_from_packets(
    const std::vector<packet>& packets,
    std::size_t start,
    unsigned count)
{
	std::vector<std::uint8_t> frame;
	if (packets.empty() || count == 0U)
		return frame;

	const unsigned bounded_count = std::min(count, max_packets_per_frame);
	frame.reserve(static_cast<std::size_t>(bounded_count) * chunk_size);
	for (unsigned batch_index = 0U; batch_index < bounded_count; ++batch_index)
	{
		const packet& next = packets[(start + batch_index) % packets.size()];
		frame.insert(frame.end(), next.begin(), next.end());
	}
	return frame;
}

std::vector<std::uint8_t> mutated_frame(
    const std::vector<packet>& packets,
    std::size_t selected,
    std::uint8_t control,
    std::uint8_t value)
{
	if (packets.empty())
		return {};

	const packet& original = packets[selected % packets.size()];
	std::vector<std::uint8_t> frame(original.begin(), original.end());
	const unsigned mode = control % 8U;
	const FountainMetadata metadata(
	    reinterpret_cast<const char*>(frame.data()),
	    static_cast<unsigned>(frame.size())
	);
	switch (mode)
	{
	case 0U:
		FountainMetadata::to_uint8_arr(
		    metadata.encode_id(),
		    metadata.file_size(),
		    static_cast<std::uint16_t>(max_block_id + 1U),
		    frame.data()
		);
		break;
	case 1U:
		FountainMetadata::to_uint8_arr(
		    metadata.encode_id(),
		    0U,
		    metadata.block_id(),
		    frame.data()
		);
		break;
	case 2U:
		FountainMetadata::to_uint8_arr(
		    metadata.encode_id(),
		    max_object_size + 1U,
		    metadata.block_id(),
		    frame.data()
		);
		break;
	case 3U:
		frame.resize(frame.size() - 1U - (value % 7U));
		break;
	case 4U:
		frame.push_back(value);
		break;
	case 5U:
	{
		packet conflicting = original;
		conflicting[0] = static_cast<std::uint8_t>((conflicting[0] + 1U) & 0x7FU);
		frame.insert(frame.end(), conflicting.begin(), conflicting.end());
		break;
	}
	case 6U:
		frame.clear();
		break;
	default:
		frame[1] = 0U;
		frame[2] = 0U;
		frame[3] = 0U;
		frame.resize(frame.size() - 1U);
		break;
	}
	return frame;
}

void assert_exact_object(
    fountain_decoder_session& session,
    const std::vector<std::uint8_t>& expected,
    unsigned& completion_count)
{
	if (++completion_count > 1U)
		invariant_failure();

	std::optional<std::vector<std::uint8_t>> object = session.take_completed_object();
	if (!object)
		invariant_failure();
	if (object->size() != expected.size() ||
	    !std::equal(object->begin(), object->end(), expected.begin()))
	{
		invariant_failure();
	}
	if (session.has_completed_object() || session.take_completed_object())
		invariant_failure();
}

void submit_frame(
    fountain_decoder_session& session,
    const std::vector<std::uint8_t>& frame,
    const std::vector<std::uint8_t>& expected,
    unsigned& completion_count)
{
	const char* frame_data = frame.empty()
	    ? nullptr
	    : reinterpret_cast<const char*>(frame.data());
	const std::int64_t result = session.submit_frame(
	    frame_data,
	    static_cast<unsigned>(frame.size())
	);
	if (result == 1)
	{
		if (!session.has_completed_object())
			invariant_failure();
		assert_exact_object(session, expected, completion_count);
		return;
	}
	if (result > 1 || session.has_completed_object())
		invariant_failure();
}

void reset_session(
    fountain_decoder_session& session,
    unsigned& completion_count,
    std::size_t& cursor)
{
	session.reset();
	if (!session.good() ||
	    session.status() != FountainSessionStatus::receiving ||
	    session.has_completed_object() ||
	    session.take_completed_object())
	{
		invariant_failure();
	}
	completion_count = 0U;
	cursor = 0U;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (data == nullptr || size < 4U)
		return 0;

	std::size_t offset = 0U;
	const std::uint8_t encode_id = data[offset++] & 0x7FU;
	const std::uint16_t object_seed = read_u16(data + offset, size - offset);
	offset += 2U;
	const unsigned object_size = packet_size + 1U +
	    static_cast<unsigned>(object_seed % (max_object_size - packet_size));
	const std::uint8_t source_seed_control = data[offset++];
	const std::size_t requested_source_seed =
	    1U + static_cast<std::size_t>(source_seed_control % 32U);
	const std::size_t source_seed_size = std::min(requested_source_seed, size - offset);
	const std::vector<std::uint8_t> source_seed(data + offset, data + offset + source_seed_size);
	offset += source_seed_size;

	const std::vector<std::uint8_t> source =
	    make_source(source_seed.data(), source_seed.size(), encode_id, object_size);
	const std::vector<packet> packets = encode_packets(source, encode_id);
	if (packets.empty())
		return 0;

	fountain_decoder_sink::time_point now{};
	fountain_decoder_session session(
	    chunk_size,
	    sequence_policy(),
	    [&now] { return now; }
	);
	if (!session.good())
		invariant_failure();

	std::vector<std::uint8_t> last_frame;
	std::size_t cursor = 0U;
	unsigned completion_count = 0U;
	unsigned operations = 0U;

	while (offset < size && operations++ < max_operations)
	{
		const std::uint8_t operation = data[offset++];
		switch (operation % 10U)
		{
		case 0U:
			last_frame = single_packet_frame(packets[cursor % packets.size()]);
			++cursor;
			submit_frame(session, last_frame, source, completion_count);
			break;
		case 1U:
			++cursor;
			break;
		case 2U:
			if (!last_frame.empty())
				submit_frame(session, last_frame, source, completion_count);
			break;
		case 3U:
		{
			const std::uint8_t selector = read_byte(data, size, offset, operation);
			last_frame = single_packet_frame(packets[selector % packets.size()]);
			submit_frame(session, last_frame, source, completion_count);
			break;
		}
		case 4U:
		{
			const std::uint8_t selector = read_byte(data, size, offset, operation);
			const std::uint8_t control = read_byte(data, size, offset, selector);
			const std::uint8_t value = read_byte(data, size, offset, control);
			last_frame = mutated_frame(packets, selector, control, value);
			submit_frame(session, last_frame, source, completion_count);
			break;
		}
		case 5U:
			now += max_transfer_duration + std::chrono::milliseconds(1);
			break;
		case 6U:
			reset_session(session, completion_count, cursor);
			last_frame.clear();
			break;
		case 7U:
		{
			const std::uint8_t count_selector = read_byte(data, size, offset, operation);
			const unsigned count = 1U + static_cast<unsigned>(count_selector % max_packets_per_frame);
			last_frame = batch_from_packets(packets, cursor, count);
			cursor += count;
			submit_frame(session, last_frame, source, completion_count);
			break;
		}
		case 8U:
			while (session.status() == FountainSessionStatus::receiving &&
			       cursor < packets.size())
			{
				last_frame = batch_from_packets(packets, cursor, max_packets_per_frame);
				cursor += max_packets_per_frame;
				submit_frame(session, last_frame, source, completion_count);
			}
			break;
		default:
			if (session.has_completed_object() || session.take_completed_object())
				invariant_failure();
			break;
		}
	}

	if (session.has_completed_object() || session.take_completed_object())
		invariant_failure();
	return 0;
}
