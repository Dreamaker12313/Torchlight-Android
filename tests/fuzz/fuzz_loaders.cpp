// Structured fuzz targets: the loaders that turn a parsed game file into the engine's data,
// and the level script, fed a random `AdmFile` (names from the loaders' own string literals)
// or `CmpScene`. `tl_fuzz <iterations> <seed> <target>` replays a failure.
#include "fuzz.h"

#include "core/animation_manifest.h"
#include "core/breakable.h"
#include "core/combine.h"
#include "core/dungeon.h"
#include "core/effects.h"
#include "core/item_label.h"
#include "core/item_roll.h"
#include "core/item_sets.h"
#include "core/merchant.h"
#include "core/quest_book.h"
#include "core/random.h"
#include "core/script.h"
#include "core/skill_cast.h"
#include "core/skills.h"
#include "core/spawn_class.h"
#include "core/stats.h"
#include "core/unit_themes.h"
#include "core/unit_types.h"
#include "core/units.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef TL_FUZZ_SOURCE_DIR
#define TL_FUZZ_SOURCE_DIR "."
#endif

namespace tl::fuzz {
namespace {

using namespace tl::core;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;
using tl::formats::CmpField;
using tl::formats::CmpNode;
using tl::formats::CmpScene;

// ---------------------------------------------------------------------------
// Dictionaries: the string literals of the code under test
// ---------------------------------------------------------------------------

/// Every string literal of 1..48 printable characters in `src/core/<file>`.
std::vector<std::string> literals_of(const std::vector<const char*>& files) {
    std::vector<std::string> words;
    for (const char* file : files) {
        std::ifstream in(std::string(TL_FUZZ_SOURCE_DIR) + "/src/core/" + file);
        std::stringstream text;
        text << in.rdbuf();
        const std::string s = text.str();
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] != '"' || (i > 0 && s[i - 1] == '\'')) continue;
            size_t end = i + 1;
            while (end < s.size() && s[end] != '"' && s[end] != '\n') {
                if (s[end] == '\\') ++end;
                ++end;
            }
            if (end >= s.size() || s[end] != '"') continue;
            const std::string word = s.substr(i + 1, end - i - 1);
            i = end;
            if (word.empty() || word.size() > 48) continue;
            bool plain = true;
            for (char c : word) plain = plain && c >= 0x20 && c < 0x7f && c != '\\' && c != '%';
            if (plain) words.push_back(word);
        }
    }
    if (words.empty()) {
        // No source tree beside the binary: a few of the names every file uses.
        words = {"NAME", "LEVEL", "UNITTYPE", "FILE",   "TYPE",
                 "MIN",  "MAX",   "GUID",     "EFFECT", "AFFIX"};
    }
    return words;
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

constexpr uint32_t kInterestingInts[] = {
    0u,   1u,    2u,     3u,          7u,          10u,         100u,       255u,
    256u, 1000u, 65535u, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu};

float interesting_float(Rng& rng) {
    static const float values[] = {0.0f,
                                   -0.0f,
                                   1.0f,
                                   -1.0f,
                                   0.5f,
                                   2.0f,
                                   100.0f,
                                   -100.0f,
                                   1e-40f,
                                   1e30f,
                                   -1e30f,
                                   std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::infinity(),
                                   -std::numeric_limits<float>::infinity(),
                                   std::numeric_limits<float>::quiet_NaN()};
    if (rng.one_in(3)) return static_cast<float>(static_cast<int>(rng.below(41)) - 20);
    return values[rng.below(sizeof(values) / sizeof(values[0]))];
}

uint32_t interesting_int(Rng& rng) {
    if (rng.one_in(2)) return static_cast<uint32_t>(rng.below(12));
    return kInterestingInts[rng.below(sizeof(kInterestingInts) / sizeof(kInterestingInts[0]))];
}

std::string word(Rng& rng, const std::vector<std::string>& dict) {
    if (!rng.one_in(8)) return dict[rng.below(dict.size())];
    static const char* const junk[] = {"",      " ",   "-1",         "0",
                                       "1e9",   "nan", "media/../x", "|cFFFFFFFF|u",
                                       "a:b:c", ",,,", "\xff\xfe",   "LEVEL999999"};
    if (rng.one_in(10)) return std::string(200 + rng.below(200), 'A');
    return junk[rng.below(sizeof(junk) / sizeof(junk[0]))];
}

// ---------------------------------------------------------------------------
// ADM trees
// ---------------------------------------------------------------------------

AdmRecord random_record(Rng& rng, const std::vector<std::string>& dict) {
    static const AdmType types[] = {AdmType::Integer,  AdmType::Float, AdmType::Unsigned,
                                    AdmType::String,   AdmType::Bool,  AdmType::Guid,
                                    AdmType::Translate};
    AdmRecord r;
    r.key = word(rng, dict);
    r.type = types[rng.below(7)];
    switch (r.type) {
        case AdmType::Float: r.number = interesting_float(rng); break;
        case AdmType::String:
        case AdmType::Translate: r.text = word(rng, dict); break;
        case AdmType::Guid: r.guid = rng.one_in(2) ? -1 : static_cast<int64_t>(rng.next()); break;
        default: r.integer = interesting_int(rng); break;
    }
    return r;
}

AdmSection random_section(Rng& rng, const std::vector<std::string>& dict, int depth) {
    AdmSection s;
    s.name = word(rng, dict);
    const size_t records = rng.below(10);
    for (size_t i = 0; i < records; ++i) s.records.push_back(random_record(rng, dict));
    if (depth < 4) {
        const size_t children = rng.below(depth == 0 ? 6 : 3);
        for (size_t i = 0; i < children; ++i)
            s.children.push_back(random_section(rng, dict, depth + 1));
    }
    return s;
}

AdmFile random_file(Rng& rng, const std::vector<std::string>& dict, const char* root) {
    AdmFile f;
    f.ok = true;
    f.root = random_section(rng, dict, 0);
    if (!rng.one_in(4)) f.root.name = root;
    return f;
}

/// A small `UNITTYPES` hierarchy: a few ids with ancestor chains, some
/// pointing at ids that do not exist.
const UnitTypeHierarchy& unit_types() {
    static const UnitTypeHierarchy h = [] {
        AdmSection root;
        root.name = "HIERACHY";
        AdmSection types;
        types.name = "UNITTYPES";
        const uint32_t ids[] = {0, 9, 10, 25, 27, 28, 35, 41, 60};
        for (uint32_t id : ids) {
            AdmSection t;
            t.name = "T" + std::to_string(id);
            AdmRecord r;
            r.key = "ID";
            r.integer = id;
            t.records.push_back(r);
            AdmRecord c;
            c.key = "child";
            c.integer = id == 0 ? 999 : 0;
            t.records.push_back(c);
            types.children.push_back(t);
        }
        root.children.push_back(types);
        UnitTypeHierarchy out;
        out.parse(root);
        return out;
    }();
    return h;
}

// ---------------------------------------------------------------------------
// CMP scenes
// ---------------------------------------------------------------------------

CmpField text_field(const std::string& text) {
    const size_t dwords = ((text.size() + 1) * 2 + 3) / 4;
    std::vector<uint8_t> bytes(dwords * 4, 0);
    for (size_t i = 0; i < text.size(); ++i) bytes[2 * i] = static_cast<uint8_t>(text[i]);
    CmpField f;
    f.words.resize(dwords);
    std::memcpy(f.words.data(), bytes.data(), bytes.size());
    return f;
}

/// A result's bits for the digest; every NaN counts as one (x86 and AArch64
/// make NaNs of opposite sign).
size_t digest_bits(float v) {
    if (v != v) return 0x7fc00000u;
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    return bits;
}

uint32_t float_word(float v) {
    uint32_t w = 0;
    std::memcpy(&w, &v, 4);
    return w;
}

CmpField random_field(Rng& rng, const std::vector<std::string>& dict, int64_t max_id) {
    CmpField f;
    switch (rng.below(6)) {
        case 0: return text_field(word(rng, dict));
        case 1: f.words = {interesting_int(rng)}; break;
        case 2:
            f.words = {static_cast<uint32_t>(rng.below(static_cast<size_t>(max_id) + 2))};
            break;
        case 3: f.words = {float_word(interesting_float(rng))}; break;
        case 4:
            for (int i = 0; i < 3; ++i) f.words.push_back(float_word(interesting_float(rng)));
            break;
        default:
            for (size_t i = 0, n = rng.below(10); i < n; ++i)
                f.words.push_back(interesting_int(rng));
            break;
    }
    return f;
}

CmpScene random_scene(Rng& rng, const std::vector<std::string>& dict) {
    namespace t = tl::formats::cmp_type;
    static const uint32_t types[] = {t::kGroup,
                                     t::kMonster,
                                     t::kLayoutLink,
                                     t::kModel,
                                     t::kRoomPiece,
                                     t::kLight,
                                     t::kSound,
                                     t::kParticle,
                                     t::kMarker,
                                     t::kAnimationController,
                                     t::kDamageShape,
                                     t::kMissile,
                                     t::kUnitTrigger,
                                     t::kUnitSpawner,
                                     t::kTimelineLink,
                                     t::kTimeline,
                                     t::kLogicGroup,
                                     t::kPlayerSphereTrigger,
                                     t::kPlayerBoxTrigger,
                                     t::kCounter,
                                     t::kWarper,
                                     t::kTimer,
                                     t::kCameraController,
                                     t::kGameStateController,
                                     t::kQuestController,
                                     t::kPathing,
                                     t::kSkipCutscene,
                                     41,
                                     43,
                                     46,
                                     52,
                                     99};
    CmpScene scene;
    scene.ok = true;
    const int64_t count = 1 + static_cast<int64_t>(rng.below(14));
    for (int64_t id = 1; id <= count; ++id) {
        CmpNode node;
        node.type = types[rng.below(sizeof(types) / sizeof(types[0]))];
        node.id = rng.one_in(20) ? static_cast<int64_t>(rng.below(4)) : id;
        node.parent_id =
            rng.one_in(3) ? static_cast<int64_t>(1 + rng.below(static_cast<size_t>(count))) : -1;
        node.name =
            rng.one_in(3) ? std::string(rng.one_in(2) ? "Grunt" : "Ratlin") : word(rng, dict);
        if (rng.one_in(2)) {
            node.vec3s.push_back(
                {interesting_float(rng), interesting_float(rng), interesting_float(rng)});
        }
        for (size_t i = 0, n = rng.below(30); i < n; ++i) {
            node.fields.push_back(rng.one_in(6) ? text_field(rng.one_in(2) ? "Grunt" : "Ratlin")
                                                : random_field(rng, dict, count));
        }
        scene.nodes.push_back(std::move(node));
    }
    for (size_t g = 0, groups = rng.below(3); g < groups; ++g) {
        CmpNode holder;
        holder.type = t::kLogicGroup;
        holder.id = 1000 + static_cast<int64_t>(g);
        holder.name = "Logic";
        holder.fields.push_back(CmpField{{0u}});
        scene.nodes.push_back(holder);
        tl::formats::CmpLogicGroup wiring;
        wiring.node_id = holder.id;
        const size_t objects = 1 + rng.below(8);
        for (size_t o = 0; o < objects; ++o) {
            tl::formats::CmpLogicObject object;
            object.local = static_cast<uint32_t>(o);
            object.object_id = static_cast<int64_t>(1 + rng.below(static_cast<size_t>(count) + 1));
            for (size_t l = 0, links = rng.below(4); l < links; ++l) {
                tl::formats::CmpLogicLink link;
                link.to = static_cast<uint32_t>(rng.below(objects + 1));
                link.output = word(rng, dict);
                link.input = word(rng, dict);
                object.links.push_back(link);
            }
            wiring.objects.push_back(std::move(object));
        }
        scene.logic_groups.push_back(std::move(wiring));
    }
    for (const CmpNode& node : scene.nodes) {
        if (node.type != t::kTimeline || !rng.one_in(2)) continue;
        tl::formats::CmpTimeline timeline;
        timeline.node_id = node.id;
        for (size_t i = 0, targets = rng.below(4); i < targets; ++i) {
            tl::formats::CmpTimelineTarget target;
            target.object_id = static_cast<int64_t>(1 + rng.below(static_cast<size_t>(count)));
            for (size_t j = 0, tracks = rng.below(4); j < tracks; ++j) {
                tl::formats::CmpTimelineTrack track;
                track.tag = static_cast<uint32_t>(rng.below(32));
                track.is_event = rng.one_in(4);
                for (size_t k = 0, keys = rng.below(5); k < keys; ++k) {
                    tl::formats::CmpTimelineKey key;
                    key.time =
                        rng.one_in(4) ? interesting_float(rng) : static_cast<float>(k) * 0.25f;
                    key.interpolation = static_cast<uint32_t>(rng.below(9));
                    for (size_t w = 0, words = rng.below(4); w < words; ++w) {
                        key.words.push_back(rng.one_in(2) ? float_word(interesting_float(rng))
                                                          : interesting_int(rng));
                    }
                    track.keys.push_back(std::move(key));
                }
                target.tracks.push_back(std::move(track));
            }
            timeline.targets.push_back(std::move(target));
        }
        scene.timelines.push_back(std::move(timeline));
    }
    return scene;
}

/// `TL_FUZZ_DUMP=1`: every scene the script target builds, printed before it
/// runs, so the last one printed before a crash is the one that crashed it.
bool dumping() {
    static const bool on = std::getenv("TL_FUZZ_DUMP") != nullptr;
    return on;
}

void dump(const CmpScene& scene) {
    std::fprintf(stderr, "scene:\n");
    for (const CmpNode& n : scene.nodes) {
        std::fprintf(stderr, "  node type %u id %lld parent %lld '%s'\n", n.type,
                     static_cast<long long>(n.id), static_cast<long long>(n.parent_id),
                     n.name.c_str());
        for (size_t i = 0; i < n.fields.size(); ++i) {
            std::fprintf(stderr, "    field %zu:", i);
            for (uint32_t w : n.fields[i].words) {
                float f = 0.0f;
                std::memcpy(&f, &w, 4);
                std::fprintf(stderr, " %08x(%g)", w, static_cast<double>(f));
            }
            std::fprintf(stderr, "\n");
        }
    }
    for (const auto& g : scene.logic_groups) {
        for (const auto& o : g.objects) {
            for (const auto& l : o.links) {
                std::fprintf(stderr, "  wire %lld: %u(%lld) '%s' -> %u '%s'\n",
                             static_cast<long long>(g.node_id), o.local,
                             static_cast<long long>(o.object_id), l.output.c_str(), l.to,
                             l.input.c_str());
            }
        }
    }
    for (const auto& t : scene.timelines) {
        for (const auto& target : t.targets) {
            for (const auto& track : target.tracks) {
                std::fprintf(stderr,
                             "  timeline %lld -> %lld tag %u%s:", static_cast<long long>(t.node_id),
                             static_cast<long long>(target.object_id), track.tag,
                             track.is_event ? " event" : "");
                for (const auto& k : track.keys)
                    std::fprintf(stderr, " %g", static_cast<double>(k.time));
                std::fprintf(stderr, "\n");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Targets
// ---------------------------------------------------------------------------

size_t skill_step(Rng& rng) {
    static const std::vector<std::string> dict =
        literals_of({"skills.cpp", "skill_cast.cpp", "skill_stats.cpp"});
    const AdmFile file = random_file(rng, dict, "SKILL");
    const SkillDef def = parse_skill_def(file);
    size_t digest = def.levels.size() + def.name.size();
    KnownSkill known;
    known.def = &def;
    known.invested = static_cast<uint32_t>(rng.below(30));
    known.effective = rng.one_in(4) ? 0u : static_cast<uint32_t>(rng.below(30));
    digest += known.level_count() + known.max_rank();
    for (uint32_t level = 0; level < 32; level += 1 + static_cast<uint32_t>(rng.below(5))) {
        digest += known.property(level) != nullptr;
    }
    file.root.visit([&](const AdmSection& s) {
        digest += static_cast<size_t>(read_skill_event(s).weapon_damage != 0.0f);
    });
    digest += parse_class_skills(file).size() + parse_skill_tab_names(file)[0].size();
    return digest;
}

size_t effects_step(Rng& rng) {
    static const std::vector<std::string> dict =
        literals_of({"effects.cpp", "affix_book.cpp", "effect_manager.cpp"});
    EffectTypeTable table;
    table.load(random_file(rng, dict, "EFFECTS"));
    const AdmFile file = random_file(rng, dict, "AFFIX");
    size_t digest = table.size();
    file.root.visit([&](const AdmSection& s) { digest += parse_effect(s, table).values.size(); });
    digest += parse_affix(file, table, &unit_types()).effects.size();
    return digest;
}

size_t dungeon_step(Rng& rng) {
    static const std::vector<std::string> dict = literals_of({"dungeon.cpp"});
    const DungeonDef def = parse_dungeon_def(random_file(rng, dict, "DUNGEON"));
    LevelTemplateFiles files;
    files.exists = [](const std::string& layout) { return layout.size() % 2 == 0; };
    files.list_layouts = [](const std::string& folder) {
        return std::vector<std::string>{folder + "/A", folder + "/B/C"};
    };
    LevelTemplate level;
    RunicRandom draws(static_cast<uint32_t>(rng.next()));
    const bool loaded =
        level.load("media/layouts/cave/rules.dat", random_file(rng, dict, "LEVEL"), files, draws);
    return def.name.size() + loaded;
}

size_t quest_step(Rng& rng) {
    static const std::vector<std::string> dict = literals_of({"quest_book.cpp", "dialog.cpp"});
    const AdmFile file = random_file(rng, dict, "QUEST");
    QuestDefinition quest;
    size_t digest = parse_quest_definition(file.root, quest) ? 1 : 0;
    file.root.visit([&](const AdmSection& s) { digest += parse_dialog_line(s).texts.size(); });
    return digest + quest.name.size();
}

size_t spawn_step(Rng& rng) {
    static const std::vector<std::string> dict = literals_of({"spawn_class.cpp"});
    const AdmFile file = random_file(rng, dict, "SPAWNCLASS");
    std::vector<SpawnUnitData> pool;
    file.root.visit([&](const AdmSection& s) {
        if (pool.size() < 4) pool.push_back(SpawnUnitData::from_adm(s));
    });
    SpawnContext context;
    context.unit_types = &unit_types();
    context.get_spawn_class = [](const std::string&) -> const SpawnClass* { return nullptr; };
    context.get_units_by_type = [&pool](uint32_t) { return pool; };
    context.get_unit_by_name = [&pool](const std::string&) -> const SpawnUnitData* {
        return pool.empty() ? nullptr : &pool.front();
    };
    const SpawnClass spawn = parse_spawn_class(file.root, context);
    RunicRandom draws(static_cast<uint32_t>(rng.next()));
    SpawnCharacterContext character;
    character.level = static_cast<int32_t>(rng.below(60));
    size_t digest = spawn.entries().size();
    for (const SpawnClassEntry& entry : spawn.entries()) {
        if (!entry.data) continue;
        std::vector<std::string> units;
        std::vector<bool> flags;
        const int32_t level = static_cast<int32_t>(rng.below(60)) - 5;
        const int32_t bonus = static_cast<int32_t>(rng.below(5));
        const int32_t rarity = static_cast<int32_t>(rng.below(5)) - 1;
        const int32_t magic_find = static_cast<int32_t>(rng.below(300));
        const uint32_t roll_flags = static_cast<uint32_t>(rng.below(8));
        entry.data->roll_spawn_class_data(units, &flags, &character, &character, level, bonus,
                                          rarity, magic_find, roll_flags, draws, context);
        digest += units.size();
    }
    return digest;
}

size_t items_step(Rng& rng) {
    static const std::vector<std::string> dict =
        literals_of({"item_roll.cpp", "item_label.cpp", "breakable.cpp", "merchant.cpp",
                     "combine.cpp", "item_sets.cpp", "units.cpp"});
    const AdmFile file = random_file(rng, dict, "UNIT");
    size_t digest = 0;
    const ItemRollGlobals globals = read_item_roll_globals(file);
    const ItemLabelColours colours = read_item_label_colours(file);
    ItemStats item;
    read_item_roll_data(item, file.root);
    (void)globals;
    (void)colours;
    digest += read_breakable(file).mesh.size();
    digest += static_cast<size_t>(read_merchant_data(file.root).default_tab);
    digest += load_recipe(file.root, unit_types()).ingredients.size();
    const AffixBook affixes;
    digest += load_item_set(file.root, affixes).name.size();
    return digest;
}

size_t graphs_step(Rng& rng) {
    static const std::vector<std::string> dict =
        literals_of({"stats.cpp", "unit_types.cpp", "unit_themes.cpp", "animation_manifest.cpp"});
    const AdmFile file = random_file(rng, dict, "GRAPH");
    size_t digest = 0;
    file.root.visit([&](const AdmSection& s) {
        const StatGraph graph = StatGraph::from(s);
        for (int i = 0; i < 4; ++i) {
            digest += digest_bits(graph.at(interesting_float(rng)));
        }
        digest += UnitThemeParticle::load(s).unit_type.size();
    });
    UnitTypeHierarchy hierarchy;
    hierarchy.parse(file.root);
    // Every draw its own statement: arguments are evaluated in an unspecified
    // order, and x86 and AArch64 GCC differ, which the digests would show.
    const uint32_t type = interesting_int(rng);
    const uint32_t want = interesting_int(rng);
    digest += hierarchy.isa(type, want);
    const AnimationManifest manifest = AnimationManifest::from(file);
    const std::string animation = word(rng, dict);
    const std::string tag = word(rng, dict);
    return digest + manifest.size() + digest_bits(manifest.seconds_of(animation, tag, -1.0f));
}

size_t script_step(Rng& rng) {
    static const std::vector<std::string> dict = literals_of(
        {"script.cpp", "script_units.cpp", "script_state.cpp", "timeline.cpp", "logic_events.cpp"});
    static const UnitBook units = [] {
        UnitBook book;
        UnitStats grunt;
        book.add("Grunt", grunt);
        UnitStats ratlin;
        ratlin.alignment = UnitStats::Alignment::Friendly;
        ratlin.movement.running_speed = 0.0f;
        book.add("Ratlin", ratlin);
        return book;
    }();
    const CmpScene scene = random_scene(rng, dict);
    if (dumping()) dump(scene);
    LevelScript script;
    script.set_units(&units);
    script.set_unit_types(&unit_types());
    QuestState quests;
    std::vector<ScriptEvent> events;
    script.build(scene, {});
    script.level_activated(quests, events);
    size_t digest = events.size();
    const float step = rng.one_in(8) ? 0.5f : 1.0f / 30.0f;
    for (int frame = 0; frame < 40; ++frame) {
        events.clear();
        const tl::core::Vec3 player{static_cast<float>(frame) * 0.2f, 0.0f, 0.0f};
        script.update(step, player, quests, events);
        digest += events.size();
    }
    std::vector<tl::formats::LogicNodeState> states;
    script.store_logic_states(0, 0, states);
    LevelScript again;
    again.set_units(&units);
    again.build(scene, {});
    again.restore_logic_states(0, 0, states, quests, events);
    return digest + states.size();
}

}  // namespace

std::vector<StructuredTarget> structured_targets() {
    return {
        {"skill", skill_step},   {"effects", effects_step}, {"dungeon", dungeon_step},
        {"quest", quest_step},   {"spawn", spawn_step},     {"items", items_step},
        {"graphs", graphs_step}, {"script", script_step},
    };
}

}  // namespace tl::fuzz
