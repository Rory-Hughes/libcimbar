/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "FountainDecoderLimits.h"
#include "FountainMetadata.h"
#include "fountain_decoder_sink.h"

#include <cstdint>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

enum class FountainObjectClass : std::uint8_t
{
	unspecified = 0,
	pairing,
	message,
	wallet,
	firmware_update,
};

struct FountainTransferPolicy
{
	FountainObjectClass object_class = FountainObjectClass::unspecified;
	FountainDecoderLimits decoder_limits;

	bool valid() const
	{
		const bool known_object_class =
		    object_class == FountainObjectClass::pairing ||
		    object_class == FountainObjectClass::message ||
		    object_class == FountainObjectClass::wallet ||
		    object_class == FountainObjectClass::firmware_update;
		return known_object_class &&
		       decoder_limits.valid() &&
		       decoder_limits.maximum_active_streams == 1U &&
		       decoder_limits.maximum_active_object_bytes == decoder_limits.maximum_object_size &&
		       decoder_limits.maximum_codec_memory_bytes > 0U &&
		       decoder_limits.maximum_active_codec_memory_bytes == decoder_limits.maximum_codec_memory_bytes &&
		       decoder_limits.maximum_completed_transfers == 0U;
	}
};

enum class FountainSessionStatus : std::uint8_t
{
	invalid = 0,
	receiving,
	completed,
	object_taken,
	cancelled,
	failed,
};

// Product-facing fountain reconstruction boundary. It accepts only an explicit
// policy, exposes no callback/filename/filesystem/decompression surface, and
// transfers ownership of one exact-length opaque byte vector at most once.
class fountain_decoder_session
{
public:
	static constexpr std::int64_t invalid_policy = -100;
	static constexpr std::int64_t session_not_receiving = -101;
	static constexpr std::int64_t output_allocation_failed = -102;
	static constexpr std::int64_t decoder_allocation_failed = -103;

#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
	enum class TestFault : std::uint8_t
	{
		none = 0,
		decoder_allocation,
		output_allocation,
		output_refusal,
	};
#endif

	fountain_decoder_session(
	    unsigned chunk_size,
	    FountainTransferPolicy policy,
	    fountain_decoder_sink::now_function now=[] { return fountain_decoder_sink::clock::now(); }
	)
		: _policy(std::move(policy))
		, _sink(chunk_size, nullptr, _policy.decoder_limits, std::move(now))
		, _status(_policy.valid() && _sink.good()
		      ? FountainSessionStatus::receiving
		      : FountainSessionStatus::invalid)
	{
	}

	bool good() const
	{
		return _status != FountainSessionStatus::invalid;
	}

	FountainSessionStatus status() const
	{
		return _status;
	}

	FountainObjectClass object_class() const
	{
		return _policy.object_class;
	}

	std::int64_t submit_frame(const char* data, unsigned size)
	{
		if (_status == FountainSessionStatus::invalid)
			return invalid_policy;
		if (_status != FountainSessionStatus::receiving)
			return session_not_receiving;

		std::int64_t result = 0;
		try
		{
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
			if (consume_test_fault(TestFault::decoder_allocation))
				throw std::bad_alloc();
#endif
			result = _sink.decode_frame(data, size);
		}
		catch (const std::bad_alloc&)
		{
			fail();
			return decoder_allocation_failed;
		}
		catch (const std::length_error&)
		{
			fail();
			return decoder_allocation_failed;
		}
		if (result < 0)
		{
			fail();
			return result;
		}
		if (result == 0)
			return 0;

		const FountainMetadata metadata(static_cast<std::uint32_t>(result));
		std::vector<std::uint8_t> candidate;
		try
		{
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
			if (consume_test_fault(TestFault::output_allocation))
				throw std::bad_alloc();
#endif
			candidate.resize(metadata.file_size());
		}
		catch (const std::bad_alloc&)
		{
			fail();
			return output_allocation_failed;
		}
		catch (const std::length_error&)
		{
			fail();
			return output_allocation_failed;
		}

		bool recovered = false;
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
		if (!consume_test_fault(TestFault::output_refusal))
#endif
			recovered = _sink.recover(
		        metadata.id(),
		        candidate.data(),
		        static_cast<unsigned>(candidate.size())
		    );
		if (!recovered)
		{
			fail();
			return fountain_decoder_sink::output_recovery_failed;
		}

		_completed.emplace(std::move(candidate));
		_status = FountainSessionStatus::completed;
		return 1;
	}

	bool has_completed_object() const
	{
		return _status == FountainSessionStatus::completed && _completed.has_value();
	}

	std::optional<std::vector<std::uint8_t>> take_completed_object()
	{
		if (!has_completed_object())
			return std::nullopt;

		std::optional<std::vector<std::uint8_t>> result(std::move(*_completed));
		_completed.reset();
		_status = FountainSessionStatus::object_taken;
		return result;
	}

	void cancel()
	{
		if (_status == FountainSessionStatus::invalid)
			return;
		_sink.reset();
		_completed.reset();
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
		_nextTestFault = TestFault::none;
#endif
		_status = FountainSessionStatus::cancelled;
	}

	void reset()
	{
		_sink.reset();
		_completed.reset();
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
		_nextTestFault = TestFault::none;
#endif
		_status = _policy.valid() && _sink.good()
		    ? FountainSessionStatus::receiving
		    : FountainSessionStatus::invalid;
	}

#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
	void inject_test_fault(TestFault fault)
	{
		_nextTestFault = fault;
	}
#endif

private:
	void fail()
	{
		_sink.reset();
		_completed.reset();
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
		_nextTestFault = TestFault::none;
#endif
		_status = FountainSessionStatus::failed;
	}

#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
	bool consume_test_fault(TestFault fault)
	{
		if (_nextTestFault != fault)
			return false;
		_nextTestFault = TestFault::none;
		return true;
	}
#endif

	FountainTransferPolicy _policy;
	fountain_decoder_sink _sink;
	FountainSessionStatus _status;
	std::optional<std::vector<std::uint8_t>> _completed;
#if defined(CIMBAR_ENABLE_SESSION_TEST_FAULTS)
	TestFault _nextTestFault = TestFault::none;
#endif
};
