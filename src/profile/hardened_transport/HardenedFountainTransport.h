/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "fountain/fountain_decoder_session.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace cimbar {

// Compiled product-profile boundary for corrected fountain packets. The public
// surface accepts an explicit resource policy and returns one opaque object. It
// intentionally has no filename, path, output-directory, decompression, file,
// callback, or application-routing operation.
class HardenedFountainTransport final
{
public:
	HardenedFountainTransport(unsigned chunk_size, FountainTransferPolicy policy);
	~HardenedFountainTransport();

	HardenedFountainTransport(HardenedFountainTransport&&) noexcept;
	HardenedFountainTransport& operator=(HardenedFountainTransport&&) noexcept;

	HardenedFountainTransport(const HardenedFountainTransport&) = delete;
	HardenedFountainTransport& operator=(const HardenedFountainTransport&) = delete;

	bool good() const;
	FountainSessionStatus status() const;
	FountainObjectClass object_class() const;
	std::int64_t submit_frame(const char* data, unsigned size);
	bool has_completed_object() const;
	std::optional<std::vector<std::uint8_t>> take_completed_object();
	void cancel();
	void reset();

private:
	class Implementation;
	std::unique_ptr<Implementation> _implementation;
};

} // namespace cimbar
