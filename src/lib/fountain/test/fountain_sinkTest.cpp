/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "FountainMetadata.h"
#include "fountain_encoder_stream.h"
#include "fountain_decoder_sink.h"

#include "serialize/format.h"
#include "serialize/str_join.h"
#include "util/File.h"
#include "util/MakeTempDirectory.h"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;
using namespace std;

namespace {
	std::array<char, FountainMetadata::md_size> metadata_frame(uint8_t encode_id, unsigned size, uint16_t block_id=0)
	{
		std::array<char, FountainMetadata::md_size> frame{};
		FountainMetadata::to_uint8_arr(
		    encode_id,
		    size,
		    block_id,
		    reinterpret_cast<uint8_t*>(frame.data())
		);
		return frame;
	}

	stringstream dummyContents(unsigned size)
	{
		stringstream input;
		for (unsigned i = 0; i < (size/10); ++i)
			input << "0123456789";
		return input;
	}

	string createFrame(fountain_encoder_stream& fes)
	{
		stringstream ss;
		std::array<char, 115> buff;

		// for a 6900 byte frame, 115*60 does the trick
		for (int i = 0; i < 60; ++i)
		{
			unsigned res = fes.readsome(buff.data(), buff.size());
			assertEquals( res, buff.size() );
			ss << string(buff.data(), buff.size());
		}
		return ss.str();
	}

	string createFrame(uint8_t encode_id, unsigned size)
	{
		stringstream input = dummyContents(size);
		fountain_encoder_stream::ptr fes = fountain_encoder_stream::create(input, 690, encode_id);
		return createFrame(*fes);
	}
}

TEST_CASE( "FountainSinkTest/testDefault", "[unit]" )
{
	MakeTempDirectory tempdir;

	fountain_decoder_sink sink(690, write_on_store<std::ofstream>(tempdir.path()));
	string iframe = createFrame(0, 1200);
	assertEquals( 6900, iframe.size() );

	FountainMetadata md(iframe.data(), iframe.size());
	assertEquals( 1200, md.file_size() );
	assertEquals( 0, md.encode_id() );

	assertEquals( true, sink.write(iframe.data(), iframe.size()) );
	assertEquals( true, sink.is_done(md.id()) );
	assertEquals( false, sink.write(iframe.data(), iframe.size()) );

	string frame2 = createFrame(1, 1600);
	assertEquals( true, sink.write(frame2.data(), frame2.size()) );
	assertEquals( true, sink.is_done(FountainMetadata(1, 1600, 0).id()) );

	assertEquals( 0, sink.num_streams() );
	assertEquals( 2, sink.num_done() );

	assertEquals( "", turbo::str::join(sink.get_progress()) );
	assertEquals( "1.1600 0.1200", turbo::str::join(sink.get_done()) );

	string contents = File(tempdir.path() / "0.1200").read_all();
	assertEquals( 1200, contents.size() );
	contents = File(tempdir.path() / "1.1600").read_all();
	assertEquals( 1600, contents.size() );
}

TEST_CASE( "FountainSinkTest/testMultipart", "[unit]" )
{
	MakeTempDirectory tempdir;

	fountain_decoder_sink sink(690, write_on_store<std::ofstream>(tempdir.path()));

	stringstream input = dummyContents(20000);
	fountain_encoder_stream::ptr fes = fountain_encoder_stream::create(input, 690, 2);

	for (int i = 0; i < 4; ++i)
	{
		string iframe = createFrame(*fes);
		assertEquals( 6900, iframe.size() );

		FountainMetadata md(iframe.data(), iframe.size());
		assertEquals( 20000, md.file_size() );
		assertEquals( 2, md.encode_id() );

		assertMsg( ((i == 2) == sink.write(iframe.data(), iframe.size())), fmt::format("failed {}", i) );
		assertMsg( ((i >= 2) == sink.is_done(md.id())), fmt::format("failed {}", i) );
	}

	assertEquals( 0, sink.num_streams() );
	assertEquals( 1, sink.num_done() );

	string contents = File(tempdir.path() / "2.20000").read_all();
	assertEquals( 20000, contents.size() );
}

TEST_CASE( "FountainSinkTest/testSameFrameManyTimes", "[unit]" )
{
	// if you give wirehair the same frame (under certain circumstances), you get a seg fault
	// sometimes it's fine. The docs say "don't do it", so FountainDecoder acts as the bouncer.
	MakeTempDirectory tempdir;

	fountain_decoder_sink sink(690, write_on_store<std::ofstream>(tempdir.path()));

	stringstream input = dummyContents(20000);
	fountain_encoder_stream::ptr fes = fountain_encoder_stream::create(input, 690, 3);

	string iframe = createFrame(*fes);
	assertEquals( 6900, iframe.size() );

	FountainMetadata md(iframe.data(), iframe.size());
	assertEquals( 20000, md.file_size() );
	assertEquals( 3, md.encode_id() );

	// don't crash!
	for (int i = 0; i < 40; ++i)
		assertFalse( sink.write(iframe.data(), iframe.size()) );

	assertEquals( 1, sink.num_streams() );
	assertEquals( 0, sink.num_done() );

	assertEquals( "0.333333", turbo::str::join(sink.get_progress()) ); // 33% done
	assertEquals( "", turbo::str::join(sink.get_done()) );
}

TEST_CASE( "FountainSinkTest/testRejectsObjectLargerThanPolicy", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_object_size = 1024;
	limits.maximum_unique_blocks = 32;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto frame = metadata_frame(0, 1025);

	assertEquals(
	    fountain_decoder_sink::object_size_exceeds_limit,
	    sink.decode_frame(frame.data(), frame.size())
	);
	assertEquals( 0, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testRejectsConcurrentTransferBeyondPolicy", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_active_streams = 1;
	limits.maximum_unique_blocks = 32;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto first = metadata_frame(0, 2048);
	const auto second = metadata_frame(1, 2048);

	assertEquals( 0, sink.decode_frame(first.data(), first.size()) );
	assertEquals( 1, sink.num_streams() );
	assertEquals(
	    fountain_decoder_sink::active_stream_limit_reached,
	    sink.decode_frame(second.data(), second.size())
	);
	assertEquals( 1, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testRejectsDifferentTransferInSameStreamSlot", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_active_streams = 1;
	limits.maximum_unique_blocks = 32;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto first = metadata_frame(0, 2048);
	const auto conflicting = metadata_frame(8, 2048);

	assertEquals( 0, sink.decode_frame(first.data(), first.size()) );
	assertEquals(
	    fountain_decoder_sink::conflicting_stream,
	    sink.decode_frame(conflicting.data(), conflicting.size())
	);
	assertEquals( 1, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testBoundsCompletedTransferCache", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_completed_transfers = 2;
	fountain_decoder_sink sink(690, nullptr, limits);
	const FountainMetadata first(0, 100, 0);
	const FountainMetadata second(1, 100, 0);
	const FountainMetadata third(2, 100, 0);

	sink.mark_done(first, "first");
	sink.mark_done(second, "second");
	sink.mark_done(third, "third");

	assertEquals( 2, sink.num_done() );
	assertFalse( sink.is_done(first.id()) );
	assertTrue( sink.is_done(second.id()) );
	assertTrue( sink.is_done(third.id()) );

	sink.reset();
	assertEquals( 0, sink.num_done() );
	assertEquals( 0, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testCancelsTransferAtFrameLimit", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_frames_per_transfer = 2;
	limits.maximum_no_progress_frames = 2;
	fountain_decoder_sink sink(690, nullptr, limits);
	const string iframe = createFrame(4, 20000);
	const FountainMetadata md(iframe.data(), iframe.size());

	assertEquals( 0, sink.decode_frame(iframe.data(), 690) );
	assertEquals(
	    fountain_decoder_sink::frame_limit_reached,
	    sink.decode_frame(iframe.data() + 690, 690)
	);
	assertEquals( 0, sink.num_streams() );
	assertTrue( sink.is_cancelled(md.id()) );
	assertEquals(
	    fountain_decoder_sink::frame_limit_reached,
	    sink.decode_frame(iframe.data(), 690)
	);
}

TEST_CASE( "FountainSinkTest/testRejectsOversizedFrame", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_packets_per_frame = 1;
	fountain_decoder_sink sink(690, nullptr, limits);
	string iframe = createFrame(4, 20000);

	assertEquals(
	    fountain_decoder_sink::frame_size_exceeds_limit,
	    sink.decode_frame(iframe.data(), 691)
	);
	assertEquals( 0, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testCancelsTransferAfterNoProgress", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_frames_per_transfer = 4;
	limits.maximum_no_progress_frames = 2;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto frame = metadata_frame(5, 2048);
	const FountainMetadata md(frame.data(), frame.size());

	assertEquals( 0, sink.decode_frame(frame.data(), frame.size()) );
	assertEquals(
	    fountain_decoder_sink::no_progress_limit_reached,
	    sink.decode_frame(frame.data(), frame.size())
	);
	assertEquals( 0, sink.num_streams() );
	assertEquals( 1, sink.num_cancelled() );
	assertTrue( sink.is_cancelled(md.id()) );
	assertEquals(
	    fountain_decoder_sink::no_progress_limit_reached,
	    sink.decode_frame(frame.data(), frame.size())
	);

	sink.reset();
	assertEquals( 0, sink.num_cancelled() );
	assertFalse( sink.is_cancelled(md.id()) );
}

TEST_CASE( "FountainSinkTest/testBoundsCancelledTransferCache", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_frames_per_transfer = 1;
	limits.maximum_no_progress_frames = 1;
	limits.maximum_cancelled_transfers = 1;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto first = metadata_frame(6, 2048);
	const auto second = metadata_frame(7, 2048);
	const FountainMetadata first_md(first.data(), first.size());
	const FountainMetadata second_md(second.data(), second.size());

	assertEquals(
	    fountain_decoder_sink::no_progress_limit_reached,
	    sink.decode_frame(first.data(), first.size())
	);
	assertEquals(
	    fountain_decoder_sink::no_progress_limit_reached,
	    sink.decode_frame(second.data(), second.size())
	);
	assertEquals( 1, sink.num_cancelled() );
	assertFalse( sink.is_cancelled(first_md.id()) );
	assertTrue( sink.is_cancelled(second_md.id()) );
}

TEST_CASE( "FountainSinkTest/testExpiresTransferAtDurationLimit", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_transfer_duration = std::chrono::milliseconds(100);
	limits.maximum_no_progress_frames = 10;
	auto now = fountain_decoder_sink::time_point{};
	fountain_decoder_sink sink(690, nullptr, limits, [&now] { return now; });
	const auto frame = metadata_frame(1, 2048);
	const FountainMetadata md(frame.data(), frame.size());

	assertEquals( 0, sink.decode_frame(frame.data(), frame.size()) );
	assertEquals( 1, sink.num_streams() );
	now += std::chrono::milliseconds(99);
	assertEquals( 0, sink.expire_transfers() );
	assertEquals( 1, sink.num_streams() );
	now += std::chrono::milliseconds(1);
	assertEquals( 1, sink.expire_transfers() );
	assertEquals( 0, sink.num_streams() );
	assertEquals( 0, sink.active_object_bytes() );
	assertTrue( sink.is_cancelled(md.id()) );
	assertEquals(
	    fountain_decoder_sink::transfer_duration_exceeded,
	    sink.decode_frame(frame.data(), frame.size())
	);
}

TEST_CASE( "FountainSinkTest/testBoundsAggregateActiveObjectBytes", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_object_size = 2048;
	limits.maximum_active_object_bytes = 3072;
	limits.maximum_active_streams = 2;
	limits.maximum_unique_blocks = 32;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto first = metadata_frame(2, 2048);
	const auto second = metadata_frame(3, 2048);

	assertEquals( 0, sink.decode_frame(first.data(), first.size()) );
	assertEquals( 2048, sink.active_object_bytes() );
	assertEquals(
	    fountain_decoder_sink::active_object_bytes_limit_reached,
	    sink.decode_frame(second.data(), second.size())
	);
	assertEquals( 1, sink.num_streams() );
	assertEquals( 2048, sink.active_object_bytes() );

	sink.reset();
	assertEquals( 0, sink.num_streams() );
	assertEquals( 0, sink.active_object_bytes() );
}

TEST_CASE( "FountainSinkTest/testRejectsInvalidChunkSize", "[unit]" )
{
	fountain_decoder_sink sink(FountainMetadata::md_size);
	const auto frame = metadata_frame(0, 256);

	assertFalse( sink.good() );
	assertEquals(
	    fountain_decoder_sink::invalid_decoder_configuration,
	    sink.decode_frame(frame.data(), frame.size())
	);
}

TEST_CASE( "FountainSinkTest/testRejectsNullFrame", "[unit]" )
{
	fountain_decoder_sink sink(690);
	assertEquals(
	    fountain_decoder_sink::frame_too_short,
	    sink.decode_frame(nullptr, FountainMetadata::md_size)
	);
}
