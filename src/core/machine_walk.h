// The level's machines walked as the list stood when the walk began
// (core/script.h's LevelScript::level_machines).
#pragma once

#include <cstddef>
#include <vector>

namespace tl::core {

class LevelScript;

/// One walk of the level's machines (`LevelScript::level_machines`): the list
/// as long as it was when the walk began, read by index, because a machine linked inside the
/// loop grows the vector under a range-for. One that joins is first reached by the next walk.
class MachineWalk {
public:
    class iterator {
    public:
        iterator(const std::vector<LevelScript*>* list, size_t index)
            : list_(list), index_(index) {}
        // PORT: a walk over the level's machines.
        LevelScript* operator*() const { return (*list_)[index_]; }
        // PORT: a walk over the level's machines.
        iterator& operator++() {
            ++index_;
            return *this;
        }
        // PORT: a walk over the level's machines.
        bool operator!=(const iterator& other) const { return index_ != other.index_; }

    private:
        const std::vector<LevelScript*>* list_;
        size_t index_;
    };

    explicit MachineWalk(const std::vector<LevelScript*>& list)
        : list_(&list), count_(list.size()) {}
    // PORT: a walk over the level's machines.
    iterator begin() const { return {list_, 0}; }
    // PORT: a walk over the level's machines.
    iterator end() const { return {list_, count_}; }
    // PORT: a walk over the level's machines.
    bool empty() const { return count_ == 0; }
    // PORT: a walk over the level's machines.
    LevelScript* front() const { return (*list_)[0]; }

private:
    const std::vector<LevelScript*>* list_;
    size_t count_;
};

}  // namespace tl::core
