// The GPU's time over a frame and its passes; see gpu_timer.h.
#include "render/gpu_timer.h"

#include "core/log.h"

#include <OgreRenderQueue.h>
#include <OgreRenderSystem.h>
#include <OgreRenderTarget.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreViewport.h>

// After the ones above: OgreRenderQueueSortingGrouping.h uses their
// MapIterator without including it.
#include <OgreOverlayElement.h>
#include <OgrePass.h>
#include <OgreRenderOperation.h>
#include <OgreRenderQueueSortingGrouping.h>
#include <OgreTechnique.h>
#include <OgreTextureUnitState.h>

#include <cstdlib>

#if defined(TL_PLATFORM_SWITCH)
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cstring>
#endif

namespace tl::render {

namespace {

/// A viewport's pass, by its target: the window's view is z-order 0 and the
/// minimap sits above it; the light and shadow maps are the projectors'
/// render textures (setup_light_map, setup_shadow_map).
int pass_of(const Ogre::Viewport* viewport) {
    const Ogre::RenderTarget* target = viewport->getTarget();
    if (target->getName().find("ProjectorRTTLight") != Ogre::String::npos)
        return GpuTimer::kLightMap;
    if (target->getName().find("ProjectorRTT") != Ogre::String::npos) return GpuTimer::kShadowMap;
    if (dynamic_cast<const Ogre::RenderWindow*>(target) != nullptr) {
        return viewport->getZOrder() == 0 ? GpuTimer::kView : GpuTimer::kMinimap;
    }
    return GpuTimer::kOtherTexture;
}

/// Lists what the interface queue draws, in order (`TL_OVERLAY_DUMP` set):
/// a diagnostic for how far its draws could be merged.
class OverlayLister final : public Ogre::QueuedRenderableVisitor {
public:
    const char* what = "";
    unsigned priority = 0;
    int index = 0;
    void visit(Ogre::RenderablePass* rp) override { line(rp->renderable, rp->pass); }
    void visit(const Ogre::Pass* p, Ogre::RenderableList& rs) override {
        for (Ogre::Renderable* r : rs) line(r, p);
    }

private:
    void line(Ogre::Renderable* r, const Ogre::Pass* p) {
        if (r == nullptr) return;
        std::string name = "?";
        float w = 0.0f;
        float h = 0.0f;
        if (auto* element = dynamic_cast<Ogre::OverlayElement*>(r)) {
            name = element->getName();
            if (element->getTypeName() == "TextArea") {
                name += " caption '" + element->getCaption() + "'";
            }
            w = element->_getWidth();
            h = element->_getHeight();
        }
        std::string texture;
        if (p != nullptr && p->getNumTextureUnitStates() > 0) {
            texture = p->getTextureUnitState(0)->getTextureName();
        }
        const Ogre::MaterialPtr& material = r->getMaterial();
        TL_LOG_INFO("overlay draw %3d: prio %u %s pass %p mat '%s' tex '%s' %s (%.3f x %.3f)",
                    index++, priority, what, static_cast<const void*>(p),
                    material ? material->getName().c_str() : "", texture.c_str(), name.c_str(),
                    static_cast<double>(w), static_cast<double>(h));
    }
};

}  // namespace

const char* GpuTimer::pass_name(int pass) {
    static const char* const kNames[kPasses] = {"light map", "shadow map", "other texture", "view",
                                                "minimap"};
    return pass >= 0 && pass < kPasses ? kNames[pass] : "?";
}

bool GpuTimer::take_mean(double& frame_ms, double (&pass_ms)[kPasses], unsigned& samples) {
    samples = count_;
    const double n = count_ > 0 ? static_cast<double>(count_) : 1.0;
    frame_ms = sum_frame_ms_ / n;
    for (int i = 0; i < kPasses; ++i) {
        pass_ms[i] = sum_pass_ms_[i] / n;
        sum_pass_ms_[i] = 0.0;
    }
    sum_frame_ms_ = 0.0;
    count_ = 0;
    return samples > 0;
}

void GpuTimer::take_view_groups(double (&group_ms)[kGroups], double (&gap_ms)[3]) {
    const double n = count_ > 0 ? static_cast<double>(count_) : 1.0;
    for (int i = 0; i < 3; ++i) {
        gap_ms[i] = sum_gap_ms_[i] / n;
        sum_gap_ms_[i] = 0.0;
    }
    for (int i = 0; i < kGroups; ++i) {
        group_ms[i] = sum_group_ms_[i] / n;
        sum_group_ms_[i] = 0.0;
    }
}

bool GpuTimer::take_batches(double (&pass)[kPasses], double& interface_batches,
                            double (&view_groups)[kGroups]) {
    const unsigned frames = batch_frames_;
    const double n = frames > 0 ? static_cast<double>(frames) : 1.0;
    for (int i = 0; i < kGroups; ++i) {
        view_groups[i] = static_cast<double>(group_batch_sum_[i]) / n;
        group_batch_sum_[i] = 0;
    }
    for (int i = 0; i < kPasses; ++i) {
        pass[i] = static_cast<double>(batch_sum_[i]) / n;
        batch_sum_[i] = 0;
    }
    interface_batches = static_cast<double>(interface_sum_) / n;
    interface_sum_ = 0;
    batch_frames_ = 0;
    return frames > 0;
}

void GpuTimer::attach_to_targets(std::initializer_list<Ogre::RenderTarget*> targets) {
    // Idempotent, and render textures come and go with levels, so it is done
    // again each frame with the targets the renderer has now.
    for (Ogre::RenderTarget* target : targets) {
        if (target == nullptr) continue;
        target->removeListener(this);
        target->addListener(this);
    }
}

void GpuTimer::preViewportUpdate(const Ogre::RenderTargetViewportEvent& evt) {
    if (evt.source == nullptr) return;
    current_pass_ = pass_of(evt.source);
    begin_timing(current_pass_, current_pass_ == kView ? kGapStart : -1);
}

void GpuTimer::postViewportUpdate(const Ogre::RenderTargetViewportEvent& evt) {
    // The view's last stretch, after its last queue group.
    if (open_ && current_pass_ == kView && !timed_.empty() && timed_.back().group == kGapBetween) {
        timed_.back().group = kGapEnd;
    }
    end_timing();
    if (evt.source != nullptr && current_pass_ >= 0) {
        batch_sum_[current_pass_] +=
            static_cast<unsigned long long>(evt.source->_getNumRenderedBatches());
    }
    current_pass_ = -1;
}

void GpuTimer::renderQueueStarted(Ogre::uint8 queue, const Ogre::String&, bool&) {
    if (queue == Ogre::RENDER_QUEUE_OVERLAY) dump_overlay_queue();
    if (current_pass_ != kView) return;
    const size_t now = Ogre::Root::getSingleton().getRenderSystem()->_getBatchCount();
    group_start_ = now;
    if (queue == Ogre::RENDER_QUEUE_OVERLAY) interface_start_ = now;
    end_timing();
    begin_timing(kView, queue);
}

void GpuTimer::dump_overlay_queue() {
    {
        if (dump_overlay_next && scene_ != nullptr) {
            dump_overlay_next = false;
            OverlayLister lister;
            Ogre::RenderQueueGroup* group =
                scene_->getRenderQueue()->getQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);
            for (const auto& entry : group->getPriorityGroups()) {
                lister.priority = entry.first;
                lister.what = "solid";
                entry.second->getSolidsBasic().acceptVisitor(
                    &lister, Ogre::QueuedRenderableCollection::OM_PASS_GROUP);
                lister.what = "unsorted";
                entry.second->getTransparentsUnsorted().acceptVisitor(
                    &lister, Ogre::QueuedRenderableCollection::OM_PASS_GROUP);
                lister.what = "sorted";
                entry.second->getTransparents().acceptVisitor(
                    &lister, Ogre::QueuedRenderableCollection::OM_SORT_DESCENDING);
            }
        }
    }
}

void GpuTimer::renderQueueEnded(Ogre::uint8 queue, const Ogre::String&, bool&) {
    if (current_pass_ != kView) return;
    const size_t now = Ogre::Root::getSingleton().getRenderSystem()->_getBatchCount();
    if (now >= group_start_)
        group_batch_sum_[queue] += static_cast<unsigned long long>(now - group_start_);
    if (queue == Ogre::RENDER_QUEUE_OVERLAY && now >= interface_start_) {
        interface_sum_ += static_cast<unsigned long long>(now - interface_start_);
    }
    end_timing();
    begin_timing(kView, kGapBetween);
}

void GpuTimer::frame_started(Ogre::SceneManager* scene,
                             std::initializer_list<Ogre::RenderTarget*> targets) {
    if (scene != scene_) {
        if (scene_ != nullptr) scene_->removeRenderQueueListener(this);
        scene_ = scene;
        if (scene_ != nullptr) scene_->addRenderQueueListener(this);
    }
    attach_to_targets(targets);
    ++batch_frames_;
#if defined(TL_PLATFORM_SWITCH)
    if (!tried_ && !start()) return;
    if (!usable_) return;
    sampling_ = false;
    if (++frame_ % kEvery != 0) return;
    collect();
    // Still unread: this sample is skipped rather than the queries reused.
    if (pending_) return;
    timed_.clear();
    sampling_ = true;
#endif
}

#if defined(TL_PLATFORM_SWITCH)
namespace {

PFNGLGENQUERIESEXTPROC gen_queries = nullptr;
PFNGLDELETEQUERIESEXTPROC delete_queries = nullptr;
PFNGLBEGINQUERYEXTPROC begin_query = nullptr;
PFNGLENDQUERYEXTPROC end_query = nullptr;
PFNGLGETQUERYOBJECTUIVEXTPROC query_uiv = nullptr;
PFNGLGETQUERYOBJECTUI64VEXTPROC query_ui64v = nullptr;

template <typename T>
bool resolve(T& out, const char* name) {
    out = reinterpret_cast<T>(eglGetProcAddress(name));
    return out != nullptr;
}

}  // namespace

bool GpuTimer::start() {
    tried_ = true;
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions == nullptr ||
        std::strstr(extensions, "GL_EXT_disjoint_timer_query") == nullptr) {
        TL_LOG_INFO(
            "gpu timer: the driver has no GL_EXT_disjoint_timer_query; GPU time not logged");
        return false;
    }
    if (!resolve(gen_queries, "glGenQueriesEXT") ||
        !resolve(delete_queries, "glDeleteQueriesEXT") ||
        !resolve(begin_query, "glBeginQueryEXT") || !resolve(end_query, "glEndQueryEXT") ||
        !resolve(query_uiv, "glGetQueryObjectuivEXT") ||
        !resolve(query_ui64v, "glGetQueryObjectui64vEXT")) {
        TL_LOG_INFO(
            "gpu timer: the timer query's entry points did not resolve; GPU time not logged");
        return false;
    }
    usable_ = true;
    TL_LOG_INFO("gpu timer: one frame in %u timed on the GPU, by pass", kEvery);
    return true;
}

void GpuTimer::collect() {
    if (!pending_) return;
    for (const Timed& t : timed_) {
        GLuint available = 0;
        query_uiv(t.query, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
        if (available == 0) return;  // read at the next sample instead
    }
    pending_ = false;
    double frame = 0.0;
    double passes[kPasses] = {};
    for (const Timed& t : timed_) {
        GLuint64 elapsed_ns = 0;
        query_ui64v(t.query, GL_QUERY_RESULT_EXT, &elapsed_ns);
        const double ms = static_cast<double>(elapsed_ns) / 1.0e6;
        passes[t.pass] += ms;
        frame += ms;
    }
    double groups[kGroups] = {};
    double gaps[3] = {};
    for (const Timed& t : timed_) {
        if (t.group >= kGroups || t.group == -1) continue;
        GLuint64 elapsed_ns = 0;
        query_ui64v(t.query, GL_QUERY_RESULT_EXT, &elapsed_ns);
        const double ms = static_cast<double>(elapsed_ns) / 1.0e6;
        if (t.group >= 0) {
            groups[t.group] += ms;
        } else {
            gaps[t.group == kGapStart ? 0 : t.group == kGapBetween ? 1 : 2] += ms;
        }
    }
    // A disjoint event (a clock change, a power state) voids what was timed
    // across it; the flag is read and cleared together.
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint != 0 || timed_.empty()) return;
    sum_frame_ms_ += frame;
    for (int i = 0; i < kPasses; ++i) sum_pass_ms_[i] += passes[i];
    for (int i = 0; i < kGroups; ++i) sum_group_ms_[i] += groups[i];
    for (int i = 0; i < 3; ++i) sum_gap_ms_[i] += gaps[i];
    ++count_;
}

void GpuTimer::frame_queued() {
    if (!sampling_) return;
    sampling_ = false;
    end_timing();
    pending_ = !timed_.empty();
}

void GpuTimer::begin_timing(int pass, int group) {
    if (!sampling_ || open_) return;
    if (timed_.size() == queries_.size()) {
        GLuint query = 0;
        gen_queries(1, &query);
        if (query == 0) return;
        queries_.push_back(query);
    }
    Timed t;
    t.query = queries_[timed_.size()];
    t.pass = pass;
    t.group = group;
    begin_query(GL_TIME_ELAPSED_EXT, t.query);
    timed_.push_back(t);
    open_ = true;
}

void GpuTimer::end_timing() {
    if (!open_) return;
    end_query(GL_TIME_ELAPSED_EXT);
    open_ = false;
}

void GpuTimer::stop(Ogre::SceneManager* scene, std::initializer_list<Ogre::RenderTarget*> targets) {
    for (Ogre::RenderTarget* target : targets) {
        if (target != nullptr) target->removeListener(this);
    }
    if (scene != nullptr) scene->removeRenderQueueListener(this);
    scene_ = nullptr;
    if (usable_) {
        if (open_) end_query(GL_TIME_ELAPSED_EXT);
        if (!queries_.empty()) {
            std::vector<GLuint> ids(queries_.begin(), queries_.end());
            delete_queries(static_cast<GLsizei>(ids.size()), ids.data());
        }
    }
    queries_.clear();
    timed_.clear();
    usable_ = false;
    tried_ = false;
    open_ = pending_ = sampling_ = false;
    sum_frame_ms_ = 0.0;
    for (double& v : sum_pass_ms_) v = 0.0;
    count_ = 0;
}

#else

bool GpuTimer::start() { return false; }
void GpuTimer::collect() {}
void GpuTimer::frame_queued() {}
void GpuTimer::begin_timing(int, int) {}
void GpuTimer::end_timing() {}

void GpuTimer::stop(Ogre::SceneManager* scene, std::initializer_list<Ogre::RenderTarget*> targets) {
    for (Ogre::RenderTarget* target : targets) {
        if (target != nullptr) target->removeListener(this);
    }
    if (scene != nullptr) scene->removeRenderQueueListener(this);
    scene_ = nullptr;
}

#endif

}  // namespace tl::render
