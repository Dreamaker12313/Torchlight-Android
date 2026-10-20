// Shared by tl_fuzz's two kinds of target: the byte-level readers
// (fuzz_formats.cpp) and the structured ones (fuzz_loaders.cpp), which build
// a parsed tree or scene at random instead of corrupting bytes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tl::fuzz {

/// xorshift64*: deterministic, so a failing run replays from its seed.
struct Rng {
    uint64_t s;
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 0x2545F4914F6CDD1Dull;
    }
    size_t below(size_t n) { return n == 0 ? 0 : static_cast<size_t>(next() % n); }
    bool one_in(size_t n) { return below(n) == 0; }
};

/// One input built from `rng` and handed to the code under test. Returns a digest of what
/// came out (counts, float bits), the same on every machine for one seed.
struct StructuredTarget {
    const char* name;
    std::function<size_t(Rng&)> step;
};

std::vector<StructuredTarget> structured_targets();

}  // namespace tl::fuzz
