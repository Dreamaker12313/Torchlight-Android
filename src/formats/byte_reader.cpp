#include "formats/byte_reader.h"

#include <cstring>

namespace tl::formats {

bool ByteReader::want(size_t count) noexcept {
    if (!ok_) return false;
    if (count > size_ - offset_) {  // no overflow: offset_ <= size_ always
        ok_ = false;
        return false;
    }
    return true;
}

bool ByteReader::seek(size_t absolute) noexcept {
    if (!ok_) return false;
    if (absolute > size_) {
        ok_ = false;
        return false;
    }
    offset_ = absolute;
    return true;
}

bool ByteReader::skip(size_t count) noexcept {
    if (!want(count)) return false;
    offset_ += count;
    return true;
}

bool ByteReader::bytes(void* dest, size_t count) noexcept {
    if (!want(count)) return false;
    std::memcpy(dest, data_ + offset_, count);
    offset_ += count;
    return true;
}

uint8_t ByteReader::u8() noexcept {
    if (!want(1)) return 0;
    return data_[offset_++];
}

uint16_t ByteReader::u16() noexcept {
    if (!want(2)) return 0;
    const uint16_t v =
        static_cast<uint16_t>(data_[offset_]) | static_cast<uint16_t>(data_[offset_ + 1]) << 8;
    offset_ += 2;
    return v;
}

uint32_t ByteReader::u32() noexcept {
    if (!want(4)) return 0;
    const uint32_t v = static_cast<uint32_t>(data_[offset_]) |
                       static_cast<uint32_t>(data_[offset_ + 1]) << 8 |
                       static_cast<uint32_t>(data_[offset_ + 2]) << 16 |
                       static_cast<uint32_t>(data_[offset_ + 3]) << 24;
    offset_ += 4;
    return v;
}

int32_t ByteReader::i32() noexcept {
    const uint32_t raw = u32();
    int32_t out;
    std::memcpy(&out, &raw, sizeof(out));
    return out;
}

uint64_t ByteReader::u64() noexcept {
    const uint64_t low = u32();
    const uint64_t high = u32();
    return low | high << 32;
}

int64_t ByteReader::i64() noexcept {
    const uint64_t raw = u64();
    int64_t out;
    std::memcpy(&out, &raw, sizeof(out));
    return out;
}

float ByteReader::f32() noexcept {
    const uint32_t raw = u32();
    float out;
    std::memcpy(&out, &raw, sizeof(out));
    return out;
}

std::string ByteReader::utf16le(size_t characters) {
    const size_t byte_count = characters * 2;
    if (!want(byte_count)) return {};
    std::string out = utf16le_to_utf8(data_ + offset_, byte_count);
    offset_ += byte_count;
    return out;
}

std::string ByteReader::counted_utf16le() {
    const uint32_t characters = u32();
    if (!ok_) return {};
    return utf16le(characters);
}

namespace {

void append_utf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace

std::string utf16le_to_utf8(const uint8_t* data, size_t byte_count) {
    std::string out;
    out.reserve(byte_count);  // ASCII-dominant data, so this is usually exact

    const size_t units = byte_count / 2;
    for (size_t i = 0; i < units; ++i) {
        const uint32_t unit =
            static_cast<uint32_t>(data[i * 2]) | static_cast<uint32_t>(data[i * 2 + 1]) << 8;

        if (unit >= 0xD800 && unit <= 0xDBFF) {  // high surrogate
            if (i + 1 < units) {
                const uint32_t low = static_cast<uint32_t>(data[(i + 1) * 2]) |
                                     static_cast<uint32_t>(data[(i + 1) * 2 + 1]) << 8;
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    append_utf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
                    ++i;
                    continue;
                }
            }
            append_utf8(out, 0xFFFD);  // unpaired high
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            append_utf8(out, 0xFFFD);  // stray low
        } else {
            append_utf8(out, unit);
        }
    }
    return out;
}

void ByteWriter::u8(uint8_t value) { bytes_.push_back(value); }

void ByteWriter::u16(uint16_t value) {
    bytes_.push_back(static_cast<uint8_t>(value));
    bytes_.push_back(static_cast<uint8_t>(value >> 8));
}

void ByteWriter::u32(uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes_.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void ByteWriter::i32(int32_t value) {
    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    u32(raw);
}

void ByteWriter::u64(uint64_t value) {
    u32(static_cast<uint32_t>(value));
    u32(static_cast<uint32_t>(value >> 32));
}

void ByteWriter::i64(int64_t value) {
    uint64_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    u64(raw);
}

void ByteWriter::f32(float value) {
    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    u32(raw);
}

void ByteWriter::raw(const void* data, size_t count) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + count);
}

void ByteWriter::patch_u32(size_t offset, uint32_t value) {
    if (offset + 4 > bytes_.size()) return;
    for (int i = 0; i < 4; ++i) bytes_[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}

size_t ByteWriter::utf16le(const std::string& text) {
    const std::vector<uint16_t> units = utf8_to_utf16(text);
    for (uint16_t unit : units) u16(unit);
    return units.size();
}

std::vector<uint16_t> utf8_to_utf16(const std::string& text) {
    std::vector<uint16_t> out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        const uint8_t c = static_cast<uint8_t>(text[i]);
        uint32_t cp = 0xFFFD;
        size_t length = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = (c & 0x1Fu) << 6 | (static_cast<uint8_t>(text[i + 1]) & 0x3Fu);
            length = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = (c & 0x0Fu) << 12 | (static_cast<uint8_t>(text[i + 1]) & 0x3Fu) << 6 |
                 (static_cast<uint8_t>(text[i + 2]) & 0x3Fu);
            length = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = (c & 0x07u) << 18 | (static_cast<uint8_t>(text[i + 1]) & 0x3Fu) << 12 |
                 (static_cast<uint8_t>(text[i + 2]) & 0x3Fu) << 6 |
                 (static_cast<uint8_t>(text[i + 3]) & 0x3Fu);
            length = 4;
        }
        i += length;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<uint16_t>(cp));
        }
    }
    return out;
}

}  // namespace tl::formats
