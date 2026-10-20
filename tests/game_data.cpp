#include "roundtrip.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace tl::test {

std::vector<uint8_t> load_game_file_or_skip(const std::string& relative_path) {
    // TORCHLIGHT_GAME_DATA points the tests at an install that is not in the
    // default location.
    const char* env = std::getenv("TORCHLIGHT_GAME_DATA");
    const std::string root = env ? env : "game_data";

    const std::filesystem::path full = std::filesystem::path(root) / relative_path;

    std::ifstream f(full, std::ios::binary);
    if (!f) {
        TL_SKIP("no game data at '" + full.string() +
                "' - supply your own Torchlight install to run this test");
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

}  // namespace tl::test
