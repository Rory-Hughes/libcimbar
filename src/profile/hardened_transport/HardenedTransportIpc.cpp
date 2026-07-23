/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "HardenedTransportIpc.h"

#include <cstdint>
#include <limits>

namespace cimbar {
namespace {
	constexpr std::uint8_t ipc_version = 1U;
	constexpr std::uint8_t ipc_magic[4] = {'L', 'C', 'I', 'P'};

	std::uint16_t read_le16(const std::uint8_t* data)
	{
		return static_cast<std::uint16_t>(data[0]) |
		       static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
	}

	std::uint32_t read_le32(const std::uint8_t* data)
	{
		return static_cast<std::uint32_t>(data[0]) |
		       (static_cast<std::uint32_t>(data[1]) << 8U) |
		       (static_cast<std::uint32_t>(data[2]) << 16U) |
		       (static_cast<std::uint32_t>(data[3]) << 24U);
	}

	std::uint64_t read_le64(const std::uint8_t* data)
	{
		return static_cast<std::uint64_t>(read_le32(data)) |
		       (static_cast<std::uint64_t>(read_le32(data + 4U)) << 32U);
	}

	std::int32_t read_le_i32(const std::uint8_t* data)
	{
		const std::uint32_t raw = read_le32(data);
		if (raw <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
			return static_cast<std::int32_t>(raw);
		return -1 - static_cast<std::int32_t>(~raw);
	}

	bool known_type(HardenedIpcMessageType type)
	{
		switch (type)
		{
		case HardenedIpcMessageType::submit_frame:
		case HardenedIpcMessageType::cancel_transfer:
		case HardenedIpcMessageType::reset_transfer:
		case HardenedIpcMessageType::completed_object:
		case HardenedIpcMessageType::transfer_status:
		case HardenedIpcMessageType::transfer_failed:
			return true;
		case HardenedIpcMessageType::invalid:
			return false;
		}
		return false;
	}

	bool known_status(HardenedIpcStatusCode status)
	{
		switch (status)
		{
		case HardenedIpcStatusCode::ok:
		case HardenedIpcStatusCode::receiving:
		case HardenedIpcStatusCode::completed:
		case HardenedIpcStatusCode::object_taken:
		case HardenedIpcStatusCode::cancelled:
		case HardenedIpcStatusCode::invalid_generation:
		case HardenedIpcStatusCode::invalid_frame:
		case HardenedIpcStatusCode::resource_limit:
		case HardenedIpcStatusCode::sandbox_violation:
		case HardenedIpcStatusCode::internal_error:
			return true;
		}
		return false;
	}

	bool non_negative_status(HardenedIpcStatusCode status)
	{
		return status == HardenedIpcStatusCode::ok ||
		       status == HardenedIpcStatusCode::receiving ||
		       status == HardenedIpcStatusCode::completed ||
		       status == HardenedIpcStatusCode::object_taken ||
		       status == HardenedIpcStatusCode::cancelled;
	}

	bool negative_status(HardenedIpcStatusCode status)
	{
		return status == HardenedIpcStatusCode::invalid_generation ||
		       status == HardenedIpcStatusCode::invalid_frame ||
		       status == HardenedIpcStatusCode::resource_limit ||
		       status == HardenedIpcStatusCode::sandbox_violation ||
		       status == HardenedIpcStatusCode::internal_error;
	}

	HardenedIpcParseResult reject(HardenedIpcParseStatus status)
	{
		HardenedIpcParseResult result;
		result.status = status;
		return result;
	}
}

HardenedIpcParseResult parse_hardened_ipc_message(
    const std::uint8_t* data,
    std::size_t size,
    const HardenedIpcLimits& limits
)
{
	if (data == nullptr)
		return reject(size == 0U
		    ? HardenedIpcParseStatus::message_too_short
		    : HardenedIpcParseStatus::null_input);
	if (size < hardened_ipc_header_size())
		return reject(HardenedIpcParseStatus::message_too_short);
	if (size > limits.maximum_ipc_message_size)
		return reject(HardenedIpcParseStatus::message_too_large);
	for (std::size_t index = 0U; index < sizeof(ipc_magic); ++index)
	{
		if (data[index] != ipc_magic[index])
			return reject(HardenedIpcParseStatus::bad_magic);
	}
	if (data[4] != ipc_version)
		return reject(HardenedIpcParseStatus::unsupported_version);

	const HardenedIpcMessageType type = static_cast<HardenedIpcMessageType>(data[5]);
	if (!known_type(type))
		return reject(HardenedIpcParseStatus::unknown_type);
	if (read_le16(data + 6U) != 0U)
		return reject(HardenedIpcParseStatus::reserved_flags);

	const std::uint64_t generation = read_le64(data + 8U);
	if (generation == 0U)
		return reject(HardenedIpcParseStatus::generation_zero);

	const std::uint32_t encoded_payload_size = read_le32(data + 16U);
	const std::size_t payload_size = static_cast<std::size_t>(encoded_payload_size);
	if (payload_size > std::numeric_limits<std::size_t>::max() - hardened_ipc_header_size())
		return reject(HardenedIpcParseStatus::size_mismatch);
	if (hardened_ipc_header_size() + payload_size != size)
		return reject(HardenedIpcParseStatus::size_mismatch);

	const HardenedIpcStatusCode status = static_cast<HardenedIpcStatusCode>(read_le_i32(data + 20U));
	if (!known_status(status))
		return reject(HardenedIpcParseStatus::invalid_status_code);

	switch (type)
	{
	case HardenedIpcMessageType::submit_frame:
		if (status != HardenedIpcStatusCode::ok)
			return reject(HardenedIpcParseStatus::invalid_status_code);
		if (payload_size == 0U)
			return reject(HardenedIpcParseStatus::payload_required);
		if (payload_size > limits.maximum_frame_payload_size)
			return reject(HardenedIpcParseStatus::payload_too_large);
		break;
	case HardenedIpcMessageType::completed_object:
		if (status != HardenedIpcStatusCode::ok)
			return reject(HardenedIpcParseStatus::invalid_status_code);
		if (payload_size == 0U)
			return reject(HardenedIpcParseStatus::payload_required);
		if (payload_size > limits.maximum_object_payload_size)
			return reject(HardenedIpcParseStatus::payload_too_large);
		break;
	case HardenedIpcMessageType::cancel_transfer:
	case HardenedIpcMessageType::reset_transfer:
		if (status != HardenedIpcStatusCode::ok)
			return reject(HardenedIpcParseStatus::invalid_status_code);
		if (payload_size != 0U)
			return reject(HardenedIpcParseStatus::payload_not_allowed);
		break;
	case HardenedIpcMessageType::transfer_status:
		if (!non_negative_status(status))
			return reject(HardenedIpcParseStatus::invalid_status_code);
		if (payload_size != 0U)
			return reject(HardenedIpcParseStatus::payload_not_allowed);
		break;
	case HardenedIpcMessageType::transfer_failed:
		if (!negative_status(status))
			return reject(HardenedIpcParseStatus::invalid_status_code);
		if (payload_size != 0U)
			return reject(HardenedIpcParseStatus::payload_not_allowed);
		break;
	case HardenedIpcMessageType::invalid:
		return reject(HardenedIpcParseStatus::unknown_type);
	}

	HardenedIpcParseResult result;
	result.status = HardenedIpcParseStatus::ok;
	result.message.type = type;
	result.message.transfer_generation = generation;
	result.message.status_code = status;
	result.message.payload_offset = hardened_ipc_header_size();
	result.message.payload_size = payload_size;
	return result;
}

} // namespace cimbar
