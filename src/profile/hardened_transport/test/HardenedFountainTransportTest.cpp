/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "HardenedFountainTransport.h"
#include "HardenedTransportIpc.h"

#include "fountain/FountainEncoder.h"
#include "fountain/FountainMetadata.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

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

	void write_le16(std::vector<std::uint8_t>& output, std::size_t offset, std::uint16_t value)
	{
		output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
		output[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	}

	void write_le32(std::vector<std::uint8_t>& output, std::size_t offset, std::uint32_t value)
	{
		output[offset] = static_cast<std::uint8_t>(value & 0xFFU);
		output[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
		output[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
		output[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
	}

	void write_le64(std::vector<std::uint8_t>& output, std::size_t offset, std::uint64_t value)
	{
		write_le32(output, offset, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
		write_le32(output, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
	}

	std::uint32_t encode_status(cimbar::HardenedIpcStatusCode status)
	{
		const auto value = static_cast<std::int32_t>(status);
		return static_cast<std::uint32_t>(value);
	}

	std::vector<std::uint8_t> ipc_message(
	    cimbar::HardenedIpcMessageType type,
	    std::uint64_t generation,
	    cimbar::HardenedIpcStatusCode status,
	    const std::vector<std::uint8_t>& payload={}
	)
	{
		std::vector<std::uint8_t> output(cimbar::hardened_ipc_header_size() + payload.size(), 0U);
		output[0] = 'L';
		output[1] = 'C';
		output[2] = 'I';
		output[3] = 'P';
		output[4] = 1U;
		output[5] = static_cast<std::uint8_t>(type);
		write_le16(output, 6U, 0U);
		write_le64(output, 8U, generation);
		write_le32(output, 16U, static_cast<std::uint32_t>(payload.size()));
		write_le32(output, 20U, encode_status(status));
		std::copy(payload.begin(), payload.end(), output.begin() + cimbar::hardened_ipc_header_size());
		return output;
	}

	bool expect_ipc_status(
	    const std::vector<std::uint8_t>& input,
	    cimbar::HardenedIpcParseStatus expected,
	    const cimbar::HardenedIpcLimits& limits=cimbar::HardenedIpcLimits{}
	)
	{
		const auto result = cimbar::parse_hardened_ipc_message(input.data(), input.size(), limits);
		return result.status == expected;
	}

	int verify_ipc_contract()
	{
		const std::vector<std::uint8_t> frame_payload{0x01U, 0x02U, 0x03U, 0x04U};
		const auto submit = ipc_message(
		    cimbar::HardenedIpcMessageType::submit_frame,
		    12U,
		    cimbar::HardenedIpcStatusCode::ok,
		    frame_payload
		);
		auto parsed = cimbar::parse_hardened_ipc_message(submit.data(), submit.size());
		if (!parsed.ok())
			return 1;
		if (parsed.message.type != cimbar::HardenedIpcMessageType::submit_frame ||
		    parsed.message.transfer_generation != 12U ||
		    parsed.message.status_code != cimbar::HardenedIpcStatusCode::ok ||
		    parsed.message.payload_offset != cimbar::hardened_ipc_header_size() ||
		    parsed.message.payload_size != frame_payload.size())
			return 2;

		const auto completed = ipc_message(
		    cimbar::HardenedIpcMessageType::completed_object,
		    13U,
		    cimbar::HardenedIpcStatusCode::ok,
		    std::vector<std::uint8_t>{0xAAU}
		);
		if (!expect_ipc_status(completed, cimbar::HardenedIpcParseStatus::ok))
			return 3;

		if (!expect_ipc_status(
		        ipc_message(cimbar::HardenedIpcMessageType::reset_transfer, 13U, cimbar::HardenedIpcStatusCode::ok),
		        cimbar::HardenedIpcParseStatus::ok
		    ))
			return 4;
		if (!expect_ipc_status(
		        ipc_message(
		            cimbar::HardenedIpcMessageType::reset_transfer,
		            13U,
		            cimbar::HardenedIpcStatusCode::ok,
		            std::vector<std::uint8_t>{0x00U}
		        ),
		        cimbar::HardenedIpcParseStatus::payload_not_allowed
		    ))
			return 5;
		if (!expect_ipc_status(
		        ipc_message(cimbar::HardenedIpcMessageType::submit_frame, 0U, cimbar::HardenedIpcStatusCode::ok, frame_payload),
		        cimbar::HardenedIpcParseStatus::generation_zero
		    ))
			return 6;
		if (!expect_ipc_status(
		        ipc_message(cimbar::HardenedIpcMessageType::submit_frame, 1U, cimbar::HardenedIpcStatusCode::ok),
		        cimbar::HardenedIpcParseStatus::payload_required
		    ))
			return 7;
		if (!expect_ipc_status(
		        ipc_message(
		            cimbar::HardenedIpcMessageType::transfer_failed,
		            1U,
		            cimbar::HardenedIpcStatusCode::internal_error,
		            std::vector<std::uint8_t>{'d', 'i', 'a', 'g'}
		        ),
		        cimbar::HardenedIpcParseStatus::payload_not_allowed
		    ))
			return 8;
		if (!expect_ipc_status(
		        ipc_message(cimbar::HardenedIpcMessageType::transfer_failed, 1U, cimbar::HardenedIpcStatusCode::ok),
		        cimbar::HardenedIpcParseStatus::invalid_status_code
		    ))
			return 9;

		auto bad_version = submit;
		bad_version[4] = 2U;
		if (!expect_ipc_status(bad_version, cimbar::HardenedIpcParseStatus::unsupported_version))
			return 10;

		auto unknown_type = submit;
		unknown_type[5] = 99U;
		if (!expect_ipc_status(unknown_type, cimbar::HardenedIpcParseStatus::unknown_type))
			return 11;

		auto size_mismatch = submit;
		size_mismatch.pop_back();
		if (!expect_ipc_status(size_mismatch, cimbar::HardenedIpcParseStatus::size_mismatch))
			return 12;

		auto reserved_flags = submit;
		write_le16(reserved_flags, 6U, 1U);
		if (!expect_ipc_status(reserved_flags, cimbar::HardenedIpcParseStatus::reserved_flags))
			return 13;

		cimbar::HardenedIpcLimits tight_limits;
		tight_limits.maximum_ipc_message_size = submit.size() - 1U;
		if (!expect_ipc_status(submit, cimbar::HardenedIpcParseStatus::message_too_large, tight_limits))
			return 14;

		cimbar::HardenedIpcLimits tiny_object;
		tiny_object.maximum_object_payload_size = 0U;
		if (!expect_ipc_status(completed, cimbar::HardenedIpcParseStatus::payload_too_large, tiny_object))
			return 15;

		const auto null_result = cimbar::parse_hardened_ipc_message(nullptr, submit.size());
		if (null_result.status != cimbar::HardenedIpcParseStatus::null_input)
			return 16;
		return 0;
	}
}

int main()
{
	const int message_result = verify_profile(FountainObjectClass::message, 7U);
	if (message_result != 0)
		return message_result;
	const int wallet_result = verify_profile(FountainObjectClass::wallet, 8U);
	if (wallet_result != 0)
		return 10 + wallet_result;
	const int ipc_result = verify_ipc_contract();
	return ipc_result == 0 ? 0 : 20 + ipc_result;
}
