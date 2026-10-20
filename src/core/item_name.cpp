// `CEquipment::getFullItemName` and
// `CEquipment::getItemName`.
#include "core/item_name.h"

#include "core/runic_strings.h"

#include <cctype>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper_ascii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

}  // namespace

const std::string& item_name(const ItemStats& item) {
    // CEquipment::getItemName: `+0x2d0` while `+0x348` is clear
    // and `+0x2d0` is not empty, else `+0x2d8`.
    if (!item.identified && !item.unidentified_name.empty()) {
        return item.unidentified_name;
    }
    return item.base_name;
}

std::string item_full_name(ItemStats& item, bool force_full, const UnitTypeHierarchy& unit_types) {
    // CEquipment::getFullItemName:
    //
    if (!force_full && !item.identified && !item.unidentified_name.empty()) {
        return item.unidentified_name;
    }

    // operate on copy of display_name (+0x210)
    std::string name = item.display_name.empty() ? item.name : item.display_name;
    std::string kept_text;
    const size_t open = name.find('{');
    if (open != std::string::npos) {
        const size_t close = name.find('}', open);
        if (close != std::string::npos && open + 1 < close) {
            kept_text = upper_ascii(name.substr(open + 1, close - open - 1));
            name.erase(open, close - open + 1);
        }
    }

    // an unidentified item asked for its plain name, a unique
    // (ISA 0x36) or a set item (`getSet`) is the name as it
    // stands, with the span removed.
    // INVENTED: the exit for an item without an effect manager (`+0x1b8` null)
    // is not modelled; the port goes on as though it had one, so an
    // item with no affixes still takes its cached parts below.
    if ((!force_full && !item.identified) || unit_types.isa(item.unittype, unit_type::kUnique) ||
        !item.set.empty()) {
        return name;
    }

    // The best words start as the cached parts `+0x2e0` / `+0x2e8`,
    // so with no affixes they are what the name takes.
    std::string best_prefix = item.name_part_2e0;
    std::string best_suffix = item.name_part_2e8;

    // The manager's affix list (`+0x10`) in order: `CBaseUnit::addAffix`
    // appends, as `ItemStats::affixes` does.
    // A socketed gem's affixes are on the gem (`ItemStats::socketed`), not on
    // this list.
    if (!item.affixes.empty()) {
        // One running rank for both words: a prefix needs a rank
        // above it, a suffix only a rank of 0 or more,
        // and either word taken sets it to its affix's rank.
        // An empty word is passed over.
        int best_rank = -1;
        for (const ItemStats::RolledAffix& rolled : item.affixes) {
            const Affix& affix = rolled.affix;
            if (affix.rank > best_rank && !affix.prefix.empty()) {
                best_prefix = affix.prefix;
                best_rank = affix.rank;
            }
            if (affix.rank >= 0 && !affix.suffix.empty()) {
                best_suffix = affix.suffix;
                best_rank = affix.rank;
            }
        }
    }

    // replace [ITEM] in best_prefix
    if (!best_prefix.empty()) {
        name = replace_wstring(best_prefix, "[ITEM]", name);
        item.name_part_2e0 = best_prefix;  // write +0x2e0
    }

    // replace [ITEM] in best_suffix
    if (!best_suffix.empty()) {
        name = replace_wstring(best_suffix, "[ITEM]", name);
        item.name_part_2e8 = best_suffix;  // write +0x2e8
    }

    // second brace loop
    if (!kept_text.empty()) {
        while (true) {
            const size_t tag_open = name.find('{');
            if (tag_open == std::string::npos) break;
            const size_t tag_close = name.find('}', tag_open);
            if (tag_close == std::string::npos) break;

            const std::string tag = name.substr(tag_open + 1, tag_close - tag_open - 1);
            const std::string tag_upper = upper_ascii(tag);

            if (tag_upper == kept_text) {
                // is L"{/". Find "{/" + tag
                const std::string close_needle = "{/" + tag;
                const size_t close_pos = name.find(close_needle, tag_close);
                if (close_pos == std::string::npos) break;
                // find '}' after close_pos. Not tested against -1:
                // the length `close_end - close_pos + 1` then goes negative,
                // and `replace` clamps it, erasing to the end.
                const size_t close_end = name.find('}', close_pos + 1);

                // erase closing tag first
                if (close_end == std::string::npos) {
                    name.erase(close_pos);
                } else {
                    name.erase(close_pos, close_end - close_pos + 1);
                }
                // erase opening tag
                name.erase(tag_open, tag_close - tag_open + 1);
            } else {
                // mismatched tag. The loop ends when the tag's `}`
                // sits in the last two characters; else the next '}' after it.
                if (tag_close >= name.size() - 2) break;
                const size_t next_close = name.find('}', tag_close + 1);
                if (next_close != std::string::npos) {
                    name.erase(tag_open, next_close - tag_open + 1);
                } else {
                    break;
                }
            }
        }
    }

    return name;
}

}  // namespace tl::core
