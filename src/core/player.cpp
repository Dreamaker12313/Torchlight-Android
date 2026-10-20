#include "core/player.h"

#include "core/gamepad.h"
#include "core/navigation.h"
#include "core/passability.h"

#include <cmath>

namespace tl::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;

/// The shortest way round from `from` to `to`, in radians.
// PORT: the shortest turn between two of the port's yaws.
float angle_delta(float from, float to) {
    float d = to - from;
    while (d > kPi) d -= 2.0f * kPi;
    while (d < -kPi) d += 2.0f * kPi;
    return d;
}

}  // namespace

// PORT: the player's motion put at a point.
void Player::place(const Vec3& at, float facing_radians) {
    position_ = at;
    facing_ = facing_radians;
    speed_ = 0.0f;
    blocked_ = false;
    motion_ = MotionState{};
    motion_.radius = body_.radius;
    motion_.position = {at.x, at.y + body_.radius, at.z};
}

// PORT: the player's motion moved without a reset.
void Player::nudge(const Vec3& at) {
    position_ = at;
    motion_.position = {at.x, at.y + motion_.radius, at.z};
}

// PORT: the player's body (its radius) set.
void Player::set_body(const Body& body) {
    body_ = body;
    motion_.radius = body.radius;
    motion_.position = {position_.x, position_.y + body.radius, position_.z};
}

// INVENTED: the player's animation from its motion alone, `Run` or `Idle`.
const char* Player::animation() const { return moving() ? "Run" : "Idle"; }

// INVENTED: the run's playback rate, the speed over the running speed.
float Player::animation_rate() const {
    if (!moving() || stats_.running_speed <= 0.0f) return 1.0f;
    return speed_ / stats_.running_speed;
}

// PORT: a route for the port's own walk along a path.
void Player::follow(std::vector<Vec3> route) {
    route_ = std::move(route);
    stalled_ = 0;
    last_remaining_ = -1.0f;
}

// PORT: the port's own walk along a path dropped.
void Player::stop_following() {
    route_.clear();
    stalled_ = 0;
    last_remaining_ = -1.0f;
}

void Player::update(float delta, const InputState& stick, float camera_yaw,
                    const CollisionWorld& world, const PassabilityMap* passability,
                    const NavGrid* nav) {
    blocked_ = false;
    if (!(delta > 0.0f)) return;

    // The stick wins over a followed route.
    if (stick.moving()) stop_following();

    InputState in = stick;
    if (!route_.empty()) {
        const Vec3& next = route_.front();
        const float dx = next.x - position_.x;
        const float dz = next.z - position_.z;
        const float remaining = std::sqrt(dx * dx + dz * dz);
        if (remaining <= kArriveRadius) {
            route_.erase(route_.begin());
            stalled_ = 0;
            last_remaining_ = -1.0f;
            if (route_.empty()) {
                speed_ = 0.0f;
                return;
            }
        } else {
            // Measured, not `blocked_`: a push-out happens after this returns.
            if (last_remaining_ >= 0.0f && remaining > last_remaining_ - kProgressPerFrame) {
                if (++stalled_ >= kStallFrames) {
                    stop_following();
                    speed_ = 0.0f;
                    return;
                }
            } else {
                stalled_ = 0;
            }
            last_remaining_ = remaining;
        }
        if (!route_.empty()) {
            // The screen-relative axes that would produce this world direction: the
            // inverse of the rotation below.
            const Vec3& want = route_.front();
            const float wx = want.x - position_.x;
            const float wz = want.z - position_.z;
            const float length = std::sqrt(wx * wx + wz * wz);
            if (length > 1e-4f) {
                const float sin_c = std::sin(camera_yaw);
                const float cos_c = std::cos(camera_yaw);
                in.move_y = (wx * sin_c + wz * cos_c) / length;
                in.move_x = (-wx * cos_c + wz * sin_c) / length;
            }
        }
    }

    motion_.radius = body_.radius;
    motion_.walking_speed = stats_.walking_speed;
    motion_.running_speed = stats_.running_speed;

    const float magnitude = in.magnitude();
    // No destination below a tenth of full deflection (`CGameClient` stick target).
    //
    if (magnitude <= tl::core::kStickThreshold) {
        speed_ = 0.0f;
        // Still subject to the motion: a character off the ground settles.
        if (!world.empty()) {
            motion_.pathing = false;
            update_motion(delta, world, motion_, passability);
            position_ = {motion_.position.x, motion_.position.y - motion_.radius,
                         motion_.position.z};
        }
        return;
    }

    // Screen-relative to world: the direction to the console's stick destination point.
    //
    const float sin_yaw = std::sin(camera_yaw);
    const float cos_yaw = std::cos(camera_yaw);
    float target_x = 0.0f;
    float target_z = 0.0f;
    stick_destination(in.move_x, in.move_y, camera_yaw, position_.x, position_.z, std::sin(facing_),
                      std::cos(facing_), false, &target_x, &target_z);
    float dir_x = target_x - position_.x;
    float dir_z = target_z - position_.z;

    const float len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    if (!(len > 0.0f)) {
        speed_ = 0.0f;
        return;
    }
    dir_x /= len;
    dir_z /= len;

    // Deflection picks a speed between the two the unit data authors.
    speed_ = stats_.walking_speed + (stats_.running_speed - stats_.walking_speed) * magnitude;

    if (world.empty()) {
        // No collision loaded at all: move freely.
        position_.x = position_.x + dir_x * speed_ * delta;
        position_.z = position_.z + dir_z * speed_ * delta;
    } else {
        // DIVERGENCE: walks straight at the stick's point where the console routes to it.
        // So a step into a cell `CLevel::calculatePassability` closed, or
        // into a `Path Node Occupation` blocker, is refused, sliding on one open axis.
        // Standing in a closed map cell lifts the map's gate until out; never a blocker's.
        auto blocker = [&](const Vec3& at) {
            return nav != nullptr && nav->ready() && nav->blocked(at.x, at.z);
        };
        const bool map_gate =
            passability != nullptr && passability->ready() && passability->map_passable(position_);
        const bool blocker_gate = !blocker(position_);
        if (map_gate || blocker_gate) {
            const float reach = speed_ * delta;
            auto open = [&](float dx, float dz) {
                const Vec3 at{position_.x + dx * reach, position_.y, position_.z + dz * reach};
                if (map_gate && !passability->map_passable(at)) return false;
                if (blocker_gate && blocker(at)) return false;
                return true;
            };
            if (!open(dir_x, dir_z)) {
                if (std::fabs(dir_x) > 1e-3f && open(dir_x, 0.0f)) {
                    dir_z = 0.0f;
                } else if (std::fabs(dir_z) > 1e-3f && open(0.0f, dir_z)) {
                    dir_x = 0.0f;
                } else {
                    dir_x = 0.0f;
                    dir_z = 0.0f;
                }
            }
        }
        if (dir_x == 0.0f && dir_z == 0.0f) {
            motion_.pathing = false;
            update_motion(delta, world, motion_, passability);
            position_ = {motion_.position.x, motion_.position.y - motion_.radius,
                         motion_.position.z};
            blocked_ = true;
            const float wanted_facing = std::atan2(in.move_y * sin_yaw - in.move_x * cos_yaw,
                                                   in.move_y * cos_yaw + in.move_x * sin_yaw);
            const float step = kTurnRate * delta;
            const float diff = angle_delta(facing_, wanted_facing);
            facing_ =
                std::fabs(diff) <= step ? wanted_facing : facing_ + (diff > 0.0f ? step : -step);
            return;
        }
        motion_.pathing = true;
        steer(motion_, dir_x, dir_z, speed_);
        const Vec3 was = motion_.position;
        update_motion(delta, world, motion_, passability);
        // A slide that ends inside a gated blocker or closed map cell is not made.
        const Vec3 moved{motion_.position.x, motion_.position.y - motion_.radius,
                         motion_.position.z};
        if ((blocker_gate && blocker(moved)) || (map_gate && !passability->map_passable(moved))) {
            motion_.position = was;
        }
        position_ = {motion_.position.x, motion_.position.y - motion_.radius, motion_.position.z};
        const float mx = motion_.position.x - was.x;
        const float mz = motion_.position.z - was.z;
        blocked_ = std::sqrt(mx * mx + mz * mz) < speed_ * delta * 0.5f;
    }

    // Turn towards the step's direction, not where collision's slide went.
    const float wanted_facing = std::atan2(dir_x, dir_z);
    const float step = kTurnRate * delta;
    const float diff = angle_delta(facing_, wanted_facing);
    if (std::fabs(diff) <= step) {
        facing_ = wanted_facing;
    } else {
        facing_ += (diff > 0.0f ? step : -step);
    }
}

}  // namespace tl::core
