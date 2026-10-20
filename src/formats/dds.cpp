#include "formats/dds.h"

namespace tl::formats {
namespace {

uint32_t read32(const uint8_t* at) {
    return static_cast<uint32_t>(at[0]) | (static_cast<uint32_t>(at[1]) << 8) |
           (static_cast<uint32_t>(at[2]) << 16) | (static_cast<uint32_t>(at[3]) << 24);
}

constexpr uint32_t fourcc(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

// The header: magic, then `DDS_HEADER` (124 bytes) with `DDS_PIXELFORMAT` at
// its offset 72.
constexpr size_t kHeaderSize = 128;
constexpr size_t kHeight = 12;
constexpr size_t kWidth = 16;
constexpr size_t kPixelFlags = 80;
constexpr size_t kFourCc = 84;
constexpr size_t kBitCount = 88;
constexpr size_t kAlphaMask = 104;
constexpr uint32_t kFlagAlphaPixels = 0x1;
constexpr uint32_t kFlagFourCc = 0x4;

struct Counter {
    DdsAlphaCensus& census;
    unsigned clear_at;

    void texel(unsigned alpha) {
        ++census.texels;
        if (alpha == 255) ++census.opaque;
        if (alpha <= clear_at) ++census.clear;
    }
};

// A DXT3 alpha block: sixteen 4-bit values, 0xF being 255.
void count_dxt3(const uint8_t* block, Counter& count) {
    for (int b = 0; b < 8; ++b) {
        count.texel((block[b] & 0xFu) * 17u);
        count.texel((block[b] >> 4) * 17u);
    }
}

// A DXT5 alpha block: two endpoints and sixteen 3-bit indices.
void count_dxt5(const uint8_t* block, Counter& count) {
    const unsigned a0 = block[0];
    const unsigned a1 = block[1];
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
    for (int texel = 0; texel < 16; ++texel) {
        const unsigned index = static_cast<unsigned>((bits >> (3 * texel)) & 0x7);
        unsigned value = 0;
        if (index == 0) {
            value = a0;
        } else if (index == 1) {
            value = a1;
        } else if (a0 > a1) {
            value = ((8 - index) * a0 + (index - 1) * a1) / 7;
        } else if (index == 6) {
            value = 0;
        } else if (index == 7) {
            value = 255;
        } else {
            value = ((6 - index) * a0 + (index - 1) * a1) / 5;
        }
        count.texel(value);
    }
}

}  // namespace

DdsAlphaCensus dds_alpha_census(const uint8_t* data, size_t size, uint8_t clear_at) {
    DdsAlphaCensus census;
    if (data == nullptr || size < kHeaderSize) return census;
    if (read32(data) != fourcc('D', 'D', 'S', ' ')) return census;
    const uint32_t height = read32(data + kHeight);
    const uint32_t width = read32(data + kWidth);
    if (width == 0 || height == 0 || width > 16384 || height > 16384) return census;
    const uint32_t flags = read32(data + kPixelFlags);
    const uint8_t* texels = data + kHeaderSize;
    const size_t left = size - kHeaderSize;
    Counter count{census, clear_at};

    if ((flags & kFlagFourCc) != 0) {
        const uint32_t code = read32(data + kFourCc);
        if (code == fourcc('D', 'X', 'T', '1')) {
            census.kind = DdsAlpha::None;
            return census;
        }
        const bool dxt3 = code == fourcc('D', 'X', 'T', '3') || code == fourcc('D', 'X', 'T', '2');
        const bool dxt5 = code == fourcc('D', 'X', 'T', '5') || code == fourcc('D', 'X', 'T', '4');
        if (!dxt3 && !dxt5) return census;
        const size_t blocks = static_cast<size_t>((width + 3) / 4) * ((height + 3) / 4);
        if (left / 16 < blocks) return census;
        for (size_t i = 0; i < blocks; ++i) {
            if (dxt3) {
                count_dxt3(texels + i * 16, count);
            } else {
                count_dxt5(texels + i * 16, count);
            }
        }
    } else {
        const uint32_t mask = read32(data + kAlphaMask);
        if ((flags & kFlagAlphaPixels) == 0 || mask == 0) {
            census.kind = DdsAlpha::None;
            return census;
        }
        // The archive's uncompressed textures keep alpha in one whole byte.
        unsigned shift = 0;
        while (shift < 32 && ((mask >> shift) & 1u) == 0) ++shift;
        if (read32(data + kBitCount) != 32 || (mask >> shift) != 0xFFu) return census;
        const size_t pixels = static_cast<size_t>(width) * height;
        if (left / 4 < pixels) return census;
        for (size_t i = 0; i < pixels; ++i) {
            count.texel((read32(texels + i * 4) >> shift) & 0xFFu);
        }
    }
    census.kind = census.opaque == census.texels ? DdsAlpha::Opaque : DdsAlpha::Varies;
    return census;
}

DdsAlpha dds_alpha(const uint8_t* data, size_t size) { return dds_alpha_census(data, size).kind; }

}  // namespace tl::formats
