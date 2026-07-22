/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "HardenedFountainTransport.h"

#include <utility>

namespace cimbar {

class HardenedFountainTransport::Implementation
{
public:
	Implementation(unsigned chunk_size, FountainTransferPolicy policy)
		: session(chunk_size, std::move(policy))
	{
	}

	fountain_decoder_session session;
};

HardenedFountainTransport::HardenedFountainTransport(
    unsigned chunk_size,
    FountainTransferPolicy policy
)
	: _implementation(std::make_unique<Implementation>(chunk_size, std::move(policy)))
{
}

HardenedFountainTransport::~HardenedFountainTransport() = default;
HardenedFountainTransport::HardenedFountainTransport(HardenedFountainTransport&&) noexcept = default;
HardenedFountainTransport& HardenedFountainTransport::operator=(HardenedFountainTransport&&) noexcept = default;

bool HardenedFountainTransport::good() const
{
	return _implementation && _implementation->session.good();
}

FountainSessionStatus HardenedFountainTransport::status() const
{
	return _implementation
	    ? _implementation->session.status()
	    : FountainSessionStatus::invalid;
}

FountainObjectClass HardenedFountainTransport::object_class() const
{
	return _implementation
	    ? _implementation->session.object_class()
	    : FountainObjectClass::unspecified;
}

std::int64_t HardenedFountainTransport::submit_frame(const char* data, unsigned size)
{
	return _implementation
	    ? _implementation->session.submit_frame(data, size)
	    : fountain_decoder_session::invalid_policy;
}

bool HardenedFountainTransport::has_completed_object() const
{
	return _implementation && _implementation->session.has_completed_object();
}

std::optional<std::vector<std::uint8_t>> HardenedFountainTransport::take_completed_object()
{
	return _implementation
	    ? _implementation->session.take_completed_object()
	    : std::nullopt;
}

void HardenedFountainTransport::cancel()
{
	if (_implementation)
		_implementation->session.cancel();
}

void HardenedFountainTransport::reset()
{
	if (_implementation)
		_implementation->session.reset();
}

} // namespace cimbar
