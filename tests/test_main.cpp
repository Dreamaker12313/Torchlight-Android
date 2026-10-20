#include "test_support.h"

#include "core/random.h"
#include "core/script.h"

#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

namespace tl::test {

namespace {

/// Puts the engine's process-wide state back as a fresh process has it. The game carries it
/// over on purpose (`UpdateTimer`, the two generators), so only the runner resets it.
void reset_process_state() {
    tl::core::LevelScript::reset_near_list_timer();
    tl::core::volatile_random() = tl::core::RunicRandom(0);
    tl::core::game_random() = tl::core::RunicRandom(0);
}

}  // namespace

int run_all(const char* filter, unsigned shuffle_seed) {
    int passed = 0, failed = 0, skipped = 0;

    std::vector<Case> cases = registry();
    if (shuffle_seed != 0) {
        // std::shuffle's use of mt19937 is unspecified; our own Fisher-Yates keeps one order
        // per seed on every standard library.
        std::mt19937 rng(shuffle_seed);
        for (size_t i = cases.size(); i > 1; --i) {
            const size_t j = static_cast<size_t>(rng() % i);
            std::swap(cases[i - 1], cases[j]);
        }
        std::printf("shuffled with seed %u\n", shuffle_seed);
    }

    for (const Case& c : cases) {
        if (filter != nullptr && c.name.find(filter) == std::string::npos) continue;
        reset_process_state();
        try {
            c.fn();
            std::printf("[ PASS ] %s\n", c.name.c_str());
            ++passed;
        } catch (const Skip& s) {
            std::printf("[ SKIP ] %s - %s\n", c.name.c_str(), s.reason.c_str());
            ++skipped;
        } catch (const Failure& f) {
            std::printf("[ FAIL ] %s\n         %s\n", c.name.c_str(), f.message.c_str());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", c.name.c_str(),
                        e.what());
            ++failed;
        }
    }

    std::printf("\n%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    // A filter that matches nothing is a typo, not a pass.
    if (filter != nullptr && passed + failed + skipped == 0) return 1;
    return failed == 0 ? 0 : 1;
}

}  // namespace tl::test

// `tl_tests [--shuffle <seed>] [name]`: the tests whose name contains `name`, in an order
// drawn from `seed` (a test that passes only after another has run is wrong).
int main(int argc, char** argv) {
    const char* filter = nullptr;
    unsigned seed = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shuffle") == 0 && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else {
            filter = argv[i];
        }
    }
    return tl::test::run_all(filter, seed);
}
