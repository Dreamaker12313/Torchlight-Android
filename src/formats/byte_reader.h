// Bounds-checked sequential reader over a borrowed byte range; standard library only.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

/// Result of a read attempt. Parsers check `ok()` rather than throwing, so that
/// malformed game data produces a diagnosable failure instead of a crash.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) noexcept : data_(data), size_(size) {}

    /// Bytes consumed so far.
    size_t offset() const noexcept { return offset_; }
    size_t size() const noexcept { return size_; }
    size_t remaining() const noexcept { return ok_ ? size_ - offset_ : 0; }

    /// False once any read has run past the end. Sticky: one bad read poisons
    /// the reader so callers can check once at the end rather than every step.
    bool ok() const noexcept { return ok_; }

    bool seek(size_t absolute) noexcept;
    bool skip(size_t count) noexcept;

    uint8_t u8() noexcept;
    uint16_t u16() noexcept;  ///< little-endian
    uint32_t u32() noexcept;  ///< little-endian
    int32_t i32() noexcept;   ///< little-endian
    uint64_t u64() noexcept;  ///< little-endian
    int64_t i64() noexcept;   ///< little-endian
    float f32() noexcept;     ///< little-endian IEEE-754

    /// Raw bytes. Returns false and consumes nothing if fewer than `count`
    /// remain.
    bool bytes(void* dest, size_t count) noexcept;

    /// UTF-16LE string of exactly `characters` code units, transcoded to UTF-8.
    /// Runic's formats store a uint32 character count (not a byte count) before it.
    std::string utf16le(size_t characters);

    /// Convenience: a uint32 character count followed by that many UTF-16LE
    /// code units. This is the shape used throughout .adm and .cmp.
    std::string counted_utf16le();

private:
    bool want(size_t count) noexcept;

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t offset_ = 0;
    bool ok_ = true;
};

/// Sequential little-endian writer into a growing buffer; the twin of
/// ByteReader for the formats this engine writes (the character save file).
class ByteWriter {
public:
    const std::vector<uint8_t>& bytes() const noexcept { return bytes_; }
    std::vector<uint8_t>& bytes() noexcept { return bytes_; }
    size_t size() const noexcept { return bytes_.size(); }

    void u8(uint8_t value);
    void u16(uint16_t value);
    void u32(uint32_t value);
    void i32(int32_t value);
    void u64(uint64_t value);
    void i64(int64_t value);
    void f32(float value);
    void raw(const void* data, size_t count);

    /// Overwrite four bytes already written, at `offset`.
    void patch_u32(size_t offset, uint32_t value);

    /// UTF-8 text as UTF-16LE code units, no count and no terminator. Returns
    /// the number of units written.
    size_t utf16le(const std::string& text);

private:
    std::vector<uint8_t> bytes_;
};

/// UTF-8 to UTF-16LE code units, surrogate pairs for anything past the BMP.
std::vector<uint16_t> utf8_to_utf16(const std::string& text);

/// Transcode UTF-16LE code units to UTF-8, handling surrogate pairs.
/// Unpaired surrogates are replaced with U+FFFD rather than rejected, because
/// shipped game data is not guaranteed to be well-formed.
std::string utf16le_to_utf8(const uint8_t* data, size_t byte_count);

}  // namespace tl::formats
