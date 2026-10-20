// Fixed starting points for the game's generators, for tests only: the game seeds `g_Rand`
// and `g_RandVolatile` from the clock, so a test of drawn figures puts them there itself.
#pragma once

#include "core/random.h"

namespace tl::test {

inline constexpr int32_t kTestVolatileSeed = 0x5eed7071;
inline constexpr int32_t kTestGameSeed = 0x1e5eed2a;
inline constexpr uint32_t kTestCrtSeed = 0x7a11c0deu;

/// Both Runic generators and `rand()` at known states, standing for the clock
/// a level load reads.
inline void seed_generators() {
    tl::core::volatile_random().set_seed(kTestVolatileSeed);
    tl::core::game_random().set_seed(kTestGameSeed);
    tl::core::crt_random().seed_with(kTestCrtSeed);
}

}  // namespace tl::test
