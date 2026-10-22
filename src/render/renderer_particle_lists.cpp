// The level's particle systems as lists: the index script events find theirs by, their
// destruction, and the transients' alive test (render/particle_systems.h).
#include "render/renderer_impl.h"

namespace tl::render {

void Renderer::Impl::particle_index_add(size_t i) {
    if (particle_index.stale) return;
    const auto put = [i](std::vector<size_t>& list) {
        list.insert(std::lower_bound(list.begin(), list.end(), i), i);
    };
    const ParticleSystem& system = particle_systems[i];
    put(particle_index.by_owner[{system.owner_nodes, system.owner}]);
    if (system.wrapper) {
        put(particle_index.wrapper_by_node[{system.node_owner, system.node}]);
        put(particle_index.wrappers_in[system.node_owner]);
    }
}

void Renderer::Impl::particle_index_remove(size_t i) {
    if (particle_index.stale) return;
    const auto drop = [i](std::vector<size_t>& list) {
        const auto at = std::lower_bound(list.begin(), list.end(), i);
        if (at != list.end() && *at == i) list.erase(at);
    };
    const ParticleSystem& system = particle_systems[i];
    drop(particle_index.by_owner[{system.owner_nodes, system.owner}]);
    if (system.wrapper) {
        drop(particle_index.wrapper_by_node[{system.node_owner, system.node}]);
        drop(particle_index.wrappers_in[system.node_owner]);
    }
}

ParticleRefs Renderer::Impl::indexed_particles(
    std::initializer_list<ParticleIndex::Key> keys,
    std::initializer_list<ParticleIndex::Key> wrapper_keys) {
    if (particle_index.stale) {
        particle_index.by_owner.clear();
        particle_index.wrapper_by_node.clear();
        particle_index.wrappers_in.clear();
        particle_index.stale = false;
        for (size_t i = 0; i < particle_systems.size(); ++i) particle_index_add(i);
    }
    std::vector<size_t> out;
    const auto take = [&out](const auto& map, const ParticleIndex::Key& key) {
        const auto found = map.find(key);
        if (found != map.end()) out.insert(out.end(), found->second.begin(), found->second.end());
    };
    for (const ParticleIndex::Key& key : keys) take(particle_index.by_owner, key);
    for (const ParticleIndex::Key& key : wrapper_keys) take(particle_index.wrapper_by_node, key);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    ParticleRefs systems;
    systems.reserve(out.size());
    for (const size_t i : out) systems.emplace_back(particle_systems[i]);
    return systems;
}

ParticleRefs Renderer::Impl::map_wrappers(const NodeMap& nodes) {
    (void)indexed_particles({}, {});
    ParticleRefs systems;
    const auto found = particle_index.wrappers_in.find(&nodes);
    if (found == particle_index.wrappers_in.end()) return systems;
    for (const size_t i : found->second) systems.emplace_back(particle_systems[i]);
    return systems;
}

ParticleRefs Renderer::Impl::link_systems(const NodeMap& nodes, int64_t node) {
    return indexed_particles({{&nodes, node}}, {});
}

ParticleRefs Renderer::Impl::link_or_free_systems(const NodeMap& nodes, int64_t node) {
    return indexed_particles({{&nodes, node}, {nullptr, node}}, {});
}

ParticleRefs Renderer::Impl::node_wrappers(const NodeMap& nodes, int64_t node) {
    return indexed_particles({}, {{&nodes, node}});
}

ParticleRefs Renderer::Impl::shown_systems(const NodeMap& nodes, int64_t node) {
    return indexed_particles({{&nodes, node}, {nullptr, node}}, {{&nodes, node}});
}

// PORT: drawing, a node's `Show`/`Hide` reaching the systems it placed; how many it reached.
size_t Renderer::Impl::show_node_particles(const tl::core::ScriptEvent& event, NodeMap& nodes) {
    size_t systems = 0;
    std::set<uint32_t> started;
    for (ParticleSystem& system : shown_systems(nodes, event.node_id)) {
        // A `Particle` node of the level's own: `CParticleWrapper::
        // setVisible`, its node and its system.
        if (system.wrapper && system.node == event.node_id && system.node_owner == &nodes) {
            set_torchlight_wrapper_visible(system.pu, event.on);
            continue;
        }
        if (system.owner != event.node_id) continue;
        // The link's own `CParticle`, not that of another copy of
        // its file (`CLayout::callFunctionOnObjects`
        // on the link's layout,
        // `+0x1f0`).
        if (system.owner_nodes != nullptr && system.owner_nodes != &nodes) continue;
        // Start/Stop Particle: CParticle::Start, CParticle::Stop(false); Show
        // and Hide via CLayout::setVisible.
        if (event.on) {
            restart_particle_system(system);
            if (system.cparticle != 0) started.insert(system.cparticle);
        } else {
            stop_particle_emission(system);
        }
        ++systems;
    }
    for (const uint32_t id : started) cparticle_started(id);
    return systems;
}

void Renderer::Impl::destroy_particle_holders(std::vector<Ogre::SceneNode*>& holders) {
    // One pass over the systems and the node maps for the whole list.
    std::set<const Ogre::SceneNode*> going;
    for (Ogre::SceneNode* holder : holders) {
        if (holder != nullptr) going.insert(holder);
    }
    if (going.empty()) {
        holders.clear();
        return;
    }
    size_t kept = 0;
    for (size_t i = 0; i < particle_systems.size(); ++i) {
        ParticleSystem& system = particle_systems[i];
        if (system.pu.system != nullptr && going.count(system.holder()) != 0) {
            // The holder is the system's own node, destroyed below.
            system.pu.node = nullptr;
            destroy_torchlight_particle(scene, system.pu);
            continue;
        }
        if (kept != i) particle_systems[kept] = std::move(system);
        ++kept;
    }
    if (kept != particle_systems.size()) particle_index.stale = true;
    particle_systems.resize(kept);
    // No node map may keep a placed node once it is gone (`node_sink`).
    const auto forget = [&going](NodeMap& nodes) {
        for (auto it = nodes.begin(); it != nodes.end();) {
            it = going.count(it->second) != 0 ? nodes.erase(it) : std::next(it);
        }
    };
    forget(scripted_nodes);
    for (NestedScript& machine : nested_scripts) {
        if (!machine.nodes.empty()) forget(machine.nodes);
    }
    for (Ogre::SceneNode* holder : holders) {
        if (holder == nullptr || going.erase(holder) == 0) continue;
        scripted_scene_nodes.erase(holder);
        scene->destroySceneNode(holder);
    }
    holders.clear();
}

// PORT: drawing, whether a system hanging from `holders` is still running; `stop` first lets
// each die out.
void Renderer::Impl::view_transients_alive(
    const std::vector<const std::vector<Ogre::SceneNode*>*>& lists, const std::vector<char>& stop,
    std::vector<char>& alive) {
    alive.assign(lists.size(), 0);
    std::unordered_map<const Ogre::SceneNode*, std::vector<size_t>> list_of;
    for (size_t i = 0; i < lists.size(); ++i) {
        for (const Ogre::SceneNode* holder : *lists[i]) {
            if (holder != nullptr) list_of[holder].push_back(i);
        }
    }
    for (ParticleSystem& system : particle_systems) {
        const Ogre::SceneNode* holder = system.holder();
        if (holder == nullptr) continue;
        const auto found = list_of.find(holder);
        if (found == list_of.end()) continue;
        for (size_t i : found->second) {
            if (stop[i] != 0) stop_particle_emission(system);
            if (!system.finished) alive[i] = 1;
        }
    }
}

}  // namespace tl::render
