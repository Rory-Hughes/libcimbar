/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>

class escrow_buffer_writer
{
public:
	escrow_buffer_writer(unsigned char* bufspace, unsigned bufcount, unsigned bufsize)
		: _bufspace(bufspace)
		, _bufcount(bufcount)
		, _bufsize(bufsize)
	{
		const std::size_t maximum_offset =
		    static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
		if (_bufspace == nullptr || _bufcount == 0U || _bufsize == 0U ||
		    _bufcount > maximum_offset / _bufsize)
			_good = false;
	}

	bool good() const
	{
		return _good;
	}

	unsigned chunk_size() const
	{
		return _bufsize;
	}

	std::size_t tellp() const
	{
		return _totalCount;
	}

	unsigned buffers_in_use() const
	{
		return _bufIdx;
	}

	escrow_buffer_writer& write(const char* data, unsigned length)
	{
		// TODO: should this also act on multiples of `length`?

		// we can only write if the bufsize matches
		// and if we have buffers left
		if (data == nullptr || length != _bufsize || _bufIdx >= _bufcount)
			_good = false;

		if (!good())
			return *this;

		const std::size_t offset = _bufIdx * _bufsize;
		std::copy(data, data + length, _bufspace + offset);

		_totalCount += length;
		++_bufIdx;
		return *this;
	}

	escrow_buffer_writer& operator<<(const std::string& buffer)
	{
		return write(buffer.data(), buffer.size());
	}

protected:
	unsigned char* _bufspace;
	std::size_t _bufcount;
	std::size_t _bufsize;

	std::size_t _bufIdx = 0U;
	std::size_t _totalCount = 0U;
	bool _good = true;
};
