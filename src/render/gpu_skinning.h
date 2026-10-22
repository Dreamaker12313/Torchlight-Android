// Skinning on the GPU: characters' vertices moved by their bones in the vertex shader. OGRE skins
// in software unless a material's vertex program skins, which RTSS writes only when a pass carries
// its `HardwareSkinning` sub-render state.
//
// Per material, never globally: the sub-render state goes into each skinned material's own pass
// render state. All or nothing per entity, as OGRE is: a character is handed over only when every
// submesh can be (blend data, few enough bones, an agreed weight count), else left to the CPU.
// Materials are shared by name and swapped at run time, so guard() draws a skinning material on a
// CPU-skinned or skeletonless renderable with a non-skinning copy; regenerating() puts skinning
// back before RTSS rewrites a material's shaders (`apply_rim_light()`), and step() hands back to
// the CPU anything that still slips through.
#pragma once

#include <OgreMaterial.h>
#include <OgrePrerequisites.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace Ogre::RTShader {
class ShaderGenerator;
}

namespace tl::render {

class GpuSkinning {
public:
    /// Whether characters are skinned on the GPU. A process-wide setting, so it
    /// survives `Renderer::restart()`; the debug menu flips it.
    static bool enabled();
    static void set_enabled(bool on);

    /// After the shader generator is up. Says once whether this device can.
    void start(Ogre::RTShader::ShaderGenerator* generator);
    /// Before the generator and the materials go.
    void stop();

    /// Once a frame, after the frame is queued: hand every skinned entity the
    /// CPU still skins to the GPU, and give back any whose material lost its
    /// skinning. Follows set_enabled() both ways.
    void step(Ogre::SceneManager* scene);

    /// From the view's renderable listener: replaces a skinning technique on a
    /// renderable the CPU skins with one that does not.
    void guard(Ogre::Renderable* renderable, Ogre::Technique** technique);

    /// For a stand-in material drawn in a renderable's place (the shadow map's
    /// `RenderShadow` variants): the bones and weights the renderable's entity
    /// is skinned with on the GPU; false when the CPU skins it, or nothing does.
    bool gpu_skin(const Ogre::Renderable* renderable, unsigned& bones, unsigned& weights) const;
    /// Gives `material` the skinning such a renderable needs, kept through
    /// regenerations as every marked material is.
    bool skin_material(const Ogre::MaterialPtr& material, unsigned bones, unsigned weights) {
        return active_ && generator_ != nullptr && mark(material, bones, weights);
    }

    /// From the material listener, between RTSS making a material's technique
    /// entry and writing its shaders: puts a marked material's skinning back.
    void regenerating(const Ogre::Material& material);

    bool available() const { return available_; }
    /// As the last step() found them: entities with a skeleton, and of those
    /// the ones OGRE skins on the GPU. A count of results, not of hand-overs.
    size_t skinned() const { return skinned_; }
    size_t on_gpu() const { return on_gpu_; }

private:
    struct Marked {
        Ogre::MaterialPtr material;  ///< held, so the key's address stays its own
        unsigned bones = 0;
        unsigned weights = 0;
    };

    bool hand_over(Ogre::Entity* entity);
    /// step()'s answer to an entity on the GPU whose materials no longer fit it: handed over
    /// again (true), or left to OGRE to skin on the CPU.
    bool refit(Ogre::Entity* entity);
    /// For guard(): the technique of a marked copy of a GPU-skinned submesh's material that
    /// skins for its weight count and bones, when its own does not; null otherwise.
    Ogre::Technique* fitting_technique(Ogre::SubEntity* sub) const;
    /// Whether a submesh's material skins for its weights and at least its bones (or is
    /// not marked at all).
    bool material_fits(const Ogre::SubEntity* sub) const;
    /// Whether every submesh of an entity on the GPU still wears a material
    /// marked for its own weight count.
    bool weights_agree(const Ogre::Entity* entity) const;
    /// `held` again under its own name for `weights` blend weights, brought up
    /// to date with `held`: for a submesh whose material skins for another count.
    Ogre::MaterialPtr weight_variant(const Ogre::MaterialPtr& held, unsigned weights);
    bool mark(const Ogre::MaterialPtr& material, unsigned bones, unsigned weights);
    bool apply(const Ogre::Material& material, unsigned bones, unsigned weights, bool& changed);
    void unmark_all();
    /// Logs `what` about an entity's mesh, once a run for each mesh and message.
    void note(const Ogre::Entity* entity, const std::string& what);
    Ogre::Technique* cpu_technique(Ogre::Renderable* renderable);

    Ogre::RTShader::ShaderGenerator* generator_ = nullptr;
    bool available_ = false;
    bool active_ = false;      ///< what step() last applied
    std::uint64_t round_ = 1;  ///< changes with every on/off, so every entity is looked at again
    unsigned max_bones_ = 0;
    size_t skinned_ = 0;
    size_t on_gpu_ = 0;
    std::map<const Ogre::Material*, Marked> marked_;
    /// Why the last hand_over() or mark() refused, for the log.
    const char* refusal_ = nullptr;
    /// Mesh and reason pairs already logged, so each is said once a run.
    std::set<std::string> refusals_logged_;
    /// Material -> its copy without skinning, for guard().
    std::map<const Ogre::Material*, std::pair<Ogre::MaterialPtr, Ogre::MaterialPtr>> cpu_copies_;
};

}  // namespace tl::render
