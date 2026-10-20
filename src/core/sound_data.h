// The game's sound definitions: `CSoundBankDataInformation`, the table every `CSoundBank`
// looks its samples up in, by `NAME` (unit and interface banks) or by `GUID` (layout `Sound`
// nodes, particles). `CSoundBankDataInformation::reload` reads them once.
#pragma once

#include "formats/adm.h"
#include "formats/cmp.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tl::core {

/// One definition: `CSoundData`, as `CSoundBankDataInformation::createSoundData`
/// fills it.
struct SoundData {
    std::string name;                  ///< +0x10 `NAME`, upper-cased
    std::string category;              ///< +0x18 `CATEGORY`, upper-cased
    int64_t guid = -1;                 ///< +0x20 `GUID`
    float volume = 1.0f;               ///< +0x28 `VOLUME`
    bool loops = false;                ///< +0x2c `LOOPS`
    float frequency_variation = 0.0f;  ///< +0x30 `FREQUENCYVARIATION`
    float volume_variation = 0.0f;     ///< +0x34 `VOLUMEVARIATION`
    float radius = 0.0f;               ///< +0x38 `RADIUS`
    /// +0x3c `PLAYCHANCE`, -1 when unauthored: `playSample` does not roll.
    int32_t play_chance = -1;
    /// +0x40: every value of the `[FILES]` group, whatever its key, in order.
    /// Never empty.
    std::vector<std::string> files;
};

/// `CSoundBank::addSample(slot, guid)`: which of a definition's recordings
/// a bank keeps: one of one, else `ceil(count * 0.2f)`, drawn with `g_Rand` at 100.
/// Quirk kept: it lists the loop counter, not the draw, so the
/// first recordings are kept and the draws only decide how many.
std::vector<size_t> bank_recordings(const SoundData& data);

/// `CSoundBankDataInformation::createSoundData(CDataGroup*)`: a definition, or
/// nothing for a group with no `[FILES]` values or no `GUID`.
std::optional<SoundData> create_sound_data(const tl::formats::AdmSection& group);

/// `CSoundBankDataInformation`
/// (`CSoundBankDataInformation::CSoundBankDataInformation(CResourceSettings*)`);
/// its files, `MEDIA/SOUNDS`, are fed in by the caller.
class SoundDataInformation {
public:
    /// One file of `CSoundBankDataInformation::parseSoundData`.
    /// A known GUID keeps its first definition; a name repeated in a category
    /// files the later one.
    void parse_file(const tl::formats::AdmSection& root);

    /// `CSoundBankDataInformation::getSoundDataObject(const std::wstring&)`:
    /// the categories in first-seen order; `name` exact
    /// (the caller upper-cases).
    const SoundData* find(const std::string& name) const;

    /// `CSoundBankDataInformation::getSoundDataObject(category, name)`:
    /// the one category's.
    const SoundData* find(const std::string& category, const std::string& name) const;

    /// `CSoundBankDataInformation::getSoundDataObjectByGuid`.
    const SoundData* find_by_guid(int64_t guid) const;

    /// `CSoundBankDataInformation::getSoundDataObjectByGuidThatIsClosest`:
    /// the GUID's own, else the first in GUID order sharing the most
    /// leading bits (the lowest when none shares any).
    const SoundData* find_closest_guid(int64_t guid) const;

    /// `CSoundBankDataInformation::getCategoryNameByID`.
    const std::string* category_name(size_t id) const;

    /// `CSoundBankDataInformation::getSoundNameByCategoryIndex(category,
    /// index)`: the `index`-th name in the category, in
    /// name order, or empty (`CSoundObject::setSoundBankNameIndex`).
    std::string sound_name_by_category_index(size_t category, size_t index) const;

    // PORT: an accessor of the port's own bookkeeping.
    size_t size() const { return all_.size(); }
    /// The `index`-th definition in parse order (X360 `+0x38`).
    const SoundData& at(size_t index) const { return *all_[index]; }
    size_t category_count() const { return categories_.size(); }
    void clear();

private:
    std::vector<std::unique_ptr<SoundData>> all_;  ///< X360 +0x38, in parse order
    std::map<int64_t, const SoundData*> by_guid_;  ///< X360 +0x28
    std::vector<std::string> categories_;          ///< X360 +0x8
    /// X360 +0x18: by category index, then by name.
    std::map<size_t, std::map<std::string, const SoundData*>> by_category_;
};

/// Whether making `scene`'s objects adds a sound sample, which leaves `g_Rand` its low word:
/// a `Sound` node (`CSoundObjectDescriptor::Set_setSoundBankGuidByString`,
/// `STRINGS::GetInt64`) whose GUID names a definition (`CSoundObject::setSoundBankGuid`
/// > `CSoundBank::addSample`).
bool layout_adds_sound_sample(const tl::formats::CmpScene& scene,
                              const SoundDataInformation& sounds);

}  // namespace tl::core
