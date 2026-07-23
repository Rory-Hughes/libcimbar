/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>

struct FountainDecoderLimits
{
	// The existing six-byte metadata format carries a 25-bit object size.
	static constexpr unsigned protocol_maximum_object_size = (1U << 25U) - 1U;
	static constexpr unsigned protocol_maximum_block_id = (1U << 16U) - 1U;
	static constexpr unsigned wirehair_maximum_blocks = 64000U;
	static constexpr unsigned extra_unique_blocks = 1024U;

	// Defaults retain the documented generic protocol range while constraining
	// concurrent state. Secure consumers must select a smaller object limit.
	unsigned maximum_object_size = protocol_maximum_object_size;
	// Aggregate claimed reconstruction bytes; third-party codec overhead is separate.
	std::size_t maximum_active_object_bytes = protocol_maximum_object_size;
	// Zero keeps the generic compatibility surface unchanged. Product-facing
	// policies must set both limits and are admitted using Wirehair's worst-case
	// decoder heap estimate before codec construction.
	std::size_t maximum_codec_memory_bytes = 0U;
	std::size_t maximum_active_codec_memory_bytes = 0U;
	unsigned maximum_active_streams = 1U;
	unsigned maximum_completed_transfers = 8U;
	unsigned maximum_unique_blocks = 0U;
	// Enforced for every packet in a batched frame before Wirehair sees it.
	unsigned maximum_block_id = protocol_maximum_block_id;
	unsigned maximum_packets_per_frame = 16U;
	unsigned maximum_frames_per_transfer = wirehair_maximum_blocks;
	unsigned maximum_no_progress_frames = extra_unique_blocks;
	unsigned maximum_cancelled_transfers = 8U;
	// Enforced on frame submission or an explicit sink expiry sweep.
	std::chrono::milliseconds maximum_transfer_duration = std::chrono::hours(1);

	bool valid() const
	{
		const bool codec_limits_consistent =
		    (maximum_codec_memory_bytes == 0U && maximum_active_codec_memory_bytes == 0U) ||
		    (maximum_codec_memory_bytes > 0U &&
		     maximum_active_codec_memory_bytes >= maximum_codec_memory_bytes);
		return maximum_object_size > 0U &&
		       maximum_object_size <= protocol_maximum_object_size &&
		       maximum_active_object_bytes >= maximum_object_size &&
		       maximum_active_streams > 0U &&
		       codec_limits_consistent &&
		       maximum_block_id <= protocol_maximum_block_id &&
		       maximum_packets_per_frame > 0U &&
		       maximum_frames_per_transfer > 0U &&
		       maximum_no_progress_frames > 0U &&
		       maximum_no_progress_frames <= maximum_frames_per_transfer &&
		       maximum_cancelled_transfers > 0U &&
		       maximum_transfer_duration.count() > 0;
	}

	unsigned unique_block_limit(std::size_t packet_size) const
	{
		if (packet_size == 0U)
			return 0U;

		if (maximum_unique_blocks > 0U)
			return std::min(maximum_unique_blocks, wirehair_maximum_blocks);

		const std::size_t blocks =
		    (static_cast<std::size_t>(maximum_object_size) + packet_size - 1U) / packet_size;
		const std::size_t bounded_blocks = std::min(
		    blocks + extra_unique_blocks,
		    static_cast<std::size_t>(wirehair_maximum_blocks)
		);
		return static_cast<unsigned>(bounded_blocks);
	}
};
