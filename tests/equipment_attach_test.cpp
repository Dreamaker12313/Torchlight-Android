// Where an equipped item is drawn (core/equipment_attach.h):
// `CEquipment::attachToGivenLocation` and `CEquipment::isWardrobed`.

#include "core/equipment_attach.h"
#include "core/unit_types.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::ItemStats;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;

AdmSection unit_type(const std::string& name, uint32_t id, const std::vector<uint32_t>& ancestors) {
    AdmSection section;
    section.name = name;
    AdmRecord key;
    key.key = "ID";
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

/// Weapon 8, Helmet 12, Shield 21, and an item of each.
UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Weapon", 8, {0}));
    all.children.push_back(unit_type("Helmet", 12, {0}));
    all.children.push_back(unit_type("Shield", 21, {0}));
    all.children.push_back(unit_type("SWORD", 300, {8, 0}));
    all.children.push_back(unit_type("NORMAL SHIELD", 301, {21, 0}));
    all.children.push_back(unit_type("NORMAL HELMET", 302, {12, 0}));
    root.children.push_back(all);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

ItemStats item(const std::string& type) {
    ItemStats out;
    out.unittype = type;
    return out;
}

}  // namespace

// `KEQUIP_LOCATION_BONES` and its second table.
TL_TEST(equip_location_bones_are_the_static_tables) {
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(0)), std::string("tag_righthand"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(1)), std::string("tag_lefthand"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(3)), std::string("Bip01 Head"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(5)), std::string("Bip01 L Clavicle"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(11)), std::string("tag_leftarm"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(4)), std::string());
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone(12)), std::string());
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone_secondary(5)),
                std::string("Bip01 R Clavicle"));
    TL_CHECK_EQ(std::string(tl::core::equip_location_bone_secondary(0)), std::string());
}

// `isWardrobed`: a block for the class (any block for no class) naming a MESH
// or a TEXTURE.
TL_TEST(an_item_is_wardrobed_for_a_class_whose_block_names_a_mesh_or_texture) {
    ItemStats helm = item("NORMAL HELMET");
    ItemStats::Wardrobe vanquisher;
    vanquisher.character_class = "Vanquisher";
    vanquisher.mesh = "media/wardrobe/vanquisher/helm.mesh";
    helm.wardrobe.push_back(vanquisher);
    ItemStats::Wardrobe empty;
    empty.character_class = "ALCHEMIST";
    helm.wardrobe.push_back(empty);
    TL_CHECK(tl::core::item_is_wardrobed(helm, "VANQUISHER"));
    TL_CHECK(tl::core::item_is_wardrobed(helm, "vanquisher"));
    TL_CHECK(!tl::core::item_is_wardrobed(helm, "Alchemist"));  // its block names nothing
    TL_CHECK(!tl::core::item_is_wardrobed(helm, "Destroyer"));
    TL_CHECK(tl::core::item_is_wardrobed(helm, ""));
    ItemStats::Wardrobe texture;
    texture.character_class = "Destroyer";
    texture.texture = "media/wardrobe/destroyer/helm.dds";
    helm.wardrobe.push_back(texture);
    TL_CHECK(tl::core::item_is_wardrobed(helm, "DESTROYER"));
}

// `attachToGivenLocation`: the location's bone, a shield's on the arm, the
// scale by kind, nothing for a wardrobed item or a location with no bone.
TL_TEST(attach_to_given_location_picks_the_bone_and_the_scale) {
    const UnitTypeHierarchy types = hierarchy();
    const auto sword =
        tl::core::equipment_attachment(item("SWORD"), 1, "Alchemist", 0.8f, 0.7f, types);
    TL_CHECK(sword.attach);
    TL_CHECK_EQ(sword.bone, std::string("tag_lefthand"));
    TL_CHECK_EQ(sword.scale, 0.8f);
    TL_CHECK_EQ(sword.node_location, 1);

    const auto shield =
        tl::core::equipment_attachment(item("NORMAL SHIELD"), 1, "Alchemist", 0.8f, 0.7f, types);
    TL_CHECK(shield.attach);
    TL_CHECK_EQ(shield.bone, std::string("tag_leftarm"));
    TL_CHECK_EQ(shield.scale, 0.7f);
    TL_CHECK_EQ(shield.node_location, 11);

    const auto helm =
        tl::core::equipment_attachment(item("NORMAL HELMET"), 3, "Alchemist", 0.8f, 0.7f, types);
    TL_CHECK(helm.attach);
    TL_CHECK_EQ(helm.bone, std::string("Bip01 Head"));
    TL_CHECK_EQ(helm.scale, 1.0f);

    ItemStats worn = item("NORMAL HELMET");
    ItemStats::Wardrobe block;
    block.character_class = "ALCHEMIST";
    block.mesh = "helm.mesh";
    worn.wardrobe.push_back(block);
    TL_CHECK(!tl::core::equipment_attachment(worn, 3, "Alchemist", 0.8f, 0.7f, types).attach);

    // The chest has no bone: nothing goes on.
    TL_CHECK(
        !tl::core::equipment_attachment(item("NORMAL HELMET"), 4, "X", 1.0f, 1.0f, types).attach);
    // The shoulders' second model has a bone of its own.
    TL_CHECK_EQ(tl::core::equipment_attachment(item("NORMAL HELMET"), 5, "X", 1.0f, 1.0f, types)
                    .secondary_bone,
                std::string("Bip01 R Clavicle"));
}
