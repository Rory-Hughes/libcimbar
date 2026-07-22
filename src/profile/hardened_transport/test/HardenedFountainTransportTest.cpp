/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "HardenedFountainTransport.h"

#include "fountain/FountainEncoder.h"
#include "fountain/FountainMetadata.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {
	constexpr unsigned chunk_size = 690U;
	constexpr std::size_t object_size = 1200U;
	constexpr std::size_t encoded_size = 10U * chunk_size;
	using object_buffer = std::array<std::uint8_t, object_size>;
	using encoded_buffer = std::array<char, encoded_size>;

	FountainTransferPolicy profile_policy(FountainObjectClass object_class)
	{
		FountainTransferPolicy policy;
		policy.object_class = object_class;
		policy.decoder_limits.maximum_object_size = 2048U;
		policy.decoder_limits.maximum_active_object_bytes = 2048U;
		policy.decoder_limits.maximum_codec_memory_bytes = 1024U * 1024U;
		policy.decoder_limits.maximum_active_codec_memory_bytes = 1024U * 1024U;
		policy.decoder_limits.maximum_active_streams = 1U;
		policy.decoder_limits.maximum_completed_transfers = 0U;
		policy.decoder_limits.maximum_unique_blocks = 64U;
		policy.decoder_limits.maximum_block_id = 64U;
		policy.decoder_limits.maximum_packets_per_frame = 16U;
		policy.decoder_limits.maximum_frames_per_transfer = 64U;
		policy.decoder_limits.maximum_no_progress_frames = 16U;
		return policy;
	}

	bool encode(const object_buffer& input, std::uint8_t encode_id, encoded_buffer& encoded)
	{
		constexpr unsigned metadata_size = FountainMetadata::md_size;
		constexpr unsigned payload_size = chunk_size - metadata_size;
		FountainEncoder encoder(
		    reinterpret_cast<const std::uint8_t*>(input.data()),
		    input.size(),
		    payload_size
		);
		if (!encoder.good())
			return false;

		unsigned block = 0U;
		for (std::size_t offset = 0U; offset < encoded.size(); offset += chunk_size)
		{
			auto* packet = reinterpret_cast<std::uint8_t*>(encoded.data() + offset);
			std::size_t written = encoder.encode(block++, packet + metadata_size, payload_size);
			if (written != payload_size)
				written = encoder.encode(block++, packet + metadata_size, payload_size);
			if (written != payload_size)
				return false;
			FountainMetadata::to_uint8_arr(
			    encode_id & 0x7FU,
			    static_cast<unsigned>(input.size()),
			    block - 1U,
			    packet
			);
		}
		return true;
	}

	int verify_profile(FountainObjectClass object_class, std::uint8_t encode_id)
	{
		object_buffer expected{};
		expected.fill(static_cast<std::uint8_t>('S'));
		encoded_buffer encoded{};
		if (!encode(expected, encode_id, encoded))
			return 1;

		cimbar::HardenedFountainTransport transport(
		    chunk_size,
		    profile_policy(object_class)
		);
		if (!transport.good() || transport.object_class() != object_class)
			return 2;
		if (transport.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())) != 1)
			return 3;
		if (!transport.has_completed_object())
			return 4;

		auto object = transport.take_completed_object();
		if (!object || object->size() != expected.size())
			return 5;
		if (!std::equal(expected.begin(), expected.end(), object->begin()))
			return 6;
		if (transport.take_completed_object().has_value())
			return 7;
		return 0;
	}
}

int main()
{
	const int message_result = verify_profile(FountainObjectClass::message, 7U);
	if (message_result != 0)
		return message_result;
	const int wallet_result = verify_profile(FountainObjectClass::wallet, 8U);
	return wallet_result == 0 ? 0 : 10 + wallet_result;
}
