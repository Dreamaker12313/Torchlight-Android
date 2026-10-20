// `core/wardrobe` - `CWardrobe::update`'s rules: what each slot draws, which
// submeshes stay, and the skin's blend. Numbers and names only.
#include "core/wardrobe.h"

#include "core/inventory.h"
#include "formats/adm.h"
#include "test_support.h"

#include <array>
#include <string>
#include <vector>

namespace {

tl::formats::AdmSection group(const char* name, uint32_t id, std::initializer_list<uint32_t> up) {
    tl::formats::AdmSection out;
    out.name = name;
    tl::formats::AdmRecord record;
    record.key = "ID";
    record.type = tl::formats::AdmType::Integer;
    record.integer = id;
    out.records.push_back(record);
    for (uint32_t ancestor : up) {
        record.key = "child";
        record.integer = ancestor;
        out.records.push_back(record);
    }
    return out;
}

/// The ancestries `unittypes.hie` gives these.
tl::core::UnitTypeHierarchy types() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(group("CHEST ARMOR", 15, {13, 31, 2, 0}));
    all.children.push_back(group("HELMET", 12, {13, 31, 2, 0}));
    all.children.push_back(group("SHOULDER ARMOR", 20, {13, 31, 2, 0}));
    all.children.push_back(group("BOOTS", 16, {13, 31, 2, 0}));
    root.children.push_back(all);
    tl::core::UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

tl::core::ItemStats::Wardrobe dress(const char* cls, const char* mesh, const char* texture) {
    tl::core::ItemStats::Wardrobe entry;
    entry.character_class = cls;
    entry.mesh = mesh;
    entry.texture = texture;
    entry.has_mesh = mesh[0] != '\0';
    entry.has_texture = texture[0] != '\0';
    return entry;
}

tl::core::WardrobeBase alchemist() {
    tl::core::WardrobeBase base;
    base.skin = "body.png";
    base.textures[0] = "starting_chest.png";
    base.textures[1] = "starting_gloves.png";
    base.textures[2] = "starting_boots.png";
    base.meshes[3] = "alchemist_hair.mesh";
    return base;
}

}  // namespace

TL_TEST(wardrobe_pieces_without_items_are_the_class_outfit) {
    const auto pieces = tl::core::wardrobe_pieces(alchemist(), nullptr, "Alchemist", types());
    TL_CHECK_EQ(pieces[0].texture, std::string("starting_chest.png"));
    TL_CHECK_EQ(pieces[1].texture, std::string("starting_gloves.png"));
    TL_CHECK_EQ(pieces[2].texture, std::string("starting_boots.png"));
    TL_CHECK(pieces[0].mesh.empty());
    TL_CHECK_EQ(pieces[3].mesh, std::string("alchemist_hair.mesh"));
    TL_CHECK(pieces[3].texture.empty());
    TL_CHECK(pieces[4].mesh.empty() && pieces[4].texture.empty());
    TL_CHECK(!pieces[0].worn);
}

TL_TEST(wardrobe_pieces_take_the_worn_items_group_for_the_class) {
    const tl::core::UnitTypeHierarchy t = types();
    tl::core::Inventory inv;
    tl::core::ItemStats vest;
    vest.name = "a Leather Vest";
    vest.unittype = "CHEST ARMOR";
    vest.max_stack = 1;
    // The base file's groups first, then the item's own: the last match wins.
    vest.wardrobe.push_back(dress("DESTROYER", "leather_set.mesh", "turneddown_chest.png"));
    vest.wardrobe.push_back(dress("ALCHEMIST", "old_suit.mesh", "old_chest.png"));
    vest.wardrobe.push_back(dress("alchemist", "alchemist_suit01.mesh", "chest_set1.png"));
    inv.pickup_into_slot(vest, 1, tl::core::equip_location::kChest);
    // Boots with a group that names no texture: the class's overlay stays.
    tl::core::ItemStats boots;
    boots.name = "Shoes";
    boots.unittype = "BOOTS";
    boots.max_stack = 1;
    boots.wardrobe.push_back(dress("ALCHEMIST", "alchemist_suit01.mesh", ""));
    inv.pickup_into_slot(boots, 1, tl::core::equip_location::kBoots);

    const auto pieces = tl::core::wardrobe_pieces(alchemist(), &inv, "Alchemist", t);
    TL_CHECK(pieces[0].worn);
    TL_CHECK_EQ(pieces[0].mesh, std::string("alchemist_suit01.mesh"));
    TL_CHECK_EQ(pieces[0].texture, std::string("chest_set1.png"));
    TL_CHECK_EQ(pieces[2].mesh, std::string("alchemist_suit01.mesh"));
    TL_CHECK_EQ(pieces[2].texture, std::string("starting_boots.png"));

    // A class with no skin keeps its own overlays even under an item.
    tl::core::WardrobeBase bare = alchemist();
    bare.skin.clear();
    const auto plain = tl::core::wardrobe_pieces(bare, &inv, "Alchemist", t);
    TL_CHECK_EQ(plain[0].texture, std::string("starting_chest.png"));
    TL_CHECK_EQ(plain[0].mesh, std::string("alchemist_suit01.mesh"));

    // Another class's groups do nothing.
    const auto other = tl::core::wardrobe_pieces(alchemist(), &inv, "Vanquisher", t);
    TL_CHECK(other[0].mesh.empty());
    TL_CHECK_EQ(other[0].texture, std::string("starting_chest.png"));
}

TL_TEST(a_helmet_that_would_draw_the_class_mesh_draws_none) {
    const tl::core::UnitTypeHierarchy t = types();
    tl::core::Inventory inv;
    tl::core::ItemStats cap;
    cap.name = "Cap";
    cap.unittype = "HELMET";
    cap.max_stack = 1;
    // No group for the class: the slot keeps the hair, which is then dropped.
    cap.wardrobe.push_back(dress("DESTROYER", "plate_set.mesh", "plate_helm.png"));
    inv.pickup_into_slot(cap, 1, tl::core::equip_location::kHelmet);
    auto pieces = tl::core::wardrobe_pieces(alchemist(), &inv, "Alchemist", t);
    TL_CHECK(pieces[3].worn);
    TL_CHECK(pieces[3].mesh.empty());

    // A group naming its own mesh draws it, and its texture is composited.
    inv.item_in_slot(tl::core::equip_location::kHelmet)
        ->wardrobe.push_back(dress("ALCHEMIST", "alchemist_suit01.mesh", "helm_tex.png"));
    pieces = tl::core::wardrobe_pieces(alchemist(), &inv, "Alchemist", t);
    TL_CHECK_EQ(pieces[3].mesh, std::string("alchemist_suit01.mesh"));
    TL_CHECK_EQ(pieces[3].texture, std::string("helm_tex.png"));
}

TL_TEST(wardrobe_submesh_rules) {
    std::array<bool, tl::core::kWardrobeSlotCount> none{};
    // Nothing drawn: the body's chest, gloves, boots and face stay; its own
    // helmet and shoulders never do; anything naming BASE always does.
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Starter_Set/chest", none));
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Starter_Set/gloves", none));
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Starter_Set/boots", none));
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Starter_Set/face", none));
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/helmet", none));
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/shoulders", none));
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/hands", none));
    std::array<bool, tl::core::kWardrobeSlotCount> all{};
    all.fill(true);
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Nude/base", all));
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/chest", all));
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/face", all));
    std::array<bool, tl::core::kWardrobeSlotCount> hat{};
    hat[3] = true;
    TL_CHECK(!tl::core::wardrobe_keeps_body_submesh("Starter_Set/face", hat));
    TL_CHECK(tl::core::wardrobe_keeps_body_submesh("Starter_Set/chest", hat));

    // A piece gives its slot's submesh, and the helmet slot its face too.
    TL_CHECK(tl::core::wardrobe_keeps_piece_submesh(0, "Leather_Set/chest"));
    TL_CHECK(!tl::core::wardrobe_keeps_piece_submesh(0, "Leather_Set/boots"));
    TL_CHECK(tl::core::wardrobe_keeps_piece_submesh(3, "Hair/helmet"));
    TL_CHECK(tl::core::wardrobe_keeps_piece_submesh(3, "Hair/face"));
    TL_CHECK(!tl::core::wardrobe_keeps_piece_submesh(4, "Hair/face"));
    TL_CHECK(tl::core::wardrobe_keeps_piece_submesh(4, "Leather_Set/shoulders"));

    TL_CHECK(tl::core::wardrobe_skin_covers("Hair/face"));
    TL_CHECK(!tl::core::wardrobe_skin_covers("Hair/helmet"));
    TL_CHECK(!tl::core::wardrobe_skin_covers("Leather_Set/Shoulders"));
}

TL_TEST(wardrobe_blend_mixes_in_single_floats_and_truncates) {
    // skin, overlay: opaque replaces; transparent keeps; half mixes.
    std::vector<uint8_t> skin = {10, 20, 30, 255, 10, 20, 30, 255, 200, 100, 0, 255, 0, 0, 0, 77};
    const std::vector<uint8_t> overlay = {99, 98, 97,  255, 250, 250, 250, 0,
                                          0,  0,  255, 128, 255, 255, 255, 1};
    tl::core::wardrobe_blend(skin.data(), overlay.data(), 4);
    TL_CHECK_EQ(skin[0], 99);
    TL_CHECK_EQ(skin[1], 98);
    TL_CHECK_EQ(skin[2], 97);
    TL_CHECK_EQ(skin[4], 10);
    TL_CHECK_EQ(skin[5], 20);
    TL_CHECK_EQ(skin[6], 30);
    // a = 128/255: 200 * (1 - a) = 99.6 -> 99, worked as the original does, in floats.
    const float a = 128.0f / 255.0f;
    TL_CHECK_EQ(skin[8], static_cast<uint8_t>(static_cast<int>(0.0f * a + 200.0f * (1.0f - a))));
    TL_CHECK_EQ(skin[9], static_cast<uint8_t>(static_cast<int>(0.0f * a + 100.0f * (1.0f - a))));
    TL_CHECK_EQ(skin[10], static_cast<uint8_t>(static_cast<int>(255.0f * a + 0.0f * (1.0f - a))));
    TL_CHECK_EQ(skin[8], 99);
    // Every result is opaque.
    for (size_t i = 3; i < skin.size(); i += 4) TL_CHECK_EQ(skin[i], 255);
    // alpha 1 over black: 255/255*1 = 1.0 -> 1.
    TL_CHECK_EQ(skin[12], 1);
}

// A `Monster` node's `WARDROBE` setters: the class outfit's slot is replaced,
// which `update` then draws; a character with no wardrobe takes nothing.
TL_TEST(a_layout_nodes_wardrobe_setters_replace_the_class_outfit) {
    using tl::core::WardrobeSlot;
    tl::core::WardrobeBase base = alchemist();
    TL_CHECK(tl::core::set_wardrobe_base_mesh(&base, WardrobeSlot::Shoulders, "pads.mesh"));
    TL_CHECK(tl::core::set_wardrobe_base_texture(&base, WardrobeSlot::Boots, "red_boots.png"));
    const auto pieces = tl::core::wardrobe_pieces(base, nullptr, "Alchemist", types());
    TL_CHECK_EQ(pieces[4].mesh, std::string("pads.mesh"));
    TL_CHECK_EQ(pieces[2].texture, std::string("red_boots.png"));
    TL_CHECK_EQ(pieces[3].mesh, std::string("alchemist_hair.mesh"));
    TL_CHECK(!tl::core::set_wardrobe_base_mesh(nullptr, WardrobeSlot::Chest, "x.mesh"));
}
