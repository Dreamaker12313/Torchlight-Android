#include "core/hp_events.h"
#include "core/logic_events.h"

namespace tl::core {

namespace {

// Table at native binary address (.rodata)
constexpr float kUnitHpPercentMarks[9] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};

// Table at native binary address (.rodata)
constexpr uint32_t kUnitHpEvents[9] = {
    logic::kHp90Pct,  // 0x39
    logic::kHp80Pct,  // 0x3a
    logic::kHp70Pct,  // 0x3b
    logic::kHp60Pct,  // 0x3c
    logic::kHp50Pct,  // 0x3d
    logic::kHp40Pct,  // 0x3e
    logic::kHp30Pct,  // 0x3f
    logic::kHp20Pct,  // 0x40
    logic::kHp10Pct,  // 0x41
};

// Table at native binary address (.rodata)
constexpr float kPlayerHpPercentMarks[9] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};

// Table at native binary address (.rodata)
constexpr uint32_t kPlayerHpBelowEvents[9] = {
    logic::kPlayerHpBelow90Pct,  // 0x43
    logic::kPlayerHpBelow80Pct,  // 0x44
    logic::kPlayerHpBelow70Pct,  // 0x45
    logic::kPlayerHpBelow60Pct,  // 0x46
    logic::kPlayerHpBelow50Pct,  // 0x47
    logic::kPlayerHpBelow40Pct,  // 0x48
    logic::kPlayerHpBelow30Pct,  // 0x49
    logic::kPlayerHpBelow20Pct,  // 0x4a
    logic::kPlayerHpBelow10Pct,  // 0x4b
};

// Table at native binary address (.rodata)
constexpr uint32_t kPlayerHpAboveEvents[9] = {
    logic::kPlayerHpAbove90Pct,  // 0x4c
    logic::kPlayerHpAbove80Pct,  // 0x4d
    logic::kPlayerHpAbove70Pct,  // 0x4e
    logic::kPlayerHpAbove60Pct,  // 0x4f
    logic::kPlayerHpAbove50Pct,  // 0x50
    logic::kPlayerHpAbove40Pct,  // 0x51
    logic::kPlayerHpAbove30Pct,  // 0x52
    logic::kPlayerHpAbove20Pct,  // 0x53
    logic::kPlayerHpAbove10Pct,  // 0x54
};

}  // namespace

// CBaseUnit::broadcastHPThreshholdEvents
void broadcast_unit_hp_events(float max_hp, float old_hp, float min_hp, float delta, bool fire_all,
                              std::vector<uint32_t>& out) {
    if (!(delta < 0.0f)) return;
    const float new_hp = old_hp + delta;
    if (!(new_hp < min_hp)) return;

    const float old_pct = old_hp / max_hp;
    const float new_pct = new_hp / max_hp;

    if (!fire_all) {
        for (int i = 8; i >= 0; --i) {
            const float mark = kUnitHpPercentMarks[i];
            if (old_pct > mark && new_pct <= mark) {
                out.push_back(kUnitHpEvents[i]);
                return;
            }
        }
    } else {
        for (int i = 0; i < 9; ++i) {
            const float mark = kUnitHpPercentMarks[i];
            if (old_pct > mark && new_pct <= mark) {
                out.push_back(kUnitHpEvents[i]);
            }
        }
    }
}

// CGameStateController::broadcastPlayerHPThreshholdEvents
void broadcast_player_hp_events(float max_hp, float prev_hp, float delta,
                                std::vector<uint32_t>& out) {
    if (!(max_hp > 0.0f)) return;
    const float prev_pct = prev_hp / max_hp;
    if (prev_pct == 0.0f) return;

    const float new_hp = prev_hp + delta;
    const float new_pct = new_hp / max_hp;

    for (int i = 0; i < 9; ++i) {
        const float mark = kPlayerHpPercentMarks[i];
        if (prev_pct > mark && new_pct <= mark) {
            out.push_back(kPlayerHpBelowEvents[i]);
        } else if (prev_pct < mark && new_pct >= mark) {
            out.push_back(kPlayerHpAboveEvents[i]);
        }
    }
}

}  // namespace tl::core
