#pragma once

#include <cstdint>
#include <vector>

namespace tl::core {

/// `CBaseUnit::broadcastHPThreshholdEvents`:
/// the HP marks crossed, as event IDs appended to `out`. Marks 0.9 .. 0.1;
/// events HP 90 PCT (0x39) .. HP 10 PCT (0x41).
/// Only for `delta < 0` taking the HP below the lowest so far (`min_hp`);
/// `max_hp` is divided by unguarded. The caller tests `+0x1d0` and not-a-player
/// first. `fire_all` false walks 10% up to 90% and fires the first crossed; true
/// walks 90% down and fires every one.
void broadcast_unit_hp_events(float max_hp, float old_hp, float min_hp, float delta, bool fire_all,
                              std::vector<uint32_t>& out);

/// `CGameStateController::broadcastPlayerHPThreshholdEvents`: the
/// player's marks crossed, as event IDs appended to `out`. Marks 0.9 .. 0.1;
/// Below events 0x43 .. 0x4b, Above 0x4c .. 0x54.
/// Only with `max_hp > 0` and `prev_hp / max_hp != 0`. From 90%
/// down: crossing downward fires Below, else crossing upward fires Above.
void broadcast_player_hp_events(float max_hp, float prev_hp, float delta,
                                std::vector<uint32_t>& out);

}  // namespace tl::core
