// `.adm` — Torchlight's compiled game-data records: the binary form of the DAT schema
// (`<TYPE>KEY:VALUE` inside `[SECTION]` blocks).
// No dependencies beyond the standard library: no file I/O, no OGRE, no logging.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::formats {

/// Value types, as they appear in the file. A `Guid` record is 16 bytes, every other
/// 12; getting that wrong misreads the rest of the section silently.
enum class AdmType : uint32_t {
    Integer = 1,
    Float = 2,
    Unsigned = 4,  ///< the source's `<UNSIGNED INT>`; `RESOURCEGROUP`, `UNIQUE_GUID`
    String = 5,
    Bool = 6,
    Guid = 7,  ///< signed 64-bit; the record is 16 bytes, not 12

    /// The source's `<TRANSLATE>`: a localised string, stored exactly like a `String`
    /// (an id into the file's string table). `string_or()` returns either.
    Translate = 8,
};

struct AdmRecord {
    std::string key;
    AdmType type = AdmType::Integer;

    uint32_t integer = 0;  ///< Integer, Unsigned, Bool
    float number = 0.0f;   ///< Float
    std::string text;      ///< String, Translate
    int64_t guid = 0;      ///< Guid

    /// The raw value word as a console reader sees it, whatever the type, after
    /// `CDataGroup::GetDataValue(group, name)` (e.g.
    /// `CAIStatWatcher::CAIStatWatcher`). The word is big-endian.
    /// PORT: a string's word is an id the port does not carry; both read 0 for one.
    float word_as_float() const;
    uint8_t word_first_byte() const;
    /// `*(int *)(value + 8)`: the word as an integer (`CPlayer::unitInit`
    /// reads `STRENGTH` so).
    uint32_t word_as_integer() const;
};

/// One `[SECTION]` and everything inside it. Sections nest arbitrarily.
struct AdmSection {
    std::string name;
    std::vector<AdmRecord> records;
    std::vector<AdmSection> children;

    const AdmRecord* find(const std::string& key) const;

    /// Typed lookups that fall back rather than throwing; a missing key is normal.
    uint32_t integer_or(const std::string& key, uint32_t fallback) const;
    float float_or(const std::string& key, float fallback) const;
    /// A `String` **or** a `Translate`; see AdmType::Translate.
    std::string string_or(const std::string& key, const std::string& fallback) const;
    bool bool_or(const std::string& key, bool fallback) const;
    int64_t guid_or(const std::string& key, int64_t fallback) const;

    /// An 0-255 colour triple stored as three `<prefix> RED/GREEN/BLUE` records,
    /// returned as 0..1 floats. False when none of the three is present, so a
    /// caller can tell "absent" from "black".
    bool colour(const std::string& prefix, float& r, float& g, float& b) const;

    /// Depth-first walk over this section and everything under it.
    void visit(const std::function<void(const AdmSection&)>& fn) const;
};

struct AdmFile {
    AdmSection root;

    /// Bytes the reader did not consume; non-zero is an unseen shape, reported.
    size_t undecoded_bytes = 0;

    bool ok = false;
    std::string error;

    /// Convenience for the common case of reading the root section.
    const std::string& section() const { return root.name; }
    const std::vector<AdmRecord>& records() const { return root.records; }
    uint32_t integer_or(const std::string& k, uint32_t f) const { return root.integer_or(k, f); }
    float float_or(const std::string& k, float f) const { return root.float_or(k, f); }
    std::string string_or(const std::string& k, const std::string& f) const {
        return root.string_or(k, f);
    }
    bool bool_or(const std::string& k, bool f) const { return root.bool_or(k, f); }
    bool colour(const std::string& p, float& r, float& g, float& b) const {
        return root.colour(p, r, g, b);
    }
    void visit(const std::function<void(const AdmSection&)>& fn) const { root.visit(fn); }
};

/// Parse a `.adm` from bytes the caller owns. Never throws.
AdmFile parse_adm(const uint8_t* data, size_t size);

}  // namespace tl::formats
