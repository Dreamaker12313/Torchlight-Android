// The unit type hierarchy (`CHierarchy::parseHierachy` / `ISA`), with ids and
// ancestor lists as `media/UNITTYPES.HIE` authors them.
#include "core/unit_types.h"
#include "test_support.h"

namespace {

tl::formats::AdmSection type(const std::string& name, uint32_t id,
                             const std::vector<uint32_t>& ancestors) {
    tl::formats::AdmSection section;
    section.name = name;
    tl::formats::AdmRecord key;
    key.key = "ID";
    key.type = tl::formats::AdmType::Integer;
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        tl::formats::AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.type = tl::formats::AdmType::Integer;
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

}  // namespace

TL_TEST(a_gambler_is_a_merchant_and_a_miner_is_an_npc) {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("any", 0, {}));
    types.children.push_back(type("Character", 25, {0}));
    types.children.push_back(type("InteractiveCharacter", 131, {25, 0}));
    types.children.push_back(type("Merchant", 41, {131, 25, 0}));
    types.children.push_back(type("Gambler", 127, {41, 131, 25, 0}));
    types.children.push_back(type("NPC", 104, {131, 25, 0}));
    root.children.push_back(types);

    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    namespace ut = tl::core::unit_type;

    TL_CHECK_EQ(hierarchy.id_of("gambler"), ut::kGambler);
    TL_CHECK(hierarchy.isa("GAMBLER", ut::kMerchant));
    TL_CHECK(hierarchy.isa("Gambler", ut::kInteractiveCharacter));
    TL_CHECK(!hierarchy.isa("Merchant", ut::kGambler));
    TL_CHECK(hierarchy.isa("NPC", ut::kNpc));
    TL_CHECK(hierarchy.isa("NPC", ut::kInteractiveCharacter));
    TL_CHECK(!hierarchy.isa("NPC", ut::kMerchant));
    TL_CHECK(!hierarchy.isa("NoSuchType", ut::kCharacter));
}
