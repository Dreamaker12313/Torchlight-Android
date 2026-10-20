// Round-trip harness: parse bytes into a struct, re-serialise it, compare byte for byte.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "test_support.h"

namespace tl::test {

/// Report the first differing byte, with a little context. A bare "not equal"
/// is nearly useless when debugging a binary format.
inline void expect_bytes_equal(const std::vector<uint8_t>& original,
                               const std::vector<uint8_t>& rewritten, const std::string& what) {
    const size_t n = std::min(original.size(), rewritten.size());
    for (size_t i = 0; i < n; ++i) {
        if (original[i] != rewritten[i]) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s: first difference at offset %zu (0x%zx): "
                          "original 0x%02x, rewritten 0x%02x",
                          what.c_str(), i, i, original[i], rewritten[i]);
            throw Failure{buf};
        }
    }
    if (original.size() != rewritten.size()) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "%s: identical for %zu bytes but lengths differ - "
                      "original %zu, rewritten %zu",
                      what.c_str(), n, original.size(), rewritten.size());
        throw Failure{buf};
    }
}

/// Parse then re-serialise, and require the bytes to match.
///
///   round_trip<AdmFile>(bytes, "BOSS_RESISTS.DAT.adm",
///                       AdmFile::parse, AdmFile::serialise);
template <typename T, typename ParseFn, typename SerialiseFn>
void round_trip(const std::vector<uint8_t>& original, const std::string& what, ParseFn parse,
                SerialiseFn serialise) {
    T value = parse(original.data(), original.size());
    const std::vector<uint8_t> rewritten = serialise(value);
    expect_bytes_equal(original, rewritten, what);
}

/// Load a file from the user's game data, or skip the test if it is absent.
std::vector<uint8_t> load_game_file_or_skip(const std::string& relative_path);

}  // namespace tl::test
