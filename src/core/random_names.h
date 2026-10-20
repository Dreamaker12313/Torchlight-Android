// `CRandomNames`: champion names from `media/RANDOMNAMES.DAT`, for quests
// (`CQuestUnitData::getBaseUnitDataGroup`) and levels
// (`CLevel::populateSectionOfLevel`, `CCharacter::unitInit`).
//
#pragma once

#include "core/random.h"
#include "core/randomizer.h"
#include "formats/adm.h"

#include <string>
#include <vector>

namespace tl::core {

class RandomNames {
public:
    /// One `[NAME]` group (0x48 bytes in the original).
    struct Set {
        std::vector<std::string> prefixes;  ///< +0x8 (count +0x10), `PREFIX`
        std::vector<std::string> suffixes;  ///< +0x20 (count +0x28), `SUFFIX`
        std::string center = "'";           ///< +0x38 `CENTER`, default `'`
        int32_t center_chance = 0;          ///< +0x40 `CENTERCHANCE`, default 0
    };

    /// `CRandomNames::load(const wchar_t*)`: each `[NAME]`'s
    /// `WEIGHT` (default 1) in file order, then the `[TITLE]` (`CHANCE` default 100).
    void load(const tl::formats::AdmSection& root);

    bool empty() const { return sets_.empty(); }
    const std::vector<Set>& sets() const { return sets_; }
    const std::vector<std::string>& titles() const { return title_suffixes_; }
    const std::vector<std::string>& title_prefixes() const { return title_prefixes_; }
    const Randomizer& picker() const { return picker_; }
    int32_t title_chance() const { return title_chance_; }

    /// `CRandomNames::generateName(bool)`. Draws on `rng` in
    /// order: set (`CRandomizer::getRandom`), prefix, suffix, centre roll (if the set has a
    /// chance), title roll and pick. The centre goes in when its 0..100 roll <= the chance.
    std::string generate_name(bool with_title, RunicRandom& rng) const;

private:
    std::vector<Set> sets_;
    /// `+0x50`. `getRandom` only recomputes its table, so a name can be drawn
    /// from a const book.
    mutable Randomizer picker_;
    int32_t title_chance_ = 100;               ///< +0x58
    std::vector<std::string> title_prefixes_;  ///< +0x20, read and never used
    std::vector<std::string> title_suffixes_;  ///< +0x38 (count +0x40)
};

}  // namespace tl::core
