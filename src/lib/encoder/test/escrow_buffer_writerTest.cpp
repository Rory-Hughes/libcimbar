/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "encoder/aligned_stream.h"
#include "encoder/escrow_buffer_writer.h"

#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <vector>


TEST_CASE( "escrow_buffer_writerTest/testWrites", "[unit]" )
{
	std::vector<unsigned char> bufspace;
	bufspace.resize(60, 0);

	escrow_buffer_writer ebw(bufspace.data(), 4, 15); // two params should == overall buffer size

	std::string sample = "0123456789abcde";
	ebw << sample;
	assertEquals( 1, ebw.buffers_in_use() );
	assertTrue( ebw.good() );

	ebw << sample;
	ebw << sample;
	ebw << sample;
	assertEquals( 4, ebw.buffers_in_use() );
	assertEquals( sample+sample+sample+sample, std::string_view((char*)bufspace.data(), bufspace.size()) );

	assertTrue( ebw.good() );

	ebw << sample;
	assertFalse( ebw.good() );
	assertEquals( 4, ebw.buffers_in_use() );
}

TEST_CASE( "escrow_buffer_writerTest/testAlign", "[unit]" )
{
	// the way we expect to use this class is with the aligner wrapping it
	std::vector<unsigned char> bufspace;
	bufspace.resize(50, 0);

	escrow_buffer_writer ebw(bufspace.data(), 5, 10); // two params should == overall buffer size
	aligned_stream aligner(ebw, 10, 0);
	assertTrue( aligner.good() );

	aligner.write("01234567890123456789012345", 26);
	assertTrue( aligner.good() );

	assertEquals( 20, aligner.tellp() );
	assertEquals( 2, ebw.buffers_in_use() );
	assertEquals( 20, ebw.tellp() );
	assertEquals( "01234567890123456789", std::string_view((char*)bufspace.data(), 20) );

	aligner.mark_bad_chunk(10);
	aligner.write("abcdefABCDEFGH", 14);
	assertEquals( 30, aligner.tellp() );
	assertEquals( 3, ebw.buffers_in_use() );
	assertEquals( 30, ebw.tellp() );
	assertEquals( "01234567890123456789efABCDEFGH", std::string_view((char*)bufspace.data(), 30) );
}

TEST_CASE( "escrow_buffer_writerTest/rejectsInvalidBufferContracts", "[unit][security]" )
{
	std::array<unsigned char, 8> storage{};
	escrow_buffer_writer null_output(nullptr, 1U, 8U);
	assertFalse(null_output.good());

	escrow_buffer_writer null_input(storage.data(), 1U, 8U);
	null_input.write(nullptr, 8U);
	assertFalse(null_input.good());
	assertEquals(0U, null_input.buffers_in_use());

	escrow_buffer_writer impossible_span(
	    storage.data(),
	    std::numeric_limits<unsigned>::max(),
	    std::numeric_limits<unsigned>::max()
	);
	assertFalse(impossible_span.good());
}
