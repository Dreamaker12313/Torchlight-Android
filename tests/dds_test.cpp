// What a `.dds` holds in its alpha channel, on fixtures built here (a header and a few blocks).
#include "formats/dds.h"

#include "test_support.h"

#include <cstring>
#include <vector>

namespace {

void put32(std::vector<uint8_t>& out, size_t at, uint32_t value) {
    out[at] = static_cast<uint8_t>(value & 0xFF);
    out[at + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[at + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[at + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

// An 8x8 texture: four blocks of 16 bytes for DXT3 and DXT5.
std::vector<uint8_t> compressed(const char* code, uint8_t fill) {
    std::vector<uint8_t> file(128 + 4 * 16, fill);
    std::memset(file.data(), 0, 128);
    std::memcpy(file.data(), "DDS ", 4);
    put32(file, 4, 124);
    put32(file, 12, 8);
    put32(file, 16, 8);
    put32(file, 80, 0x4);
    std::memcpy(file.data() + 84, code, 4);
    return file;
}

std::vector<uint8_t> uncompressed(uint32_t alpha_mask, uint32_t pixel) {
    std::vector<uint8_t> file(128 + 4 * 4 * 4, 0);
    std::memcpy(file.data(), "DDS ", 4);
    put32(file, 4, 124);
    put32(file, 12, 4);
    put32(file, 16, 4);
    put32(file, 80, alpha_mask != 0 ? 0x41 : 0x40);
    put32(file, 88, 32);
    put32(file, 104, alpha_mask);
    for (size_t i = 0; i < 16; ++i) put32(file, 128 + i * 4, pixel);
    return file;
}

using tl::formats::dds_alpha;
using tl::formats::DdsAlpha;

}  // namespace

TL_TEST(dds_alpha_says_nothing_of_what_is_not_a_dds) {
    const std::vector<uint8_t> junk(200, 0x41);
    TL_CHECK(dds_alpha(junk.data(), junk.size()) == DdsAlpha::Unknown);
    TL_CHECK(dds_alpha(nullptr, 0) == DdsAlpha::Unknown);
    std::vector<uint8_t> cut = compressed("DXT3", 0xFF);
    cut.resize(128 + 20);
    TL_CHECK(dds_alpha(cut.data(), cut.size()) == DdsAlpha::Unknown);
}

TL_TEST(dds_alpha_takes_dxt1_as_having_none) {
    const std::vector<uint8_t> file = compressed("DXT1", 0x00);
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::None);
}

TL_TEST(dds_alpha_reads_dxt3_nibbles) {
    std::vector<uint8_t> file = compressed("DXT3", 0xFF);
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
    // One texel of the last block at 14/15: the colour half does not count.
    file[128 + 3 * 16 + 5] = 0xEF;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Varies);
    file[128 + 3 * 16 + 5] = 0xFF;
    file[128 + 3 * 16 + 12] = 0x00;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
}

TL_TEST(dds_alpha_decodes_dxt5_blocks) {
    // Both endpoints 255, every index on an endpoint.
    std::vector<uint8_t> file = compressed("DXT5", 0x00);
    for (size_t block = 0; block < 4; ++block) {
        file[128 + block * 16] = 255;
        file[128 + block * 16 + 1] = 255;
    }
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
    // Equal endpoints are the six-value mode, where index 6 is 0.
    file[128 + 3 * 16 + 2] = 0x06;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Varies);
    file[128 + 3 * 16 + 2] = 0x00;
    // Endpoints 255 and 0 with every index 0: still 255 everywhere.
    file[128 + 1] = 0;
    for (size_t i = 2; i < 8; ++i) file[128 + i] = 0;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
    // One texel on index 1 reads the other endpoint.
    file[128 + 2] = 0x01;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Varies);
    // Endpoints 0 and 255 (the six-value mode): index 7 is 255, index 6 is 0.
    file[128] = 0;
    file[128 + 1] = 255;
    for (size_t i = 2; i < 8; ++i) file[128 + i] = 0xFF;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
    file[128 + 2] = 0xFE;
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Varies);
}

TL_TEST(dds_alpha_scans_uncompressed_pixels) {
    std::vector<uint8_t> file = uncompressed(0xFF000000u, 0xFF102030u);
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Opaque);
    put32(file, 128 + 7 * 4, 0x80102030u);
    TL_CHECK(dds_alpha(file.data(), file.size()) == DdsAlpha::Varies);
    const std::vector<uint8_t> plain = uncompressed(0, 0x00102030u);
    TL_CHECK(dds_alpha(plain.data(), plain.size()) == DdsAlpha::None);
}

TL_TEST(dds_alpha_census_counts_what_a_rejection_removes) {
    // Two of four DXT3 blocks cut away entirely, one texel at 5/15 in a third.
    std::vector<uint8_t> file = compressed("DXT3", 0xFF);
    for (size_t b = 0; b < 8; ++b) {
        file[128 + b] = 0x00;
        file[128 + 16 + b] = 0x00;
    }
    file[128 + 2 * 16] = 0xF5;
    const tl::formats::DdsAlphaCensus census =
        tl::formats::dds_alpha_census(file.data(), file.size());
    TL_CHECK(census.kind == DdsAlpha::Varies);
    TL_CHECK(census.texels == 64);
    TL_CHECK(census.clear == 32);
    TL_CHECK(census.opaque == 31);
    TL_CHECK(census.clear_share() == 0.5f);
    // 5/15 is 85: below a threshold of 100 it is cleared too.
    TL_CHECK(tl::formats::dds_alpha_census(file.data(), file.size(), 100).clear == 33);
}
