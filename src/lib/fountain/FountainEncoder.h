/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "FountainInit.h"
#include "wirehair/wirehair.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

class FountainEncoder
{
protected:
	void swap(FountainEncoder& other) throw()
	{
		std::swap(_codec, other._codec);
		std::swap(_packetSize, other._packetSize);
	}

public:
	FountainEncoder(const std::uint8_t* data, std::size_t length, std::size_t packet_size)
	    : _packetSize(packet_size)
	{
		FountainInit::init();
		_codec = wirehair_encoder_create(nullptr, data, length, packet_size);
	}

	~FountainEncoder()
	{
		wirehair_free(_codec);
	}

	FountainEncoder& operator=(FountainEncoder temp)
	{
		temp.swap(*this);
		return *this;
	}

	bool good() const
	{
		return _codec != nullptr;
	}

	std::size_t encode(unsigned block_num, std::uint8_t* buff, std::size_t size)
	{
		assert(size == _packetSize);
		std::uint32_t written = 0;
		WirehairResult res = wirehair_encode(_codec, block_num, buff, size, &written);
		if (res != Wirehair_Success)
			return 0;
		return written;
	}

	std::size_t packet_size() const
	{
		return _packetSize;
	}

protected:
	WirehairCodec _codec;
	std::size_t _packetSize;
};
