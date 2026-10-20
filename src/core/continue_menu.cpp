#include "core/continue_menu.h"

#include <cmath>

#include "core/level_memory.h"

namespace tl::core {

namespace {

/// `std::wstring::size()` on the console, where `wchar_t` is 16 bits: one for a
/// character of the basic plane, two above it.
// PORT: of the port's UTF-8.
size_t utf16_length(const std::string& text) {
    size_t units = 0;
    for (size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        size_t bytes = 1;
        if (lead >= 0xf0) bytes = 4;
        else if (lead >= 0xe0) bytes = 3;
        else if (lead >= 0xc0) bytes = 2;
        units += bytes == 4 ? 2 : 1;
        i += bytes;
    }
    return units;
}

/// `std::wstring::assign(text, 0, units)` on the console's 16-bit units: the
/// bytes of the first `units` units (a cut through a surrogate pair drops the
/// whole character here).
// PORT: of the port's UTF-8.
std::string utf16_prefix(const std::string& text, size_t units) {
    size_t taken = 0;
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        size_t bytes = 1;
        if (lead >= 0xf0) bytes = 4;
        else if (lead >= 0xe0) bytes = 3;
        else if (lead >= 0xc0) bytes = 2;
        const size_t cost = bytes == 4 ? 2 : 1;
        if (taken + cost > units) break;
        taken += cost;
        i += bytes;
    }
    return text.substr(0, i);
}

}  // namespace

// INVENTED: the health the Continue list shows for a saved character, its saved
// figure, a retired one's and a locked trial one's; the console's list is not
// transcribed here.
float continue_listed_hp(float saved_hp, bool retired, int32_t level, bool full_game) {
    float hp = saved_hp;
    if (retired) hp = kContinueRetiredHp;
    if (!full_game && level > 7) hp = kContinueLockedHp;
    return hp;
}

std::string continue_play_time(float play_seconds) {
    // `ceilf`, `fctidz`, then 32-bit divisions of the low word.
    const uint64_t whole = static_cast<uint64_t>(std::ceil(play_seconds));
    const uint32_t low = static_cast<uint32_t>(whole);
    const uint32_t minutes_all = low / 60u;
    const uint32_t hours = minutes_all / 60u;
    const int32_t seconds = static_cast<int32_t>(low - minutes_all * 60u);
    const uint32_t minutes = minutes_all % 60u;
    return std::to_string(hours) + "hrs" + " " + std::to_string(minutes) + "mins" + " " +
           std::to_string(seconds) + "secs";
}

void ContinueMenu::reload(std::vector<ContinueEntry> entries) {
    // CContinueGameMenu::reloadFiles: the
    // game's current file forgotten, the scroll to 0, the lists refilled.
    current_file_.clear();
    top_ = 0;
    entries_ = std::move(entries);
}

// PORT: an accessor of the Continue list.
const ContinueEntry* ContinueMenu::selected_entry() const {
    if (selected_ < 0 || selected_ >= count()) return nullptr;
    return &entries_[static_cast<size_t>(selected_)];
}

bool ContinueMenu::can_continue() const {
    // CContinueGameMenu::canContinue. The
    // console's answer also wants a storage device,
    // which is the platform's.
    const ContinueEntry* entry = selected_entry();
    return entry != nullptr && 0.0f < entry->hp;
}

bool ContinueMenu::select_character(int64_t offset, bool force,
                                    const std::vector<std::string>& mods, ContinuePress& out) {
    // CContinueGameMenu::selectCharacter
    current_file_.clear();
    const int32_t index =
        static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(top_) + offset));
    if (selected_ == index && !force) {
        // The original names the file with no test of the index; an index
        // past the list names nothing here.
        if (const ContinueEntry* entry = selected_entry()) current_file_ = entry->path;
        return count() != 0;
    }
    selected_ = index;
    if (index < count()) {
        // The index is compared signed: a negative one passes and
        // the original then reads before the list. Nothing here.
        if (const ContinueEntry* entry = selected_entry()) {
            out.apply_character = true;
            current_file_ = entry->path;
        }
    }
    bool warning = false;
    if (const ContinueEntry* entry = selected_entry()) {
        if (entry->mods.size() == mods.size()) {
            for (size_t i = 0; i < entry->mods.size(); ++i) {
                if (upper_ascii(entry->mods[i]) != upper_ascii(mods[i])) {
                    warning = true;
                    break;
                }
            }
        } else {
            warning = true;
        }
    }
    mods_warning_ = warning;
    return count() != 0;
}

ContinuePress ContinueMenu::open(std::vector<ContinueEntry> entries,
                                 const std::vector<std::string>& mods) {
    // CContinueGameMenu::setOpen
    ContinuePress out;
    reload(std::move(entries));
    open_ = true;
    delete_confirm_ = false;
    select_character(0, true, mods, out);
    return out;
}

void ContinueMenu::close() {
    // `setOpen(false)`: `DeleteConfirm` is hidden either way;
    // `DeadConfirm` is left as it stands.
    open_ = false;
    delete_confirm_ = false;
}

void ContinueMenu::move_up() {
    //
    const uint32_t was = static_cast<uint32_t>(selected_);
    bool scroll = was == 0;
    if (!scroll) {
        selected_ = static_cast<int32_t>(was - 1);
        scroll = was - static_cast<uint32_t>(top_) == 0;
    }
    if (scroll) {
        const int64_t top = static_cast<int64_t>(static_cast<uint32_t>(top_)) - 1;
        top_ = static_cast<int32_t>(top);
        if (top < 0) top_ = 0;
    }
}

void ContinueMenu::move_down() {
    //
    const int32_t was = selected_;
    if (was != count() - 1) {
        selected_ = was + 1;
        if (was - top_ != 4) return;
    }
    const int32_t next = top_ + 1;
    top_ = next;
    const int32_t limit = count() - kContinueRows;
    if (limit < next) {
        top_ = limit;
        if (limit < 0) top_ = 0;
    }
}

ContinuePress ContinueMenu::press(PadButton button, bool bumpers_held, bool loading,
                                  const std::vector<std::string>& mods) {
    // CContinueGameMenu::vfunc_12, the pad's events.
    ContinuePress out;
    out.handled = true;
    const bool deleting = delete_confirm_;
    const bool dead = dead_confirm_;
    switch (button) {
        case PadButton::DPadUp:
        case PadButton::LeftStickUp:
            if (!deleting && !dead) move_up();
            break;
        case PadButton::DPadDown:
        case PadButton::LeftStickDown:
            if (!deleting && !dead) move_down();
            break;
        case PadButton::A: {
            if (dead) {
                dead_confirm_ = false;
                break;
            }
            if (deleting) {
                delete_confirm_ = false;
                // CContinueGameMenu::deleteCharacter,
                // its first half: the file under
                // the save path is removed. The rest is `finish_delete`.
                if (const ContinueEntry* entry = selected_entry()) {
                    out.delete_file = true;
                    out.delete_path = entry->path;
                } else if (count() == 0) {
                    out.closed = true;
                    out.request = GameStateRequest{0, 0};
                    close();
                }
                break;
            }
            // The original reads the selected state with no test of the
            // index; nothing selected is nothing to start here.
            const ContinueEntry* entry = selected_entry();
            if (entry == nullptr) break;
            const float hp = entry->hp;
            if (0.0f < hp) {
                if (bumpers_held) out.at_level_start = true;
                out.apply_character = true;
                current_file_ = entry->path;
                out.closed = true;
                out.request = GameStateRequest{2, 0};
                close();
                break;
            }
            if (hp == 0.0f) {
                dead_confirm_ = true;
                break;
            }
            if (0.0f <= hp) break;
            out.play_error = true;
            break;
        }
        case PadButton::B:
            if (dead) {
                dead_confirm_ = false;
                break;
            }
            if (deleting) {
                delete_confirm_ = false;
                break;
            }
            out.closed = true;
            out.request = GameStateRequest{0, 0};
            close();
            break;
        case PadButton::X:
            if (dead) {
                dead_confirm_ = false;
                break;
            }
            if (deleting || loading) break;
            select_character(static_cast<int64_t>(static_cast<uint32_t>(selected_)) -
                                 static_cast<int64_t>(static_cast<uint32_t>(top_)),
                             true, mods, out);
            break;
        case PadButton::Y:
            if (dead) dead_confirm_ = false;
            if (!deleting) delete_confirm_ = true;
            break;
        default: break;
    }
    return out;
}

ContinuePress ContinueMenu::finish_delete(std::vector<ContinueEntry> entries,
                                          const std::vector<std::string>& mods) {
    // CContinueGameMenu::deleteCharacter.
    //
    ContinuePress out;
    out.handled = true;
    selected_ = selected_ - 1;
    if (selected_ < 0) selected_ = 0;
    reload(std::move(entries));
    select_character(static_cast<int64_t>(static_cast<uint32_t>(selected_)), true, mods, out);
    if (count() == 0) {
        out.closed = true;
        out.request = GameStateRequest{0, 0};
        close();
    }
    return out;
}

int ContinueMenu::highlight_row() const {
    // the row whose number is `selected - top`, unsigned.
    const uint32_t row = static_cast<uint32_t>(selected_) - static_cast<uint32_t>(top_);
    return row < static_cast<uint32_t>(kContinueRows) ? static_cast<int>(row) : -1;
}

std::array<ContinueRow, kContinueRows> ContinueMenu::rows() const {
    // CContinueGameMenu::updateCharacterList
    std::array<ContinueRow, kContinueRows> rows;
    for (int32_t index = top_; index < top_ + kContinueRows; ++index) {
        ContinueRow& row = rows[static_cast<size_t>(index - top_)];
        if (index < 0 || index >= count()) continue;
        const ContinueEntry& entry = entries_[static_cast<size_t>(index)];
        row.shown = true;

        std::string name;
        row.name_colour = kContinueColourLiving;
        if (entry.hardcore) row.name_colour = kContinueColourHardcore;
        if (entry.hp == kContinueRetiredHp) {
            name = std::string("(") + "Retired" + ")";
            row.name_colour = kContinueColourRetired;
        } else if (entry.hp <= 0.0f) {
            row.name_colour = kContinueColourDead;
        }
        name += entry.name;
        if (utf16_length(name) > 0xf) name = utf16_prefix(name, 0xf) + "...";
        row.name = name;

        std::string text = std::string("Level") + ":" + std::to_string(entry.level);
        if (!entry.hardcore) {
            if (entry.hp == kContinueLockedHp) text.append(" - ").append("Locked");
        } else if (0.0f < entry.hp) {
            text.append(" - ").append("Hardcore");
        } else {
            text.append(" - ").append("Dead");
        }
        text += "\n";
        text.append(" ").append("Difficulty").append(":");
        switch (entry.difficulty) {
            case 0: text += "Easy"; break;
            case 1: text += "Normal"; break;
            case 2: text += "Hard"; break;
            case 3: text += "Very Hard"; break;
            default: break;
        }
        text += "\n";
        row.description = text;
    }
    return rows;
}

}  // namespace tl::core
