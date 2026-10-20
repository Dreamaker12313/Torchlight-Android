// `<mesh>.ANIMATION`: the trigger tags each animation of a character carries (the
// hit frame, sounds, weapon trails, particles, camera shakes). **`FRAME` is in
// frames at 30 fps**, not in seconds.
//
// No OGRE and no file I/O: whoever owns the archive parses the `.adm`.
#pragma once

#include "formats/adm.h"

#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// One trigger tag on one animation.
struct AnimationTag {
    /// `NAME`, the tag type, upper-cased. One that is no table word (a few
    /// shipped keys say `unknown`, one is empty) reads as `HIT`
    /// (`keyframe_type_of`); kept as authored.
    std::string name;

    /// `FRAME`, at 30 fps. See seconds().
    float frame = 0.0f;

    /// `SOUND`, `LAYOUT` or `CAMERASHAKE`, whichever is authored last: one
    /// field on the console, which `sound` and `layout` both hold. A bare
    /// `ATTACKSOUND` plays the weapon's `ATTACK_SOUND`.
    std::string sound;

    /// `LAYOUT` - the particle system a `SPAWNPARTICLE` tag releases.
    std::string layout;

    /// `UNITTHEME`, upper-cased: the unit theme
    /// the key's particle belongs to. `CCharacter::loadModel`
    /// hands it to the key's trigger, which runs only while the
    /// unit has it (`unitThemeUpdated`).
    std::string unit_theme;

    /// `BONE` and `BONEOFFSET` - where on the character it is released.
    std::string bone;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;

    /// `PARTICLEATTACHES`: the particle rides its bone (true, the default) or is
    /// left in the world where the key released it.
    bool attaches = true;

    /// `PARTICLECOUNT`: how many triggers the key makes; 1
    /// when absent or below 2.
    uint32_t particle_count = 1;

    /// `CKeyframe +0x7c`: a `CAMERASHAKE` or `CAMERASHAKE_NO_FALLOFF` key that
    /// names a file makes a camera shake of it, lasting
    /// `CAMERASHAKEDURATION` seconds, at least 0.01.
    bool camera_shake = false;
    float camera_shake_duration = 0.0f;

    /// Where in the animation this tag falls, in seconds.
    float seconds() const;
};

/// Every animation's tags, for one character mesh.
class AnimationManifest {
public:
    /// Read a parsed `[ANIMATIONS]` file. Keys are the `FILE` stems, without
    /// the `.skeleton` suffix and matched without case - the manifest spells
    /// them `Attack1.skeleton` and `polearm1.skeleton` in the same file.
    static AnimationManifest from(const tl::formats::AdmFile& file);

    /// The tags on one animation, or an empty list.
    const std::vector<AnimationTag>& tags_for(const std::string& animation) const;

    /// The tags of the entry whose `FILE` stem is exactly `file`'s, without
    /// tags_for()'s fall back to a state; null when the manifest has none.
    const std::vector<AnimationTag>* tags_of_file(const std::string& file) const;

    /// When the named tag fires on the named animation, in seconds (the first,
    /// if it carries several), or `fallback` when it has no such tag.
    float seconds_of(const std::string& animation, const std::string& tag, float fallback) const;

    /// The `FILE` stems in the order the manifest lists them - the order
    /// `CGenericModel`'s animation list (+0x1e0 -> +0x28) keeps.
    // PORT: an accessor of the port's own bookkeeping.
    const std::vector<std::string>& order() const { return order_; }

    // PORT: an accessor of the port's own bookkeeping.
    bool empty() const { return by_animation_.empty(); }
    // PORT: an accessor of the port's own bookkeeping.
    size_t size() const { return by_animation_.size(); }

private:
    std::map<std::string, std::vector<AnimationTag>> by_animation_;

    /// The first file that plays each animation *state*, so a lookup by state
    /// finds the tags of the file that carries them. See tags_for().
    std::map<std::string, std::string> states_;
    std::vector<std::string> order_;
};

}  // namespace tl::core
