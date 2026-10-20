// CRandomizer: the weighted pick every generator goes through (chunk type, room piece,
// layout, spawn-class entry). Console layout: values +0x08, odds +0x18, normalised +0x28,
// type +0x38, decay +0x3c, dirty +0x40 (`CRandomizer::CRandomizer`).
// Every pick draws exactly one `UTILITIES::randomBetween(0, 1)`, keeping a seeded stream's
// draw count the original's. Header-only.
#pragma once

#include "core/random.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tl::core {

/// `TArrayList<T>` indexing as the original inlines it: an index past the end reads
/// element 0. (The original bounds by capacity; nothing here reaches the difference.)
// PORT: a `TArrayList` read on the port's vector (the console's falls back to `data[0]` past the end).
template <typename T>
T& tarray_at(std::vector<T>& list, size_t index) {
    return index < list.size() ? list[index] : list[0];
}

// PORT: a `TArrayList` read on the port's vector (the console's falls back to `data[0]` past the end).
template <typename T>
const T& tarray_at(const std::vector<T>& list, size_t index) {
    return index < list.size() ? list[index] : list[0];
}

/// `ERANDOMIZER_TYPE`. The binary holds no names for it; these are the two
/// values the code tests. 1 multiplies a picked entry's odds by `decay`.
enum class RandomizerType : int32_t {
    kNormal = 0,
    kDecay = 1,
};

class Randomizer {
public:
    /// `CRandomizer::CRandomizer(ERANDOMIZER_TYPE)`.
    ///
    explicit Randomizer(RandomizerType type = RandomizerType::kNormal) : type_(type) {}

    /// +0x5c, 1.0 from the constructor. No CRandomizer method writes it; its
    /// users set it directly.
    float decay = 1.0f;

    RandomizerType type() const { return type_; }

    /// The odds list's count (+0x30), which is what callers test as "has
    /// choices" (`CLevelTemplateData::createRandomLayout` reads it directly).
    uint32_t choice_count() const { return static_cast<uint32_t>(odds_.size()); }

    // PORT: an accessor of the randomizer's values.
    int32_t value_at(size_t index) const { return tarray_at(values_, index); }

    /// `CRandomizer::addChoice(int, int)`.
    /// Returns the choice count before the add.
    uint32_t add_choice(int32_t value, int32_t odds) {
        const uint32_t before = choice_count();
        odds_.push_back(static_cast<float>(odds));
        values_.push_back(value);
        dirty_ = true;
        return before;
    }

    /// `CRandomizer::setChoiceOdds(int, int)`:
    /// the first entry holding `value` only.
    void set_choice_odds(int32_t value, int32_t odds) {
        for (size_t i = 0; i < values_.size(); ++i) {
            if (tarray_at(values_, i) != value) continue;
            tarray_at(odds_, i) = static_cast<float>(odds);
            dirty_ = true;
            return;
        }
    }

    /// `CRandomizer::removeChoice(unsigned int)`:
    /// the first entry holding `value` is
    /// overwritten by the last one, in all three lists.
    void remove_choice(uint32_t value) {
        for (size_t i = 0; i < values_.size(); ++i) {
            if (static_cast<uint32_t>(tarray_at(values_, i)) != value) continue;
            if (i < odds_.size()) {
                odds_[i] = odds_.back();
                odds_.pop_back();
            }
            if (i < values_.size()) {
                values_[i] = values_.back();
                values_.pop_back();
            }
            if (i < normalised_.size()) {
                normalised_[i] = normalised_.back();
                normalised_.pop_back();
            }
            dirty_ = true;
            return;
        }
    }

    /// `CRandomizer::getOdds(int)`: truncated to int.
    int32_t get_odds(uint32_t index) const { return static_cast<int32_t>(tarray_at(odds_, index)); }

    /// The lists as they stand, for a test to compare with the console's.
    float odds_at(size_t index) const { return odds_[index]; }
    size_t normalised_count() const { return normalised_.size(); }
    float normalised_at(size_t index) const { return normalised_[index]; }
    bool dirty() const { return dirty_; }

    /// `CRandomizer::clear()`. The type and
    /// decay survive.
    void clear() {
        odds_.clear();
        normalised_.clear();
        values_.clear();
        dirty_ = true;
    }

    /// `CRandomizer::computeOdds()`: each entry's odds times
    /// the reciprocal of their float sum, as the console does; dividing
    /// each, as Linux does, is an ulp off (a lone choice of 83 would not reach 1.0f).
    void compute_odds() {
        float total = 0.0f;
        for (size_t i = 0; i < odds_.size(); ++i) total += tarray_at(odds_, i);
        normalised_.clear();
        const float reciprocal = 1.0f / total;
        for (size_t i = 0; i < odds_.size(); ++i)
            normalised_.push_back(tarray_at(odds_, i) * reciprocal);
        dirty_ = false;
    }

    /// `CRandomizer::hasValidChoices()`: any normalised entry above 0.
    bool has_valid_choices() {
        if (dirty_) compute_odds();
        for (size_t i = 0; i < normalised_.size(); ++i) {
            if (0.0f < tarray_at(normalised_, i)) return true;
        }
        return false;
    }

    /// `CRandomizer::setRandomizerToNormal(float)`: every entry's
    /// odds become `odds` (a non-positive value leaves them alone), the type
    /// becomes normal, and the odds are recomputed.
    void set_randomizer_to_normal(float odds) {
        if (0.0f < odds && !odds_.empty()) {
            for (size_t i = 0; i < odds_.size(); ++i) tarray_at(odds_, i) = odds;
        }
        type_ = RandomizerType::kNormal;
        dirty_ = true;
        compute_odds();
    }

    /// `CRandomizer::setRandomSeed(unsigned int)` is `setSeed`.
    static void set_random_seed(RunicRandom& rng, uint32_t seed) {
        rng.set_seed(static_cast<int32_t>(seed));
    }

    /// `CRandomizer::getRandom()` and
    /// `CRandomizer::getRandom(int&)`. One roll in [0, 1)
    /// against the running sum of normalised odds; none exceeded returns 0, index -1. Decay
    /// scales the picked entry's odds (entry 0's when none was picked).
    int32_t get_random(RunicRandom& rng, int32_t* index = nullptr) {
        if (dirty_) compute_odds();
        const float roll = rng.between(0.0f, 1.0f);
        float cumulative = 0.0f;
        for (size_t i = 0; i < normalised_.size(); ++i) {
            cumulative += tarray_at(normalised_, i);
            if (roll < cumulative) {
                if (type_ == RandomizerType::kDecay) {
                    float& odds = tarray_at(odds_, i);
                    odds = odds * decay;
                    dirty_ = true;
                }
                if (index != nullptr) *index = static_cast<int32_t>(i);
                return tarray_at(values_, i);
            }
        }
        // The original dereferences the odds list unconditionally here, which
        // faults on an empty decaying randomizer; an empty one is left alone.
        if (type_ == RandomizerType::kDecay && !odds_.empty()) {
            odds_[0] = odds_[0] * decay;
            dirty_ = true;
        }
        if (index != nullptr) *index = -1;
        return 0;
    }

private:
    RandomizerType type_;            ///< +0x58
    std::vector<int32_t> values_;    ///< +0x10
    std::vector<float> odds_;        ///< +0x28
    std::vector<float> normalised_;  ///< +0x40
    bool dirty_ = true;              ///< +0x60
};

}  // namespace tl::core
