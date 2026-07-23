/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "HardenedTransportIpc.h"

#include <cassert>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	cimbar::HardenedIpcLimits limits;
	if (size > 0U)
		limits.maximum_frame_payload_size = 1U + static_cast<std::size_t>(data[0] & 0x3FU);
	if (size > 1U)
		limits.maximum_object_payload_size = 1U + static_cast<std::size_t>(data[1] & 0x7FU);
	if (size > 2U)
		limits.maximum_ipc_message_size =
		    cimbar::hardened_ipc_header_size() + static_cast<std::size_t>(data[2]);

	const auto result = cimbar::parse_hardened_ipc_message(data, size, limits);
	if (!result.ok())
		return 0;

	assert(result.message.type != cimbar::HardenedIpcMessageType::invalid);
	assert(result.message.transfer_generation != 0U);
	assert(result.message.payload_offset == cimbar::hardened_ipc_header_size());
	assert(result.message.payload_offset + result.message.payload_size == size);
	assert(size <= limits.maximum_ipc_message_size);

	switch (result.message.type)
	{
	case cimbar::HardenedIpcMessageType::submit_frame:
		assert(result.message.status_code == cimbar::HardenedIpcStatusCode::ok);
		assert(result.message.payload_size > 0U);
		assert(result.message.payload_size <= limits.maximum_frame_payload_size);
		break;
	case cimbar::HardenedIpcMessageType::completed_object:
		assert(result.message.status_code == cimbar::HardenedIpcStatusCode::ok);
		assert(result.message.payload_size > 0U);
		assert(result.message.payload_size <= limits.maximum_object_payload_size);
		break;
	case cimbar::HardenedIpcMessageType::cancel_transfer:
	case cimbar::HardenedIpcMessageType::reset_transfer:
	case cimbar::HardenedIpcMessageType::transfer_status:
	case cimbar::HardenedIpcMessageType::transfer_failed:
		assert(result.message.payload_size == 0U);
		break;
	case cimbar::HardenedIpcMessageType::invalid:
		assert(false);
	}
	return 0;
}
