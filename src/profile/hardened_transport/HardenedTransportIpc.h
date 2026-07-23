/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace cimbar {

enum class HardenedIpcMessageType : std::uint8_t
{
	invalid = 0,
	submit_frame = 1,
	cancel_transfer = 2,
	reset_transfer = 3,
	completed_object = 4,
	transfer_status = 5,
	transfer_failed = 6,
};

enum class HardenedIpcStatusCode : std::int32_t
{
	ok = 0,
	receiving = 1,
	completed = 2,
	object_taken = 3,
	cancelled = 4,
	invalid_generation = -1,
	invalid_frame = -2,
	resource_limit = -3,
	sandbox_violation = -4,
	internal_error = -5,
};

enum class HardenedIpcParseStatus : std::uint8_t
{
	ok = 0,
	null_input,
	message_too_short,
	bad_magic,
	unsupported_version,
	reserved_flags,
	size_mismatch,
	message_too_large,
	unknown_type,
	generation_zero,
	invalid_status_code,
	payload_not_allowed,
	payload_required,
	payload_too_large,
};

struct HardenedIpcLimits
{
	std::size_t maximum_ipc_message_size = 64U * 1024U;
	std::size_t maximum_frame_payload_size = 16U * 1024U;
	std::size_t maximum_object_payload_size = 64U * 1024U;
};

struct HardenedIpcMessage
{
	HardenedIpcMessageType type = HardenedIpcMessageType::invalid;
	std::uint64_t transfer_generation = 0U;
	HardenedIpcStatusCode status_code = HardenedIpcStatusCode::ok;
	std::size_t payload_offset = 0U;
	std::size_t payload_size = 0U;
};

struct HardenedIpcParseResult
{
	HardenedIpcParseStatus status = HardenedIpcParseStatus::message_too_short;
	HardenedIpcMessage message;

	bool ok() const
	{
		return status == HardenedIpcParseStatus::ok;
	}
};

constexpr std::size_t hardened_ipc_header_size()
{
	return 24U;
}

HardenedIpcParseResult parse_hardened_ipc_message(
    const std::uint8_t* data,
    std::size_t size,
    const HardenedIpcLimits& limits=HardenedIpcLimits{}
);

} // namespace cimbar
