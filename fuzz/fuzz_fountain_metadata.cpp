#include "FountainMetadata.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

[[noreturn]] void invariant_failure()
{
#if defined(__clang__) || defined(__GNUC__)
    __builtin_trap();
#else
    std::abort();
#endif
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < FountainMetadata::md_size)
        return 0;

    FountainMetadata parsed(
        reinterpret_cast<const char*>(data),
        FountainMetadata::md_size
    );

    const std::uint8_t encode_id = parsed.encode_id();
    const unsigned file_size = parsed.file_size();
    const std::uint16_t block_id = parsed.block_id();
    const std::uint32_t identifier = parsed.id();

    if (encode_id > 0x7FU)
        invariant_failure();

    // FountainMetadata encodes file length in 25 bits.
    if (file_size > 0x1FFFFFFU)
        invariant_failure();

    std::array<std::uint8_t, FountainMetadata::md_size> canonical{};
    FountainMetadata::to_uint8_arr(
        encode_id,
        file_size,
        block_id,
        canonical.data()
    );

    FountainMetadata reparsed(
        reinterpret_cast<const char*>(canonical.data()),
        static_cast<unsigned>(canonical.size())
    );

    if (reparsed.encode_id() != encode_id ||
        reparsed.file_size() != file_size ||
        reparsed.block_id() != block_id)
    {
        invariant_failure();
    }

    // Exercise the implementation-defined identifier representation without
    // asserting cross-endian equality. Cross-build differential testing will
    // determine whether this value is suitable for a wire-level identifier.
    (void)identifier;

    return 0;
}
