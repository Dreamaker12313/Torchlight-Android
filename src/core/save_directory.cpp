#include "core/save_directory.h"

#include "core/log.h"
#include "core/platform.h"

#include <dirent.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace tl::core {

// Where characters are saved: `FILESYSTEM::GetSaveDataPath`. Each target's own
// writable place, like the log.
std::string save_directory() { return platform_data_root() + "save/"; }

bool read_whole_file(const std::string& path, std::vector<uint8_t>& bytes) {
    std::FILE* in = std::fopen(path.c_str(), "rb");
    if (in == nullptr) return false;
    bytes.clear();
    uint8_t chunk[65536];
    size_t got = 0;
    while ((got = std::fread(chunk, 1, sizeof(chunk), in)) > 0) {
        bytes.insert(bytes.end(), chunk, chunk + got);
    }
    std::fclose(in);
    return true;
}

// The settings the Options screen keeps (`CSettings`; the console's file is on
// its storage device, this one beside the saves). A missing file leaves the
// defaults.
void load_game_settings(const std::string& path, GameSettings& settings) {
    std::FILE* in = std::fopen(path.c_str(), "rb");
    if (in == nullptr) return;
    std::string text;
    char buffer[512];
    size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, in)) > 0) text.append(buffer, got);
    std::fclose(in);
    read_game_settings(text, settings);
    TL_LOG_INFO("settings: read '%s'", path.c_str());
}

void make_save_directory() {
    std::string directory = save_directory();
    if (!directory.empty() && directory.back() == '/') directory.pop_back();
#if defined(_WIN32)
    _mkdir(directory.c_str());
#else
    mkdir(directory.c_str(), 0777);
#endif
}

// The save files, newest first: `CContinueGameMenu::reloadFiles`
// lists `*.svt` and sorts it with
// `FileTimeSorter`.
std::vector<std::string> list_saves() {
    std::vector<std::pair<long long, std::string>> found;
    if (DIR* dir = opendir(save_directory().c_str())) {
        while (dirent* entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name.size() <= 4 || name.compare(name.size() - 4, 4, ".svt") != 0) continue;
            const std::string path = save_directory() + name;
            struct stat info;
            if (stat(path.c_str(), &info) != 0) continue;
            found.emplace_back(static_cast<long long>(info.st_mtime), path);
        }
        closedir(dir);
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::string> out;
    out.reserve(found.size());
    for (const auto& entry : found) out.push_back(entry.second);
    return out;
}

// `CNewGameMenu::getEmptySave`: the lowest
// number no save file is named after, then `.svt`.
std::string empty_save_path() {
    std::vector<int> taken;
    for (const std::string& path : list_saves()) {
        const size_t slash = path.find_last_of("/\\");
        const std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
        taken.push_back(std::atoi(name.substr(0, name.size() - 4).c_str()));
    }
    int number = 0;
    for (bool clear = false; !clear;) {
        clear = true;
        for (int used : taken) {
            if (number == used) {
                ++number;
                clear = false;
            }
        }
    }
    for (;; ++number) {
        const std::string path = save_directory() + std::to_string(number) + ".svt";
        struct stat info;
        if (stat(path.c_str(), &info) != 0) return path;
    }
}

// `CContinueGameMenu::canContinue` for the
// newest save: a character whose saved hit points are above 0. Fills `file` and
// `path` when there is one.
bool newest_living_save(tl::formats::CharacterFile& file, std::string& path) {
    const std::vector<std::string> saves = list_saves();
    if (saves.empty()) return false;
    std::vector<uint8_t> bytes;
    if (!read_whole_file(saves.front(), bytes)) return false;
    tl::formats::CharacterFile read;
    if (!tl::formats::read_character_file(bytes.data(), bytes.size(), read)) return false;
    // `CContinueGameMenu::reloadFiles`: a retired character is
    // listed with HP -999, which `canContinue`
    // refuses.
    if (read.retired != 0) read.player.hp = -999.0f;
    if (!(0.0f < read.player.hp)) return false;
    file = std::move(read);
    path = saves.front();
    return true;
}

// The save directory as Load Character lists it: `CContinueGameMenu::reloadFiles`,
// in `list_saves()`'s order, unreadable
// files passed over. The trial-version check is not applied.
std::vector<ContinueEntry> list_continue_entries() {
    std::vector<ContinueEntry> entries;
    for (const std::string& path : list_saves()) {
        std::vector<uint8_t> bytes;
        tl::formats::CharacterFile file;
        if (!read_whole_file(path, bytes) ||
            !tl::formats::read_character_file(bytes.data(), bytes.size(), file)) {
            continue;
        }
        ContinueEntry entry;
        entry.path = path;
        entry.class_name = file.class_name;
        entry.name = file.player.name;
        entry.level = file.player.level;
        entry.hp = continue_listed_hp(file.player.hp, file.retired != 0, file.player.level,
                                      /*full_game=*/true);
        entry.play_seconds = file.player.clock;
        entry.hardcore = file.hardcore != 0;
        entry.difficulty = file.difficulty;
        entry.mods = file.player.mods;
        entries.push_back(std::move(entry));
    }
    return entries;
}

// The save `menu_backdrop` reads: `from`, or the newest. Empty with none.
std::string menu_backdrop_file(const std::string& from) {
    if (!from.empty()) return from;
    const std::vector<std::string> saves = list_saves();
    return saves.empty() ? std::string() : saves.front();
}

}  // namespace tl::core
