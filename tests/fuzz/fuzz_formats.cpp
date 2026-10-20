// A mutation fuzzer for the byte-in, struct-out readers: a reader may reject anything, but
// must not crash, read out of bounds or hang (build with -fsanitize=address,undefined).
//
//   tl_fuzz [iterations] [seed] [target]   fuzz (every target by default)
//   tl_fuzz --run <target> <file>          replay one saved input
//
// Under a sanitizer, the failing input is written to `fuzz-crash-<target>.bin`.
#include "fuzz.h"

#include "core/log.h"
#include "core/text.h"
#include "formats/adm.h"
#include "formats/byte_reader.h"
#include "formats/cegui.h"
#include "formats/cmp.h"
#include "formats/ogg.h"
#include "formats/save_file.h"
#include "formats/uilayout.h"
#include "formats/wav.h"

#include <algorithm>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#if defined(__SANITIZE_ADDRESS__)
#define TL_FUZZ_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define TL_FUZZ_SANITIZER 1
#endif
#endif
#if defined(TL_FUZZ_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

namespace {

using Bytes = std::vector<uint8_t>;

// ---------------------------------------------------------------------------
// Seeds
// ---------------------------------------------------------------------------

// One insert rather than four push_backs: GCC 15 misreads the push_backs into a
// freshly sized vector as a write past its end (-Wstringop-overflow).
void put_u32(Bytes& out, uint32_t v) {
    const uint8_t le[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
                           static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)};
    out.insert(out.end(), le, le + 4);
}

void put_f32(Bytes& out, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    put_u32(out, bits);
}

void put_utf16(Bytes& out, const std::string& text) {
    for (char c : text) {
        out.push_back(static_cast<uint8_t>(c));
        out.push_back(0);
    }
}

Bytes text(const std::string& s) { return Bytes(s.begin(), s.end()); }

/// `.adm`: version 1, a string table, a root section with one record of each
/// common type and one child section (tests/adm_test.cpp's shape).
std::vector<Bytes> adm_seeds() {
    Bytes out;
    put_u32(out, 1);
    const char* const strings[] = {"LEVEL", "NAME", "Town", "SCALE", "CHILD", "COUNT"};
    put_u32(out, 6);
    for (uint32_t i = 0; i < 6; ++i) {
        put_u32(out, i + 1);
        put_u32(out, static_cast<uint32_t>(std::strlen(strings[i])));
        put_utf16(out, strings[i]);
    }
    put_u32(out, 1);                                       // root: LEVEL
    put_u32(out, 3);                                       // records
    put_u32(out, 2), put_u32(out, 2), put_u32(out, 3);     // NAME string "Town"
    put_u32(out, 4), put_u32(out, 1), put_f32(out, 1.5f);  // SCALE float
    put_u32(out, 6), put_u32(out, 3), put_u32(out, 7);     // COUNT integer
    put_u32(out, 1);                                       // one child
    put_u32(out, 5);                                       // CHILD
    put_u32(out, 1);
    put_u32(out, 6), put_u32(out, 3), put_u32(out, 9);
    put_u32(out, 0);
    return {out};
}

/// `.cmp`: the camera and sound files from tests/cmp_test.cpp.
std::vector<Bytes> cmp_seeds() {
    Bytes b;
    const uint8_t magic[8] = {'Z', 'E', 'H', 'C', 0x0f, 0x3a, 0x1a, 0xb8};
    b.insert(b.end(), magic, magic + 8);
    put_u32(b, 7);
    put_u32(b, 45);  // node type: camera
    b.insert(b.end(), 4, 0xDE);
    b.insert(b.end(), 4, 0x01);
    b.insert(b.end(), 8, 0xff);  // root
    put_u32(b, 6);
    put_utf16(b, "Camera");
    put_u32(b, 1);
    put_u32(b, 1);
    put_u32(b, 3), put_f32(b, 73.3f), put_f32(b, 2.5f), put_f32(b, 10.8f);
    put_u32(b, 3), put_f32(b, -0.9f), put_f32(b, 0.0f), put_f32(b, 0.3f);
    put_u32(b, 8);  // "Camera Position" + NUL, UTF-16, 8 dwords
    Bytes field(32, 0);
    const char* s = "Camera Position";
    for (size_t i = 0; s[i] != 0; ++i) field[2 * i] = static_cast<uint8_t>(s[i]);
    b.insert(b.end(), field.begin(), field.end());
    put_u32(b, 0);
    b.insert(b.end(), 8, 0xAA);
    put_u32(b, static_cast<uint32_t>(b.size()));

    // The other header shape (a zero at 0x08, then a node count and payload
    // size), holding one Sound node.
    Bytes sound(magic, magic + 8);
    put_u32(sound, 0);
    put_u32(sound, 0);
    put_u32(sound, 0);
    put_u32(sound, 12);  // node type: sound
    sound.insert(sound.end(), 4, 0xDE);
    sound.insert(sound.end(), 4, 0x01);
    sound.insert(sound.end(), 8, 0xff);
    put_u32(sound, 6);
    put_utf16(sound, "Sound2");
    put_u32(sound, 3), put_f32(sound, -1.78f), put_f32(sound, 0.0f), put_f32(sound, 1.79f);
    put_u32(sound, 0);
    sound.insert(sound.end(), 8, 0xAA);
    put_u32(sound, static_cast<uint32_t>(sound.size()));
    return {b, sound};
}

/// A RIFF/WAVE file with `bits`-bit PCM (or 32-bit float with `format` 3).
Bytes make_wav(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits, uint32_t frames) {
    const uint32_t block = channels * (bits / 8u);
    Bytes b = text("RIFF");
    put_u32(b, 36 + frames * block);
    const Bytes wave = text("WAVEfmt ");
    b.insert(b.end(), wave.begin(), wave.end());
    put_u32(b, 16);
    b.push_back(static_cast<uint8_t>(format)), b.push_back(0);
    b.push_back(static_cast<uint8_t>(channels)), b.push_back(0);
    put_u32(b, rate);
    put_u32(b, rate * block);
    b.push_back(static_cast<uint8_t>(block)), b.push_back(0);
    b.push_back(static_cast<uint8_t>(bits)), b.push_back(0);
    const Bytes data = text("data");
    b.insert(b.end(), data.begin(), data.end());
    put_u32(b, frames * block);
    for (uint32_t i = 0; i < frames * block; ++i) b.push_back(static_cast<uint8_t>(i * 37));
    return b;
}

std::vector<Bytes> wav_seeds() {
    return {make_wav(1, 1, 22050, 8, 16), make_wav(1, 2, 44100, 16, 16),
            make_wav(1, 1, 44100, 24, 8), make_wav(3, 2, 48000, 32, 8)};
}

/// Two Ogg pages: an identification header (`\x01vorbis`, rate 44100) and a
/// last page with a granule position.
std::vector<Bytes> ogg_seeds() {
    Bytes b;
    auto page = [&b](uint8_t type, uint64_t granule, const Bytes& body) {
        const Bytes caps = text("OggS");
        b.insert(b.end(), caps.begin(), caps.end());
        b.push_back(0);
        b.push_back(type);
        for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(granule >> (8 * i)));
        put_u32(b, 1);  // serial
        put_u32(b, 0);  // sequence
        put_u32(b, 0);  // checksum
        b.push_back(1);
        b.push_back(static_cast<uint8_t>(body.size()));
        b.insert(b.end(), body.begin(), body.end());
    };
    Bytes id = {0x01, 'v', 'o', 'r', 'b', 'i', 's'};
    put_u32(id, 0);      // version
    id.push_back(2);     // channels
    put_u32(id, 44100);  // rate
    id.resize(30, 0);
    page(0x02, 0, id);
    page(0x04, 441000, Bytes(10, 0x55));
    return {b};
}

std::vector<Bytes> xml_seeds() {
    return {text("<?xml version=\"1.0\" ?>\n"
                 "<GUILayout>\n"
                 "  <Window Type=\"Button\" Name=\"Quit &amp; Save\">\n"
                 "    <Property Name=\"UnifiedPosition\" Value=\"{{0.5,-10},{0,20}}\" />\n"
                 "    <Property Name=\"Text\" Value=\"&#65;&#x42;&lt;\" />\n"
                 "    <!-- a comment -->\n"
                 "    <Window Type=\"StaticImage\" Name=\"Child\"/>\n"
                 "  </Window>\n"
                 "</GUILayout>\n"),
            text("<Imageset Name=\"HUD\" Imagefile=\"hud.png\" NativeHorzRes=\"1280\">\n"
                 "  <Image Name=\"Orb\" XPos=\"1\" YPos=\"2\" Width=\"64\" Height=\"64\" />\n"
                 "</Imageset>\n")};
}

std::vector<Bytes> ui_layout_seeds() {
    const std::string layout =
        "[UI]\r\n\t<INTEGER>VERSION:3\r\n\t<UNSIGNED INT>COUNT:2\r\n\t[OBJECTS]\r\n"
        "\t\t[BASEOBJECT]\r\n\t\t\t[PROPERTIES]\r\n"
        "\t\t\t\t<STRING>DESCRIPTOR:Image\r\n\t\t\t\t<STRING>NAME:HPBubble\r\n"
        "\t\t\t\t<INTEGER64>ID:1\r\n\t\t\t\t<FLOAT>OFFSET X:65\r\n"
        "\t\t\t\t<BOOL>ALWAYS ON TOP:true\r\n\t\t\t[/PROPERTIES]\r\n"
        "\t\t\t[CHILDREN]\r\n\t\t\t\t[BASEOBJECT]\r\n\t\t\t\t\t[PROPERTIES]\r\n"
        "\t\t\t\t\t\t<STRING>WIDGET NAME:Bar\r\n\t\t\t\t\t[/PROPERTIES]\r\n"
        "\t\t\t\t[/BASEOBJECT]\r\n\t\t\t[/CHILDREN]\r\n"
        "\t\t[/BASEOBJECT]\r\n\t[/OBJECTS]\r\n";
    Bytes utf16 = {0xFF, 0xFE};
    put_utf16(utf16, layout);
    return {text(layout), utf16};
}

std::vector<Bytes> character_seeds() {
    using namespace tl::formats;
    CharacterFile file;
    file.class_name = "Destroyer";
    file.player.inventory.resize(2);
    file.player.inventory[0].sockets.resize(1);
    file.pets.resize(1);
    file.levels.resize(1);
    file.levels[0].dungeon = "TOWN";
    file.levels[0].automap = {1.0f, 2.0f};
    file.levels[0].items.resize(1);
    file.levels[0].logic.resize(1);
    file.levels[0].chunks = {"a", "b"};
    file.trackers.resize(1);
    file.trackers[0].ints = {1, 2, 3};
    file.quests.completed = {"Q1"};
    file.quests.active.resize(1);
    file.quests.active[0].dialogs[0].push_back({1, 0});
    file.quests.active[0].objectives[0].resize(1);
    file.quests.active[0].objectives[0][0].resize(1);
    file.quests.active[0].objectives[0][0][0].children.resize(1);
    return {write_character_file(CharacterFile{}), write_character_file(file)};
}

std::vector<Bytes> stash_seeds() {
    return {tl::formats::write_shared_stash_file({}),
            tl::formats::write_shared_stash_file(
                {tl::formats::ItemSaveState{}, tl::formats::ItemSaveState{}})};
}

std::vector<Bytes> markup_seeds() {
    return {text("The |cFFFFBA00Gleaming Ember|u glows"), text("|c12345678open |u|u||c|cZZ"),
            text("plain")};
}

// ---------------------------------------------------------------------------
// Targets
// ---------------------------------------------------------------------------

/// Kept so the calls cannot be optimised away.
volatile size_t g_sink = 0;

/// A float's exact bits, to match across CPUs; every NaN is one value (x86 and AArch64 differ
/// in its sign).
size_t bits_of(float v) {
    if (v != v) return 0x7fc00000u;
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    return bits;
}

struct Target {
    const char* name;
    std::function<std::vector<Bytes>()> seeds;
    /// Whether the reader accepted the input.
    std::function<bool(const uint8_t*, size_t)> run;
};

std::vector<Target> targets() {
    using namespace tl::formats;
    return {
        {"adm", adm_seeds,
         [](const uint8_t* d, size_t n) {
             const AdmFile f = parse_adm(d, n);
             size_t count = 0;
             f.root.visit([&count](const AdmSection&) { ++count; });
             g_sink = g_sink + count + f.error.size();
             return f.ok;
         }},
        {"cmp", cmp_seeds,
         [](const uint8_t* d, size_t n) {
             const CmpScene s = parse_cmp(d, n);
             g_sink = g_sink + s.nodes.size() + s.logic_groups.size() + s.error.size();
             return s.ok;
         }},
        {"wav", wav_seeds,
         [](const uint8_t* d, size_t n) {
             const WavSound s = parse_wav(d, n);
             const float seconds = wav_duration(d, n);
             g_sink = g_sink + s.samples.size() + bits_of(seconds);
             return s.ok;
         }},
        {"ogg", ogg_seeds,
         [](const uint8_t* d, size_t n) {
             const float seconds = ogg_duration(d, n);
             g_sink = g_sink + bits_of(seconds);
             return seconds > 0.0f;
         }},
        {"xml", xml_seeds,
         [](const uint8_t* d, size_t n) {
             const XmlDocument doc = parse_xml(d, n);
             const Imageset set = parse_imageset(d, n);
             const GuiLayout layout = parse_gui_layout(d, n);
             g_sink = g_sink + doc.root.children.size() + set.images.size() + layout.error.size();
             const std::string s(reinterpret_cast<const char*>(d), n);
             UVector2 v;
             std::string a, b;
             float r = 0, g = 0, bl = 0, al = 0;
             g_sink = g_sink + parse_uvector2(s, v) + parse_image_reference(s, a, b) +
                      parse_colour(s, r, g, bl, al) + bits_of(v.x.scale) + bits_of(v.x.offset) +
                      bits_of(r) + bits_of(al);
             return doc.ok;
         }},
        {"ui_layout", ui_layout_seeds,
         [](const uint8_t* d, size_t n) {
             const UiLayout l = parse_ui_layout(d, n);
             g_sink = g_sink + l.objects.size() + (l.find_widget("Bar") != nullptr);
             return l.ok;
         }},
        {"character", character_seeds,
         [](const uint8_t* d, size_t n) {
             CharacterFile f;
             if (!read_character_file(d, n, f)) return false;
             // What was read must write out and read back.
             const Bytes again = write_character_file(f);
             CharacterFile g;
             if (!read_character_file(again.data(), again.size(), g)) {
                 std::fprintf(stderr, "character: a file that read does not read back\n");
                 std::abort();
             }
             g_sink = g_sink + f.player.inventory.size() + f.levels.size();
             return true;
         }},
        {"stash", stash_seeds,
         [](const uint8_t* d, size_t n) {
             std::vector<ItemSaveState> items;
             if (!read_shared_stash_file(d, n, items)) return false;
             const Bytes again = write_shared_stash_file(items);
             std::vector<ItemSaveState> back;
             if (!read_shared_stash_file(again.data(), again.size(), back) ||
                 back.size() != items.size()) {
                 std::fprintf(stderr, "stash: a file that read does not read back\n");
                 std::abort();
             }
             g_sink = g_sink + items.size();
             return true;
         }},
        {"utf16", [] { return std::vector<Bytes>{{'A', 0, 0x3d, 0xd8, 0x00, 0xde, 'z', 0}}; },
         [](const uint8_t* d, size_t n) {
             const std::string s = utf16le_to_utf8(d, n);
             g_sink = g_sink + utf8_to_utf16(s).size();
             return true;
         }},
        {"markup", markup_seeds,
         [](const uint8_t* d, size_t n) {
             const std::string s(reinterpret_cast<const char*>(d), n);
             std::string joined;
             for (const tl::core::MarkupRun& run : tl::core::markup_runs(s)) {
                 joined += run.text;
                 g_sink =
                     g_sink + bits_of(run.a) + bits_of(run.r) + bits_of(run.g) + bits_of(run.b);
             }
             // core/text.h's promise: the runs joined are exactly the stripped text.
             if (joined != tl::core::strip_markup(s)) {
                 std::fprintf(stderr, "markup: runs and strip_markup disagree\n");
                 std::abort();
             }
             return true;
         }},
    };
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

using tl::fuzz::Rng;

/// A target's stream: the run's seed and the target's name (FNV-1a), so a
/// target run alone draws exactly what it drew in a full run.
Rng stream_for(uint64_t seed, const char* name) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (const char* c = name; *c != 0; ++c) h = (h ^ static_cast<uint8_t>(*c)) * 0x100000001b3ull;
    return Rng{seed * 0x9E3779B97F4A7C15ull ^ h};
}

void mutate(Bytes& b, Rng& rng, const std::vector<Bytes>& pool) {
    static const uint32_t kInteresting[] = {
        0u,      1u,      2u,       0x7fu,       0x80u,       0xffu,       0x100u,     0x7fffu,
        0x8000u, 0xffffu, 0x10000u, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu};
    const int rounds = 1 + static_cast<int>(rng.below(4));
    for (int r = 0; r < rounds; ++r) {
        switch (rng.below(9)) {
            case 0:  // flip a bit
                if (!b.empty()) b[rng.below(b.size())] ^= static_cast<uint8_t>(1u << rng.below(8));
                break;
            case 1:  // a random byte
                if (!b.empty()) b[rng.below(b.size())] = static_cast<uint8_t>(rng.next());
                break;
            case 2: {  // a boundary integer over a dword - most often a length
                if (b.size() < 4) break;
                const size_t at = rng.below(b.size() - 3);
                uint32_t v =
                    kInteresting[rng.below(sizeof(kInteresting) / sizeof(kInteresting[0]))];
                if (rng.below(4) == 0) v = static_cast<uint32_t>(b.size() + rng.below(9) - 4);
                for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>(v >> (8 * i));
                break;
            }
            case 3:  // truncate
                b.resize(rng.below(b.size() + 1));
                break;
            case 4: {  // cut a chunk
                if (b.empty()) break;
                const size_t at = rng.below(b.size());
                const size_t len = 1 + rng.below(std::min<size_t>(64, b.size() - at));
                b.erase(b.begin() + static_cast<std::ptrdiff_t>(at),
                        b.begin() + static_cast<std::ptrdiff_t>(at + len));
                break;
            }
            case 5: {  // repeat a chunk
                if (b.empty() || b.size() > (1u << 16)) break;
                const size_t at = rng.below(b.size());
                const size_t len = 1 + rng.below(std::min<size_t>(64, b.size() - at));
                const Bytes chunk(b.begin() + static_cast<std::ptrdiff_t>(at),
                                  b.begin() + static_cast<std::ptrdiff_t>(at + len));
                b.insert(b.begin() + static_cast<std::ptrdiff_t>(rng.below(b.size() + 1)),
                         chunk.begin(), chunk.end());
                break;
            }
            case 6: {  // splice in part of another seed
                const Bytes& other = pool[rng.below(pool.size())];
                if (other.empty()) break;
                const size_t from = rng.below(other.size());
                const size_t len = 1 + rng.below(other.size() - from);
                const size_t at = rng.below(b.size() + 1);
                b.insert(b.begin() + static_cast<std::ptrdiff_t>(at),
                         other.begin() + static_cast<std::ptrdiff_t>(from),
                         other.begin() + static_cast<std::ptrdiff_t>(from + len));
                break;
            }
            case 7:  // insert random bytes
                for (size_t i = 0, n = 1 + rng.below(8); i < n; ++i) {
                    // Two draws, sequenced: function arguments are evaluated in
                    // an unspecified order, and x86 and AArch64 GCC differ.
                    const size_t at = rng.below(b.size() + 1);
                    const uint8_t value = static_cast<uint8_t>(rng.next());
                    b.insert(b.begin() + static_cast<std::ptrdiff_t>(at), value);
                }
                break;
            default:  // an interesting byte
                if (!b.empty())
                    b[rng.below(b.size())] = static_cast<uint8_t>(kInteresting[rng.below(6)]);
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------

const char* g_current_target = "";
const Bytes* g_current_input = nullptr;
/// Set while a structured target runs: its input is rebuilt, not saved.
unsigned long g_current_iteration = 0;
uint64_t g_current_seed = 0;
bool g_structured = false;

void report_structured() {
    if (!g_structured) return;
    std::fprintf(stderr,
                 "tl_fuzz: failed in %s, seed %llu, iteration %lu - rerun `tl_fuzz %lu %llu %s`\n",
                 g_current_target, static_cast<unsigned long long>(g_current_seed),
                 g_current_iteration, (g_current_iteration + 1) * 4,
                 static_cast<unsigned long long>(g_current_seed), g_current_target);
}

void save_input(const char* target, const Bytes& input) {
    const std::string path = std::string("fuzz-crash-") + target + ".bin";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(input.data()),
              static_cast<std::streamsize>(input.size()));
    std::fprintf(stderr, "tl_fuzz: input saved to %s (%zu bytes)\n", path.c_str(), input.size());
}

#if defined(TL_FUZZ_SANITIZER)
void on_death() {
    if (g_current_input != nullptr) save_input(g_current_target, *g_current_input);
    report_structured();
}
#endif

void on_abort(int) {
    if (g_current_input != nullptr) save_input(g_current_target, *g_current_input);
    report_structured();
    std::_Exit(134);
}

int replay(const std::string& name, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "tl_fuzz: cannot read %s\n", path.c_str());
        return 2;
    }
    const Bytes input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (const Target& t : targets()) {
        if (name != t.name) continue;
        const bool ok = t.run(input.data(), input.size());
        std::printf("tl_fuzz: %s on %s (%zu bytes): no failure (%s)\n", t.name, path.c_str(),
                    input.size(), ok ? "read" : "rejected");
        return 0;
    }
    std::fprintf(stderr, "tl_fuzz: no target '%s'\n", name.c_str());
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--run") return replay(argv[2], argv[3]);

    const unsigned long iterations = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 20000ul;
    const uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1ull;
    const std::string only = argc > 3 ? argv[3] : "";

#if defined(TL_FUZZ_SANITIZER)
    __sanitizer_set_death_callback(on_death);
#endif
    std::signal(SIGABRT, on_abort);
    // The level script logs as it plays; a million random layouts would bury
    // the results.
    tl::core::log_set_min_level(tl::core::LogLevel::Error);

    int ran = 0;
    for (const Target& t : targets()) {
        if (!only.empty() && only != t.name) continue;
        ++ran;
        const std::vector<Bytes> pool = t.seeds();
        g_current_target = t.name;
        // Every seed must read without trouble before anything is corrupted.
        size_t seeds_ok = 0;
        for (const Bytes& s : pool) {
            g_current_input = &s;
            seeds_ok += t.run(s.data(), s.size()) ? 1 : 0;
        }
        if (seeds_ok != pool.size()) {
            // A seed the reader rejects explores nothing past its first check.
            std::fprintf(stderr, "tl_fuzz: %s: only %zu of %zu seeds read\n", t.name, seeds_ok,
                         pool.size());
            return 1;
        }
        Rng rng = stream_for(seed, t.name);
        Bytes input;
        unsigned long accepted = 0;
        g_sink = 0;
        for (unsigned long i = 0; i < iterations; ++i) {
            input = pool[rng.below(pool.size())];
            mutate(input, rng, pool);
            g_current_input = &input;
            accepted += t.run(input.data(), input.size()) ? 1 : 0;
        }
        g_current_input = nullptr;
        // The digest sums what was read, so two machines given one seed print the same line.
        std::printf("tl_fuzz: %-10s %lu inputs, %zu seeds, %lu still read, digest %zu: ok\n",
                    t.name, iterations, pool.size(), accepted, static_cast<size_t>(g_sink));
    }
    // The structured targets: a quarter as many inputs, each a whole parsed
    // file or scene (the script target runs 40 frames of its layout).
    for (const tl::fuzz::StructuredTarget& t : tl::fuzz::structured_targets()) {
        if (!only.empty() && only != t.name) continue;
        ++ran;
        g_current_target = t.name;
        g_current_seed = seed;
        g_structured = true;
        const unsigned long count = std::max(1ul, iterations / 4);
        Rng rng = stream_for(seed, t.name);
        size_t digest = 0;
        // `TL_FUZZ_TRACE=1`: each input's digest, to find where two machines
        // first part ways.
        const bool trace = std::getenv("TL_FUZZ_TRACE") != nullptr;
        for (unsigned long i = 0; i < count; ++i) {
            g_current_iteration = i;
            const size_t one = t.step(rng);
            if (trace) std::printf("%s %lu %zu\n", t.name, i, one);
            digest += one;
        }
        g_structured = false;
        std::printf("tl_fuzz: %-10s %lu inputs, digest %zu: ok\n", t.name, count, digest);
    }
    if (ran == 0) {
        std::fprintf(stderr, "tl_fuzz: no target '%s'\n", only.c_str());
        return 2;
    }
    return 0;
}
