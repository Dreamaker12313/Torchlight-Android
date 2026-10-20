#include "core/sound_data.h"

#include "core/random.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

/// `CDataValue::GetValueString(true)` for one value of `[FILES]`.
std::string value_string(const tl::formats::AdmRecord& record) {
    switch (record.type) {
        case tl::formats::AdmType::String:
        case tl::formats::AdmType::Translate: return record.text;
        case tl::formats::AdmType::Float: return std::to_string(record.number);
        case tl::formats::AdmType::Guid: return std::to_string(record.guid);
        case tl::formats::AdmType::Integer:
            return std::to_string(static_cast<int32_t>(record.integer));
        default: return std::to_string(record.integer);
    }
}

}  // namespace

// `CSoundBankDataInformation::createSoundData(CDataGroup*)`.
//
std::optional<SoundData> create_sound_data(const tl::formats::AdmSection& group) {
    // `GetDataGroupByName(L"FILES")`: the first such group, and it must hold a
    // value.
    const tl::formats::AdmSection* files = nullptr;
    for (const tl::formats::AdmSection& child : group.children) {
        if (child.name == "FILES") {
            files = &child;
            break;
        }
    }
    if (files == nullptr || files->records.empty()) return std::nullopt;
    SoundData data;
    // No `GUID` (-1): nothing made.
    data.guid = group.guid_or("GUID", -1);
    if (data.guid == -1) return std::nullopt;
    data.radius = group.float_or("RADIUS", 0.0f);
    data.volume = group.float_or("VOLUME", 1.0f);
    data.frequency_variation = group.float_or("FREQUENCYVARIATION", 0.0f);
    data.volume_variation = group.float_or("VOLUMEVARIATION", 0.0f);
    data.name = upper(group.string_or("NAME", ""));
    data.category = upper(group.string_or("CATEGORY", ""));
    data.loops = group.bool_or("LOOPS", false);
    data.play_chance = static_cast<int32_t>(group.integer_or("PLAYCHANCE", 0xffffffffu));
    // Every value of the group, its key unread.
    for (const tl::formats::AdmRecord& record : files->records) {
        data.files.push_back(value_string(record));
    }
    return data;
}

// `CSoundBankDataInformation::parseSoundData`,
// the body of its loop over the listed files.
void SoundDataInformation::parse_file(const tl::formats::AdmSection& root) {
    for (const tl::formats::AdmSection& group : root.children) {
        std::optional<SoundData> made = create_sound_data(group);
        if (!made) continue;
        // A GUID already in the map (+0x28): the new definition is deleted.
        if (by_guid_.count(made->guid) != 0) continue;
        all_.push_back(std::make_unique<SoundData>(std::move(*made)));
        const SoundData* data = all_.back().get();
        by_guid_[data->guid] = data;
        // `getCategoryID`, and the category appended when it is new.
        size_t id = categories_.size();
        for (size_t i = 0; i < categories_.size(); ++i) {
            if (categories_[i] == data->category) {
                id = i;
                break;
            }
        }
        if (id == categories_.size()) categories_.push_back(data->category);
        by_category_[id][data->name] = data;
    }
}

// `CSoundBankDataInformation::getSoundDataObject(const std::wstring&)`.
//
const SoundData* SoundDataInformation::find(const std::string& name) const {
    for (size_t id = 0; id < categories_.size(); ++id) {
        const auto category = by_category_.find(id);
        if (category == by_category_.end()) return nullptr;
        const auto found = category->second.find(name);
        if (found != category->second.end()) return found->second;
    }
    return nullptr;
}

// `CSoundBankDataInformation::getSoundDataObject(const std::wstring&, const
// std::wstring&)`.
const SoundData* SoundDataInformation::find(const std::string& category,
                                            const std::string& name) const {
    for (size_t id = 0; id < categories_.size(); ++id) {
        if (categories_[id] != category) continue;
        const auto names = by_category_.find(id);
        if (names == by_category_.end()) return nullptr;
        const auto found = names->second.find(name);
        return found != names->second.end() ? found->second : nullptr;
    }
    return nullptr;
}

// `CSoundBankDataInformation::getSoundDataObjectByGuid`.
const SoundData* SoundDataInformation::find_by_guid(int64_t guid) const {
    const auto found = by_guid_.find(guid);
    return found != by_guid_.end() ? found->second : nullptr;
}

// `CSoundBankDataInformation::getSoundDataObjectByGuidThatIsClosest`.
//
const SoundData* SoundDataInformation::find_closest_guid(int64_t guid) const {
    if (const SoundData* exact = find_by_guid(guid)) return exact;
    if (by_guid_.empty()) return nullptr;
    // From the lowest GUID up: how many leading bits agree with `guid`, and
    // the first to agree on more than any before it.
    const SoundData* best = by_guid_.begin()->second;
    uint32_t best_bits = 0;
    for (const auto& [key, data] : by_guid_) {
        const uint64_t differ = static_cast<uint64_t>(guid) ^ static_cast<uint64_t>(key);
        uint32_t bits = 0;
        for (int bit = 63; bit >= 0; --bit) {
            if ((differ >> bit) & 1u) break;
            ++bits;
        }
        if (bits > best_bits) {
            best = data;
            best_bits = bits;
        }
    }
    return best;
}

// `CSoundBankDataInformation::getCategoryNameByID`.
const std::string* SoundDataInformation::category_name(size_t id) const {
    return id < categories_.size() ? &categories_[id] : nullptr;
}

std::string SoundDataInformation::sound_name_by_category_index(size_t category,
                                                               size_t index) const {
    const auto filed = by_category_.find(category);
    if (filed == by_category_.end()) return std::string();
    for (const auto& entry : filed->second) {
        if (index == 0) return entry.first;
        --index;
    }
    return std::string();
}

// PORT: the sound table emptied.
void SoundDataInformation::clear() {
    all_.clear();
    by_guid_.clear();
    categories_.clear();
    by_category_.clear();
}

bool layout_adds_sound_sample(const tl::formats::CmpScene& scene,
                              const SoundDataInformation& sounds) {
    for (const tl::formats::CmpNode& node : scene.nodes) {
        if (node.type != tl::formats::cmp_type::kSound) continue;
        const tl::formats::CmpField* f = node.field(tl::formats::cmp_field::sound::kGuid);
        if (f == nullptr) continue;
        const std::string text = f->as_text();
        if (text.empty()) continue;
        // `STRINGS::GetInt64`: the leading digits.
        const int64_t guid = std::strtoll(text.c_str(), nullptr, 10);
        if (guid != -1 && sounds.find_by_guid(guid) != nullptr) return true;
    }
    return false;
}

}  // namespace tl::core

namespace tl::core {

// `CSoundBank::addSample(slot, guid)`, its choice of recordings.
std::vector<size_t> bank_recordings(const SoundData& data) {
    std::vector<size_t> kept;
    const uint32_t count = static_cast<uint32_t>(data.files.size());
    if (count == 0) return kept;
    // one of one; else ceil(count * 0.2f), at least 1.
    uint32_t wanted = 1;
    if (count != 1) {
        wanted = static_cast<uint32_t>(
            static_cast<int64_t>(std::ceil(static_cast<float>(count) * 0.2f)));
        if (wanted == 0) wanted = 1;
    }
    // `g_Rand` at 100 for the draws.
    RunicRandom rolls(100);
    const uint32_t most = wanted + wanted * 2;
    uint32_t tries = 0;
    for (uint32_t at = 0; at < wanted;) {
        const uint32_t drawn =
            static_cast<uint32_t>(rolls.integer_between(0, static_cast<int32_t>(count) - 1));
        if (++tries > most) break;
        if (std::find(kept.begin(), kept.end(), static_cast<size_t>(drawn)) != kept.end()) continue;
        kept.push_back(at);
        ++at;
    }
    return kept;
}

}  // namespace tl::core
