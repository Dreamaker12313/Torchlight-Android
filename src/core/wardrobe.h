// What a character wears, per wardrobe slot: `CWardrobe::update(CInventory*)`,
// its slot loop and two submesh passes. The mesh
// merge is the renderer's (`Renderer::Impl::dress_player`).
// No OGRE here: images are RGBA bytes, meshes and textures are paths.
#pragma once

#include "core/inventory.h"
#include "core/unit_types.h"
#include "core/units.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tl::core {

/// `EWardrobeSlot`, the order of every table below.
enum class WardrobeSlot : int { Chest = 0, Gloves, Boots, Helmet, Shoulders };
inline constexpr int kWardrobeSlotCount = 5;

/// `KWardrobeEquipLocations`: the equip location each slot reads.
inline constexpr std::array<int, kWardrobeSlotCount> kWardrobeEquipLocations = {4, 2, 6, 3, 5};

/// `KWardrobeSlotNames` and `KWardrobeSlotNames2` (the static initialiser):
/// the words a submesh's material name is searched for. The second
/// table names only `FACE`, for the helmet slot.
const char* wardrobe_slot_name(int slot);
const char* wardrobe_slot_name2(int slot);

/// The class's own outfit, `CCharacter +0x4f0..+0x518` from the unit's
/// `WARDROBE_BASE`, `CHEST_BASE`, `BOOTS_BASE`, `GLOVES_BASE`,
/// `HELMET_BASE_MESH` and `SHOULDER_BASE_MESH` (`CCharacter::unitInit`),
/// as `CWardrobe::CWardrobe`
/// files them by slot.
struct WardrobeBase {
    std::string skin;                                      ///< `+0x58`'s image
    std::array<std::string, kWardrobeSlotCount> textures;  ///< `+0x60 + slot * 8`
    std::array<std::string, kWardrobeSlotCount> meshes;    ///< `+0x88 + slot * 8`
};

/// A `Monster` node's `WARDROBE` meshes: `CCharacter::setWardrobeChestMesh`,
/// `CCharacter::setWardrobeGlovesMesh`,
/// `CCharacter::setWardrobeBootsMesh`, `CCharacter::setWardrobeHelmMesh`,
/// `CCharacter::setWardrobeShoulderMesh`; inline on the console
/// in the node's setters.
/// Replaces the slot's class mesh; `CWardrobe::update` and
/// `CCharacter::getBones` follow. Returns whether the caller must dress again
/// (false with no wardrobe).
bool set_wardrobe_base_mesh(WardrobeBase* wardrobe, WardrobeSlot slot, const std::string& mesh);

/// A `Monster` node's `WARDROBE` textures: `CCharacter::setWardrobeChestTexture`,
/// `CCharacter::setWardrobeGlovesTexture`,
/// `CCharacter::setWardrobeBootsTexture`; inline on the console in the
/// setters, through
/// `CWardrobe::setBaseTexture`. Then `update` and
/// `getBones`, as above.
bool set_wardrobe_base_texture(WardrobeBase* wardrobe, WardrobeSlot slot,
                               const std::string& texture);

/// One slot as `update` settles it.
struct WardrobePiece {
    /// The mesh drawn for the slot; empty for none.
    std::string mesh;
    /// The image composited over the skin for the slot; empty for none.
    std::string texture;
    /// An item is worn at the slot's location.
    bool worn = false;
};

/// `CWardrobe::update`'s slot loop for `character_class` (unit
/// name, compared upper-cased) wearing `inventory` (null: the class outfit alone).
/// Matching `[WARDROBE]` groups overwrite in order, the last winning; a worn helmet or
/// shoulder piece left on the class's own mesh draws none.
std::array<WardrobePiece, kWardrobeSlotCount> wardrobe_pieces(const WardrobeBase& base,
                                                              const Inventory* inventory,
                                                              const std::string& character_class,
                                                              const UnitTypeHierarchy& unit_types);

/// The body's submeshes that stay: `BASE`, and a slot's own while
/// that slot draws no mesh; never the body's helmet or shoulders. `drawn[slot]` is
/// whether the slot has a mesh.
bool wardrobe_keeps_body_submesh(const std::string& material,
                                 const std::array<bool, kWardrobeSlotCount>& drawn);

/// A piece's submeshes that are taken: those whose
/// material names the slot's word, and for the helmet slot `FACE` too.
bool wardrobe_keeps_piece_submesh(int slot, const std::string& material);

/// `CGenericModel::setTextureOverride(texture, "HELMET", "SHOULDERS")`: the
/// composited skin goes on every submesh but those two, which keep the
/// materials their meshes came with.
bool wardrobe_skin_covers(const std::string& material);

/// The composite's blend, one RGBA8 overlay over the skin; mixed
/// in single floats and truncated.
void wardrobe_blend(uint8_t* skin, const uint8_t* overlay, size_t pixels);

}  // namespace tl::core
