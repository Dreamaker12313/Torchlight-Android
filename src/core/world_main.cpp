// The gameplay half of render/renderer.cpp (core/world.h).
#include "core/world.h"
#include "formats/dds.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

void World::load_tileset() {
    if (tileset_loaded) return;
    tileset_loaded = true;

    // `media/massfile.dat.ADM` is the master index: every room piece and prop in
    // the game, each a PIECE section carrying a GUID and the mesh that draws it.
    // A layout refers to pieces only by GUID, so this is the one table that
    // turns a menu backdrop into geometry.
    const std::string path = archive_path_unique("massfile.dat.ADM");
    if (path.empty()) {
        TL_LOG_WARN("no massfile.dat.ADM; room pieces cannot be resolved");
        return;
    }

    const std::vector<uint8_t> bytes = read_resource(path);
    if (bytes.empty()) return;

    const tl::formats::AdmFile index = tl::formats::parse_adm(bytes.data(), bytes.size());
    if (!index.ok) {
        TL_LOG_ERROR("could not parse '%s': %s", path.c_str(), index.error.c_str());
        return;
    }

    index.visit([&](const tl::formats::AdmSection& section) {
        // Only the level sets' `[PIECE]` groups: the console's MASSFILE holds every data file,
        // and a sound or a unit carries a GUID too.
        if (section.name != "PIECE") return;
        const int64_t guid = section.guid_or("GUID", 0);
        if (guid == 0) return;
        tileset_known.insert(guid);
        if (tl::core::upper_ascii(section.string_or("NAME", "")) == "ERROR") {
            tileset_default_guid = guid;
        }
        // Every FILE and COLLISIONFILE, in order, as
        // `CRoomPieceDataInformation::ParseRoomPieceData`
        // fills its two lists
        // (`FillArrayList`): 89 of the 1,619 pieces list several.
        std::vector<std::string> files;
        std::vector<std::string> collision_files;
        for (const tl::formats::AdmRecord& record : section.records) {
            if (record.text.empty()) continue;
            if (record.key == "FILE") files.push_back(record.text);
            if (record.key == "COLLISIONFILE") collision_files.push_back(record.text);
        }
        if (!files.empty()) tileset.emplace(guid, std::move(files));

        // The collision geometry, from the same record. TorchED's plaintext
        // twin of this file shows the pairing:
        //
        //     <STRING>FILE:media/levelsets/town1/blend_brick.mesh
        //     <STRING>COLLISIONFILE:media/levelsets/town1/collision_terrain.mesh
        //
        // Most pieces have none - only 5 of town1's 277 do, because the town's
        // collision is a handful of large hand-authored meshes rather than one
        // per tile - so its absence is normal and not worth a warning.
        if (!collision_files.empty()) tileset_collision.emplace(guid, std::move(collision_files));

        // `PRERENDER` / `POSTRENDER` (`CRoomPieceDataInformation`): which of
        // `CLevel::bake`'s three batches the piece goes to.
        if (section.bool_or("PRERENDER", false)) {
            tileset_render_phase.emplace(guid, -1);
        } else if (section.bool_or("POSTRENDER", false)) {
            tileset_render_phase.emplace(guid, 1);
        }
    });

    TL_LOG_INFO("tileset index: %zu piece(s) from %s, %zu with collision", tileset.size(),
                path.c_str(), tileset_collision.size());
}

bool World::animator_owns(Ogre::Entity* entity, const Ogre::AnimationState* state) const {
    const auto it = entity_animations.find(entity);
    if (it == entity_animations.end()) return false;
    return std::find(it->second.states.begin(), it->second.states.end(), state) !=
           it->second.states.end();
}

bool World::step_entity_animation(Ogre::Entity* entity, float delta) {
    // `CGenericModel::updateAnimation` on the
    // entity's animator: each record's own speed, the blends on the frame's
    // time as it is.
    const auto it = entity_animations.find(entity);
    if (it == entity_animations.end()) return false;
    it->second.animator.update(delta, false);
    write_entity_animation(it->second);
    return true;
}

bool World::particle_universe_takes(const std::string& layout_path) const {
    const std::string path = tl::core::upper_ascii(layout_path);
    for (const std::string& prefix : config.pu_particle_layouts) {
        if (prefix == "*") return true;
        if (path.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
}

}  // namespace tl::core
