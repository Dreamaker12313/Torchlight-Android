// The renderer's implementation: the main menu's debug bestiary (the engine's own tool).
// Every unit under `media/units/monsters/` is placed in turn by `spawn_unit()` on the
// backdrop's `Point of Interest`, so the screen shows what a level draws.
#include "render/renderer_impl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace tl::render {
namespace {

constexpr const char* kUnitsDir = "media/units/monsters/";

/// The screen's rows, top to bottom.
enum BestiaryRow { kFolderRow = 0, kUnitRow = 1, kAnimationRow = 2 };

/// The right stick: degrees a second the unit turns at full deflection, how
/// fast (share of the framing distance a second) the camera comes in, and how
/// close and how far it goes.
constexpr float kTurnDegreesPerSecond = 150.0f;
constexpr float kZoomPerSecond = 0.9f;
constexpr float kNearestZoom = 0.2f;
constexpr float kFarthestZoom = 3.0f;

/// A dumped frame is taken after this long, in steps of a thirtieth, so an
/// idle has moved and a particle body has had time to emit.
constexpr int kDumpSteps = 15;
constexpr float kDumpStep = 1.0f / 30.0f;

int wrapped(int index, int count) { return ((index % count) + count) % count; }

std::string basename_of(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string row_caption(const char* label, int index, int count, bool zero_is_none) {
    std::string at = zero_is_none ? (index == 0 ? std::string("-") : std::to_string(index))
                                  : std::to_string(index + 1);
    return std::string(label) + "  " + at + " / " + std::to_string(count);
}

}  // namespace

/// Put the chosen unit on the spot, replacing whatever stood there, and list
/// the animations its model ships.
void Renderer::Impl::place_bestiary_unit() {
    Impl& m = *this;
    Bestiary& b = m.bestiary;
    m.destroy_spawned_unit(b.entity, b.id);
    b.entity = nullptr;
    b.id = 0;
    b.mesh.clear();
    b.status.clear();
    b.animations.clear();
    b.animation = 0;
    if (b.units.empty()) return;

    const BestiaryUnit& unit = b.units[static_cast<size_t>(b.unit)];
    m.index_unit(unit.name);
    b.entity = m.spawn_unit(unit.name, b.where, /*owner=*/0);
    if (b.entity == nullptr) {
        const std::string mesh = m.stats_for_unit(unit.name).mesh;
        if (mesh.empty()) {
            b.status = "no MESHFILE up its BASEFILE chain";
        } else if (resolve_archive_path(mesh + ".MESH").empty()) {
            b.status = "no " + basename_of(mesh) + ".MESH in the archive";
        } else {
            b.status = "its mesh did not load (see the log)";
        }
        return;
    }
    for (const SpawnedUnit& made : m.spawned[0]) {
        if (made.entity == b.entity) b.id = made.id;
    }
    if (Ogre::SceneNode* node = b.entity->getParentSceneNode()) {
        node->setOrientation(Ogre::Quaternion(Ogre::Degree(b.turn_degrees), Ogre::Vector3::UNIT_Y) *
                             b.facing);
    }
    b.mesh = b.entity->getMesh()->getName();

    // Every `.SKELETON` beside the mesh but its own: the animation files a
    // unit's states resolve to (`Idle`, `Attack1`, `Die_Armed2`, ...).
    const std::string dir = lowered(dir_of_asset(b.mesh));
    const std::string own = lowered(basename_of(b.entity->getMesh()->getSkeletonName()));
    for (const std::string& path : archive_paths_under(dir, ".skeleton")) {
        if (lowered(dir_of_asset(path)) != dir) continue;
        const std::string file = basename_of(path);
        if (lowered(file) == own) continue;
        b.animations.push_back(file.substr(0, file.size() - std::string(".skeleton").size()));
    }
    std::sort(b.animations.begin(), b.animations.end(),
              [](const std::string& x, const std::string& y) { return lowered(x) < lowered(y); });
}

/// The chosen animation onto the unit, looping; 0 puts back the idle it was
/// placed on, which it fidgets from as a level's units do.
void Renderer::Impl::play_bestiary_animation() {
    Impl& m = *this;
    Bestiary& b = m.bestiary;
    if (b.entity == nullptr) return;
    const std::string wanted = b.animation == 0
                                   ? std::string(kUnitIdleAnimation)
                                   : b.animations[static_cast<size_t>(b.animation - 1)];
    for (auto& entry : m.scene_units) {
        SceneUnit& unit = entry.second;
        if (unit.entity != b.entity) continue;
        Ogre::AnimationState* started = nullptr;
        if (!m.play_scene_animation(b.entity, unit.mesh, wanted, /*loops=*/true, unit.rate,
                                    /*optional=*/false, &started, unit.armed)) {
            break;
        }
        m.remember_state(b.entity, wanted, started);
        // A fidget in progress lets go of the skeleton.
        m.fidgets.erase(b.entity);
        unit.playing = wanted;
        unit.playing_state = started;
        unit.last_time = 0.0f;
        break;
    }
}

/// The camera on the unit from the side the backdrop's camera looks from, at
/// the distance that fits its bounds into the view (times `zoom`).
void Renderer::Impl::place_bestiary_camera() {
    Impl& m = *this;
    Bestiary& b = m.bestiary;
    if (m.camera == nullptr || m.camera_node == nullptr) return;
    // A model that draws nothing of its own (the wisp is a bone and its
    // particles) is framed as something of a person's size.
    Ogre::Vector3 target = b.where + Ogre::Vector3(0.0f, 1.0f, 0.0f);
    float radius = 1.5f;
    if (b.entity != nullptr) {
        Ogre::AxisAlignedBox box = b.entity->getWorldBoundingBox(true);
        // A `RENDERALWAYS` mesh already met in a level carries the
        // never-culled box (apply_render_always()); it is framed by its own.
        if (b.entity->getParentSceneNode() != nullptr &&
            authored_mesh_bounds(b.entity) != b.entity->getMesh()->getBounds()) {
            box = authored_mesh_bounds(b.entity);
            box.transform(b.entity->getParentSceneNode()->_getFullTransform());
        }
        if (box.isFinite() && box.getHalfSize().length() > 0.05f) {
            target = box.getCenter();
            radius = std::max(0.25f, box.getHalfSize().length());
        }
    }
    Ogre::Vector3 from = b.camera_position - b.where;
    if (from.isZeroLength()) from = Ogre::Vector3(0.0f, 0.5f, 1.0f);
    from.normalise();
    const float half_fov = std::max(0.05f, m.camera->getFOVy().valueRadians() * 0.5f);
    const float fit = radius / std::sin(half_fov);
    m.camera_node->setPosition(target + from * fit * b.zoom);
    m.camera_node->lookAt(target, Ogre::Node::TS_WORLD);
    TL_LOG_INFO("bestiary: camera %.2f from (%.2f, %.2f, %.2f), radius %.2f, near %.2f",
                fit * b.zoom, target.x, target.y, target.z, radius,
                static_cast<double>(m.camera->getNearClipDistance()));
}

/// The text block: what the unit is, what draws it, and what it plays.
std::string Renderer::Impl::bestiary_text() const {
    const Impl& m = *this;
    const Bestiary& b = m.bestiary;
    std::ostringstream out;
    out << "BESTIARY  " << (b.unit + 1) << " / " << b.units.size() << "\n";
    if (b.units.empty()) {
        out << "no units under " << kUnitsDir << "\n";
    } else {
        const BestiaryUnit& unit = b.units[static_cast<size_t>(b.unit)];
        const tl::core::UnitStats stats = m.stats_for_unit(unit.name);
        out << (stats.display_name.empty() ? unit.name : stats.display_name) << "\n"
            << "NAME " << unit.name;
        if (!stats.unittype.empty()) out << "   UNITTYPE " << stats.unittype;
        out << "\n" << unit.folder << "/" << basename_of(unit.file) << "\n";
        if (b.entity == nullptr) {
            out << "NOT PLACED: " << b.status << "\n";
        } else {
            out << "mesh " << basename_of(b.mesh) << "   scale " << std::fixed;
            out.precision(2);
            out << stats.scale << "\n";
            out << "    " << dir_of_asset(b.mesh) << "\n";
            const Ogre::Vector3 size =
                authored_mesh_bounds(b.entity) != b.entity->getMesh()->getBounds()
                    ? authored_mesh_bounds(b.entity).getSize()
                    : b.entity->getBoundingBox().getSize();
            out << "size " << size.x << " x " << size.y << " x " << size.z << "   submeshes "
                << b.entity->getNumSubEntities() << "\n";
            out << "animation "
                << (b.animation == 0 ? std::string("idle as placed")
                                     : b.animations[static_cast<size_t>(b.animation - 1)])
                << "   (" << b.animations.size() << " files)\n";
        }
    }
    out << "\nLeft/Right: next   LB/RB: ten at a time\n"
        << "Right stick: turn and zoom   B: back";
    return out.str();
}

void Renderer::Impl::refresh_bestiary_screen() {
    Impl& m = *this;
    const Bestiary& b = m.bestiary;
    int folder = 0;
    for (size_t i = 0; i < b.folders.size(); ++i) {
        if (b.folders[i] <= b.unit) folder = static_cast<int>(i);
    }
    m.menu.set_row_caption(
        kFolderRow, row_caption("Folder", folder, static_cast<int>(b.folders.size()), false));
    m.menu.set_row_caption(kUnitRow,
                           row_caption("Unit", b.unit, static_cast<int>(b.units.size()), false));
    m.menu.set_row_caption(kAnimationRow, row_caption("Animation", b.animation,
                                                      static_cast<int>(b.animations.size()), true));
    m.menu.set_editor_info(bestiary_text());
}

void Renderer::open_bestiary() {
    Impl& m = *impl_;
    Impl::Bestiary& b = m.bestiary;
    if (b.open) return;

    m.ensure_monster_index();
    b.units.clear();
    b.folders.clear();
    for (const auto& [name, path] : m.monster_files) {
        Impl::BestiaryUnit unit;
        unit.name = name;
        unit.file = path;
        const std::string dir = dir_of_asset(path);
        unit.folder = dir.size() > std::string(kUnitsDir).size()
                          ? lowered(dir.substr(std::string(kUnitsDir).size(),
                                               dir.size() - std::string(kUnitsDir).size() - 1))
                          : std::string(".");
        b.units.push_back(std::move(unit));
    }
    std::sort(b.units.begin(), b.units.end(),
              [](const Impl::BestiaryUnit& x, const Impl::BestiaryUnit& y) {
                  return lowered(x.file) < lowered(y.file);
              });
    for (size_t i = 0; i < b.units.size(); ++i) {
        if (i == 0 || b.units[i].folder != b.units[i - 1].folder) {
            b.folders.push_back(static_cast<int>(i));
        }
    }

    if (m.camera_node != nullptr) {
        b.camera_position = m.camera_node->getPosition();
        b.camera_orientation = m.camera_node->getOrientation();
    }
    // The pet steps aside for whatever stands in its place.
    b.hidden_pets.clear();
    for (const auto& entry : m.menu_pets) {
        if (entry.second == nullptr) continue;
        Ogre::SceneNode* node = entry.second->getParentSceneNode();
        if (node == nullptr || !entry.second->isVisible()) continue;
        node->setVisible(false);
        b.hidden_pets.push_back(node);
    }
    if (!m.menu_pet_spot(b.where, b.facing)) {
        // A backdrop without the marker: beside the character instead.
        b.facing = Ogre::Quaternion::IDENTITY;
        b.where = m.subject_node != nullptr
                      ? m.subject_node->getPosition() + Ogre::Vector3(3.0f, 0.0f, 0.0f)
                      : Ogre::Vector3::ZERO;
        TL_LOG_WARN(
            "bestiary: no Point of Interest in this layout; units stand beside the "
            "character");
    }
    b.unit = 0;
    b.animation = 0;
    b.turn_degrees = 0.0f;
    b.zoom = 1.0f;

    m.menu.show_editor({"Folder", "Unit", "Animation"});
    b.open = true;
    m.place_bestiary_unit();
    m.place_bestiary_camera();
    m.refresh_bestiary_screen();
    TL_LOG_INFO("bestiary: %zu unit(s) in %zu folder(s)", b.units.size(), b.folders.size());
}

void Renderer::close_bestiary() {
    Impl& m = *impl_;
    Impl::Bestiary& b = m.bestiary;
    if (!b.open) return;
    b.open = false;
    m.destroy_spawned_unit(b.entity, b.id);
    b.entity = nullptr;
    b.id = 0;
    for (Ogre::SceneNode* node : b.hidden_pets) node->setVisible(true);
    b.hidden_pets.clear();
    if (m.camera_node != nullptr) {
        m.camera_node->setPosition(b.camera_position);
        m.camera_node->setOrientation(b.camera_orientation);
    }
    b.units.clear();
    b.folders.clear();
    b.animations.clear();
    TL_LOG_INFO("bestiary: closed");
}

void Renderer::step_bestiary(int row, int step) {
    Impl& m = *impl_;
    Impl::Bestiary& b = m.bestiary;
    if (!b.open || b.units.empty()) return;
    if (row == kFolderRow || row == kUnitRow) {
        if (row == kFolderRow) {
            int folder = 0;
            for (size_t i = 0; i < b.folders.size(); ++i) {
                if (b.folders[i] <= b.unit) folder = static_cast<int>(i);
            }
            folder = wrapped(folder + step, static_cast<int>(b.folders.size()));
            b.unit = b.folders[static_cast<size_t>(folder)];
        } else {
            b.unit = wrapped(b.unit + step, static_cast<int>(b.units.size()));
        }
        m.place_bestiary_unit();
        m.place_bestiary_camera();
        const Impl::BestiaryUnit& unit = b.units[static_cast<size_t>(b.unit)];
        if (b.entity != nullptr) {
            TL_LOG_INFO("bestiary: %d/%zu %s (%s) placed: %s, %zu animation file(s)", b.unit + 1,
                        b.units.size(), unit.name.c_str(), unit.file.c_str(), b.mesh.c_str(),
                        b.animations.size());
        } else {
            TL_LOG_WARN("bestiary: %d/%zu %s (%s) NOT placed: %s", b.unit + 1, b.units.size(),
                        unit.name.c_str(), unit.file.c_str(), b.status.c_str());
        }
    } else if (row == kAnimationRow) {
        if (b.entity == nullptr || b.animations.empty()) return;
        b.animation = wrapped(b.animation + step, static_cast<int>(b.animations.size()) + 1);
        m.play_bestiary_animation();
        TL_LOG_INFO("bestiary: %s plays %s", b.units[static_cast<size_t>(b.unit)].name.c_str(),
                    b.animation == 0 ? kUnitIdleAnimation
                                     : b.animations[static_cast<size_t>(b.animation - 1)].c_str());
    } else {
        return;
    }
    m.refresh_bestiary_screen();
}

void Renderer::steer_bestiary(const tl::core::InputState& input, float delta) {
    Impl& m = *impl_;
    Impl::Bestiary& b = m.bestiary;
    if (!b.open) return;
    if (std::abs(input.look_x) > 0.05f) {
        b.turn_degrees =
            std::fmod(b.turn_degrees + input.look_x * kTurnDegreesPerSecond * delta, 360.0f);
        if (b.entity != nullptr) {
            if (Ogre::SceneNode* node = b.entity->getParentSceneNode()) {
                node->setOrientation(
                    Ogre::Quaternion(Ogre::Degree(b.turn_degrees), Ogre::Vector3::UNIT_Y) *
                    b.facing);
            }
        }
    }
    if (std::abs(input.zoom) > 0.05f) {
        b.zoom =
            std::clamp(b.zoom - input.zoom * kZoomPerSecond * delta, kNearestZoom, kFarthestZoom);
        m.place_bestiary_camera();
    }
}

bool Renderer::dump_bestiary(const std::string& png_path, const std::string& picks) {
    Impl& m = *impl_;
    if (!m.window) return false;
    m.loading.hide();
    if (!m.menu.create(kGameGroup, m.config.width, m.config.height)) {
        TL_LOG_WARN("the main menu could not be built; nothing to dump");
        return false;
    }
    open_bestiary();
    Impl::Bestiary& b = m.bestiary;

    auto settle_and_dump = [&](const std::string& path) {
        for (int i = 0; i < kDumpSteps; ++i) {
            m.advance(kDumpStep);
            // Rendered, not just stepped: ParticleUniverse's systems and the
            // material controllers advance on OGRE's frame events only.
            if (!m.root->renderOneFrame(kDumpStep)) break;
        }
        return dump_frame(path);
    };

    // `<key>=<value>,...`, as dump_bestiary() is documented.
    bool all = false;
    std::stringstream list(picks);
    std::string pick;
    while (std::getline(list, pick, ',')) {
        const size_t eq = pick.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lowered(pick.substr(0, eq));
        const std::string value = pick.substr(eq + 1);
        if (key == "unit") {
            if (lowered(value) == "all") {
                all = true;
                continue;
            }
            int wanted = -1;
            const bool number =
                !value.empty() && std::all_of(value.begin(), value.end(), [](char c) {
                    return std::isdigit(static_cast<unsigned char>(c)) != 0;
                });
            if (number) {
                wanted = std::atoi(value.c_str()) - 1;
            } else {
                for (size_t i = 0; i < b.units.size(); ++i) {
                    if (b.units[i].name == lowered(value)) wanted = static_cast<int>(i);
                }
            }
            if (wanted < 0 || wanted >= static_cast<int>(b.units.size())) {
                TL_LOG_WARN("bestiary: no unit '%s'", value.c_str());
                continue;
            }
            step_bestiary(kUnitRow, wanted - b.unit);
        } else if (key == "animation") {
            step_bestiary(kAnimationRow, std::atoi(value.c_str()) - b.animation);
        } else if (key == "zoom") {
            b.zoom = std::clamp(static_cast<float>(std::atof(value.c_str())), kNearestZoom,
                                kFarthestZoom);
            m.place_bestiary_camera();
        } else if (key == "turn") {
            b.turn_degrees = static_cast<float>(std::atof(value.c_str()));
            if (b.entity != nullptr && b.entity->getParentSceneNode() != nullptr) {
                b.entity->getParentSceneNode()->setOrientation(
                    Ogre::Quaternion(Ogre::Degree(b.turn_degrees), Ogre::Vector3::UNIT_Y) *
                    b.facing);
            }
        } else {
            TL_LOG_WARN("bestiary: no pick called '%s'", key.c_str());
        }
    }

    bool ok = true;
    if (all) {
        // One frame per unit: `<stem>_<NNN>_<name>.png` beside `png_path`.
        std::string stem = png_path;
        const size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos && stem.find_first_of("/\\", dot) == std::string::npos) {
            stem.resize(dot);
        }
        size_t placed = 0;
        for (size_t i = 0; i < b.units.size(); ++i) {
            step_bestiary(kUnitRow, static_cast<int>(i) - b.unit);
            std::string name = b.units[i].name;
            for (char& c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
            char number[24];
            std::snprintf(number, sizeof number, "%03zu", i + 1);
            ok = settle_and_dump(stem + "_" + number + "_" + name + ".png") && ok;
            if (b.entity != nullptr) ++placed;
        }
        TL_LOG_INFO("bestiary: %zu of %zu unit(s) placed", placed, b.units.size());
    } else {
        ok = settle_and_dump(png_path);
    }
    close_bestiary();
    m.menu.destroy();
    return ok;
}

}  // namespace tl::render
