/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "fountain_decoder_sink.h"

#include "compression/zstd_decompressor.h"
#include "compression/zstd_header_check.h"
#include "serialize/format.h"
#include "util/File.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Compatibility helpers for the generic file-transfer applications. Hardened
// transport targets must not include this header.
template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> write_on_store(
    std::string data_dir,
    bool log_writes=false
)
{
	return [data_dir, log_writes](const std::string& filename, const std::vector<uint8_t>& data)
	{
		std::string file_path = fmt::format("{}/{}", data_dir, filename);
		OUTSTREAM f(file_path, std::ios::binary);
		f.write(reinterpret_cast<const char*>(data.data()), data.size());
		if (log_writes)
			std::printf("%s\n", file_path.c_str());
		return filename;
	};
}

template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> write_on_store(
    const std::filesystem::path& data_dir,
    bool log_writes=false
)
{
	return write_on_store<OUTSTREAM>(data_dir.string(), log_writes);
}

template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> decompress_on_store(
    std::string data_dir,
    bool log_writes=false
)
{
	return [data_dir, log_writes](const std::string& fallback_name, const std::vector<uint8_t>& data)
	{
		std::string filename = cimbar::zstd_header_check::get_filename(data.data(), data.size());
		if (!filename.empty())
			filename = File::basename(filename);
		if (filename.empty())
			filename = fallback_name;

		std::string file_path = fmt::format("{}/{}", data_dir, filename);
		cimbar::zstd_decompressor<OUTSTREAM> f(file_path, std::ios::binary);
		f.write(reinterpret_cast<const char*>(data.data()), data.size());
		if (log_writes)
			std::printf("%s\n", file_path.c_str());
		return filename;
	};
}

template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> decompress_on_store(
    const std::filesystem::path& data_dir,
    bool log_writes=false
)
{
	return decompress_on_store<OUTSTREAM>(data_dir.string(), log_writes);
}
