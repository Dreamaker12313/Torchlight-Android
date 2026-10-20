#include "core/wardrobe.h"

#include <algorithm>
#include <cctype>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

// PORT: whether an upper-cased name holds a word.
bool contains(const std::string& haystack_upper, const char* needle) {
    return needle[0] != '\0' && haystack_upper.find(needle) != std::string::npos;
}

}  // namespace

// PORT: a wardrobe slot's name (`CHEST`, `GLOVES`, `BOOTS`, `HELMET`, `SHOULDERS`).
const char* wardrobe_slot_name(int slot) {
    static const char* const kNames[kWardrobeSlotCount] = {"CHEST", "GLOVES", "BOOTS", "HELMET",
                                                           "SHOULDERS"};
    return slot >= 0 && slot < kWardrobeSlotCount ? kNames[slot] : "";
}

// PORT: a wardrobe slot's second name (`FACE` for the helmet).
const char* wardrobe_slot_name2(int slot) {
    return slot == static_cast<int>(WardrobeSlot::Helmet) ? "FACE" : "";
}

std::array<WardrobePiece, kWardrobeSlotCount> wardrobe_pieces(const WardrobeBase& base,
                                                              const Inventory* inventory,
                                                              const std::string& character_class,
                                                              const UnitTypeHierarchy& unit_types) {
    // `CWardrobe::update(CInventory*)`, the
    // loop over the five slots.
    std::array<WardrobePiece, kWardrobeSlotCount> pieces;
    const std::string wearer = upper(character_class);
    for (int slot = 0; slot < kWardrobeSlotCount; ++slot) {
        WardrobePiece& piece = pieces[static_cast<size_t>(slot)];
        std::string mesh = base.meshes[static_cast<size_t>(slot)];
        std::string texture;
        const size_t location =
            static_cast<size_t>(kWardrobeEquipLocations[static_cast<size_t>(slot)]);
        const ItemStats* item = nullptr;
        if (inventory != nullptr && inventory->occupied(location)) {
            item = &inventory->slots()[location].item;
        }
        if (item != nullptr) {
            piece.worn = true;
            // `GetDataGroupsMatchingName(L"WARDROBE")`: every matching group, in
            // order; `GetDataValue` keeps what it had when a key is missing.
            for (const ItemStats::Wardrobe& group : item->wardrobe) {
                if (upper(group.character_class) != wearer) continue;
                if (group.has_texture) texture = group.texture;
                if (group.has_mesh) mesh = group.mesh;
            }
            // A helmet (ISA 0xc) or shoulder piece (0x14) that would draw the
            // class's own mesh draws none (the literal is empty).
            const std::string& own = base.meshes[static_cast<size_t>(slot)];
            if ((unit_types.isa(item->unittype, unit_type::kHelmet) ||
                 unit_types.isa(item->unittype, unit_type::kShoulderArmor)) &&
                mesh == own) {
                mesh.clear();
            }
        }
        piece.mesh = mesh;
        // The slot's image: the class's own (`+0x60 + slot * 8`), unless an item
        // is worn, the class has a skin (`+0x58`) and the item named a texture.
        const bool item_texture = item != nullptr && !base.skin.empty() && !texture.empty();
        piece.texture = item_texture ? texture : base.textures[static_cast<size_t>(slot)];
    }
    return pieces;
}

bool wardrobe_keeps_body_submesh(const std::string& material,
                                 const std::array<bool, kWardrobeSlotCount>& drawn) {
    // the first pass over the base model's submeshes.
    const std::string name = upper(material);
    if (name.find("BASE") != std::string::npos) return true;
    bool keep = false;
    for (int slot = 0; slot < kWardrobeSlotCount; ++slot) {
        const bool first = contains(name, wardrobe_slot_name(slot));
        const bool second = !first && contains(name, wardrobe_slot_name2(slot));
        // The slot's own word counts for chest, gloves and boots only (the
        // counter that starts at -3 and is tested `1 < n`); the second word for
        // any slot that has one.
        const bool counts = second || (first && slot < static_cast<int>(WardrobeSlot::Helmet));
        if (counts && !drawn[static_cast<size_t>(slot)]) keep = true;
    }
    return keep;
}

bool wardrobe_keeps_piece_submesh(int slot, const std::string& material) {
    // the second pass, over each slot's mesh.
    const std::string name = upper(material);
    return contains(name, wardrobe_slot_name(slot)) || contains(name, wardrobe_slot_name2(slot));
}

bool wardrobe_skin_covers(const std::string& material) {
    // `CGenericModel::setTextureOverride`,
    // with "HELMET" and "SHOULDERS": a material naming either is left alone.
    const std::string name = upper(material);
    return name.find("HELMET") == std::string::npos && name.find("SHOULDERS") == std::string::npos;
}

void wardrobe_blend(uint8_t* skin, const uint8_t* overlay, size_t pixels) {
    // `a / 255.0f` and `1.0f - that`, single floats,
    // `src * a + dst * (1 - a)` per channel, truncated (`cvttss2si`).
    for (size_t i = 0; i < pixels; ++i) {
        const uint8_t* src = overlay + i * 4;
        uint8_t* dst = skin + i * 4;
        if (src[3] == 0xff) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        } else {
            const float a = static_cast<float>(src[3]) / 255.0f;
            const float rest = 1.0f - a;
            for (int c = 0; c < 3; ++c) {
                const float mixed =
                    static_cast<float>(src[c]) * a + static_cast<float>(dst[c]) * rest;
                dst[c] = static_cast<uint8_t>(static_cast<int>(mixed));
            }
        }
        dst[3] = 0xff;
    }
}

// PORT: a wardrobe slot's base mesh set.
bool set_wardrobe_base_mesh(WardrobeBase* wardrobe, WardrobeSlot slot, const std::string& mesh) {
    if (wardrobe == nullptr) return false;
    wardrobe->meshes[static_cast<size_t>(slot)] = mesh;
    return true;
}

// PORT: a wardrobe slot's base texture set.
bool set_wardrobe_base_texture(WardrobeBase* wardrobe, WardrobeSlot slot,
                               const std::string& texture) {
    if (wardrobe == nullptr) return false;
    // The image is the renderer's to load from the path, and a path that names
    // no file composites nothing (`dress_player`).
    wardrobe->textures[static_cast<size_t>(slot)] = texture;
    return true;
}

}  // namespace tl::core
