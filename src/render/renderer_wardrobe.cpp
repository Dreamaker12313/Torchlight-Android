// The main menu's debug wardrobe editor: the port's own tool. A look is worn by putting its
// item in the inventory and letting the game's own CWardrobe::update draw it.
#include "render/renderer_impl.h"

#include "core/unit_types.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <utility>

namespace tl::render {
namespace {

/// The class the editor dresses.
constexpr const char* kWardrobeClass = "Alchemist";

/// The right stick: degrees a second the character turns at full deflection,
/// and how fast (share of the backdrop's distance a second) and how close the
/// camera comes.
constexpr float kTurnDegreesPerSecond = 150.0f;
constexpr float kZoomPerSecond = 0.9f;
constexpr float kNearestZoom = 0.25f;

bool same_word(const std::string& a, const std::string& b) { return lowered(a) == lowered(b); }

std::string basename_of(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

/// Every look a wardrobe slot can give the class: the class's own first, then
/// each distinct (mesh, texture) `wardrobe_pieces` settles for an item that
/// the slot's location takes, grouped by mesh.
std::vector<Renderer::Impl::WardrobeLook> Renderer::Impl::wardrobe_armour_looks(int slot) {
    Impl& m = *this;
    using tl::core::wardrobe_pieces;
    const size_t at = static_cast<size_t>(slot);
    const int location = tl::core::kWardrobeEquipLocations[at];
    const tl::core::WardrobePiece own =
        wardrobe_pieces(m.wardrobe_base, nullptr, m.wardrobe_class, m.unit_types)[at];

    std::vector<WardrobeLook> looks;
    std::map<std::pair<std::string, std::string>, size_t> seen;
    tl::core::Inventory probe;
    for (const auto& entry : m.items.all()) {
        const tl::core::ItemStats& item = entry.second;
        if (!probe.can_equip_into_location(item, m.unit_types, location)) continue;
        probe.clear();
        probe.pickup_into_slot(item, 1, location);
        const tl::core::WardrobePiece piece =
            wardrobe_pieces(m.wardrobe_base, &probe, m.wardrobe_class, m.unit_types)[at];
        // An item with nothing of hers on it draws as the class's own.
        if (piece.mesh == own.mesh && piece.texture == own.texture) continue;
        const auto key = std::make_pair(lowered(piece.mesh), lowered(piece.texture));
        const auto found = seen.find(key);
        if (found != seen.end()) {
            ++looks[found->second].sharing;
            continue;
        }
        seen.emplace(key, looks.size());
        looks.push_back({&item, piece.mesh, piece.texture, 1});
    }
    std::sort(looks.begin(), looks.end(), [](const WardrobeLook& a, const WardrobeLook& b) {
        if (lowered(a.mesh) != lowered(b.mesh)) return lowered(a.mesh) < lowered(b.mesh);
        if (lowered(a.texture) != lowered(b.texture))
            return lowered(a.texture) < lowered(b.texture);
        return a.item->display_name < b.item->display_name;
    });
    looks.insert(looks.begin(), WardrobeLook{nullptr, own.mesh, own.texture, 1});
    return looks;
}

/// Where a weapon goes: the right hand when its gate takes it, else the left
/// (a bow), else nowhere (-1).
int Renderer::Impl::wardrobe_weapon_location(const tl::core::ItemStats& item) {
    Impl& m = *this;
    static const tl::core::Inventory gate;
    for (const int location :
         {tl::core::equip_location::kRightHand, tl::core::equip_location::kLeftHand}) {
        if (gate.can_equip_into_location(item, m.unit_types, location)) return location;
    }
    return -1;
}

/// Every weapon with a mesh, one per mesh, grouped by unit type; an empty hand
/// first.
std::vector<Renderer::Impl::WardrobeLook> Renderer::Impl::wardrobe_weapon_looks() {
    Impl& m = *this;
    std::vector<WardrobeLook> looks;
    std::map<std::string, size_t> seen;
    for (const auto& entry : m.items.all()) {
        const tl::core::ItemStats& item = entry.second;
        if (item.mesh.empty()) continue;
        if (!m.unit_types.isa(m.unit_types.id_of(item.unittype), tl::core::unit_type::kWeapon)) {
            continue;
        }
        if (wardrobe_weapon_location(item) < 0) continue;
        const std::string key = lowered(item.mesh);
        const auto found = seen.find(key);
        if (found != seen.end()) {
            ++looks[found->second].sharing;
            continue;
        }
        seen.emplace(key, looks.size());
        looks.push_back({&item, item.mesh, std::string(), 1});
    }
    std::sort(looks.begin(), looks.end(), [](const WardrobeLook& a, const WardrobeLook& b) {
        if (lowered(a.item->unittype) != lowered(b.item->unittype)) {
            return lowered(a.item->unittype) < lowered(b.item->unittype);
        }
        return a.item->display_name < b.item->display_name;
    });
    looks.insert(looks.begin(), WardrobeLook{});
    return looks;
}

/// Put a row's chosen look into the inventory (nothing redrawn yet).
void Renderer::Impl::wear_wardrobe_row(const WardrobeRow& row) {
    Impl& m = *this;
    tl::core::Inventory& inventory = m.player_inventory;
    const WardrobeLook& look = row.looks[static_cast<size_t>(row.index)];
    auto empty = [&](int location) {
        const size_t at = static_cast<size_t>(location);
        if (inventory.occupied(at)) inventory.remove_item(at, inventory.slots()[at].count);
    };
    if (row.slot >= 0) {
        const int location = tl::core::kWardrobeEquipLocations[static_cast<size_t>(row.slot)];
        empty(location);
        if (look.item != nullptr) inventory.pickup_into_slot(*look.item, 1, location);
        return;
    }
    empty(tl::core::equip_location::kRightHand);
    empty(tl::core::equip_location::kLeftHand);
    if (look.item != nullptr) {
        inventory.pickup_into_slot(*look.item, 1, wardrobe_weapon_location(*look.item));
    }
}

namespace {

template <typename Row>
std::string row_caption(const Row& row) {
    const int count = static_cast<int>(row.looks.size()) - 1;
    return row.label + "  " + (row.index == 0 ? std::string("-") : std::to_string(row.index)) +
           " / " + std::to_string(count);
}

}  // namespace

/// The text block: each row's look by name, and what draws it.
std::string Renderer::Impl::wardrobe_editor_text() const {
    const Impl& m = *this;
    std::ostringstream out;
    out << "WARDROBE - " << kWardrobeClass << "\n";
    for (const WardrobeRow& row : m.wardrobe_editor.rows) {
        const WardrobeLook& look = row.looks[static_cast<size_t>(row.index)];
        out << row.label << ": ";
        if (look.item == nullptr) {
            out << (row.slot >= 0 ? "her own" : "empty hand");
        } else {
            out << look.item->display_name;
            if (look.sharing > 1) out << " (+" << (look.sharing - 1) << " alike)";
        }
        out << "\n    ";
        if (look.mesh.empty() && look.texture.empty()) {
            out << "-";
        } else {
            out << (look.mesh.empty() ? std::string("no mesh") : basename_of(look.mesh));
            if (!look.texture.empty()) out << "  |  " << basename_of(look.texture);
        }
        out << "\n";
    }
    out << "\nLeft/Right: next look   LB/RB: ten at a time\n"
        << "Right stick: turn and zoom   B: back";
    return out.str();
}

/// Re-dress after a change and refresh the screen.
void Renderer::Impl::redress_wardrobe_editor() {
    Impl& m = *this;
    m.sync_equipped_weapon();
    for (size_t i = 0; i < m.wardrobe_editor.rows.size(); ++i) {
        m.menu.set_row_caption(i, row_caption(m.wardrobe_editor.rows[i]));
    }
    m.menu.set_editor_info(wardrobe_editor_text());
}

/// The camera at `zoom` of the backdrop's distance from the character, turned
/// onto her as it comes in (at 1 it is exactly where the backdrop put it).
void Renderer::Impl::place_wardrobe_camera() {
    Impl& m = *this;
    Impl::WardrobeEditor& editor = m.wardrobe_editor;
    if (m.camera_node == nullptr || m.subject == nullptr) return;
    const Ogre::Vector3 target = m.subject->getWorldBoundingBox(true).getCenter();
    m.camera_node->setPosition(target + (editor.camera_position - target) * editor.zoom);
    m.camera_node->setOrientation(editor.camera_orientation);
    if (editor.zoom >= 1.0f) return;
    m.camera_node->lookAt(target, Ogre::Node::TS_WORLD);
    const Ogre::Quaternion onto = m.camera_node->getOrientation();
    const float t = (1.0f - editor.zoom) / (1.0f - kNearestZoom);
    m.camera_node->setOrientation(
        Ogre::Quaternion::Slerp(std::min(1.0f, t), editor.camera_orientation, onto, true));
}

void Renderer::open_wardrobe() {
    Impl& m = *impl_;
    Impl::WardrobeEditor& editor = m.wardrobe_editor;
    if (editor.open) return;
    editor.previous_class = m.player_class;
    editor.saved_inventory = m.player_inventory;
    editor.had_weapon = m.has_weapon;
    editor.saved_weapon = m.weapon;

    if (!same_word(m.player_class, kWardrobeClass)) show_player_class(kWardrobeClass);
    if (!m.wardrobe_ready) wear_base_outfit();
    if (m.subject == nullptr || !m.wardrobe_ready) {
        TL_LOG_WARN("wardrobe editor: no %s to dress", kWardrobeClass);
    }
    m.ensure_item_index();
    m.load_unit_types();

    if (m.camera_node != nullptr) {
        editor.camera_position = m.camera_node->getPosition();
        editor.camera_orientation = m.camera_node->getOrientation();
    }
    if (m.subject_node != nullptr) editor.subject_orientation = m.subject_node->getOrientation();
    editor.zoom = 1.0f;

    editor.rows.clear();
    using tl::core::WardrobeSlot;
    const std::pair<const char*, WardrobeSlot> slots[] = {
        {"Helmet", WardrobeSlot::Helmet}, {"Shoulders", WardrobeSlot::Shoulders},
        {"Chest", WardrobeSlot::Chest},   {"Gloves", WardrobeSlot::Gloves},
        {"Boots", WardrobeSlot::Boots},
    };
    for (const auto& [label, slot] : slots) {
        Impl::WardrobeRow row;
        row.label = label;
        row.slot = static_cast<int>(slot);
        row.looks = m.wardrobe_armour_looks(row.slot);
        editor.rows.push_back(std::move(row));
    }
    Impl::WardrobeRow weapon;
    weapon.label = "Weapon";
    weapon.looks = m.wardrobe_weapon_looks();
    // She starts with the weapon she was holding, when it is in the list.
    for (size_t i = 1; i < weapon.looks.size(); ++i) {
        if (m.has_weapon && same_word(weapon.looks[i].mesh, m.weapon.mesh)) {
            weapon.index = static_cast<int>(i);
            break;
        }
    }
    editor.rows.push_back(std::move(weapon));

    m.player_inventory.clear();
    for (const Impl::WardrobeRow& row : editor.rows) m.wear_wardrobe_row(row);

    std::vector<std::string> captions;
    std::string counts;
    for (const Impl::WardrobeRow& row : editor.rows) {
        captions.push_back(row_caption(row));
        counts +=
            (counts.empty() ? "" : ", ") + row.label + " " + std::to_string(row.looks.size() - 1);
    }
    m.menu.show_editor(captions);
    editor.open = true;
    m.redress_wardrobe_editor();
    TL_LOG_INFO("wardrobe editor: %s; looks: %s", kWardrobeClass, counts.c_str());
}

void Renderer::close_wardrobe() {
    Impl& m = *impl_;
    Impl::WardrobeEditor& editor = m.wardrobe_editor;
    if (!editor.open) return;
    editor.open = false;
    editor.zoom = 1.0f;
    m.place_wardrobe_camera();
    if (m.subject_node != nullptr) m.subject_node->setOrientation(editor.subject_orientation);

    m.player_inventory = editor.saved_inventory;
    if (!editor.previous_class.empty() && !same_word(editor.previous_class, kWardrobeClass)) {
        show_player_class(editor.previous_class);
    } else {
        m.sync_equipped_weapon();
        if (editor.had_weapon) m.equip_weapon_item(editor.saved_weapon);
        m.player_class = editor.previous_class;
    }
    editor.rows.clear();
    TL_LOG_INFO("wardrobe editor: closed");
}

void Renderer::step_wardrobe(int row, int step) {
    Impl& m = *impl_;
    Impl::WardrobeEditor& editor = m.wardrobe_editor;
    if (!editor.open || row < 0 || row >= static_cast<int>(editor.rows.size())) return;
    Impl::WardrobeRow& chosen = editor.rows[static_cast<size_t>(row)];
    const int count = static_cast<int>(chosen.looks.size());
    if (count <= 1) return;
    // Wrapping, a bumper's ten included.
    chosen.index = ((chosen.index + step) % count + count) % count;
    m.wear_wardrobe_row(chosen);
    m.redress_wardrobe_editor();
    const Impl::WardrobeLook& look = chosen.looks[static_cast<size_t>(chosen.index)];
    TL_LOG_INFO("wardrobe editor: %s %d/%d %s (%s | %s)", chosen.label.c_str(), chosen.index,
                count - 1, look.item != nullptr ? look.item->name.c_str() : "-",
                look.mesh.empty() ? "-" : look.mesh.c_str(),
                look.texture.empty() ? "-" : look.texture.c_str());
}

void Renderer::steer_wardrobe(const tl::core::InputState& input, float delta) {
    Impl& m = *impl_;
    Impl::WardrobeEditor& editor = m.wardrobe_editor;
    if (!editor.open) return;
    if (m.subject_node != nullptr && std::abs(input.look_x) > 0.05f) {
        m.subject_node->yaw(Ogre::Degree(input.look_x * kTurnDegreesPerSecond * delta),
                            Ogre::Node::TS_WORLD);
    }
    if (std::abs(input.zoom) > 0.05f) {
        editor.zoom =
            std::clamp(editor.zoom - input.zoom * kZoomPerSecond * delta, kNearestZoom, 1.0f);
        m.place_wardrobe_camera();
    }
}

bool Renderer::dump_wardrobe(const std::string& png_path, const std::string& picks) {
    Impl& m = *impl_;
    if (!m.window) return false;
    m.loading.hide();
    if (!m.menu.create(kGameGroup, m.config.width, m.config.height)) {
        TL_LOG_WARN("the main menu could not be built; nothing to dump");
        return false;
    }
    open_wardrobe();
    // `<row>=<index>,...`, as the screen counts.
    std::stringstream list(picks);
    std::string pick;
    while (std::getline(list, pick, ',')) {
        const size_t eq = pick.find('=');
        if (eq == std::string::npos) continue;
        const std::string word = pick.substr(0, eq);
        const int wanted = std::atoi(pick.substr(eq + 1).c_str());
        bool found = false;
        for (size_t i = 0; i < m.wardrobe_editor.rows.size(); ++i) {
            if (!same_word(m.wardrobe_editor.rows[i].label, word)) continue;
            step_wardrobe(static_cast<int>(i), wanted - m.wardrobe_editor.rows[i].index);
            found = true;
        }
        if (!found) TL_LOG_WARN("wardrobe editor: no row called '%s'", word.c_str());
    }
    const bool ok = dump_frame(png_path);
    close_wardrobe();
    m.menu.destroy();
    return ok;
}

}  // namespace tl::render
