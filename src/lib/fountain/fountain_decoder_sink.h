/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "fountain_decoder_stream.h"
#include "FountainDecoderLimits.h"
#include "FountainMetadata.h"

#include <bitset>
#include <chrono>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class fountain_decoder_sink
{
public:
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using now_function = std::function<time_point()>;

	static constexpr int64_t frame_too_short = -10;
	static constexpr int64_t empty_object = -11;
	static constexpr int64_t conflicting_stream = -12;
	static constexpr int64_t object_size_exceeds_limit = -13;
	static constexpr int64_t active_stream_limit_reached = -14;
	static constexpr int64_t invalid_decoder_configuration = -15;
	static constexpr int64_t output_recovery_failed = -16;
	static constexpr int64_t frame_limit_reached = -17;
	static constexpr int64_t no_progress_limit_reached = -18;
	static constexpr int64_t frame_size_exceeds_limit = -19;
	static constexpr int64_t transfer_duration_exceeded = -20;
	static constexpr int64_t active_object_bytes_limit_reached = -21;
	static constexpr int64_t transfer_already_completed = -22;
	static constexpr int64_t frame_size_misaligned = -23;
	static constexpr int64_t conflicting_packet_metadata = -24;
	static constexpr int64_t block_id_exceeds_limit = -25;
	static constexpr int64_t codec_memory_exceeds_limit = -26;
	static constexpr int64_t active_codec_memory_limit_reached = -27;
	static constexpr std::size_t encode_id_count = 128U;

	fountain_decoder_sink(
	    unsigned chunk_size,
	    const std::function<std::string(const std::string&, const std::vector<uint8_t>&)>& on_store=nullptr,
	    FountainDecoderLimits limits=FountainDecoderLimits(),
	    now_function now=[] { return clock::now(); }
	)
		: _chunkSize(chunk_size)
		, _onStore(on_store)
		, _limits(limits)
		, _now(std::move(now))
	{
	}

	bool good() const
	{
		return _chunkSize > FountainMetadata::md_size && _limits.valid() && static_cast<bool>(_now);
	}

	unsigned chunk_size() const
	{
		return _chunkSize;
	}

	std::string get_filename(const FountainMetadata& md) const
	{
		return std::to_string(md.encode_id()) + "." + std::to_string(md.file_size());
	}

	bool store(const FountainMetadata& md, fountain_decoder_stream& s)
	{
		if (_onStore)
		{
			auto res = s.recover();
			if (!res)
				return false;
			std::string filename = _onStore(get_filename(md), *res);
			mark_done(md, filename);
		}
		return true;
	}

	void mark_done(const FountainMetadata& md, const std::string& filename)
	{
		if (_limits.maximum_completed_transfers > 0U)
		{
			auto done = _done.find(md.id());
			if (done != _done.end())
			{
				done->second = filename;
			}
			else
			{
				while (_done.size() >= _limits.maximum_completed_transfers)
				{
					_done.erase(_doneOrder.front());
					_doneOrder.pop_front();
				}
				_done.emplace(md.id(), filename);
				_doneOrder.push_back(md.id());
			}
		}
		mark_done(md);
	}

	void mark_done(const FountainMetadata& md)
	{
		_completedEncodeIds.set(md.encode_id());
		erase_stream(stream_slot(md));
	}

	void reset()
	{
		_streams.clear();
		_streamIds.clear();
		_streamBudgets.clear();
		_done.clear();
		_doneOrder.clear();
		_cancelled.clear();
		_cancelledOrder.clear();
		_completedEncodeIds.reset();
		_activeObjectBytes = 0U;
		_activeCodecMemoryBytes = 0U;
	}

	unsigned num_streams() const
	{
		return _streams.size();
	}

	unsigned num_done() const
	{
		return _done.size();
	}

	unsigned num_cancelled() const
	{
		return _cancelled.size();
	}

	std::size_t active_object_bytes() const
	{
		return _activeObjectBytes;
	}

	std::size_t active_codec_memory_bytes() const
	{
		return _activeCodecMemoryBytes;
	}

	std::vector<std::string> get_done() const
	{
		std::vector<std::string> done;
		for (auto&& [id, filename] : _done)
			done.push_back( filename );
		return done;
	}

	std::vector<double> get_progress() const
	{
		std::vector<double> progress;
		for (auto&& [slot, s] : _streams)
		{
			unsigned br = s.blocks_required();
			if (br)
				progress.push_back( s.progress() * 1.0 / s.blocks_required() );
		}
		return progress;
	}

	bool is_done(uint32_t id) const
	{
		return _completedEncodeIds.test(FountainMetadata(id).encode_id());
	}

	bool is_cancelled(uint32_t id) const
	{
		return _cancelled.find(id) != _cancelled.end();
	}

	unsigned expire_transfers()
	{
		if (!good())
			return 0U;
		return expire_transfers(_now());
	}

	int64_t decode_frame(const char* data, unsigned size)
	{
		if (!good())
			return invalid_decoder_configuration;

		if (data == nullptr || size < FountainMetadata::md_size)
			return frame_too_short;

		FountainMetadata md(data, size);
		if (!md.file_size())
		{
			/*std::cout << fmt::format("decode frame {} ... {},{},{},{}",
									 md.file_size(), (unsigned)data[0], (unsigned)data[1], (unsigned)data[2], (unsigned)data[3]) << std::endl;*/
			return empty_object;
		}

		if (md.file_size() > _limits.maximum_object_size)
			return object_size_exceeds_limit;

		const uint64_t maximum_frame_size =
		    static_cast<uint64_t>(_chunkSize) * _limits.maximum_packets_per_frame;
		if (static_cast<uint64_t>(size) > maximum_frame_size)
			return frame_size_exceeds_limit;
		if (size < _chunkSize || size % _chunkSize != 0U)
			return frame_size_misaligned;

		// Validate every packet before creating or mutating decoder state. A frame
		// may batch packets, but all packets must belong to the same transfer and
		// each block identifier must satisfy the selected policy.
		for (unsigned offset = 0U; offset < size; offset += _chunkSize)
		{
			FountainMetadata packet(data + offset, _chunkSize);
			if (packet.id() != md.id())
				return conflicting_packet_metadata;
			if (packet.block_id() > _limits.maximum_block_id)
				return block_id_exceeds_limit;
		}

		const time_point now = _now();
		expire_transfers(now);

		// check if already done
		if (is_done(md.id()))
			return transfer_already_completed;

		auto cancelled = _cancelled.find(md.id());
		if (cancelled != _cancelled.end())
			return cancelled->second;

		const uint8_t slot = stream_slot(md);
		auto stream = _streams.find(slot);
		auto stream_id = _streamIds.find(slot);
		if (stream == _streams.end() && _streams.size() >= _limits.maximum_active_streams)
			return active_stream_limit_reached;
		if (stream != _streams.end() && (stream_id == _streamIds.end() || stream_id->second != md.id()))
			return conflicting_stream;
		if (stream == _streams.end() &&
		    static_cast<std::size_t>(md.file_size()) > _limits.maximum_active_object_bytes - _activeObjectBytes)
			return active_object_bytes_limit_reached;

		std::size_t codec_memory_required = 0U;
		if (stream == _streams.end())
		{
			const auto required = FountainDecoder::decoder_memory_required(
			    md.file_size(),
			    _chunkSize - FountainMetadata::md_size
			);
			if (!required)
				return invalid_decoder_configuration;
			codec_memory_required = *required;
			if (_limits.maximum_codec_memory_bytes > 0U &&
			    codec_memory_required > _limits.maximum_codec_memory_bytes)
				return codec_memory_exceeds_limit;
			if (_limits.maximum_active_codec_memory_bytes > 0U &&
			    (_activeCodecMemoryBytes > _limits.maximum_active_codec_memory_bytes ||
			     codec_memory_required > _limits.maximum_active_codec_memory_bytes - _activeCodecMemoryBytes))
				return active_codec_memory_limit_reached;
		}

		const unsigned maximum_unique_blocks = _limits.unique_block_limit(
		    _chunkSize - FountainMetadata::md_size
		);
		if (maximum_unique_blocks == 0U)
			return invalid_decoder_configuration;

		// Find or create after every metadata-derived allocation input has been checked.
		auto p = _streams.try_emplace(
		    slot,
		    md.file_size(),
		    _chunkSize,
		    maximum_unique_blocks,
		    _limits.maximum_block_id
		);
		if (p.second)
		{
			_streamIds.insert_or_assign(slot, md.id());
			_streamBudgets.insert_or_assign(slot, StreamBudget{now, 0U, 0U, codec_memory_required});
			_activeObjectBytes += md.file_size();
			_activeCodecMemoryBytes += codec_memory_required;
		}
		fountain_decoder_stream& s = p.first->second;
		if (!s.good() || s.data_size() != md.file_size())
		{
			if (p.second)
				erase_stream(slot);
			return conflicting_stream;
		}

		auto budget = _streamBudgets.find(slot);
		if (budget == _streamBudgets.end())
		{
			erase_stream(slot);
			return conflicting_stream;
		}

		const unsigned progress_before = s.progress();
		const bool finished = s.write(data, size);
		++budget->second.frames;
		if (s.progress() > progress_before)
			budget->second.no_progress_frames = 0U;
		else
			++budget->second.no_progress_frames;

		if (!finished && budget->second.no_progress_frames >= _limits.maximum_no_progress_frames)
			return cancel_stream(md, no_progress_limit_reached);
		if (!finished && budget->second.frames >= _limits.maximum_frames_per_transfer)
			return cancel_stream(md, frame_limit_reached);
		if (!finished)
			return 0;

		// when you provide a write callback,
		// store() will call mark_done() afterwards
		// -- and the assembled file will be dropped from RAM.
		// but if no callback is provided, you can do something else.
		if (!store(md, s))
			return output_recovery_failed;
		return (int64_t)0 + md.id();
	}

	bool write(const char* data, unsigned length)
	{
		return decode_frame(data, length) > 0;
	}

	fountain_decoder_sink& operator<<(const std::string& buffer)
	{
		write(buffer.data(), buffer.size());
		return *this;
	}

	bool recover(uint32_t id, unsigned char* data, unsigned size)
	{
		// iff you don't provide a write callback
		// this finalizes the write.
		// after the data is copied to `data`,
		// the stream will be dropped from RAM (`mark_done()`)
		FountainMetadata md(id);
		const uint8_t slot = stream_slot(md);
		auto p = _streams.find(slot);
		auto stream_id = _streamIds.find(slot);
		if (p == _streams.end() || stream_id == _streamIds.end() || stream_id->second != md.id())
			return false;

		fountain_decoder_stream& s = p->second;
		if (s.data_size() != md.file_size())
			return false;
		bool res = s.recover(data, size);
		if (res)
		{
			if (_limits.maximum_completed_transfers > 0U)
				mark_done(md, get_filename(md));
			else
				mark_done(md);
		}
		return res;
	}

protected:
	struct StreamBudget
	{
		time_point started_at;
		unsigned frames = 0U;
		unsigned no_progress_frames = 0U;
		std::size_t codec_memory_bytes = 0U;
	};

	// streams is limited to at most 8 decoders at a time. Currently, we just use the lower bits of the encode_id.
	uint8_t stream_slot(const FountainMetadata& md) const
	{
		return md.encode_id() & 0x7;
	}

	void erase_stream(uint8_t slot)
	{
		auto stream = _streams.find(slot);
		if (stream != _streams.end())
		{
			_activeObjectBytes -= stream->second.data_size();
			auto budget = _streamBudgets.find(slot);
			if (budget != _streamBudgets.end())
				_activeCodecMemoryBytes -= budget->second.codec_memory_bytes;
			_streams.erase(stream);
		}
		_streamIds.erase(slot);
		_streamBudgets.erase(slot);
	}

	unsigned expire_transfers(time_point now)
	{
		std::vector<uint32_t> expired;
		for (const auto& [slot, budget] : _streamBudgets)
		{
			if (now - budget.started_at < _limits.maximum_transfer_duration)
				continue;
			auto id = _streamIds.find(slot);
			if (id != _streamIds.end())
				expired.push_back(id->second);
		}

		for (uint32_t id : expired)
			cancel_stream(FountainMetadata(id), transfer_duration_exceeded);
		return static_cast<unsigned>(expired.size());
	}

	int64_t cancel_stream(const FountainMetadata& md, int64_t reason)
	{
		erase_stream(stream_slot(md));

		auto cancelled = _cancelled.find(md.id());
		if (cancelled != _cancelled.end())
		{
			cancelled->second = reason;
			return reason;
		}

		while (_cancelled.size() >= _limits.maximum_cancelled_transfers)
		{
			_cancelled.erase(_cancelledOrder.front());
			_cancelledOrder.pop_front();
		}
		_cancelled.emplace(md.id(), reason);
		_cancelledOrder.push_back(md.id());
		return reason;
	}

protected:
	unsigned _chunkSize;
	std::function<std::string(const std::string&, const std::vector<uint8_t>&)> _onStore;
	FountainDecoderLimits _limits;
	now_function _now;
	std::size_t _activeObjectBytes = 0U;
	std::size_t _activeCodecMemoryBytes = 0U;

	std::unordered_map<uint8_t, fountain_decoder_stream> _streams;
	// A stream slot is only a storage bucket; the full metadata ID binds it to a transfer.
	std::unordered_map<uint8_t, uint32_t> _streamIds;
	std::unordered_map<uint8_t, StreamBudget> _streamBudgets;
	// Track a bounded FIFO set of completed transfer identifiers.
	std::unordered_map<uint32_t, std::string> _done;
	std::deque<uint32_t> _doneOrder;
	// Exact bounded terminal history for the protocol's seven-bit encode ID.
	std::bitset<encode_id_count> _completedEncodeIds;
	// Retain bounded cancellation tombstones so a rejected transfer cannot restart immediately.
	std::unordered_map<uint32_t, int64_t> _cancelled;
	std::deque<uint32_t> _cancelledOrder;
};
