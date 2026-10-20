#include "core/animation_manifest.h"

#include "core/breakable.h"
#include "core/skill_animation.h"

#include <algorithm>
#include <cctype>

namespace tl::core {
namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string lowered(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// `Attack1.skeleton` -> `attack1`. Also copes with a manifest that names a
/// path rather than a bare file, which the shipped data does not but a mod may.
// PORT: asset lookup, an animation file's stem.
std::string stem_of(const std::string& file) {
    std::string name = file;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name.resize(dot);
    return lowered(name);
}

// PORT: an empty tag list.
const std::vector<AnimationTag>& no_tags() {
    static const std::vector<AnimationTag> empty;
    return empty;
}

/// The animation *state* a file stem names: `Attack1` -> `attack`,
/// `Die_Armed2` -> `die`, `Rslash` -> `rslash`. Files are named
/// `<STATE>[_Armed|_Unarmed][<n>]`.
// PORT: asset lookup, an animation file's state.
std::string state_of(const std::string& stem) {
    std::string name = stem;
    size_t end = name.size();
    while (end > 0 && name[end - 1] >= '0' && name[end - 1] <= '9') --end;
    name.resize(end);
    for (const char* suffix : {"_unarmed", "_armed"}) {
        const size_t n = std::char_traits<char>::length(suffix);
        if (name.size() > n && name.compare(name.size() - n, n, suffix) == 0) {
            name.resize(name.size() - n);
            break;
        }
    }
    return name;
}

}  // namespace

namespace {

// `CKeyframe::CKeyframe(owner, group)`, the
// reads. One is built for every `[KEY]`; a missing name is type 0, `HIT`. A string
// read is "" for a value that is not one; a number is the value's
// word whatever its type.
void read_keyframe(const tl::formats::AdmSection& key, AnimationTag& tag) {
    const auto text = [&key](const char* name, bool& found) {
        const tl::formats::AdmRecord* record = key.find(name);
        found = record != nullptr;
        if (record == nullptr) return std::string();
        return (record->type == tl::formats::AdmType::String ||
                record->type == tl::formats::AdmType::Translate)
                   ? record->text
                   : std::string();
    };
    bool found = false;
    tag.name = text("NAME", found);
    // +0x54: `UNITTHEME`, upper-cased.
    tag.unit_theme = text("UNITTHEME", found);
    for (char& c : tag.unit_theme) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    // +0x08: `FRAME`'s word, 0 when not there.
    const tl::formats::AdmRecord* frame = key.find("FRAME");
    tag.frame = frame != nullptr ? frame->word_as_float() : 0.0f;
    // +0x0c, one field: `SOUND`, then `LAYOUT` and
    // `CAMERASHAKE` each over it when there, through `FILESYSTEM::CleanPath`.
    std::string file = text("SOUND", found);
    const std::string layout = text("LAYOUT", found);
    if (found) file = layout;
    const std::string shake = text("CAMERASHAKE", found);
    if (found) file = shake;
    file = breakable_clean_path(file);
    tag.sound = file;
    tag.layout = file;
    // +0x78: `PARTICLEATTACHES`' first byte, true when not there.
    const tl::formats::AdmRecord* attaches = key.find("PARTICLEATTACHES");
    tag.attaches = attaches != nullptr ? attaches->word_first_byte() != 0 : true;
    // +0x28: `BONE`, narrowed.
    tag.bone = text("BONE", found);
    // +0x74: `PARTICLECOUNT`'s word, 1 unless it is 2 or more.
    const tl::formats::AdmRecord* count = key.find("PARTICLECOUNT");
    const int32_t particles = count != nullptr ? static_cast<int32_t>(count->word_as_integer()) : 1;
    tag.particle_count = particles < 2 ? 1u : static_cast<uint32_t>(particles);
    // +0x48..+0x50: the first `[BONEOFFSET]`'s `X`, `Y`, `Z` words.
    for (const tl::formats::AdmSection& offset : key.children) {
        if (offset.name != "BONEOFFSET") continue;
        const auto word = [&offset](const char* name) {
            const tl::formats::AdmRecord* record = offset.find(name);
            return record != nullptr ? record->word_as_float() : 0.0f;
        };
        tag.offset_x = word("X");
        tag.offset_y = word("Y");
        tag.offset_z = word("Z");
        break;
    }
    // +0x7c: a `CCameraShake` for a `CAMERASHAKE` (0xe) or `CAMERASHAKE_NO_FALLOFF`
    // (0x1a) key that names a file, lasting `CAMERASHAKEDURATION`
    // but at least 0.01.
    const int type = keyframe_type_of(tag.name);
    tag.camera_shake = (type == 0xe || type == 0x1a) && !file.empty();
    if (tag.camera_shake) {
        const tl::formats::AdmRecord* duration = key.find("CAMERASHAKEDURATION");
        const float seconds = duration != nullptr ? duration->word_as_float() : 0.0f;
        tag.camera_shake_duration = seconds <= 0.01f ? 0.01f : seconds;
    }
}

}  // namespace

// `CGenericModel::updateAnimation`: the frame times 0.033333335f,
// a product, which differs from `frame / 30` in the last place for some
// frames.
float AnimationTag::seconds() const { return frame * 0.033333335f; }

AnimationManifest AnimationManifest::from(const tl::formats::AdmFile& file) {
    AnimationManifest manifest;
    file.visit([&](const tl::formats::AdmSection& section) {
        if (section.name != "ANIMATION") return;
        const std::string stem = stem_of(section.string_or("FILE", ""));
        if (stem.empty()) return;

        if (manifest.by_animation_.find(stem) == manifest.by_animation_.end()) {
            manifest.order_.push_back(stem);
        }
        std::vector<AnimationTag>& tags = manifest.by_animation_[stem];
        manifest.states_.emplace(state_of(stem), stem);
        for (const tl::formats::AdmSection& key : section.children) {
            if (key.name != "KEY") continue;
            AnimationTag tag;
            read_keyframe(key, tag);
            tags.push_back(std::move(tag));
        }
    });
    return manifest;
}

// PORT: an animation file's keys from its manifest.
const std::vector<AnimationTag>* AnimationManifest::tags_of_file(const std::string& file) const {
    const auto it = by_animation_.find(stem_of(file));
    return it != by_animation_.end() ? &it->second : nullptr;
}

// PORT: an animation's keys from its manifest, by file or else by state.
const std::vector<AnimationTag>& AnimationManifest::tags_for(const std::string& animation) const {
    const std::string stem = stem_of(animation);
    const auto it = by_animation_.find(stem);
    if (it != by_animation_.end()) return it->second;

    // Asked for by state rather than by file: the engine names `Attack`, the mesh
    // ships `Attack1`, and the manifest is keyed on the file.
    const auto state = states_.find(state_of(stem));
    if (state == states_.end()) return no_tags();
    const auto found = by_animation_.find(state->second);
    return found == by_animation_.end() ? no_tags() : found->second;
}

// PORT: the time of an animation's key of this name.
float AnimationManifest::seconds_of(const std::string& animation, const std::string& tag,
                                    float fallback) const {
    const std::string wanted = lowered(tag);
    for (const AnimationTag& entry : tags_for(animation)) {
        if (lowered(entry.name) == wanted) return entry.seconds();
    }
    return fallback;
}

}  // namespace tl::core
