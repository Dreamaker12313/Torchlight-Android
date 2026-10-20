// `CSoundBankDataInformation` (core/sound_data.h): what `parseSoundData` keeps,
// and how a name and a GUID find a definition.

#include "core/sound_data.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>

namespace {

using tl::core::SoundData;
using tl::core::SoundDataInformation;
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

AdmRecord guid(int64_t value) {
    AdmRecord r;
    r.key = "GUID";
    r.type = AdmType::Guid;
    r.guid = value;
    return r;
}

AdmSection sound(const std::string& name, const std::string& category, int64_t id,
                 const std::string& file) {
    AdmSection s;
    s.name = "SOUND";
    s.records.push_back(text("NAME", name));
    s.records.push_back(text("CATEGORY", category));
    s.records.push_back(guid(id));
    AdmSection files;
    files.name = "FILES";
    if (!file.empty()) files.records.push_back(text("WHATEVER", file));
    s.children.push_back(files);
    return s;
}

}  // namespace

// `createSoundData`: no `GUID` or no `[FILES]` value makes
// nothing; a `[FILES]` value counts whatever its key; names are upper-cased.
TL_TEST(sound_data_needs_a_guid_and_a_file) {
    AdmSection no_guid = sound("Bark", "Dog", 0, "bark.ogg");
    no_guid.records.erase(no_guid.records.begin() + 2);
    TL_CHECK(!tl::core::create_sound_data(no_guid));
    TL_CHECK(!tl::core::create_sound_data(sound("Bark", "Dog", 5, "")));
    const auto made = tl::core::create_sound_data(sound("Bark", "Dog", 5, "bark.ogg"));
    TL_CHECK(made.has_value());
    TL_CHECK_EQ(made->name, std::string("BARK"));
    TL_CHECK_EQ(made->category, std::string("DOG"));
    TL_CHECK_EQ(made->files.front(), std::string("bark.ogg"));
    TL_CHECK_EQ(made->play_chance, -1);
    TL_CHECK(made->volume == 1.0f);
}

// `parseSoundData`: a GUID seen before keeps its first
// definition; a name met again in its category is filed over the first; a
// name is found in the earliest category that has it.
TL_TEST(sound_data_files_by_guid_first_and_by_name_last) {
    AdmSection root;
    root.children.push_back(sound("Bark", "Dog", 5, "first.ogg"));
    root.children.push_back(sound("Growl", "Dog", 5, "dropped.ogg"));  // GUID 5 again
    root.children.push_back(sound("Bark", "Dog", 6, "second.ogg"));    // same name
    root.children.push_back(sound("Bark", "Cat", 7, "cat.ogg"));       // later category
    SoundDataInformation info;
    info.parse_file(root);
    TL_CHECK_EQ(info.size(), size_t{3});
    TL_CHECK(info.find("GROWL") == nullptr);
    TL_CHECK_EQ(info.find_by_guid(5)->files.front(), std::string("first.ogg"));
    TL_CHECK_EQ(info.find("BARK")->files.front(), std::string("second.ogg"));
    TL_CHECK_EQ(info.find("CAT", "BARK")->files.front(), std::string("cat.ogg"));
    TL_CHECK(info.find("bark") == nullptr);  // stored upper-cased, matched exactly
    TL_CHECK_EQ(*info.category_name(1), std::string("CAT"));
}

// `getSoundDataObjectByGuidThatIsClosest`: the GUID sharing
// the most leading bits, the lowest first on a tie.
TL_TEST(sound_data_closest_guid_shares_the_most_leading_bits) {
    AdmSection root;
    root.children.push_back(sound("A", "X", 0x10, "a.ogg"));
    root.children.push_back(sound("B", "X", 0x100, "b.ogg"));
    root.children.push_back(sound("C", "X", 0x101, "c.ogg"));
    SoundDataInformation info;
    info.parse_file(root);
    TL_CHECK_EQ(info.find_closest_guid(0x101)->name, std::string("C"));
    TL_CHECK_EQ(info.find_closest_guid(0x103)->name, std::string("B"));  // a tie: the lower
    TL_CHECK_EQ(info.find_closest_guid(0x11)->name, std::string("A"));
    TL_CHECK_EQ(info.find_closest_guid(0x180)->name, std::string("B"));
}

// `getSoundNameByCategoryIndex`: the n-th
// name in the category, in name order; empty past the end or for a category
// with nothing filed.
TL_TEST(sound_data_names_a_category_index_in_name_order) {
    AdmSection root;
    root.children.push_back(sound("Growl", "Dog", 5, "g.ogg"));
    root.children.push_back(sound("Bark", "Dog", 6, "b.ogg"));
    root.children.push_back(sound("Purr", "Cat", 7, "p.ogg"));
    SoundDataInformation info;
    info.parse_file(root);
    TL_CHECK_EQ(info.sound_name_by_category_index(0, 0), std::string("BARK"));
    TL_CHECK_EQ(info.sound_name_by_category_index(0, 1), std::string("GROWL"));
    TL_CHECK_EQ(info.sound_name_by_category_index(0, 2), std::string());
    TL_CHECK_EQ(info.sound_name_by_category_index(1, 0), std::string("PURR"));
    TL_CHECK_EQ(info.sound_name_by_category_index(9, 0), std::string());
}
