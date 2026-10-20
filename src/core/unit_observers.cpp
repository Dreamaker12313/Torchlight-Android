#include "core/unit_observers.h"

namespace tl::core {

namespace {

/// `TArrayList<T>::remove(T)`: the first match is
/// overwritten by the last entry and the count drops by one.
bool remove_swapping(std::vector<UnitObserver*>& list, UnitObserver* observer) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] != observer) continue;
        list[i] = list.back();
        list.pop_back();
        return true;
    }
    return false;
}

}  // namespace

void UnitObservers::add(const Unit& unit, UnitObserver* observer) {
    // `CLevel::addListenerToUnit(long long, iUnitObserver*)`.
    //
    if (unit.node == -1) return;
    std::vector<UnitObserver*>& list = lists_[unit];
    for (UnitObserver* listening : list) {
        if (listening == observer) return;
    }
    list.push_back(observer);
}

void UnitObservers::remove(const Unit& unit, UnitObserver* observer) {
    // `CLevel::removeListenerFromUnit(CBaseUnit*, iUnitObserver*)`.
    //
    if (observer == nullptr) return;
    const auto found = lists_.find(unit);
    if (found == lists_.end()) return;
    remove_swapping(found->second, observer);
}

void UnitObservers::remove_all(UnitObserver* observer) {
    // `CLevel::removeListenerFromUnits(iUnitObserver*)`.
    //
    if (observer == nullptr) return;
    for (auto& entry : lists_) remove_swapping(entry.second, observer);
}

void UnitObservers::broadcast(const Unit& unit, int state) {
    // `CLevel::unitBroadcastMessage(CBaseUnit*, EUNIT_STATES)`.
    const auto found = lists_.find(unit);
    if (found == lists_.end()) return;
    // By index, the count read again each time: a call may shrink the list.
    for (size_t i = 0; i < found->second.size(); ++i) {
        found->second[i]->unit_state_change(unit.machine, unit.node, state);
    }
    if (state == unit_state::kDestroyed) lists_.erase(unit);
}

// PORT: an accessor of the observer lists.
const std::vector<UnitObserver*>* UnitObservers::observers_of(const Unit& unit) const {
    const auto found = lists_.find(unit);
    return found == lists_.end() ? nullptr : &found->second;
}

}  // namespace tl::core
