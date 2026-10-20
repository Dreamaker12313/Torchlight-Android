#include "core/effects.h"

#include "core/random.h"
#include "core/runic_strings.h"
#include "formats/float_convert.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tl::core {
namespace {

/// `STRINGS::StringUpper`, which is what every `TYPE`, `ACTIVATION`,
/// `DURATION` and `STATMODIFYNAME` comparison runs through.
// PORT: ASCII upper-casing, the case-blind compare of animation names.
std::string upper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return text;
}

/// `STRINGS::TokenizeString(s, L",")`, which drops nothing and trims nothing.
// PORT: asset parsing, a field split on a separator.
std::vector<std::string> tokenize(const std::string& text, char separator) {
    std::vector<std::string> out;
    if (text.empty()) return out;
    size_t start = 0;
    while (true) {
        const size_t at = text.find(separator, start);
        if (at == std::string::npos) {
            out.push_back(text.substr(start));
            break;
        }
        out.push_back(text.substr(start, at - start));
        start = at + 1;
    }
    return out;
}

/// `Ogre::Math::Sign`.
// PORT: a number's sign for an effect's text.
float sign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

/// The empty-check both display passes make before inserting a separator: a
/// newline goes in only between two parts that both have something in them.
// PORT: a line added to an effect description.
void append_line(std::string& into, const std::string& part) {
    if (!into.empty() && !part.empty()) into += "\n";
    into += part;
}

/// `gEffect_Activation_Names`.
// PORT: asset parsing, `ACTIVATION`'s word as the port's enum.
EffectActivation activation_of(const std::string& name) {
    if (name == "PASSIVE") return EffectActivation::Passive;
    if (name == "TRANSFER") return EffectActivation::Transfer;
    if (name == "DYNAMIC") return EffectActivation::Dynamic;
    return EffectActivation::Dynamic;
}

/// `gDAMAGE_TYPES`.
// PORT: asset parsing, `DAMAGE_TYPE`'s word as the port's enum.
DamageType damage_type_of(const std::string& name) {
    if (name == "MAGICAL") return DamageType::Magical;
    if (name == "FIRE") return DamageType::Fire;
    if (name == "ICE") return DamageType::Ice;
    if (name == "ELECTRIC") return DamageType::Electric;
    if (name == "POISON") return DamageType::Poison;
    if (name == "ALL") return DamageType::All;
    return DamageType::Physical;
}

/// `gEFFECT_STAT_MODIFIER_NAMES`.
// PORT: asset parsing, `STAT`'s word as the port's enum.
bool stat_of(const std::string& name, EffectStat& out) {
    const std::string key = upper(name);
    if (key == "MELEE") {
        out = EffectStat::Melee;
        return true;
    }
    if (key == "RANGED") {
        out = EffectStat::Ranged;
        return true;
    }
    if (key == "DEFENSE") {
        out = EffectStat::Defense;
        return true;
    }
    if (key == "MAGIC") {
        out = EffectStat::Magic;
        return true;
    }
    if (key == "LEVEL") {
        out = EffectStat::Level;
        return true;
    }
    if (key == "OWNERLEVEL") {
        out = EffectStat::OwnerLevel;
        return true;
    }
    return false;
}

/// The effect types the display path treats specially, by name: the row index
/// is the data file's order, which a mod can change.
const char* const kSkillNameTypes[] = {
    "SKILL BONUS",          "LEARN SKILL",         "CAST SKILL", "CAST SKILL AT TARGET",
    "CAST SKILL ON STRUCK", "CAST SKILL ON DEATH", "STOP SKILL",
};
const char* const kMonsterNameTypes[] = {"TRANSFORM", "TRANSFORM PERMANENT", "DESUMMON MONSTER"};
const char* const kPerTickTypes[] = {"MANA RECHARGE", "HP RECHARGE", "MANA RECHARGE PLAYER",
                                     "HP RECHARGE PLAYER"};

template <size_t N>
bool type_is_one_of(const EffectTypeTable& table, int type, const char* const (&names)[N]) {
    const EffectTypeInfo* info = table.at(type);
    if (info == nullptr) return false;
    for (size_t i = 0; i < N; ++i) {
        if (info->name == names[i]) return true;
    }
    return false;
}

/// One stat weight out of an effect's parallel arrays, or 0 when the effect
/// does not name that stat. Both `CAffix::fillOutStatBonuses` and
/// `CSkillEffectAndAffixes::fillOutStatBonuses` inline this loop.
float stat_weight(const Effect& effect, int stat) {
    for (size_t i = 0; i < effect.stat_ids.size(); ++i) {
        if (static_cast<int>(effect.stat_ids[i]) != stat) continue;
        return i < effect.stat_values.size() ? effect.stat_values[i] : 0.0f;
    }
    return 0.0f;
}

/// The body both `fillOutStatBonuses` share, over one effect.
// `CEffect::fillOutStatBonuses`; see the header for the max rule.
void fill_from_effect(const Effect& effect, std::array<float, kEffectStatCount>& out,
                      bool take_max) {
    for (int stat = 0; stat < kEffectStatCount; ++stat) {
        const float found = stat_weight(effect, stat);
        if (take_max) {
            out[stat] = std::max(found, out[stat]);
        } else {
            // Without the max rule the assignment is unconditional, so an
            // effect that names no stat zeroes what an earlier one found. The
            // original does this; the screen only ever asks for the max.
            out[stat] = found;
        }
    }
}

}  // namespace

std::string damage_type_display_string(DamageType type) {
    // through the string table.
    switch (type) {
        case DamageType::Physical: return std::string();
        case DamageType::Magical: return "Magical";
        case DamageType::Fire: return "Fire";
        case DamageType::Ice: return "Ice";
        case DamageType::Electric: return "Electric";
        case DamageType::Poison: return "Poison";
        case DamageType::All: return "Phys/Elem";  // the console's word
    }
    return std::string();
}

void EffectTypeTable::load(const tl::formats::AdmFile& file) {
    // `parseEffects`:
    // `media/EffectsList.dat`, every `[EFFECT]` under the root in file order.
    // The row index is the effect type.
    rows_.clear();
    for (const tl::formats::AdmSection& child : file.root.children) {
        EffectTypeInfo row;
        row.name = child.string_or("NAME", std::string());
        row.good = child.string_or("GOODDES", std::string());
        row.good_ot = child.string_or("GOODDESOT", std::string());
        row.bad = child.string_or("BADDES", std::string());
        row.bad_ot = child.string_or("BADDESOT", std::string());
        for (int i = 0; i < kEffectValueCount; ++i) {
            row.value_tags[i] = child.string_or("VALUE" + value_as_string(i + 1), std::string());
            row.graphs[i] = child.string_or("GRAPH" + value_as_string(i + 1), std::string());
        }
        const std::string type = upper(child.string_or("TYPE", "VALUE"));
        row.bonus = type == "PERCENT" ? 1 : (type == "NONE" ? 2 : 0);
        row.min_value = static_cast<int>(child.integer_or("MINVAL", static_cast<uint32_t>(-1)));
        row.max_value = static_cast<int>(child.integer_or("MAXVAL", 1));
        rows_.push_back(std::move(row));
    }
}

// PORT: an effect type's row by name.
int EffectTypeTable::index_of(const std::string& type) const {
    const std::string key = upper(type);
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].name == key) return static_cast<int>(i);
    }
    return -1;
}

// PORT: an effect type's row by index.
const EffectTypeInfo* EffectTypeTable::at(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= rows_.size()) return nullptr;
    return &rows_[static_cast<size_t>(index)];
}

// The effect types `CEffect::getDisplayStats`
// names a skill for, by their rows' names.
bool effect_name_is_skill(const EffectTypeTable& table, int type) {
    return type_is_one_of(table, type, kSkillNameTypes);
}

// The effect types `CEffect::getDisplayStats`
// names a unit for, by their rows' names.
bool effect_name_is_monster(const EffectTypeTable& table, int type) {
    return type_is_one_of(table, type, kMonsterNameTypes);
}

// The recharge effect types, by their rows' names: per tick, as
// `CEffect::getVisualCalculatedValues` treats them.
bool effect_is_per_tick(const EffectTypeTable& table, int type) {
    return type_is_one_of(table, type, kPerTickTypes);
}

std::vector<Effect> unit_own_effects(const tl::formats::AdmSection& unit,
                                     const EffectTypeTable& table) {
    // CBaseUnit::reapplyEffects:
    // `GetDataGroupByName("EFFECTS")` and `("EFFECT")`, and nothing when
    // neither is there.
    const tl::formats::AdmSection* effects = nullptr;
    const tl::formats::AdmSection* effect = nullptr;
    for (const tl::formats::AdmSection& child : unit.children) {
        const std::string name = upper(child.name);
        if (effects == nullptr && name == "EFFECTS") effects = &child;
        if (effect == nullptr && name == "EFFECT") effect = &child;
    }
    std::vector<Effect> out;
    if (effects == nullptr && effect == nullptr) return out;
    const tl::formats::AdmSection& group = effects != nullptr ? *effects : unit;
    // CEffectManager::createEffects.
    if (upper(group.name) == "EFFECT") {
        out.push_back(parse_effect(group, table));
        return out;
    }
    for (const tl::formats::AdmSection& child : group.children) {
        if (upper(child.name) == "EFFECT") out.push_back(parse_effect(child, table));
    }
    return out;
}

Effect parse_effect(const tl::formats::AdmSection& section, const EffectTypeTable& table) {
    // `CEffect::CEffect(CDataGroup*, CBaseUnit*)`,
    // the read half.
    Effect effect;

    // +0x1c: `TYPE` upper-cased against the type table. No match leaves the
    // constructor's own default in place.
    effect.type = table.index_of(section.string_or("TYPE", std::string()));

    // +0x20: `ACTIVATION`, default `DYNAMIC`.
    effect.activation = activation_of(upper(section.string_or("ACTIVATION", "DYNAMIC")));

    // +0x24 / +0x28: `DURATION`. An absent key reads as `ALWAYS` and leaves the
    // "authored" flag clear; `INSTANT` is -900, and `ALWAYS`, `0` and anything
    // that is not a positive number are -1000.
    // `DURATION` is a string even when it holds a number, so the seconds come
    // out of `STRINGS::GetFloat` rather than out of a float record.
    const std::string authored = section.string_or("DURATION", std::string());
    std::string duration = upper(authored);
    if (duration.empty()) {
        duration = "ALWAYS";
        effect.authored_duration = false;
    } else {
        effect.authored_duration = true;
    }
    if (duration == "INSTANT") {
        effect.duration = kDurationInstant;
    } else {
        effect.duration = kDurationAlways;
        if (duration != "0" && duration != "ALWAYS") {
            const float seconds = static_cast<float>(std::atof(authored.c_str()));
            if (seconds > 0.0f) effect.duration = seconds;
        }
    }

    // `NAME`, `LINK_NAME`, `PARTICLE_FX` and `GRAPHOVERRIDE` are each upper-cased
    // as read (`STRINGS::StringUpper`).
    //
    effect.name = upper(section.string_or("NAME", std::string()));
    effect.link_name = upper(section.string_or("LINK_NAME", std::string()));
    effect.level = section.integer_or("LEVEL", 0);
    // +0x31: `EXCLUSIVE`, over the default `initValues` left.
    effect.exclusive = section.bool_or("EXCLUSIVE", effect.exclusive);
    // +0x130: `UNITTHEME` (string, upper-cased). `CEffect::CEffect`.
    //
    effect.unit_theme = upper(section.string_or("UNITTHEME", std::string()));
    // +0x33 `SAVE` over `initValues`' 0; +0xb0 `PARTICLE_FX` through `setFX`.
    effect.save = section.bool_or("SAVE", effect.save);
    effect.particle_fx = upper(section.string_or("PARTICLE_FX", std::string()));
    // +0x36 (X360 +0x2e): `USEOWNERLEVEL`.
    effect.use_owner_level = section.bool_or("USEOWNERLEVEL", effect.use_owner_level);

    // +0xc4..+0xd4 and the graph names: each number under the key the type's
    // `VALUEn` column names, else `VALUE`, `VALUE2`..; `NOGRAPH` takes every
    // curve away, `GRAPHOVERRIDE` replaces all with one. Quirk kept: the loop's
    // break test is `3 < i + 1`, so the fifth column is never read.
    const bool no_graph = section.bool_or("NOGRAPH", false);
    const std::string graph_override = upper(section.string_or("GRAPHOVERRIDE", std::string()));
    effect.graph_override = graph_override;  // +0x90
    const EffectTypeInfo* info = table.at(effect.type);
    for (int i = 0; i < 4; ++i) {
        if (!no_graph && info != nullptr) {
            effect.graphs[i] = graph_override.empty() ? info->graphs[i] : graph_override;
        }
        // +0x37 is set on every pass that is not `NOGRAPH`.
        if (!no_graph) effect.has_graphs = true;
        constexpr float kAbsent = -999.0f;
        float value = kAbsent;
        if (info != nullptr && !info->value_tags[i].empty()) {
            value = section.float_or(info->value_tags[i], kAbsent);
        }
        if (value == kAbsent) {
            const std::string key = i == 0 ? "VALUE" : "VALUE" + value_as_string(i + 1);
            value = section.float_or(key, kAbsent);
        }
        if (value != kAbsent) {
            effect.values[i] = value;
            // +0x32: the second number was authored.
            if (i == 1) effect.second_value = true;
        }
    }

    // +0xf8 / +0x110: the two comma-separated lists, walked in step. A percent
    // list shorter than the name list defaults to 100, and every percent is
    // stored divided by 100.
    // an empty `STATMODIFYNAME` reads nothing;
    // otherwise a list that tokenises to nothing is replaced by the whole
    // string, so an absent `STATMODIFYPERCENT` is one empty token - the first
    // stat's percent is `GetFloat("")`, 0, and the rest default to 100.
    const std::string name_list = upper(section.string_or("STATMODIFYNAME", std::string()));
    std::vector<std::string> names;
    std::vector<std::string> percents;
    if (!name_list.empty()) {
        const std::string percent_list =
            upper(section.string_or("STATMODIFYPERCENT", std::string()));
        names = tokenize(name_list, ',');
        percents = tokenize(percent_list, ',');
        if (names.empty()) names.push_back(name_list);
        if (percents.empty()) percents.push_back(percent_list);
    }
    for (size_t i = 0; i < names.size(); ++i) {
        EffectStat stat = EffectStat::Melee;
        if (!stat_of(names[i], stat)) continue;
        const std::string percent = i < percents.size() ? percents[i] : std::string("100");
        effect.stat_ids.push_back(stat);
        // `CEffect::CEffect`: the percent rounded
        // to a float and times 0.01f.
        effect.stat_values.push_back(static_cast<float>(std::atof(percent.c_str())) * 0.01f);
    }

    // the types 2, 0x11, 0x23,
    // 0x25-0x28, 0x56 and 0x76 take damage type 1 whatever `DAMAGE_TYPE` says.
    const int t = effect.type;
    if ((t < 0x29 && (0x24 < t || t == 2 || t == 0x11 || t == 0x23)) || t == 0x56 || t == 0x76) {
        effect.damage_type = DamageType::Magical;
    } else {
        effect.damage_type = damage_type_of(upper(section.string_or("DAMAGE_TYPE", "PHYSICAL")));
    }

    // The constructor's last step (which both of its paths reach):
    // a `DAMAGE` effect with a real duration is always exclusive, so a second
    // copy replaces a running one instead of stacking with it.
    constexpr int kEffectTypeDamage = 0x34;
    if (effect.type == kEffectTypeDamage && effect.duration != kDurationAlways &&
        effect.duration != kDurationInstant) {
        effect.exclusive = true;
    }
    return effect;
}

Effect clone_effect(const Effect& source) {
    // `CEffect::clone`: four numbers and four
    // curves; the port's fifth slot is never filled.
    // The authored-`DURATION` flag is copied.
    return source;
}

void set_owner_level(Effect& effect, std::optional<int32_t> owner_level) {
    // `CEffect::setOwner`: `+0x2e`, an owner, and the second
    // argument; then `+0x8 = level == -1 ? 0 : level`, and 0 when that is
    // above 1000.
    if (!effect.use_owner_level || !owner_level.has_value()) return;
    const uint32_t level = *owner_level == -1 ? 0u : static_cast<uint32_t>(*owner_level);
    effect.level = level <= 1000u ? level : 0u;
}

Effect value_effect(int type, float duration, float value, EffectActivation activation, bool flag30,
                    float f38) {
    // `CEffect::CEffect(EEFFECT_TYPE, ...)`.
    Effect effect;
    effect.type = type;                // +0x14 (X360)
    effect.duration = duration;        // +0x1c
    effect.activation = activation;    // +0x18
    effect.authored_duration = false;  // +0x21, not written
    effect.b30 = flag30;               // +0x28
    effect.f38 = f38;                  // +0x30
    // `initValues`: the graphs, the numbers,
    // the flags and the strings cleared, `+0x2d` (display) 1, `+0x34` 100,
    // and the damage type (`+0xc`) 1.
    effect.damage_type = DamageType::Magical;
    effect.has_graphs = false;
    effect.exclusive = false;
    effect.save = false;
    effect.second_value = false;
    effect.display = true;
    effect.i3c = 100;
    effect.i40 = 0;
    effect.i44 = 0;
    effect.level = 0;
    // the first number, then `calculateBaseValue(0)` twice, which
    // the manager works out again when the effect is held.
    effect.values[0] = value;
    return effect;
}

float effect_column_value(const Effect& effect, size_t column, const StatBook& graphs) {
    // CEffect::value
    const StatGraph* graph =
        effect.graphs[column].empty() ? nullptr : graphs.find(effect.graphs[column]);
    if (graph == nullptr) return effect.values[column];
    // the number times 0.01f, then times the curve.
    return (effect.values[column] * 0.01f) * graph->at(static_cast<float>(effect.level));
}

float modify_effects_by_character(const Effect& effect, const EffectOwner& owner) {
    // `CEffect::modifyEffectsByCharacter(CCharacter*)`:
    // 1 with no `STATMODIFYNAME` or no character (the owner, else the
    // wearer of the equipment). `calculateBaseValue` carries the same loop inline.
    //
    if (effect.stat_ids.empty() || !owner.present) return 1.0f;
    // each step fused onto the factor (`fmadds`), a
    // stat's as `(stat * weight) * 0.01f`.
    float factor = 1.0f;
    for (size_t i = 0; i < effect.stat_ids.size(); ++i) {
        const float weight = i < effect.stat_values.size() ? effect.stat_values[i] : 0.0f;
        switch (effect.stat_ids[i]) {
            case EffectStat::Melee:
                factor = std::fma(static_cast<float>(owner.strength) * weight, 0.01f, factor);
                break;
            case EffectStat::Ranged:
                factor = std::fma(static_cast<float>(owner.dexterity) * weight, 0.01f, factor);
                break;
            case EffectStat::Defense:
                factor = std::fma(static_cast<float>(owner.defense) * weight, 0.01f, factor);
                break;
            case EffectStat::Magic:
                factor = std::fma(static_cast<float>(owner.magic) * weight, 0.01f, factor);
                break;
            case EffectStat::Level:
            case EffectStat::OwnerLevel: {
                // Both take the level itself, floored at 1.
                const uint32_t level =
                    effect.stat_ids[i] == EffectStat::Level ? effect.level : owner.level;
                const float scale =
                    static_cast<float>(level) > 1.0f ? static_cast<float>(level) : 1.0f;
                factor = std::fma(scale, weight, factor);
                break;
            }
        }
    }
    return factor;
}

float effect_base_value(const Effect& effect, EffectCalculate which, const StatBook& graphs,
                        const EffectOwner& owner, RunicRandom* rolls) {
    // `CEffect::calculateBaseValue(ECALCULATETYPES)`.
    // The `Roll` case draws
    // `randomBetweenVolatile(smaller, larger)` only when a generator
    // is handed in; the description path never rolls.
    float max_value = effect.values[1];
    const float min_value = effect.values[0];
    if (max_value == 0.0f) max_value = min_value;

    float chosen = min_value;
    if (which == EffectCalculate::Larger) {
        chosen = std::max(max_value, min_value);
    } else if (which == EffectCalculate::Smaller) {
        chosen = std::min(max_value, min_value);
    } else if (rolls != nullptr) {
        chosen = rolls->between(std::min(max_value, min_value), std::max(max_value, min_value));
    }

    const StatGraph* graph = effect.graphs[0].empty() ? nullptr : graphs.find(effect.graphs[0]);
    if (graph == nullptr) {
        // No curve: the number itself, times the stats' factor.
        return modify_effects_by_character(effect, owner) * chosen;
    }
    const float curve = graph->at(static_cast<float>(effect.level));
    // the stats' factor times `chosen * 0.01f`,
    // then times the curve.
    return (modify_effects_by_character(effect, owner) * (chosen * 0.01f)) * curve;
}

EffectDisplayValues effect_display_values(const Effect& effect, const EffectTypeTable& table,
                                          const StatBook& graphs, const EffectOwner& owner) {
    // `CEffect::getVisualCalculatedValues(unsigned int, bool)`,
    // as the display path calls it: level -1
    // (leave the effect's own level alone) and `false` (compute both ends
    // rather than reusing a rolled value).
    EffectDisplayValues values;
    values.type = effect.type;
    values.duration = effect.duration;
    values.force_min = effect.values[2];
    values.force_max = effect.values[3];
    float max_end = effect_base_value(effect, EffectCalculate::Larger, graphs, owner);
    float min_end = effect_base_value(effect, EffectCalculate::Smaller, graphs, owner);

    const float duration = effect.duration;
    if (duration != kDurationAlways && duration != kDurationInstant &&
        effect_is_per_tick(table, effect.type)) {
        // A recharge effect's number is per tick; the screen shows the whole of
        // it over the duration, at the original's 62.5 Hz tick.
        values.per_tick = true;
        max_end = max_end * duration * 0.016f;
        min_end = min_end * duration * 0.016f;
    }
    values.min = min_end;
    values.max = max_end;
    return values;
}

std::string range_of_values(float a, float b, bool use_max) {
    // `getRangeOfValues(float, float, bool)`.
    const int first = static_cast<int>(std::fabs(std::ceil(a)));
    const int second = static_cast<int>(std::fabs(std::ceil(b)));
    if (second != 0 && second != first)
        return value_as_string(first) + "-" + value_as_string(second);
    return value_as_string(use_max ? std::max(second, first) : std::min(second, first));
}

std::string effect_display_stats(const Effect& effect, const EffectDisplayContext& context,
                                 bool colour) {
    if (!effect.display || context.types == nullptr || context.graphs == nullptr) {
        return std::string();
    }
    return effect_display_stats(
        effect, effect_display_values(effect, *context.types, *context.graphs, context.owner),
        context, colour);
}

std::string effect_display_stats(const Effect& effect, const EffectDisplayValues& values,
                                 const EffectDisplayContext& context, bool colour) {
    // `CEffect::getDisplayStats(CEffectDisplayValues&)`.
    //
    if (!effect.display || context.types == nullptr || context.graphs == nullptr) {
        return std::string();
    }
    const EffectTypeInfo* info = context.types->at(effect.type);
    if (info == nullptr) return std::string();

    // The description is the "over time" variant whenever the duration is a
    // real number rather than one of the two words, and the "bad" variant when
    // the value runs against the sign of the type's own `MAXVAL`. A `None`-typed
    // row, or a zero value, is always good.
    const bool over_time =
        effect.duration != kDurationAlways && effect.duration != kDurationInstant;
    bool good = true;
    if (info->bonus != 2 && values.max != 0.0f) {
        good = sign(values.max) == sign(static_cast<float>(info->max_value));
    }
    std::string text =
        over_time ? (good ? info->good_ot : info->bad_ot) : (good ? info->good : info->bad);

    text = replace_wstring(text, "[VALUE]", range_of_values(values.min, values.max, good));
    const std::string min_text =
        value_as_string(static_cast<int>(std::fabs(std::ceil(values.min))));
    const std::string max_text =
        value_as_string(static_cast<int>(std::fabs(std::ceil(values.max))));
    text = replace_wstring(text, "[VALUE1]", min_text);
    text = replace_wstring(text, "[MINVAL]", min_text);
    text = replace_wstring(text, "[VALUE2]", max_text);
    text = replace_wstring(text, "[MAXVAL]", max_text);
    text = replace_wstring(text, "\\n", "\n");

    std::string name = effect.name;
    if (effect_name_is_skill(*context.types, effect.type)) {
        name = context.skill_display_name ? context.skill_display_name(effect.name) : "???";
        if (name.empty()) name = "???";
    } else if (effect_name_is_monster(*context.types, effect.type)) {
        name = context.monster_display_name ? context.monster_display_name(effect.name) : "???";
        if (name.empty()) name = "???";
    }
    text = replace_wstring(text, "[NAME]", name);
    text = replace_wstring(text, "[VALUE3AND4]",
                           range_of_values(values.force_min, values.force_max, true));

    const std::string damage = damage_type_display_string(effect.damage_type);
    if (damage.empty()) {
        // Physical prints nothing, and the space beside the tag goes with it.
        text = replace_wstring(text, "[DMGTYPE] ", std::string());
        text = replace_wstring(text, " [DMGTYPE]", std::string());
    } else {
        text = replace_wstring(text, "[DMGTYPE]", damage);
    }

    const int seconds = static_cast<int>(std::ceil(values.duration));
    if (seconds != 0) text = replace_wstring(text, "[DURATION]", value_as_string(seconds));

    // only when the caller asked for colour.
    if (!text.empty() && !good && colour) text = std::string("|cFFff483f") + text + "|u";
    return text;
}

Affix parse_affix(const tl::formats::AdmFile& file, const EffectTypeTable& table,
                  const UnitTypeHierarchy* unit_types) {
    // `CAffix::parseAffixGroup`, in its order
    // of reads.
    Affix affix;
    affix.name = file.string_or("NAME", std::string());
    affix.min_spawn_range = static_cast<int>(file.integer_or("MIN_SPAWN_RANGE", 0));
    affix.max_spawn_range = static_cast<int>(file.integer_or("MAX_SPAWN_RANGE", 999999));
    affix.weight = static_cast<int>(file.integer_or("WEIGHT", 1));
    affix.rank = static_cast<int>(file.integer_or("RANK", 1));
    affix.slots_occupy = static_cast<int>(file.integer_or("SLOTS_OCCUPY", 1));
    // `parseAffixGroup` reads `DURATION` with
    // -1.0 as its default and then normalises:
    // -999 becomes -1.0 (never expires) but counts as authored; anything else
    // not above 0 becomes -1.0, unauthored; a positive value or a NaN is kept,
    // authored.
    affix.duration = file.float_or("DURATION", -1.0f);
    if (affix.duration == -999.0f) {
        affix.duration = -1.0f;
        affix.authored_duration = true;
    } else if (affix.duration <= 0.0f) {
        affix.duration = -1.0f;
        affix.authored_duration = false;
    } else {
        affix.authored_duration = true;
    }
    affix.prefix = file.string_or("PREFIX", std::string());
    affix.suffix = file.string_or("SUFFIX", std::string());
    for (const tl::formats::AdmSection& child : file.root.children) {
        const std::string group = upper(child.name);
        if (group == "EFFECT") {
            affix.effects.push_back(parse_effect(child, table));
        } else if (group == "UNITTYPES" && unit_types != nullptr) {
            // Every value of the group is a type name; `getTypeIDByName`
            // returning -1 drops it.
            for (const tl::formats::AdmRecord& record : child.records) {
                const uint32_t id = unit_types->id_of(record.text);
                affix.listed_unit_types.push_back(id);
                if (id != UnitTypeHierarchy::kUnknown) affix.unit_types.push_back(id);
            }
        }
    }
    return affix;
}

namespace {

/// The effect level `setPercent`, `setOwner` and `addEffectsToEffectManager`
/// store: -1 is 0 and so is anything above 1000.
uint32_t effect_level_of(uint32_t level) {
    // -1 is above 1000 too; the console tests it on its own, and so does this.
    // NOLINTNEXTLINE(misc-redundant-expression)
    return level == 0xffffffffu || level > 1000u ? 0u : level;
}

}  // namespace

void roll_effect_value(Effect& effect, const StatBook& graphs, const EffectOwner& owner,
                       RunicRandom& rng) {
    // `CEffect::calculateBaseValue(0)`, its result stored at
    // +0xdc.
    effect.rolled_value = effect_base_value(effect, EffectCalculate::Roll, graphs, owner, &rng);
}

void affix_set_level(Affix& affix, const Affix& source, uint32_t level, const EffectRolls& rolls) {
    // `CAffix::setLevel` then `CAffix::setPercent`.
    //
    // the level (unsigned) held to the spawn range
    // in floats, each step truncated to 64 bits and kept as its low word.
    const float at = static_cast<float>(level);
    const float low = static_cast<float>(affix.min_spawn_range);
    const float lifted = low < at ? at : low;
    const uint32_t first = static_cast<uint32_t>(static_cast<int64_t>(lifted));
    const float first_f = static_cast<float>(first);
    const float high = static_cast<float>(affix.max_spawn_range);
    const float held = first_f < high ? first_f : high;
    const uint32_t effective = static_cast<uint32_t>(static_cast<int64_t>(held));
    affix.level = static_cast<int32_t>(effective);  // +0xa8
    // the place in the range, `(level - min)` as
    // unsigned over `(max - min)` signed, times 100, at most 100, plus 0.5,
    // truncated - and kept as a signed byte; `setPercent` holds that to
    // [0, 100]. A range of nothing is 100 (0/0 and x/0 are not below 100).
    const float place =
        (static_cast<float>(effective - static_cast<uint32_t>(affix.min_spawn_range)) /
         static_cast<float>(affix.max_spawn_range - affix.min_spawn_range)) *
        100.0f;
    const float capped = place < 100.0f ? place : 100.0f;
    int percent = static_cast<int8_t>(static_cast<uint8_t>(tl::formats::ppc_fctiwz(capped + 0.5f)));
    if (percent < 0) percent = 0;
    else if (percent > 100) percent = 100;
    const float share = static_cast<float>(percent) * 0.01f;

    // `setPercent` walks the template's effects (`+0x08`'s `+0xb4`) and skips a
    // place the affix itself has no effect.
    for (size_t i = 0; i < source.effects.size(); ++i) {
        if (i >= affix.effects.size()) continue;
        const Effect& original = source.effects[i];
        Effect& effect = affix.effects[i];
        const float max_value = original.values[1];
        const float min_value = original.values[0];
        bool graphed = false;
        for (int g = 0; g < kEffectValueCount; ++g) {
            if (!original.graphs[g].empty()) graphed = true;
        }
        if (max_value != 0.0f && max_value != min_value && !graphed) {
            // An affix with no curve interpolates its one number along the
            // level's position inside the spawn range, and both ends become it.
            // `setPercent`: the percent times
            // 0.01f, the span fused onto the minimum; the low end first and a
            // roll, then the high end and a roll.
            const float value = std::fma(max_value - min_value, share, min_value);
            effect.values[0] = value;
            rolls.roll(effect);
            effect.values[1] = value;
            rolls.roll(effect);
        }
        // the affix's level on every effect, -1 and
        // above 1000 as 0, `USEOWNERLEVEL` or not (Linux: `CEffect::setAffixOwner`);
        // then `calculateBaseValue(0)`.
        effect.level = effect_level_of(effective);
        rolls.roll(effect);
    }
}

Affix affix_at_level(const Affix& source, uint32_t level) {
    Affix affix = source;
    affix_set_level(affix, source, level);
    return affix;
}

Affix copy_affix(const Affix& source, uint32_t level, const EffectRolls& rolls) {
    // `CAffix::CAffix(CAffix*, unsigned int)`:
    // the level at +0xa8, then `clone`.
    Affix affix = source;
    affix.level = static_cast<int32_t>(level);
    // Each effect `new CEffect(const CEffect*)`: `clone`
    // ends in `setOwner(the source's owner, true)`
    // - a template's effects have none - and the constructor in
    // `calculateBaseValue(0)`: two rolls, on the copied numbers.
    for (Effect& effect : affix.effects) {
        rolls.roll(effect);
        rolls.roll(effect);
    }
    // `clone` ends in `setLevel(+0xa8)` - the level handed in,
    // -1 too - and the constructor sets it again unless it is -1.
    affix_set_level(affix, source, level, rolls);
    if (level != 0xffffffffu) affix_set_level(affix, source, level, rolls);
    return affix;
}

void affix_set_owner(Affix& affix, std::optional<uint32_t> owner_level, const EffectRolls& rolls) {
    // `CAffix::setOwner`: `CEffect::setOwner
    // (owner, true)` on each effect, in order.
    for (Effect& effect : affix.effects) {
        if (effect.use_owner_level && owner_level) effect.level = effect_level_of(*owner_level);
        rolls.roll(effect);
    }
}

void affix_add_effects_rolls(Affix& affix, const EffectRolls& rolls) {
    // `CAffix::addEffectsToEffectManager`,
    // the loop for a manager with no character.
    for (Effect& effect : affix.effects) {
        if (!effect.use_owner_level)
            effect.level = effect_level_of(static_cast<uint32_t>(affix.level));
        rolls.roll(effect);
    }
}

std::string affix_display_stats(const Affix& affix, const EffectDisplayContext& context,
                                bool colour) {
    // `CAffix::getDisplayStats(unsigned int)`,
    // with the level the caller already put
    // on the effects.
    std::string text;
    for (const Effect& effect : affix.effects) {
        append_line(text, effect_display_stats(effect, context, colour));
    }
    return text;
}

void affix_fill_out_stat_bonuses(const Affix& affix, std::array<float, kEffectStatCount>& out,
                                 bool take_max) {
    // `CAffix::fillOutStatBonuses(float (&)[6], bool)`.
    for (const Effect& effect : affix.effects) {
        fill_from_effect(effect, out, take_max);
    }
}

namespace {

/// `gSKILL_EFFECT_AND_AFFIXES_TARGET_NAMES` (five
/// `std::wstring`s 0x1c apart): the index of an upper-cased `TARGET` word, or
/// none for a word not among them. Compared as `wmemcmp` compares, exactly.
std::optional<SkillTargetFilter> target_index_of(const std::string& upper_word) {
    static const char* const kNames[] = {"ENEMY", "SELF", "PET", "EVERYBODY", "FRIEND"};
    std::optional<SkillTargetFilter> found;
    for (int k = 0; k < 5; ++k) {
        if (upper_word == kNames[k]) found = static_cast<SkillTargetFilter>(k);
    }
    return found;
}

/// A value's word as `CDataValue +0x08` holds it, which the constructor reads
/// straight (`lwz`/`lfs`) whatever the value's type: an integer as its bits, a
/// float as its own.
// PORT: from the port's parsed record.
uint32_t value_word(const tl::formats::AdmRecord& r) {
    if (r.type == tl::formats::AdmType::Float) {
        uint32_t bits;
        std::memcpy(&bits, &r.number, sizeof bits);
        return bits;
    }
    return r.integer;
}

/// `CHierarchy::getTypeIDByName` of a `TARGETTYPE` when one is authored
/// (an empty string is skipped, as the constructor's `wmemcmp` against
/// `EMPTY_WSTRING` skips it), else 0.
int32_t target_type_of(const tl::formats::AdmSection& group, const UnitTypeHierarchy* unit_types) {
    const std::string name = group.string_or("TARGETTYPE", std::string());
    if (name.empty() || unit_types == nullptr) return 0;
    return static_cast<int32_t>(unit_types->id_of(name));
}

// PORT: asset parsing, whether a value is text.
bool is_string_value(const tl::formats::AdmRecord& r) {
    return r.type == tl::formats::AdmType::String || r.type == tl::formats::AdmType::Translate;
}

// PORT: asset parsing, a child group by name.
const tl::formats::AdmSection* first_child(const tl::formats::AdmSection& section,
                                           const char* name) {
    for (const tl::formats::AdmSection& child : section.children) {
        if (upper(child.name) == name) return &child;
    }
    return nullptr;
}

}  // namespace

SkillTargetFilter default_effect_target(int32_t target_type, int32_t target_alignment) {
    // `CSkillEffectAndAffixes::CSkillEffectAndAffixes`: with no
    // `TARGET`, the skill's target type picks a name out of the same table
    // - and every type it does not list leaves the
    // name empty, which `getStringIndex` answers with 0, `ENEMY`.
    switch (target_type) {
        case 2:  // `TARGET`: `FRIEND` for a skill aimed at `GOOD`, else `ENEMY`
            return target_alignment == 1 ? SkillTargetFilter::Friend : SkillTargetFilter::Enemy;
        case 3:   // `SELF`
        case 10:  // `SELFANDPETS`
            return SkillTargetFilter::Self;
        case 4:  // `EVERYBODY`
            return SkillTargetFilter::Everybody;
        case 9:  // `PETS`
            return SkillTargetFilter::Pet;
        default: return SkillTargetFilter::Enemy;
    }
}

void set_duration_override(SkillEffectAndAffixes& block, float seconds) {
    // `CSkillEffectAndAffixes::setDurationOverride`.
    // The specs' `+0x30`.
    for (SkillEffectAndAffixes::AffixSpec& spec : block.affix_specs) spec.duration = seconds;
    // the group counter `r7` picks the effect (`effects + r7 * 4`,
    // the first when it is past the end); the inner counter only repeats it.
    for (size_t group = 0; group < block.effect_groups.size(); ++group) {
        std::vector<Effect>& effects = block.effect_groups[group].effects;
        for (size_t pass = 0; pass < effects.size(); ++pass) {
            Effect& effect = group < effects.size() ? effects[group] : effects[0];
            // `+0x21`, an authored `DURATION`, keeps its own.
            if (effect.authored_duration) continue;
            effect.duration = seconds;
        }
    }
}

bool has_skill_effect_and_affixes(const tl::formats::AdmSection& section) {
    return first_child(section, "EFFECT") != nullptr ||
           first_child(section, "EFFECTS") != nullptr ||
           first_child(section, "AFFIXES") != nullptr ||
           first_child(section, "AFFIXESREMOVE") != nullptr;
}

SkillEffectAndAffixes parse_skill_effect_and_affixes(const tl::formats::AdmSection& section,
                                                     const EffectTypeTable& table,
                                                     SkillTargetFilter unauthored,
                                                     const UnitTypeHierarchy* unit_types) {
    // `CSkillEffectAndAffixes::CSkillEffectAndAffixes`.
    //
    SkillEffectAndAffixes block;

    // the section's own `TARGET`, upper-cased; an empty one is the
    // skill's default (`default_effect_target`, the jump table),
    // and the word's index in the names, 0 for one not there
    // (`STRINGS::getStringIndex(word, names, 5, 0)`).
    const std::string root_word = upper(section.string_or("TARGET", std::string()));
    SkillTargetFilter group_target = unauthored;
    if (!root_word.empty())
        group_target = target_index_of(root_word).value_or(SkillTargetFilter::Enemy);
    int32_t group_type = 0;

    // with an `[EFFECTS]` child, the effects are its own, and it
    // gives the block its `ADDITIONALDESCRIPTION`, its `TARGETTYPE`, and its
    // `TARGET` - a word it does not author, or one not among the names, keeps
    // the section's.
    const tl::formats::AdmSection* container = first_child(section, "EFFECTS");
    if (container != nullptr) {
        block.additional_description =
            container->string_or("ADDITIONALDESCRIPTION", block.additional_description);
        group_type = target_type_of(*container, unit_types);
        const std::string word = upper(container->string_or("TARGET", std::string()));
        if (!word.empty()) group_target = target_index_of(word).value_or(group_target);
    } else {
        container = &section;
    }
    // every `[EFFECT]` there, one `CEffect` each (`CEffect::
    // CEffect(group, 0)`), in one `CSkillEffectData` - made only when there is
    // at least one.
    SkillEffectAndAffixes::EffectGroup group;
    group.target_type = group_type;
    group.target = group_target;
    for (const tl::formats::AdmSection& child : container->children) {
        if (upper(child.name) != "EFFECT") continue;
        group.effects.push_back(parse_effect(child, table));
    }
    if (!group.effects.empty()) block.effect_groups.push_back(std::move(group));

    // every `[AFFIXES]` child in order. Each starts from level 1,
    // duration -1, type 0 and target 0 (`ENEMY`) - the target is not the
    // skill's default here - and reads `AFFIXLEVEL` and `DURATION` as their
    // value words, `ADDITIONALDESCRIPTION` over the block's, `TARGETTYPE` and
    // `TARGET`; then each string value keyed exactly `AFFIX` is one spec.
    for (const tl::formats::AdmSection& child : section.children) {
        if (upper(child.name) != "AFFIXES") continue;
        uint32_t level = 1;
        if (const tl::formats::AdmRecord* r = child.find("AFFIXLEVEL")) level = value_word(*r);
        float duration = -1.0f;
        if (const tl::formats::AdmRecord* r = child.find("DURATION")) {
            const uint32_t word = value_word(*r);
            std::memcpy(&duration, &word, sizeof duration);
        }
        block.additional_description =
            child.string_or("ADDITIONALDESCRIPTION", block.additional_description);
        const int32_t type = target_type_of(child, unit_types);
        SkillTargetFilter target = SkillTargetFilter::Enemy;
        const std::string word = upper(child.string_or("TARGET", std::string()));
        if (!word.empty()) target = target_index_of(word).value_or(SkillTargetFilter::Enemy);
        for (const tl::formats::AdmRecord& record : child.records) {
            if (!is_string_value(record) || record.key != "AFFIX") continue;
            SkillEffectAndAffixes::AffixSpec spec;
            spec.name = record.text;
            spec.level = level;
            spec.target_type = type;
            spec.target = target;
            spec.duration = duration;
            block.affix_specs.push_back(std::move(spec));
        }
    }

    // the first `[AFFIXESREMOVE]` and the first `[EFFECTSREMOVE]`:
    // each its `ADDITIONALDESCRIPTION` over the block's, then every string
    // value it holds, whatever the key.
    if (const tl::formats::AdmSection* removes = first_child(section, "AFFIXESREMOVE")) {
        block.additional_description =
            removes->string_or("ADDITIONALDESCRIPTION", block.additional_description);
        for (const tl::formats::AdmRecord& record : removes->records) {
            if (is_string_value(record)) block.remove_affix_names.push_back(record.text);
        }
    }
    if (const tl::formats::AdmSection* removes = first_child(section, "EFFECTSREMOVE")) {
        block.additional_description =
            removes->string_or("ADDITIONALDESCRIPTION", block.additional_description);
        for (const tl::formats::AdmRecord& record : removes->records) {
            if (is_string_value(record)) block.remove_effect_names.push_back(record.text);
        }
    }
    // a written-out `\n` in the description is a line break.
    block.additional_description = replace_wstring(block.additional_description, "\\n", "\n");
    return block;
}

std::string skill_effect_and_affixes_display_stats(const SkillEffectAndAffixes& block,
                                                   const EffectDisplayContext& context) {
    // `CSkillEffectAndAffixes::getDisplayStats()`.
    // The affix list is built here
    // (`CSkillEffectAndAffixes::createAffixesForDisplay`),
    // which is why nothing can read the bonuses
    // without asking for the text first.
    std::string text = block.additional_description;
    for (const SkillEffectAndAffixes::AffixSpec& spec : block.affix_specs) {
        if (!context.affix) continue;
        const Affix* source = context.affix(spec.name, spec.level);
        if (source == nullptr) continue;
        const Affix affix = affix_at_level(*source, spec.level);
        // no colour for a skill's lines.
        append_line(text, affix_display_stats(affix, context, false));
    }
    std::string effects;
    for (const SkillEffectAndAffixes::EffectGroup& group : block.effect_groups) {
        std::string group_text;
        for (const Effect& effect : group.effects) {
            append_line(group_text, effect_display_stats(effect, context, false));
        }
        append_line(effects, group_text);
    }
    append_line(text, effects);
    return text;
}

void skill_effect_and_affixes_fill_out_stat_bonuses(const SkillEffectAndAffixes& block,
                                                    const EffectDisplayContext& context,
                                                    std::array<float, kEffectStatCount>& out,
                                                    bool take_max) {
    // `CSkillEffectAndAffixes::fillOutStatBonuses(float (&)[6], bool)`.
    for (const SkillEffectAndAffixes::AffixSpec& spec : block.affix_specs) {
        if (!context.affix) continue;
        const Affix* source = context.affix(spec.name, spec.level);
        if (source == nullptr) continue;
        const Affix affix = affix_at_level(*source, spec.level);
        affix_fill_out_stat_bonuses(affix, out, take_max);
    }
    for (const SkillEffectAndAffixes::EffectGroup& group : block.effect_groups) {
        for (const Effect& effect : group.effects) {
            fill_from_effect(effect, out, take_max);
        }
    }
}

}  // namespace tl::core
