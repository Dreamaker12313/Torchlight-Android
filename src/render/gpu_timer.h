// How long the GPU takes over a frame and each of its passes, from the GPU's own clock
// (`GL_EXT_disjoint_timer_query`, the console only), and how many batches each pass draws (every
// frame, every target). Passes: the light map, the shadow map, the view (interface included,
// also split by render queue group), the minimap and any other render texture.
//
// Sampled: one frame in `kEvery` is timed and read when the next sample starts, since reading a
// result is a round trip that under glthread waits for the GL thread. A figure is the GPU's time
// from a pass's first command to its last, an upper bound; the frame's is the passes' sum.
//
// Batches per viewport are `Viewport::_getNumRenderedBatches`; the interface's come from the
// render system's running count at the overlay queue's start and end.
#pragma once

#include <OgreRenderQueueListener.h>
#include <OgreRenderTargetListener.h>

#include <initializer_list>
#include <vector>

namespace Ogre {
class SceneManager;
}

namespace tl::render {

class GpuTimer final : public Ogre::RenderTargetListener, public Ogre::RenderQueueListener {
public:
    /// One frame in this many is timed.
    static constexpr unsigned kEvery = 30;

    enum Pass { kLightMap = 0, kShadowMap, kOtherTexture, kView, kMinimap, kPasses };
    static const char* pass_name(int pass);

    /// At OGRE's frameStarted, with the scene and the render targets the frame
    /// draws (null ones skipped), and at frameRenderingQueued.
    void frame_started(Ogre::SceneManager* scene,
                       std::initializer_list<Ogre::RenderTarget*> targets);
    void frame_queued();

    /// The mean of the samples since the last call, in milliseconds: the
    /// frame's total and each pass's; false when there were none (the desktop,
    /// an unsupported driver, a disjoint event that voided them). Resets.
    bool take_mean(double& frame_ms, double (&pass_ms)[kPasses], unsigned& samples);

    /// Render queue groups, for the view's split.
    static constexpr int kGroups = 256;

    /// The view's mean milliseconds by queue group over the samples since the
    /// last call; call before take_mean, whose sample count it shares. Resets.
    /// `gap_ms` is the view's time outside its queue groups: before the first
    /// (the clear, and whatever the GPU had left), between them, after the last.
    void take_view_groups(double (&group_ms)[kGroups], double (&gap_ms)[3]);

    /// The mean batches a frame since the last call, by pass, and the
    /// interface's (inside the view's); false when no frame was counted. Resets.
    bool take_batches(double (&pass)[kPasses], double& interface_batches,
                      double (&view_groups)[kGroups]);

    /// Before the GL context goes, with the scene and targets still there.
    void stop(Ogre::SceneManager* scene, std::initializer_list<Ogre::RenderTarget*> targets);

    /// Set to list what the interface queue draws, in order, the next time it
    /// is drawn (a diagnostic: how far its draws could be merged).
    bool dump_overlay_next = false;

    void preViewportUpdate(const Ogre::RenderTargetViewportEvent& evt) override;
    void postViewportUpdate(const Ogre::RenderTargetViewportEvent& evt) override;
    void renderQueueStarted(Ogre::uint8 queue, const Ogre::String&, bool&) override;
    void renderQueueEnded(Ogre::uint8 queue, const Ogre::String&, bool&) override;

private:
    bool start();
    void collect();
    void attach_to_targets(std::initializer_list<Ogre::RenderTarget*> targets);
    void begin_timing(int pass, int group);
    void end_timing();
    void dump_overlay_queue();

    struct Timed {
        unsigned query = 0;
        int pass = kView;
        int group = -1;  ///< the view's queue group, or a gap (kGap*), -1 for another pass
    };

    // Used by the console's timer queries only (gpu_timer.cpp).
    [[maybe_unused]] bool tried_ = false;
    [[maybe_unused]] bool usable_ = false;
    [[maybe_unused]] unsigned frame_ = 0;
    [[maybe_unused]] bool sampling_ = false;  ///< this frame is timed
    bool open_ = false;                       ///< a query is between begin and end
    std::vector<unsigned> queries_;           ///< the pool
    std::vector<Timed> timed_;                ///< this or the last sampled frame's, in order
    [[maybe_unused]] bool pending_ = false;   ///< `timed_` has results not read yet
    double sum_frame_ms_ = 0.0;
    double sum_pass_ms_[kPasses] = {};
    unsigned count_ = 0;

    Ogre::SceneManager* scene_ = nullptr;  ///< the scene the queue listener is on
    int current_pass_ = -1;                ///< the viewport being drawn, or -1
    size_t interface_start_ = 0;           ///< the running count at the overlay queue's start
    size_t group_start_ = 0;               ///< the running count at the open group's start
    double sum_group_ms_[kGroups] = {};
    double sum_gap_ms_[3] = {};
    static constexpr int kGapStart = -2;
    static constexpr int kGapBetween = -4;
    static constexpr int kGapEnd = -3;
    unsigned long long group_batch_sum_[kGroups] = {};
    unsigned long long batch_sum_[kPasses] = {};
    unsigned long long interface_sum_ = 0;
    unsigned batch_frames_ = 0;
};

}  // namespace tl::render
