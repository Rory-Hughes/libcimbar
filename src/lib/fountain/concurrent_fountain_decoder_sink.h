/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "fountain_decoder_sink.h"

#include "concurrentqueue/concurrentqueue.h"
#include <mutex>
#include <new>
#include <utility>

class concurrent_fountain_decoder_sink
{
public:
	concurrent_fountain_decoder_sink(
	    unsigned chunk_size,
	    const std::function<std::string(const std::string&, const std::vector<uint8_t>&)>& on_store=nullptr,
	    FountainDecoderLimits limits=FountainDecoderLimits(),
	    fountain_decoder_sink::now_function now=[] { return fountain_decoder_sink::clock::now(); }
	)
		: _decoder(chunk_size, on_store, limits, std::move(now))
	{
	}

	bool good() const
	{
		std::lock_guard<std::mutex> lock(_writeMutex);
		return _decoder.good();
	}

	unsigned chunk_size() const
	{
		std::lock_guard<std::mutex> lock(_writeMutex);
		return _decoder.chunk_size();
	}

	unsigned num_streams() const
	{
		std::lock_guard<std::mutex> lock(_writeMutex);
		return _decoder.num_streams();
	}

	unsigned num_done() const
	{
		std::lock_guard<std::mutex> lock(_writeMutex);
		return _decoder.num_done();
	}

	std::vector<std::string> get_done() const
	{
		std::lock_guard<std::mutex> lock(_readMutex);
		return _done;
	}

	std::vector<double> get_progress() const
	{
		std::lock_guard<std::mutex> lock(_readMutex);
		return _progress;
	}

	void update_status()
	{
		std::lock_guard<std::mutex> lock(_writeMutex);
		update_status_locked();
	}

	void process()
	{
		// Every producer waits to become a drainer. A non-blocking try_lock can
		// strand the final packet when it is enqueued after the active drainer's
		// last dequeue but before that drainer unlocks. RAII also releases the
		// mutex if decoding or the completion callback throws.
		std::lock_guard<std::mutex> lock(_writeMutex);
		std::string buff;
		while (_backlog.try_dequeue(buff))
			_decoder << buff;

		update_status_locked();
	}

	bool write(const char* data, unsigned length)
	{
		if (data == nullptr)
			return false;
		std::string buffer(data, length);
		if (!_backlog.enqueue(buffer))
			return false;
		process();
		return true;
	}

	concurrent_fountain_decoder_sink& operator<<(const std::string& buffer)
	{
		if (!_backlog.enqueue(buffer))
			throw std::bad_alloc();
		process();
		return *this;
	}

protected:
	void update_status_locked()
	{
		// Lock order is always _writeMutex followed by _readMutex. Snapshot
		// readers take only _readMutex and therefore cannot invert this order.
		std::lock_guard<std::mutex> lock(_readMutex);
		_done = _decoder.get_done();
		_progress = _decoder.get_progress();
	}

	mutable std::mutex _writeMutex;
	mutable std::mutex _readMutex;
	fountain_decoder_sink _decoder;
	moodycamel::ConcurrentQueue< std::string > _backlog;

	std::vector<std::string> _done;
	std::vector<double> _progress;
};
