/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "FountainMetadata.h"
#include "fountain_encoder_stream.h"
#include "fountain_decoder_file_sink.h"
#include "fountain_decoder_session.h"

#include "serialize/format.h"
#include "serialize/str_join.h"
#include "util/File.h"
#include "util/MakeTempDirectory.h"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using namespace std;

namespace {
	constexpr std::size_t test_chunk_size = 690U;

	std::array<char, test_chunk_size> metadata_frame(uint8_t encode_id, unsigned size, uint16_t block_id=0)
	{
		std::array<char, test_chunk_size> frame{};
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

	FountainTransferPolicy secure_message_policy(unsigned maximum_object_size=2048U)
	{
		FountainTransferPolicy policy;
		policy.object_class = FountainObjectClass::message;
		policy.decoder_limits.maximum_object_size = maximum_object_size;
		policy.decoder_limits.maximum_active_object_bytes = maximum_object_size;
		policy.decoder_limits.maximum_codec_memory_bytes = 1024U * 1024U;
		policy.decoder_limits.maximum_active_codec_memory_bytes = 1024U * 1024U;
		policy.decoder_limits.maximum_active_streams = 1U;
		policy.decoder_limits.maximum_completed_transfers = 0U;
		policy.decoder_limits.maximum_unique_blocks = 64U;
		policy.decoder_limits.maximum_block_id = 64U;
		policy.decoder_limits.maximum_packets_per_frame = 16U;
		policy.decoder_limits.maximum_frames_per_transfer = 64U;
		policy.decoder_limits.maximum_no_progress_frames = 16U;
		return policy;
	}
}

TEST_CASE( "FountainSinkTest/testBoundsWirehairCodecMemoryBeforeStateMutation", "[unit]" )
{
	const auto required = FountainDecoder::decoder_memory_required(2048U, test_chunk_size - FountainMetadata::md_size);
	assertTrue(required.has_value());
	assertTrue(*required > 1U);

	FountainDecoderLimits limits;
	limits.maximum_object_size = 2048U;
	limits.maximum_active_object_bytes = 2048U;
	limits.maximum_codec_memory_bytes = *required - 1U;
	limits.maximum_active_codec_memory_bytes = *required;
	fountain_decoder_sink rejected(test_chunk_size, nullptr, limits);
	const auto frame = metadata_frame(2, 2048);
	assertEquals(
	    fountain_decoder_sink::codec_memory_exceeds_limit,
	    rejected.decode_frame(frame.data(), static_cast<unsigned>(frame.size()))
	);
	assertEquals(0, rejected.num_streams());
	assertEquals(0, rejected.active_codec_memory_bytes());

	limits.maximum_codec_memory_bytes = *required;
	fountain_decoder_sink admitted(test_chunk_size, nullptr, limits);
	assertEquals(0, admitted.decode_frame(frame.data(), static_cast<unsigned>(frame.size())));
	assertEquals(1, admitted.num_streams());
	assertEquals(*required, admitted.active_codec_memory_bytes());
	admitted.reset();
	assertEquals(0, admitted.active_codec_memory_bytes());
}

TEST_CASE( "FountainDecoderTest/reportsAllocatedMemoryWithinAdmissionBound", "[unit]" )
{
	const std::string encoded = createFrame(9, 1200U);
	const auto required = FountainDecoder::decoder_memory_required(1200U, test_chunk_size - FountainMetadata::md_size);
	assertTrue(required.has_value());
	FountainDecoder decoder(1200U, test_chunk_size - FountainMetadata::md_size);
	assertTrue(decoder.good());
	assertTrue(decoder.decoder_memory_allocated() > 0U);
	assertTrue(decoder.decoder_memory_allocated() <= *required);

	bool finished = false;
	for (std::size_t offset = 0U; offset < encoded.size(); offset += test_chunk_size)
	{
		const auto* packet = reinterpret_cast<const std::uint8_t*>(encoded.data() + offset);
		const unsigned block_id = static_cast<unsigned>(packet[4]) << 8U | packet[5];
		finished = decoder.decode(
		    block_id,
		    packet + FountainMetadata::md_size,
		    test_chunk_size - FountainMetadata::md_size
		);
		assertTrue(decoder.decoder_memory_allocated() <= *required);
		if (finished)
			break;
	}
	assertTrue(finished);
}

TEST_CASE( "FountainDecoderTest/rejectsBlockCountBeforeNarrowing", "[unit][security]" )
{
	constexpr std::size_t wrapped_block_count =
	    static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 3U;

	assertFalse(FountainDecoder::decoder_memory_required(wrapped_block_count, 1U).has_value());
	FountainDecoder decoder(wrapped_block_count, 1U);
	assertFalse(decoder.good());
}

TEST_CASE( "FountainDecoderTest/boundsDuplicateTrackingWithBlockBitmap", "[unit][security]" )
{
	constexpr unsigned maximum_block_id = 127U;
	const auto frame = metadata_frame(4U, 1200U, maximum_block_id);
	FountainDecoder decoder(
	    1200U,
	    test_chunk_size - FountainMetadata::md_size,
	    8U,
	    maximum_block_id
	);
	assertTrue(decoder.good());

	const auto* packet = reinterpret_cast<const std::uint8_t*>(frame.data());
	assertFalse(decoder.decode(
	    maximum_block_id,
	    packet + FountainMetadata::md_size,
	    test_chunk_size - FountainMetadata::md_size
	));
	assertEquals(1U, decoder.progress());

	// A duplicate does not consume another unique-block slot.
	assertFalse(decoder.decode(
	    maximum_block_id,
	    packet + FountainMetadata::md_size,
	    test_chunk_size - FountainMetadata::md_size
	));
	assertEquals(1U, decoder.progress());

	assertFalse(decoder.decode(
	    maximum_block_id + 1U,
	    packet + FountainMetadata::md_size,
	    test_chunk_size - FountainMetadata::md_size
	));
	assertEquals(Wirehair_InvalidInput, decoder.last_result());
	assertEquals(1U, decoder.progress());
}

TEST_CASE( "FountainSinkTest/boundsAggregateWirehairCodecMemory", "[unit]" )
{
	const auto required = FountainDecoder::decoder_memory_required(2048U, test_chunk_size - FountainMetadata::md_size);
	assertTrue(required.has_value());
	FountainDecoderLimits limits;
	limits.maximum_object_size = 2048U;
	limits.maximum_active_object_bytes = 4096U;
	limits.maximum_active_streams = 2U;
	limits.maximum_codec_memory_bytes = *required;
	limits.maximum_active_codec_memory_bytes = *required;
	fountain_decoder_sink sink(test_chunk_size, nullptr, limits);
	const auto first = metadata_frame(2, 2048U);
	const auto second = metadata_frame(3, 2048U);

	assertEquals(0, sink.decode_frame(first.data(), static_cast<unsigned>(first.size())));
	assertEquals(*required, sink.active_codec_memory_bytes());
	assertEquals(
	    fountain_decoder_sink::active_codec_memory_limit_reached,
	    sink.decode_frame(second.data(), static_cast<unsigned>(second.size()))
	);
	assertEquals(1, sink.num_streams());
	assertEquals(*required, sink.active_codec_memory_bytes());
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
	assertTrue( sink.is_done(first.id()) );
	assertTrue( sink.is_done(second.id()) );
	assertTrue( sink.is_done(third.id()) );

	sink.reset();
	assertEquals( 0, sink.num_done() );
	assertEquals( 0, sink.num_streams() );
}

TEST_CASE( "FountainSinkTest/testCompletesAtMostOnceAfterDetailEviction", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_completed_transfers = 1;
	std::unordered_map<std::string, unsigned> completion_counts;
	auto on_store = [&completion_counts](const std::string& name, const std::vector<uint8_t>&)
	{
		++completion_counts[name];
		return name;
	};
	fountain_decoder_sink sink(690, on_store, limits);
	const string first_frame = createFrame(0, 1200);
	const string second_frame = createFrame(1, 1600);
	const FountainMetadata first(first_frame.data(), first_frame.size());
	const FountainMetadata second(second_frame.data(), second_frame.size());

	assertTrue( sink.write(first_frame.data(), first_frame.size()) );
	assertTrue( sink.write(second_frame.data(), second_frame.size()) );
	assertEquals( 1, completion_counts["0.1200"] );
	assertEquals( 1, completion_counts["1.1600"] );
	assertEquals( 1, sink.num_done() );
	assertTrue( sink.is_done(first.id()) );
	assertTrue( sink.is_done(second.id()) );
	assertEquals(
		fountain_decoder_sink::transfer_already_completed,
		sink.decode_frame(first_frame.data(), first_frame.size())
	);
	assertEquals( 1, completion_counts["0.1200"] );
	assertEquals( 0, sink.num_streams() );

	const auto reused_encode_id = metadata_frame(0, 1201);
	assertEquals(
		fountain_decoder_sink::transfer_already_completed,
		sink.decode_frame(reused_encode_id.data(), reused_encode_id.size())
	);

	sink.reset();
	assertFalse( sink.is_done(first.id()) );
	assertTrue( sink.write(first_frame.data(), first_frame.size()) );
	assertEquals( 2, completion_counts["0.1200"] );
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

TEST_CASE( "FountainSinkTest/testRejectsMisalignedFrameBeforeStateMutation", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_packets_per_frame = 2;
	fountain_decoder_sink sink(test_chunk_size, nullptr, limits);
	const auto frame = metadata_frame(4, 20000);
	std::vector<char> misaligned(frame.begin(), frame.end());
	misaligned.push_back(0);

	assertEquals(
	    fountain_decoder_sink::frame_size_misaligned,
	    sink.decode_frame(misaligned.data(), static_cast<unsigned>(misaligned.size()))
	);
	assertEquals(0, sink.num_streams());
}

TEST_CASE( "FountainSinkTest/testRejectsConflictingMetadataInsideBatchedFrame", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_packets_per_frame = 2;
	fountain_decoder_sink sink(test_chunk_size, nullptr, limits);
	const auto first = metadata_frame(4, 20000, 0);
	const auto conflicting = metadata_frame(5, 20000, 1);
	std::vector<char> batched;
	batched.insert(batched.end(), first.begin(), first.end());
	batched.insert(batched.end(), conflicting.begin(), conflicting.end());

	assertEquals(
	    fountain_decoder_sink::conflicting_packet_metadata,
	    sink.decode_frame(batched.data(), static_cast<unsigned>(batched.size()))
	);
	assertEquals(0, sink.num_streams());
	assertEquals(0, sink.active_object_bytes());
}

TEST_CASE( "FountainSinkTest/testRejectsBlockIdentifierAbovePolicy", "[unit]" )
{
	FountainDecoderLimits limits;
	limits.maximum_block_id = 10;
	fountain_decoder_sink sink(test_chunk_size, nullptr, limits);
	const auto frame = metadata_frame(4, 20000, 11);

	assertEquals(
	    fountain_decoder_sink::block_id_exceeds_limit,
	    sink.decode_frame(frame.data(), static_cast<unsigned>(frame.size()))
	);
	assertEquals(0, sink.num_streams());
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
	limits.maximum_frames_per_transfer = 2;
	limits.maximum_no_progress_frames = 1;
	limits.maximum_cancelled_transfers = 1;
	fountain_decoder_sink sink(690, nullptr, limits);
	const auto first = metadata_frame(6, 2048);
	const auto second = metadata_frame(7, 2048);
	const FountainMetadata first_md(first.data(), first.size());
	const FountainMetadata second_md(second.data(), second.size());

	assertEquals(0, sink.decode_frame(first.data(), first.size()));
	assertEquals(
	    fountain_decoder_sink::no_progress_limit_reached,
	    sink.decode_frame(first.data(), first.size())
	);
	assertEquals(0, sink.decode_frame(second.data(), second.size()));
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

TEST_CASE( "FountainDecoderSession/testReturnsExactOpaqueObjectOnce", "[unit]" )
{
	fountain_decoder_session session(test_chunk_size, secure_message_policy());
	const string encoded = createFrame(0, 1200);
	const string expected = dummyContents(1200).str();

	assertTrue(session.good());
	assertEquals(FountainObjectClass::message, session.object_class());
	assertEquals(1, session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())));
	assertEquals(FountainSessionStatus::completed, session.status());
	assertTrue(session.has_completed_object());

	auto object = session.take_completed_object();
	assertTrue(object.has_value());
	assertEquals(expected.size(), object->size());
	assertTrue(std::equal(expected.begin(), expected.end(), object->begin()));
	assertEquals(FountainSessionStatus::object_taken, session.status());
	assertFalse(session.has_completed_object());
	assertFalse(session.take_completed_object().has_value());
	assertEquals(
	    fountain_decoder_session::session_not_receiving,
	    session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size()))
	);
}

TEST_CASE( "FountainDecoderSession/testRejectsInvalidPolicyAndFailsClosed", "[unit]" )
{
	FountainTransferPolicy invalid_policy;
	invalid_policy.object_class = FountainObjectClass::message;
	invalid_policy.decoder_limits.maximum_object_size = 2048U;
	fountain_decoder_session invalid(test_chunk_size, invalid_policy);
	const auto frame = metadata_frame(0, 1200);

	assertFalse(invalid.good());
	assertEquals(
	    fountain_decoder_session::invalid_policy,
	    invalid.submit_frame(frame.data(), static_cast<unsigned>(frame.size()))
	);
	FountainTransferPolicy retained_details = secure_message_policy();
	retained_details.decoder_limits.maximum_completed_transfers = 1U;
	fountain_decoder_session invalid_details(test_chunk_size, retained_details);
	assertFalse(invalid_details.good());
	FountainTransferPolicy missing_codec_budget = secure_message_policy();
	missing_codec_budget.decoder_limits.maximum_codec_memory_bytes = 0U;
	missing_codec_budget.decoder_limits.maximum_active_codec_memory_bytes = 0U;
	fountain_decoder_session invalid_codec_budget(test_chunk_size, missing_codec_budget);
	assertFalse(invalid_codec_budget.good());

	fountain_decoder_session session(test_chunk_size, secure_message_policy(1024U));
	const auto oversized = metadata_frame(0, 1025);
	assertEquals(
	    fountain_decoder_sink::object_size_exceeds_limit,
	    session.submit_frame(oversized.data(), static_cast<unsigned>(oversized.size()))
	);
	assertEquals(FountainSessionStatus::failed, session.status());
	assertFalse(session.has_completed_object());
	assertEquals(
	    fountain_decoder_session::session_not_receiving,
	    session.submit_frame(frame.data(), static_cast<unsigned>(frame.size()))
	);

	session.reset();
	assertEquals(FountainSessionStatus::receiving, session.status());
}

TEST_CASE( "FountainDecoderSession/testCancelClearsUntakenOutput", "[unit]" )
{
	fountain_decoder_session session(test_chunk_size, secure_message_policy());
	const string encoded = createFrame(0, 1200);

	assertEquals(1, session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())));
	assertTrue(session.has_completed_object());
	session.cancel();
	assertEquals(FountainSessionStatus::cancelled, session.status());
	assertFalse(session.has_completed_object());
	assertFalse(session.take_completed_object().has_value());

	session.reset();
	assertEquals(FountainSessionStatus::receiving, session.status());
	assertEquals(1, session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())));
}

TEST_CASE( "FountainDecoderSession/testAllocationAndOutputFaultsFailClosed", "[unit]" )
{
	using TestFault = fountain_decoder_session::TestFault;
	struct FaultCase
	{
		TestFault fault;
		std::int64_t expected_result;
	};
	const std::array<FaultCase, 3> faults{{
		{TestFault::decoder_allocation, fountain_decoder_session::decoder_allocation_failed},
		{TestFault::output_allocation, fountain_decoder_session::output_allocation_failed},
		{TestFault::output_refusal, fountain_decoder_sink::output_recovery_failed},
	}};
	const string encoded = createFrame(0, 1200);
	const string expected = dummyContents(1200).str();

	for (const FaultCase& fault : faults)
	{
		fountain_decoder_session session(test_chunk_size, secure_message_policy());
		session.inject_test_fault(fault.fault);

		assertEquals(
		    fault.expected_result,
		    session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size()))
		);
		assertEquals(FountainSessionStatus::failed, session.status());
		assertFalse(session.has_completed_object());
		assertFalse(session.take_completed_object().has_value());
		assertEquals(
		    fountain_decoder_session::session_not_receiving,
		    session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size()))
		);

		session.reset();
		assertEquals(FountainSessionStatus::receiving, session.status());
		assertEquals(1, session.submit_frame(encoded.data(), static_cast<unsigned>(encoded.size())));
		auto object = session.take_completed_object();
		assertTrue(object.has_value());
		assertTrue(std::equal(expected.begin(), expected.end(), object->begin()));
	}
}
