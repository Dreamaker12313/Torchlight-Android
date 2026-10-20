#include "formats/ogg.h"

#include <cstring>

namespace tl::formats {
namespace {

uint32_t u32_at(const uint8_t* d, size_t off) {
    uint32_t v = 0;
    std::memcpy(&v, d + off, 4);
    return v;
}

int64_t i64_at(const uint8_t* d, size_t off) {
    int64_t v = 0;
    std::memcpy(&v, d + off, 8);
    return v;
}

/// Everything before the lacing table of an Ogg page header.
constexpr size_t kPageHeader = 27;

}  // namespace

float ogg_duration(const uint8_t* data, size_t size) {
    if (data == nullptr || size < kPageHeader) return 0.0f;

    uint32_t rate = 0;
    int64_t last_granule = 0;

    size_t at = 0;
    while (at + kPageHeader <= size) {
        if (std::memcmp(data + at, "OggS", 4) != 0) return 0.0f;

        const int64_t granule = i64_at(data, at + 6);
        const uint8_t segments = data[at + 26];
        const size_t lacing = at + kPageHeader;
        if (lacing + segments > size) break;

        size_t payload = 0;
        for (uint8_t i = 0; i < segments; ++i) payload += data[lacing + i];

        const size_t body = lacing + segments;
        if (body + payload > size) break;

        // The first identification header's sample rate; chained streams are not read.
        if (rate == 0 && payload >= 16 && std::memcmp(data + body, "\x01vorbis", 7) == 0) {
            rate = u32_at(data, body + 12);
        }

        // -1 marks a page that completes no packet.
        if (granule >= 0) last_granule = granule;

        at = body + payload;
    }

    if (rate == 0 || last_granule <= 0) return 0.0f;
    return static_cast<float>(last_granule) / static_cast<float>(rate);
}

}  // namespace tl::formats
