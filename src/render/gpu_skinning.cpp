// Skinning on the GPU; see gpu_skinning.h.
#include "render/gpu_skinning.h"

#include "core/log.h"

#include <OgreEntity.h>
#include <OgreGpuProgramManager.h>
#include <OgreMaterialManager.h>
#include <OgreMesh.h>
#include <OgrePass.h>
#include <OgreRenderSystem.h>
#include <OgreRenderSystemCapabilities.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreShaderExHardwareSkinning.h>
#include <OgreShaderGenerator.h>
#include <OgreShaderRenderState.h>
#include <OgreShaderSubRenderState.h>
#include <OgreSubEntity.h>
#include <OgreSubMesh.h>
#include <OgreTechnique.h>

#include <algorithm>
#include <vector>

namespace tl::render {
namespace {

bool g_enabled = true;

/// What an entity carries to remember it was looked at: the round and the
/// materials it had then, so it is looked at again only when either changes.
const char* const kTriedKey = "tl/gpu_skinning";

/// Set beside `kTriedKey` when the hand-over at that signature succeeded, so
/// an entity OGRE still skins on the CPU afterwards can be told apart from one
/// that was refused.
const char* const kHandedKey = "tl/gpu_skinning_handed";

/// The suffix of guard()'s copies.
const char* const kCpuSuffix = "/tl_cpuskin";

/// Uniform vectors kept back from the bones for everything else a vertex
/// shader of this port's binds: matrices, lights, fog, texture transforms.
constexpr unsigned kOtherUniforms = 64;

/// What a submesh asks of a skinning shader.
struct SubmeshSkin {
    unsigned bones = 0;
    unsigned weights = 0;
};

/// `HardwareSkinningFactory`'s private `extractSkeletonData`, without the
/// side effects: blend data present, and how many bones and weights.
bool submesh_skin(const Ogre::Entity* entity, const Ogre::SubMesh* sub, SubmeshSkin& out) {
    const Ogre::Mesh* mesh = entity->getMesh().get();
    const Ogre::VertexData* data =
        sub->useSharedVertices ? mesh->sharedVertexData : sub->vertexData;
    if (data == nullptr || data->vertexDeclaration == nullptr) return false;
    const Ogre::VertexElement* weights =
        data->vertexDeclaration->findElementBySemantic(Ogre::VES_BLEND_WEIGHTS);
    const Ogre::VertexElement* indices =
        data->vertexDeclaration->findElementBySemantic(Ogre::VES_BLEND_INDICES);
    if (weights == nullptr || indices == nullptr) return false;
    out.weights = Ogre::VertexElement::getTypeCount(weights->getType());
    out.bones = static_cast<unsigned>(std::max(mesh->sharedBlendIndexToBoneIndexMap.size(),
                                               sub->blendIndexToBoneIndexMap.size()));
    return out.weights >= 1 && out.weights <= 4 && out.bones >= 1;
}

/// The technique RTSS builds a material's shaders from.
Ogre::Technique* source_technique(const Ogre::Material& material) {
    for (Ogre::Technique* technique : material.getTechniques()) {
        if (technique->getSchemeName() == Ogre::MSN_DEFAULT) return technique;
    }
    return nullptr;
}

/// Whether a technique's vertex program skins.
bool skins(const Ogre::Technique* technique) {
    if (technique == nullptr || technique->getNumPasses() == 0) return false;
    const Ogre::Pass* pass = technique->getPass(0);
    return pass->hasVertexProgram() && pass->getVertexProgram()->isSkeletalAnimationIncluded();
}

/// What a submesh's material would draw it with, for the log: the material,
/// the technique's scheme, its vertex program and whether that program skins.
std::string describe(Ogre::SubEntity* sub) {
    const Ogre::MaterialPtr& material = sub->getMaterial();
    if (!material) return "no material";
    const Ogre::Technique* technique = material->getBestTechnique(0, sub);
    std::string out = "'" + material->getName() + "'";
    if (technique == nullptr) return out + " has no usable technique";
    out += " draws with technique '" + technique->getSchemeName() + "'";
    if (technique->getNumPasses() == 0 || !technique->getPass(0)->hasVertexProgram()) {
        return out + ", no vertex program";
    }
    const Ogre::GpuProgramPtr& program = technique->getPass(0)->getVertexProgram();
    out += ", vertex program '" + program->getName() + "'";
    out += program->isSkeletalAnimationIncluded() ? " (skins)" : " (does not skin)";
    if (program->hasCompileError()) out += ", which failed to compile";
    return out;
}

/// The entity's materials and the round, as one number.
std::uint64_t signature(const Ogre::Entity* entity, std::uint64_t round) {
    std::uint64_t h = round * 0x9e3779b97f4a7c15ULL;
    for (const Ogre::SubEntity* sub : entity->getSubEntities()) {
        h ^= reinterpret_cast<std::uintptr_t>(sub->getMaterial().get()) + 0x9e3779b97f4a7c15ULL +
             (h << 6) + (h >> 2);
    }
    return h;
}

/// OGRE re-decides software or hardware skinning when a submesh's material is
/// set; this is the public way to make it decide again.
void reconsider(Ogre::Entity* entity) {
    if (entity->getNumSubEntities() == 0) return;
    Ogre::SubEntity* sub = entity->getSubEntity(0);
    sub->setMaterial(sub->getMaterial());
}

/// RTSS answers for the scheme MaterialManager has active, which after a frame
/// is whatever the last viewport drew with. This port draws everything with
/// the generator's.
class ShaderScheme {
public:
    ShaderScheme() : was_(Ogre::MaterialManager::getSingleton().getActiveScheme()) {
        Ogre::MaterialManager::getSingleton().setActiveScheme(Ogre::MSN_SHADERGEN);
    }
    ~ShaderScheme() { Ogre::MaterialManager::getSingleton().setActiveScheme(was_); }
    ShaderScheme(const ShaderScheme&) = delete;
    ShaderScheme& operator=(const ShaderScheme&) = delete;

private:
    Ogre::String was_;
};

}  // namespace

bool GpuSkinning::enabled() { return g_enabled; }

void GpuSkinning::set_enabled(bool on) {
    if (g_enabled == on) return;
    g_enabled = on;
    TL_LOG_INFO("gpu skinning: %s", on ? "on" : "off");
}

void GpuSkinning::start(Ogre::RTShader::ShaderGenerator* generator) {
    generator_ = generator;
    available_ = false;
    if (generator_ == nullptr ||
        Ogre::RTShader::HardwareSkinningFactory::getSingletonPtr() == nullptr) {
        TL_LOG_INFO("gpu skinning: unavailable (no RTSS skinning); characters skin on the CPU");
        return;
    }
    // RTSS's own condition: its skinning shader needs GLSL ES 3.00 on GLES.
    if (generator_->getTargetLanguage() == "glsles" &&
        !Ogre::GpuProgramManager::getSingleton().isSyntaxSupported("glsl300es")) {
        TL_LOG_INFO("gpu skinning: unavailable (no glsl300es); characters skin on the CPU");
        return;
    }
    // Three uniform vectors a bone (a 3x4 matrix), out of what the device
    // gives a vertex shader, less what the rest of the shader binds.
    unsigned vectors = 0;
    if (const Ogre::RenderSystem* rs = Ogre::Root::getSingleton().getRenderSystem()) {
        if (const Ogre::RenderSystemCapabilities* caps = rs->getCapabilities()) {
            vectors = caps->getConstantFloatCount(Ogre::GPT_VERTEX_PROGRAM);
        }
    }
    max_bones_ = vectors > kOtherUniforms ? (vectors - kOtherUniforms) / 3 : 0;
    max_bones_ = std::min<unsigned>(max_bones_, OGRE_MAX_NUM_BONES - 1);
    if (max_bones_ == 0) {
        TL_LOG_INFO("gpu skinning: unavailable (%u vertex uniforms); characters skin on the CPU",
                    vectors);
        return;
    }
    Ogre::RTShader::HardwareSkinningFactory::setMaxCalculableBoneCount(
        static_cast<Ogre::ushort>(max_bones_));
    available_ = true;
    TL_LOG_INFO("gpu skinning: available, up to %u bones a submesh (%u vertex uniforms); %s",
                max_bones_, vectors, g_enabled ? "on" : "off");
}

void GpuSkinning::stop() {
    // MaterialPtrs, and they must go while the MaterialManager is still there.
    marked_.clear();
    cpu_copies_.clear();
    generator_ = nullptr;
    available_ = false;
    active_ = false;
}

bool GpuSkinning::mark(const Ogre::MaterialPtr& material, unsigned bones, unsigned weights) {
    const auto found = marked_.find(material.get());
    // One weight count to a material: the shader reads exactly that many, so
    // a mesh with fewer would take the unused components as weights.
    if (found != marked_.end() && found->second.weights != weights) {
        refusal_ = "its material already skins for another weight count";
        return false;
    }

    Ogre::Technique* source = source_technique(*material);
    if (source == nullptr || source->getNumPasses() == 0) {
        refusal_ = "its material has no default technique";
        return false;
    }
    for (const Ogre::Pass* pass : source->getPasses()) {
        if (pass->isProgrammable()) {  // its own shaders, not RTSS's
            refusal_ = "its material carries its own shaders";
            return false;
        }
    }
    if (!generator_->createShaderBasedTechnique(*material, Ogre::MSN_DEFAULT,
                                                Ogre::MSN_SHADERGEN)) {
        refusal_ = "RTSS would not make its material a technique";
        return false;
    }

    const unsigned want = found != marked_.end() ? std::max(found->second.bones, bones) : bones;
    bool changed = found == marked_.end() || want != found->second.bones;
    if (!apply(*material, want, weights, changed)) {
        refusal_ = "RTSS has no render state for its material";
        return false;
    }
    if (changed) generator_->invalidateMaterial(Ogre::MSN_SHADERGEN, *material);
    marked_[material.get()] = Marked{material, want, weights};
    return true;
}

bool GpuSkinning::apply(const Ogre::Material& material, unsigned bones, unsigned weights,
                        bool& changed) {
    const Ogre::Technique* source = source_technique(material);
    if (source == nullptr) return false;
    for (size_t i = 0; i < source->getNumPasses(); ++i) {
        // The shader generator numbers passes in 16 bits.
        Ogre::RTShader::RenderState* state =
            generator_->getRenderState(Ogre::MSN_SHADERGEN, material, static_cast<Ogre::uint16>(i));
        if (state == nullptr) return false;
        Ogre::RTShader::SubRenderState* skin =
            state->getSubRenderState(Ogre::RTShader::SRS_HARDWARE_SKINNING);
        bool fresh = false;
        if (skin == nullptr) {
            // RTSS's entry for the material was made again since it was marked.
            skin = generator_->createSubRenderState(Ogre::RTShader::SRS_HARDWARE_SKINNING);
            state->addTemplateSubRenderState(skin);
            fresh = true;
            changed = true;
        }
        if (fresh || changed) {
            skin->setParameter("type", "linear");
            skin->setParameter("max_bone_count", std::to_string(bones));
            skin->setParameter("weight_count", std::to_string(weights));
        }
    }
    return true;
}

void GpuSkinning::regenerating(const Ogre::Material& material) {
    if (!active_ || generator_ == nullptr) return;
    const auto found = marked_.find(&material);
    if (found == marked_.end()) return;
    // RTSS dropped the material's shaders and is about to write new ones (`apply_rim_light()`
    // does, for every placed character's materials); without its skinning back first, every
    // entity on the GPU wearing it would draw its bind pose until step() noticed.
    bool changed = false;
    apply(material, found->second.bones, found->second.weights, changed);
}

bool GpuSkinning::gpu_skin(const Ogre::Renderable* renderable, unsigned& bones,
                           unsigned& weights) const {
    const auto* sub = dynamic_cast<const Ogre::SubEntity*>(renderable);
    if (sub == nullptr) return false;
    Ogre::Entity* entity = sub->getParent();
    if (!entity->hasSkeleton() || !entity->isHardwareAnimationEnabled()) return false;
    SubmeshSkin skin;
    if (!submesh_skin(entity, sub->getSubMesh(), skin)) return false;
    bones = skin.bones;
    weights = skin.weights;
    return true;
}

Ogre::MaterialPtr GpuSkinning::weight_variant(const Ogre::MaterialPtr& held, unsigned weights) {
    auto& materials = Ogre::MaterialManager::getSingleton();
    const std::string name = held->getName() + "/tl_w" + std::to_string(weights);
    Ogre::MaterialPtr variant = materials.getByName(name, held->getGroup());
    if (variant) {
        // Taken again from `held` every time: the wardrobe replaces the skin
        // texture and the rim light adds a unit to the material it was given.
        // Its mark stays: other submeshes still wear it, and mark() skins for
        // the most bones any of them asked (a worn set's boots ask 8 of the
        // copy the Destroyer's gloves skin 18 with).
        if (generator_ != nullptr) generator_->removeAllShaderBasedTechniques(*variant);
        held->copyDetailsTo(variant);
    } else {
        variant = held->clone(name);
    }
    // Not the shaders RTSS made for `held`, which skin for its own count; mark()
    // has RTSS make this one's.
    for (unsigned t = variant->getNumTechniques(); t-- > 0;) {
        if (variant->getTechnique(t)->getSchemeName() != Ogre::MSN_DEFAULT)
            variant->removeTechnique(t);
    }
    variant->load();
    return variant;
}

bool GpuSkinning::weights_agree(const Ogre::Entity* entity) const {
    for (const Ogre::SubEntity* sub : entity->getSubEntities()) {
        SubmeshSkin skin;
        if (!submesh_skin(entity, sub->getSubMesh(), skin)) return false;
        const auto found = marked_.find(sub->getMaterial().get());
        if (found == marked_.end() || found->second.weights != skin.weights) return false;
        // A shader with fewer bones than the submesh indexes reads past its
        // bone array: vertices thrown off as spikes.
        if (found->second.bones < skin.bones) return false;
    }
    return true;
}

bool GpuSkinning::hand_over(Ogre::Entity* entity) {
    // Every submesh first, before any material changes: all or nothing.
    const unsigned count = static_cast<unsigned>(entity->getNumSubEntities());
    std::vector<SubmeshSkin> skin(count);
    std::vector<Ogre::MaterialPtr> held(count);
    for (unsigned i = 0; i < count; ++i) {
        const Ogre::SubEntity* sub = entity->getSubEntity(i);
        if (!submesh_skin(entity, sub->getSubMesh(), skin[i])) {
            refusal_ = "a submesh has no blend weights or indices";
            return false;
        }
        if (skin[i].bones > max_bones_) {
            refusal_ = "a submesh has more bones than the shader takes";
            return false;
        }
        held[i] = sub->getMaterial();
        if (!held[i]) {
            refusal_ = "a submesh has no material";
            return false;
        }
    }
    // One weight count to a material, so a submesh whose material skins (or is about to, for an
    // earlier submesh) for another count wears a copy for its own: the wardrobe gives the body's
    // skin material to the hair's neck too.
    std::vector<Ogre::MaterialPtr> wear(held);
    for (unsigned i = 0; i < count; ++i) {
        const auto found = marked_.find(held[i].get());
        bool clash = found != marked_.end() && found->second.weights != skin[i].weights;
        for (unsigned j = 0; j < i && !clash; ++j) {
            clash = wear[j] == held[i] && skin[j].weights != skin[i].weights;
        }
        if (!clash) continue;
        for (unsigned j = 0; j < i; ++j) {
            if (held[j] == held[i] && skin[j].weights == skin[i].weights && wear[j] != held[i]) {
                wear[i] = wear[j];
                break;
            }
        }
        if (wear[i] == held[i]) wear[i] = weight_variant(held[i], skin[i].weights);
        if (!wear[i]) {
            refusal_ = "no copy of its material for another weight count";
            return false;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        if (wear[i] != held[i]) entity->getSubEntity(i)->setMaterial(wear[i]);
    }
    for (unsigned i = 0; i < count; ++i) {
        if (!mark(wear[i], skin[i].bones, skin[i].weights)) {
            // Those already marked stay so; this entity stays on the CPU, and
            // guard() keeps its marked ones from drawing with a skinning shader.
            return false;
        }
    }
    reconsider(entity);
    return true;
}

void GpuSkinning::unmark_all() {
    for (auto& entry : marked_) {
        const Ogre::MaterialPtr& material = entry.second.material;
        const Ogre::Technique* source = source_technique(*material);
        if (source == nullptr) continue;
        for (size_t i = 0; i < source->getNumPasses(); ++i) {
            Ogre::RTShader::RenderState* state = generator_->getRenderState(
                Ogre::MSN_SHADERGEN, *material, static_cast<Ogre::uint16>(i));
            if (state == nullptr) continue;
            if (Ogre::RTShader::SubRenderState* skin =
                    state->getSubRenderState(Ogre::RTShader::SRS_HARDWARE_SKINNING)) {
                state->removeSubRenderState(skin);
            }
        }
        generator_->invalidateMaterial(Ogre::MSN_SHADERGEN, *material);
    }
    marked_.clear();
}

void GpuSkinning::step(Ogre::SceneManager* scene) {
    if (!available_ || scene == nullptr) return;
    const bool wanted = g_enabled;
    if (wanted != active_) {
        if (!wanted) unmark_all();
        active_ = wanted;
        ++round_;
    }
    const ShaderScheme scheme;
    size_t on_gpu = 0;
    size_t skinned = 0;
    size_t handed = 0;
    for (const auto& entry : scene->getMovableObjects(Ogre::MOT_ENTITY)) {
        auto* entity = static_cast<Ogre::Entity*>(entry.second);
        if (!entity->hasSkeleton() || entity->hasVertexAnimation()) continue;
        ++skinned;

        if (entity->isHardwareAnimationEnabled()) {
            // On the GPU. Still so only while every material's shader skins:
            // one RTSS rebuilt without its skinning (a redeclared script) would
            // otherwise be handed bones it does not read, and draw the bind pose.
            bool intact = active_;
            for (Ogre::SubEntity* sub : entity->getSubEntities()) {
                if (!intact) break;
                intact = skins(sub->getMaterial()->getBestTechnique(0, sub));
            }
            // And for its own weight and bone counts: the wardrobe puts the body's
            // skin material back on a submesh that was wearing a weight_variant().
            intact = intact && weights_agree(entity);
            if (intact) {
                ++on_gpu;
                continue;
            }
            if (refit(entity)) ++handed;
            continue;
        }
        if (!active_) continue;

        Ogre::UserObjectBindings& tags = entity->getUserObjectBindings();
        const std::uint64_t now = signature(entity, round_);
        const Ogre::Any& tried = tags.getUserAny(kTriedKey);
        if (tried.has_value() && Ogre::any_cast<std::uint64_t>(tried) == now) {
            // Handed over at this signature, and still not on the GPU: OGRE
            // decided against it when the materials were set (a technique
            // that did not skin at that moment), and nothing asks it again.
            const Ogre::Any& handed_at = tags.getUserAny(kHandedKey);
            if (handed_at.has_value() && Ogre::any_cast<std::uint64_t>(handed_at) == now) {
                std::string why = "every material skins, yet OGRE did not take it";
                for (Ogre::SubEntity* sub : entity->getSubEntities()) {
                    if (!skins(sub->getMaterial()->getBestTechnique(0, sub))) {
                        why = describe(sub);
                        break;
                    }
                }
                note(entity, "was handed over but OGRE still skins it on the CPU: " + why);
            }
            continue;
        }
        tags.setUserAny(kTriedKey, Ogre::Any(now));
        refusal_ = nullptr;
        if (hand_over(entity)) {
            ++handed;
            tags.setUserAny(kHandedKey, Ogre::Any(now));
        } else if (refusal_ != nullptr) {
            note(entity, std::string("stays on the CPU: ") + refusal_);
        }
    }
    skinned_ = skinned;
    on_gpu_ = on_gpu;
    if (handed > 0) {
        TL_LOG_INFO("gpu skinning: %zu entit%s handed over, %zu already on the GPU", handed,
                    handed == 1 ? "y" : "ies", on_gpu);
    }
}

bool GpuSkinning::refit(Ogre::Entity* entity) {
    entity->getUserObjectBindings().eraseUserAny(kTriedKey);
    entity->getUserObjectBindings().eraseUserAny(kHandedKey);
    std::string why = "its weight or bone counts no longer agree";
    bool skins_all = true;
    for (Ogre::SubEntity* sub : entity->getSubEntities()) {
        if (!skins(sub->getMaterial()->getBestTechnique(0, sub))) {
            why = "no longer skins: " + describe(sub);
            skins_all = false;
            break;
        }
    }
    // A material that still skins keeps OGRE skinning the entity on the GPU, whatever this
    // says: with a shader for other weights (vertices thrown off as spikes) or fewer bones
    // (OGRE writes the submesh's bone matrices past the shader's array, into the heap). So
    // fitted again now, as a fresh hand-over does it.
    if (active_ && skins_all) {
        refusal_ = nullptr;
        if (hand_over(entity)) {
            note(entity, "was fitted again, " + why);
            return true;
        }
        why += std::string(", and could not be fitted again: ") +
               (refusal_ != nullptr ? refusal_ : "?");
    }
    // Back to the CPU for now, and looked at afresh next frame.
    if (active_) note(entity, "went back to the CPU, " + why);
    reconsider(entity);
    return false;
}

void GpuSkinning::note(const Ogre::Entity* entity, const std::string& what) {
    // Once a run for each mesh and message.
    const std::string& mesh = entity->getMesh()->getName();
    if (refusals_logged_.insert(mesh + '|' + what).second) {
        TL_LOG_INFO("gpu skinning: '%s' %s", mesh.c_str(), what.c_str());
    }
}

Ogre::Technique* GpuSkinning::cpu_technique(Ogre::Renderable* renderable) {
    const Ogre::MaterialPtr& held = renderable->getMaterial();
    if (!held) return nullptr;
    auto found = cpu_copies_.find(held.get());
    if (found == cpu_copies_.end()) {
        auto& materials = Ogre::MaterialManager::getSingleton();
        const std::string name = held->getName() + kCpuSuffix;
        Ogre::MaterialPtr copy = materials.getByName(name, held->getGroup());
        if (!copy) {
            copy = held->clone(name);
            // Not the shaders RTSS made for the original - they skin - but its
            // source technique, from which RTSS makes ones that do not.
            for (unsigned t = copy->getNumTechniques(); t-- > 0;) {
                if (copy->getTechnique(t)->getSchemeName() != Ogre::MSN_DEFAULT) {
                    copy->removeTechnique(t);
                }
            }
            if (generator_ != nullptr) generator_->removeAllShaderBasedTechniques(*copy);
            copy->load();
        }
        found = cpu_copies_.emplace(held.get(), std::make_pair(held, copy)).first;
    }
    Ogre::Material& copy = *found->second.second;
    // A copy unloaded since it was made (a material reload) has no technique until loaded again,
    // and nothing else loads it; without this the submesh kept the skinning shader.
    if (!copy.isLoaded()) copy.load();
    return copy.getBestTechnique(0, renderable);
}

bool GpuSkinning::material_fits(const Ogre::SubEntity* sub) const {
    SubmeshSkin skin;
    if (!submesh_skin(sub->getParent(), sub->getSubMesh(), skin)) return true;
    const auto found = marked_.find(sub->getMaterial().get());
    // Unmarked: nothing is known of its shader, and it is left as it is.
    return found == marked_.end() ||
           (found->second.weights == skin.weights && found->second.bones >= skin.bones);
}

Ogre::Technique* GpuSkinning::fitting_technique(Ogre::SubEntity* sub) const {
    SubmeshSkin skin;
    if (!submesh_skin(sub->getParent(), sub->getSubMesh(), skin)) return nullptr;
    const auto fits = [&](const Ogre::Material* material) {
        const auto found = marked_.find(material);
        return found != marked_.end() && found->second.weights == skin.weights &&
               found->second.bones >= skin.bones;
    };
    const Ogre::MaterialPtr& held = sub->getMaterial();
    // Unmarked: nothing is known of its shader, and it is left as it is.
    if (!held || fits(held.get()) || marked_.find(held.get()) == marked_.end()) return nullptr;
    // The material weight_variant() copied from, and its copy for this weight count.
    std::string base = held->getName();
    const size_t cut = base.rfind("/tl_w");
    if (cut != std::string::npos) base.erase(cut);
    auto& materials = Ogre::MaterialManager::getSingleton();
    for (const std::string& name : {base + "/tl_w" + std::to_string(skin.weights), base}) {
        const Ogre::MaterialPtr candidate = materials.getByName(name, held->getGroup());
        if (!candidate || !fits(candidate.get())) continue;
        if (Ogre::Technique* technique = candidate->getBestTechnique(0, sub)) return technique;
    }
    return nullptr;
}

void GpuSkinning::guard(Ogre::Renderable* renderable, Ogre::Technique** technique) {
    if (!available_ || renderable == nullptr || technique == nullptr || !skins(*technique)) return;
    // Anything but a submesh of an entity skinned on the GPU: one the CPU
    // skins, one with no skeleton, or the merged scenery sharing a material.
    auto* sub = dynamic_cast<Ogre::SubEntity*>(renderable);
    if (sub != nullptr && sub->getParent()->isHardwareAnimationEnabled()) {
        // On the GPU, but its material skins for another weight count or fewer bones: a
        // material set since step() looked (the wardrobe's, the inventory doll copying the
        // player's every frame). That shader throws the vertices off as spikes; the copy
        // marked for this submesh draws it as it is.
        if (Ogre::Technique* fitting = fitting_technique(sub)) {
            *technique = fitting;
            return;
        }
        // Nothing fits it yet (step() fits it at the frame's end): its shader's bone array may be
        // smaller than the submesh's bones, and OGRE would write past it. Unskinned this once.
        if (!material_fits(sub)) {
            if (Ogre::Technique* plain = cpu_technique(renderable)) *technique = plain;
        }
        return;
    }
    if (Ogre::Technique* plain = cpu_technique(renderable)) *technique = plain;
}

}  // namespace tl::render
