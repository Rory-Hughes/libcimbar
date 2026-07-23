/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "FountainInit.h"
#include "wirehair/wirehair.h"
#include <algorithm>
#include <optional>
#include <limits>
#include <cstdint>
#include <utility>
#include <vector>

// will need to split large files

class FountainDecoder
{
public:
	static std::optional<std::size_t> decoder_memory_required(
	    std::size_t length,
	    std::size_t packet_size)
	{
		if (packet_size > std::numeric_limits<std::uint32_t>::max())
			return std::nullopt;
		std::uint64_t required = 0U;
		if (wirehair_decoder_memory_required(length, static_cast<std::uint32_t>(packet_size), &required)
		    != Wirehair_Success || required > std::numeric_limits<std::size_t>::max())
			return std::nullopt;
		return static_cast<std::size_t>(required);
	}

	FountainDecoder(
	    size_t length,
	    size_t packet_size,
	    unsigned maximum_unique_blocks=0U,
	    unsigned maximum_block_id=std::numeric_limits<std::uint16_t>::max())
	    : _length(length)
	    , _packetSize(packet_size)
	    , _maximumUniqueBlocks(maximum_unique_blocks)
	    , _maximumBlockId(std::min<unsigned>(
	          maximum_block_id,
	          std::numeric_limits<std::uint16_t>::max()))
	    , _seenBlockWords(static_cast<std::size_t>(_maximumBlockId) / 64U + 1U, 0U)
	{
		FountainInit::init();
		if (_length > 0U && _packetSize > 0U &&
		    _packetSize <= std::numeric_limits<std::uint32_t>::max())
			_codec = wirehair_decoder_create(
			    nullptr,
			    _length,
			    static_cast<std::uint32_t>(_packetSize)
			);
	}

	FountainDecoder(const FountainDecoder&) = delete;
	FountainDecoder& operator=(const FountainDecoder&) = delete;

	FountainDecoder(FountainDecoder&& other) noexcept
	    : _codec(std::exchange(other._codec, nullptr))
	    , _res(other._res)
	    , _length(other._length)
	    , _packetSize(other._packetSize)
	    , _maximumUniqueBlocks(other._maximumUniqueBlocks)
	    , _maximumBlockId(other._maximumBlockId)
	    , _seenBlockCount(other._seenBlockCount)
	    , _seenBlockWords(std::move(other._seenBlockWords))
	{
	}

	FountainDecoder& operator=(FountainDecoder&& other) noexcept
	{
		if (this != &other)
		{
			wirehair_free(_codec);
			_codec = std::exchange(other._codec, nullptr);
			_res = other._res;
			_length = other._length;
			_packetSize = other._packetSize;
			_maximumUniqueBlocks = other._maximumUniqueBlocks;
			_maximumBlockId = other._maximumBlockId;
			_seenBlockCount = other._seenBlockCount;
			_seenBlockWords = std::move(other._seenBlockWords);
		}
		return *this;
	}

	~FountainDecoder()
	{
		wirehair_free(_codec);
	}

	unsigned progress() const
	{
		return _seenBlockCount;
	}

	size_t length() const
	{
		return _length;
	}

	bool good() const
	{
		return _codec != nullptr;
	}

	std::size_t decoder_memory_allocated() const
	{
		const std::uint64_t allocated = wirehair_decoder_memory_allocated(_codec);
		return allocated <= std::numeric_limits<std::size_t>::max()
		    ? static_cast<std::size_t>(allocated)
		    : std::numeric_limits<std::size_t>::max();
	}

	WirehairResult last_result() const
	{
		return _res;
	}

	bool decode(unsigned block_num, const uint8_t* data, size_t length)
	{
		if (!good() || data == nullptr || length == 0U || length > _packetSize)
		{
			_res = Wirehair_InvalidInput;
			return false;
		}

		if (block_num > _maximumBlockId ||
		    (_maximumUniqueBlocks > 0U && _seenBlockCount >= _maximumUniqueBlocks))
		{
			_res = Wirehair_InvalidInput;
			return false;
		}

		const std::size_t word_index = static_cast<std::size_t>(block_num) / 64U;
		const std::uint64_t mask = std::uint64_t{1U} << (block_num % 64U);
		if ((_seenBlockWords[word_index] & mask) != 0U)
			return false;
		_seenBlockWords[word_index] |= mask;
		++_seenBlockCount;

		_res = wirehair_decode(
		    _codec,
		    block_num,
		    data,
		    static_cast<std::uint32_t>(length)
		);
		if (_res != Wirehair_Success)
			return false;

		// we're theoretically done
		return true;
	}

	bool recover(unsigned char* data, unsigned size)
	{
		if (!good() || data == nullptr || size != _length)
		{
			_res = Wirehair_InvalidInput;
			return false;
		}

		_res = wirehair_recover(_codec, data, size);
		return _res == Wirehair_Success;
	}

	std::optional<std::vector<uint8_t>> recover()
	{
		std::vector<uint8_t> bytes;
		bytes.resize(_length);
		if (!recover(bytes.data(), bytes.size()))
			return std::nullopt; // :(

		return bytes;
	}

protected:
	WirehairCodec _codec = nullptr;
	WirehairResult _res = Wirehair_InvalidInput;
	size_t _length;
	size_t _packetSize;
	unsigned _maximumUniqueBlocks;
	unsigned _maximumBlockId;
	unsigned _seenBlockCount = 0U;
	// The wire format carries a 16-bit block identifier. A fixed-index bitmap
	// bounds duplicate tracking to at most 8 KiB instead of one allocation per
	// attacker-selected unique block.
	std::vector<std::uint64_t> _seenBlockWords;
};
