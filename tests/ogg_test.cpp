// `ogg_duration` (`formats/ogg.h`): the last page's granule position over the
// identification header's sample rate. Streams built page by page here.
#include "formats/ogg.h"
#include "test_support.h"

#include <cstdint>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put_u32(Bytes& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

/// One page: the 27-byte header, a one-segment lacing table, the body.
void page(Bytes& out, int64_t granule, const Bytes& body) {
    for (char c : {'O', 'g', 'g', 'S'}) out.push_back(static_cast<uint8_t>(c));
    out.push_back(0);  // version
    out.push_back(0);  // header type
    const uint64_t bits = static_cast<uint64_t>(granule);
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    put_u32(out, 1);  // serial
    put_u32(out, 0);  // sequence
    put_u32(out, 0);  // checksum
    out.push_back(1);
    out.push_back(static_cast<uint8_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

/// `\x01vorbis`, version 0, two channels, then the rate at +12.
Bytes identification(uint32_t rate) {
    Bytes id = {0x01, 'v', 'o', 'r', 'b', 'i', 's'};
    put_u32(id, 0);
    id.push_back(2);
    put_u32(id, rate);
    id.resize(30, 0);
    return id;
}

float duration(const Bytes& b) { return tl::formats::ogg_duration(b.data(), b.size()); }

}  // namespace

// 441000 samples at 44100 Hz is 10 s; 33075 at 22050 Hz is 1.5 s.
TL_TEST(ogg_duration_is_the_last_granule_over_the_rate) {
    Bytes b;
    page(b, 0, identification(44100));
    page(b, 88200, Bytes(4, 0x55));
    page(b, 441000, Bytes(4, 0x55));
    TL_CHECK_EQ(duration(b), 10.0f);

    Bytes c;
    page(c, 0, identification(22050));
    page(c, 33075, Bytes(4, 0x55));
    TL_CHECK_EQ(duration(c), 1.5f);
}

// A granule of -1 marks a page that completes no packet; it is not the length.
TL_TEST(ogg_duration_skips_a_page_that_ends_no_packet) {
    Bytes b;
    page(b, 0, identification(44100));
    page(b, 441000, Bytes(4, 0x55));
    page(b, -1, Bytes(4, 0x55));
    TL_CHECK_EQ(duration(b), 10.0f);
}

// No identification header, no rate: 0. Not an Ogg stream at all: 0.
TL_TEST(ogg_duration_needs_a_rate_and_the_capture_pattern) {
    Bytes no_id;
    page(no_id, 0, Bytes(30, 0));
    page(no_id, 441000, Bytes(4, 0x55));
    TL_CHECK_EQ(duration(no_id), 0.0f);

    Bytes not_ogg(64, 'x');
    TL_CHECK_EQ(duration(not_ogg), 0.0f);
    TL_CHECK_EQ(tl::formats::ogg_duration(nullptr, 0), 0.0f);
}

// A last page cut short is not read; the length is the last whole page's.
TL_TEST(ogg_duration_stops_at_a_truncated_page) {
    Bytes b;
    page(b, 0, identification(44100));
    page(b, 220500, Bytes(4, 0x55));  // 5 s
    page(b, 441000, Bytes(40, 0x55));
    b.resize(b.size() - 10);
    TL_CHECK_EQ(duration(b), 5.0f);
}
