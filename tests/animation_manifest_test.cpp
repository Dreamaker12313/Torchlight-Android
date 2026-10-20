// `<mesh>.ANIMATION` (`core/animation_manifest.h`): trigger tags per animation,
// keyed by file stem and found by animation state. The files are built by hand
// in the shape of `ALCHEMIST.ANIMATION.adm`.
#include "core/animation_manifest.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::AnimationManifest;
using tl::core::AnimationTag;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord text(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord number(const std::string& key, float value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Float;
    r.number = value;
    return r;
}

AdmRecord flag(const std::string& key, bool value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Bool;
    r.integer = value ? 1u : 0u;
    return r;
}

AdmSection key(const std::string& name, float frame) {
    AdmSection k;
    k.name = "KEY";
    k.records = {text("NAME", name), number("FRAME", frame)};
    return k;
}

AdmSection animation(const std::string& file, std::vector<AdmSection> keys) {
    AdmSection a;
    a.name = "ANIMATION";
    a.records = {text("FILE", file)};
    a.children = std::move(keys);
    return a;
}

AnimationManifest manifest_of(std::vector<AdmSection> animations) {
    AdmFile file;
    file.ok = true;
    file.root.name = "ANIMATIONS";
    file.root.children = std::move(animations);
    return AnimationManifest::from(file);
}

}  // namespace

// `FRAME` is at 30 fps: frame 15 is 15 / 30 = 0.5 s, frame 9 is 9 / 30 = 0.3 s.
// The key is the stem, lower-cased, whatever path or suffix the caller gives.
TL_TEST(animation_tags_are_keyed_by_file_stem_and_timed_at_30_fps) {
    AdmSection sound = key("ATTACKSOUND", 9.0f);
    sound.records.push_back(text("SOUND", "STAFFSWING"));
    const AnimationManifest m =
        manifest_of({animation("models/Polearm1.skeleton", {key("HIT", 15.0f), sound})});

    TL_CHECK_EQ(m.size(), size_t{1});
    TL_CHECK_EQ(m.tags_for("polearm1").size(), size_t{2});
    TL_CHECK_EQ(m.tags_for("POLEARM1.SKELETON").size(), size_t{2});
    TL_CHECK_EQ(m.seconds_of("polearm1", "hit", -1.0f), 0.5f);
    TL_CHECK_EQ(m.seconds_of("Polearm1", "AttackSound", -1.0f), 9.0f / 30.0f);
    TL_CHECK_EQ(m.tags_for("polearm1")[1].sound, std::string("STAFFSWING"));
    // No such tag, no such animation: the fallback.
    TL_CHECK_EQ(m.seconds_of("polearm1", "FOOTSTEP", -1.0f), -1.0f);
    TL_CHECK_EQ(m.seconds_of("rslash", "HIT", 7.0f), 7.0f);
}

// The engine asks for `Attack` and `Die`; the mesh ships `Attack1` and
// `Die_Armed2`. The state is the stem without its trailing digits and then
// without `_armed`/`_unarmed`, so `Die_Unarmed` finds `Die_Armed2` too.
TL_TEST(animation_tags_are_found_by_state) {
    const AnimationManifest m = manifest_of({
        animation("Attack1.skeleton", {key("HIT", 6.0f)}),
        animation("Die_Armed2.skeleton", {key("BLENDOUT", 30.0f)}),
    });
    // Frame times 0.033333335f, as the console times a key.
    TL_CHECK_EQ(m.seconds_of("Attack", "HIT", -1.0f), 6.0f * 0.033333335f);
    TL_CHECK_EQ(m.seconds_of("Die", "BLENDOUT", -1.0f), 30.0f * 0.033333335f);
    TL_CHECK_EQ(m.seconds_of("Die_Unarmed", "BLENDOUT", -1.0f), 30.0f * 0.033333335f);
    TL_CHECK(m.tags_for("Walk").empty());
}

// Two files play one state: the first listed is the one a lookup by state
// finds, and each is still reachable by its own name.
TL_TEST(the_first_file_of_a_state_answers_for_it) {
    const AnimationManifest m = manifest_of({
        animation("Attack1.skeleton", {key("HIT", 3.0f)}),
        animation("Attack2.skeleton", {key("HIT", 12.0f)}),
    });
    TL_CHECK_EQ(m.seconds_of("attack", "HIT", -1.0f), 3.0f * 0.033333335f);
    TL_CHECK_EQ(m.seconds_of("attack2", "HIT", -1.0f), 12.0f * 0.033333335f);
    TL_CHECK_EQ(m.order().size(), size_t{2});
    TL_CHECK_EQ(m.order()[0], std::string("attack1"));
}

// `CKeyframe::CKeyframe`: a key with no `NAME` is still a key, and
// `PARTICLEATTACHES` is true unless authored. `BONEOFFSET` is a child section.
// A second section for one file adds to its tags without listing it twice,
// and a section with no `FILE` is not an animation.
TL_TEST(animation_keys_keep_defaults_offsets_and_order) {
    AdmSection spawn = key("SPAWNPARTICLE", 4.0f);
    spawn.records.push_back(text("BONE", "Bip01 R Hand"));
    spawn.records.push_back(text("LAYOUT", "media/particles/sparks.layout"));
    spawn.records.push_back(flag("PARTICLEATTACHES", false));
    AdmSection offset;
    offset.name = "BONEOFFSET";
    offset.records = {number("X", 1.5f), number("Y", -2.0f), number("Z", 0.25f)};
    spawn.children.push_back(offset);

    AdmSection unnamed;
    unnamed.name = "KEY";
    unnamed.records = {number("FRAME", 2.0f)};

    const AnimationManifest m = manifest_of({
        animation("Idle.skeleton", {spawn}),
        animation("", {key("HIT", 1.0f)}),
        animation("idle.skeleton", {unnamed}),
    });

    TL_CHECK_EQ(m.size(), size_t{1});
    TL_CHECK_EQ(m.order().size(), size_t{1});
    const std::vector<AnimationTag>& tags = m.tags_for("idle");
    TL_CHECK_EQ(tags.size(), size_t{2});
    TL_CHECK_EQ(tags[0].bone, std::string("Bip01 R Hand"));
    TL_CHECK_EQ(tags[0].layout, std::string("media/particles/sparks.layout"));
    TL_CHECK(!tags[0].attaches);
    TL_CHECK_EQ(tags[0].offset_x, 1.5f);
    TL_CHECK_EQ(tags[0].offset_y, -2.0f);
    TL_CHECK_EQ(tags[0].offset_z, 0.25f);
    TL_CHECK_EQ(tags[1].name, std::string());
    TL_CHECK(tags[1].attaches);
    TL_CHECK_EQ(tags[1].seconds(), 2.0f / 30.0f);
}
