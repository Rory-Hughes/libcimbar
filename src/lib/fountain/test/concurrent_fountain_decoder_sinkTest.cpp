/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "concurrent_fountain_decoder_sink.h"
#include "fountain_encoder_stream.h"

#include <array>
#include <atomic>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
	constexpr unsigned chunk_size = 690U;
	constexpr unsigned frame_size = 6900U;

	std::string make_frame(fountain_encoder_stream& encoder)
	{
		std::string frame(frame_size, '\0');
		const auto read = encoder.readsome(frame.data(), frame.size());
		assertEquals(static_cast<std::streamsize>(frame.size()), read);
		return frame;
	}
}

TEST_CASE("ConcurrentFountainDecoderSink/rejectsNullInput", "[unit][concurrency]")
{
	concurrent_fountain_decoder_sink sink(chunk_size);
	assertFalse(sink.write(nullptr, chunk_size));
}

TEST_CASE("ConcurrentFountainDecoderSink/serializesWritersAndStateReaders", "[unit][concurrency]")
{
	constexpr unsigned object_size = 20000U;
	constexpr unsigned frame_count = 8U;
	constexpr unsigned writer_count = 4U;

	std::stringstream input;
	for (unsigned i = 0U; i < object_size / 10U; ++i)
		input << "0123456789";
	const std::string expected = input.str();

	auto encoder = fountain_encoder_stream::create(input, chunk_size, 37U);
	std::vector<std::string> frames;
	frames.reserve(frame_count);
	for (unsigned i = 0U; i < frame_count; ++i)
		frames.push_back(make_frame(*encoder));

	std::atomic<unsigned> completion_count{0U};
	std::string recovered;
	concurrent_fountain_decoder_sink sink(
	    chunk_size,
	    [&completion_count, &recovered](const std::string& name, const std::vector<uint8_t>& data)
	    {
		    recovered.assign(data.begin(), data.end());
		    completion_count.fetch_add(1U, std::memory_order_relaxed);
		    return name;
	    }
	);

	std::atomic<unsigned> next_frame{0U};
	std::atomic<bool> writers_running{true};
	std::atomic<bool> write_failed{false};
	std::thread observer([&sink, &writers_running]
	{
		while (writers_running.load(std::memory_order_acquire))
		{
			(void)sink.good();
			(void)sink.chunk_size();
			(void)sink.num_streams();
			(void)sink.num_done();
			(void)sink.get_done();
			(void)sink.get_progress();
			std::this_thread::yield();
		}
	});

	std::vector<std::thread> writers;
	writers.reserve(writer_count);
	for (unsigned i = 0U; i < writer_count; ++i)
	{
		writers.emplace_back([&sink, &frames, &next_frame, &write_failed]
		{
			while (true)
			{
				const unsigned index = next_frame.fetch_add(1U, std::memory_order_relaxed);
				if (index >= frames.size())
					break;
				if (!sink.write(frames[index].data(), static_cast<unsigned>(frames[index].size())))
					write_failed.store(true, std::memory_order_relaxed);
			}
		});
	}

	for (auto& writer : writers)
		writer.join();
	writers_running.store(false, std::memory_order_release);
	observer.join();

	assertFalse(write_failed.load(std::memory_order_relaxed));
	assertEquals(1U, completion_count.load(std::memory_order_relaxed));
	assertEquals(expected, recovered);
	assertEquals(0U, sink.num_streams());
	assertEquals(1U, sink.num_done());
	assertEquals(1U, sink.get_done().size());
}
