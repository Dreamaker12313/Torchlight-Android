#include "formats/byte_reader.h"
#include "test_support.h"

using tl::formats::ByteReader;

TL_TEST(byte_reader_reads_little_endian) {
    const uint8_t data[] = {0x78, 0x56, 0x34, 0x12, 0xCD, 0xAB};
    ByteReader r(data, sizeof(data));

    TL_CHECK_EQ(r.u32(), 0x12345678u);
    TL_CHECK_EQ(r.u16(), 0xABCDu);
    TL_CHECK(r.ok());
    TL_CHECK_EQ(r.remaining(), size_t{0});
}

TL_TEST(byte_reader_refuses_to_read_past_the_end) {
    const uint8_t data[] = {0x01, 0x02};
    ByteReader r(data, sizeof(data));

    r.u32();  // only two bytes available
    TL_CHECK(!r.ok());
    TL_CHECK_EQ(r.remaining(), size_t{0});

    // The failure is sticky, so callers can check once at the end.
    ByteReader r2(data, sizeof(data));
    TL_CHECK_EQ(r2.u8(), uint8_t{0x01});
    r2.u32();
    TL_CHECK(!r2.ok());
    r2.u8();
    TL_CHECK(!r2.ok());
}

TL_TEST(byte_reader_decodes_counted_utf16le) {
    // Exactly the shape Runic uses: a uint32 CHARACTER count, then UTF-16LE.
    const uint8_t data[] = {
        0x05, 0x00, 0x00, 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'I', 0x00, 'X', 0x00,
    };
    ByteReader r(data, sizeof(data));

    TL_CHECK_EQ(r.counted_utf16le(), std::string("AFFIX"));
    TL_CHECK(r.ok());
}

TL_TEST(byte_reader_handles_surrogate_pairs) {
    // U+1D11E MUSICAL SYMBOL G CLEF = surrogate pair D834 DD1E
    const uint8_t data[] = {0x34, 0xD8, 0x1E, 0xDD};
    const std::string s = tl::formats::utf16le_to_utf8(data, sizeof(data));

    TL_CHECK_EQ(s.size(), size_t{4});  // four UTF-8 bytes
    TL_CHECK_EQ(static_cast<unsigned char>(s[0]), 0xF0u);
}

TL_TEST(byte_reader_replaces_unpaired_surrogates) {
    // Shipped game data is not guaranteed well-formed, so this must not throw.
    const uint8_t data[] = {0x34, 0xD8};  // lone high surrogate
    const std::string s = tl::formats::utf16le_to_utf8(data, sizeof(data));

    TL_CHECK_EQ(s, std::string("\xEF\xBF\xBD"));  // U+FFFD
}

TL_TEST(byte_reader_seek_is_bounds_checked) {
    const uint8_t data[] = {1, 2, 3, 4};
    ByteReader r(data, sizeof(data));

    TL_CHECK(r.seek(4));  // one past the last byte is a valid position
    TL_CHECK(r.ok());
    TL_CHECK(!r.seek(5));
    TL_CHECK(!r.ok());
}
