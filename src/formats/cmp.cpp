#include "formats/cmp.h"

#include "formats/byte_reader.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace tl::formats {
namespace {

constexpr uint8_t kMagic[4] = {'Z', 'E', 'H', 'C'};
constexpr uint8_t kConstant[4] = {0x0f, 0x3a, 0x1a, 0xb8};

/// The trailer: an 8-byte node id and a 4-byte self-offset. Nodes end here.
constexpr size_t kTrailerSize = 12;

/// A parent of all-ones means "no parent".
constexpr size_t kIdSize = 8;

/// Bounds beyond which a node header is certainly not one (names run to 33 characters,
/// descriptor numbers to 59).
constexpr uint32_t kMaxNameChars = 300;
constexpr uint32_t kMaxNodeType = 127;

/// A field claiming more dwords than this is a misread.
constexpr uint32_t kMaxFieldDwords = 65536;

uint32_t u32_at(const uint8_t* d, size_t size, size_t off) {
    if (off + 4 > size) return 0;
    uint32_t v = 0;
    std::memcpy(&v, d + off, 4);
    return v;
}

bool is_root_id(const uint8_t* p) {
    for (size_t i = 0; i < kIdSize; ++i) {
        if (p[i] != 0xff) return false;
    }
    return true;
}

/// Whether the UTF-16LE code units at `off` look like a node name (no control characters).
bool plausible_name(const uint8_t* d, size_t off, uint32_t chars) {
    for (uint32_t i = 0; i < chars; ++i) {
        const uint16_t unit = static_cast<uint16_t>(d[off + 2 * i] | (d[off + 2 * i + 1] << 8));
        if (unit == 0) return false;
        if (unit < 0x20 && unit != '\t') return false;
    }
    return true;
}

/// Whether a node header plausibly starts at `off`; used to resynchronise. The parent test
/// checks the file's shared 4-byte id prefix, not the exact parent: a skipped container
/// would otherwise lose its whole subtree.
bool looks_like_node(const uint8_t* d, size_t end, size_t off,
                     const std::set<uint32_t>& id_prefixes) {
    if (off + 24 > end) return false;

    const uint8_t* parent = d + off + 12;
    if (!is_root_id(parent)) {
        uint32_t prefix = 0;
        std::memcpy(&prefix, parent, 4);
        if (id_prefixes.find(prefix) == id_prefixes.end()) return false;
    }

    const uint32_t chars = u32_at(d, end, off + 20);
    if (chars == 0) {
        // A node with no `NAME`: its type is a descriptor's, its id shares the low 16 bits of
        // one already read, and its fields read cleanly to their zero count.
        if (id_prefixes.empty()) return false;
        const uint32_t type = u32_at(d, end, off);
        if (type > kMaxNodeType) return false;
        uint32_t own = 0;
        std::memcpy(&own, d + off + 4, 4);
        bool family = false;
        for (uint32_t prefix : id_prefixes)
            family = family || (prefix & 0xffffu) == (own & 0xffffu);
        if (!family) return false;
        size_t at = off + 24;
        for (int fields = 0; fields < 64; ++fields) {
            if (at + 4 > end) return false;
            const uint32_t count = u32_at(d, end, at);
            at += 4;
            if (count == 0) return true;
            if (count > 4096 || at + 4 * static_cast<size_t>(count) > end) return false;
            at += 4 * static_cast<size_t>(count);
        }
        return false;
    }
    if (chars > kMaxNameChars) return false;
    // Every descriptor number is small; field bytes that look like a header are not.
    if (u32_at(d, end, off) > kMaxNodeType) return false;
    if (off + 24 + 2 * static_cast<size_t>(chars) > end) return false;
    return plausible_name(d, off + 24, chars);
}

/// Decode a field payload as text (UTF-16LE with a NUL, dword-padded; unlike the counted
/// node name, so not ByteReader::counted_utf16le()), or empty if it is not text.
std::string decode_text_field(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4 || payload.size() % 2 != 0) return {};

    const size_t units = payload.size() / 2;
    size_t ascii_units = 0;
    for (size_t i = 0; i < units; ++i) {
        if (payload[2 * i + 1] == 0) ++ascii_units;
    }
    // Text in this archive is ASCII-range; allow one unit of slack for padding.
    if (ascii_units + 1 < units) return {};

    ByteReader reader(payload.data(), payload.size());
    std::string text = reader.utf16le(units);
    if (!reader.ok()) return {};

    const size_t nul = text.find('\0');
    if (nul != std::string::npos) text.resize(nul);

    if (text.empty()) return {};
    for (unsigned char c : text) {
        if (c < 0x20 && c != '\t') return {};
    }
    return text;
}

bool ends_with_ci(const std::string& text, const std::string& suffix) {
    if (suffix.size() > text.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

}  // namespace

float CmpField::as_float(size_t index) const {
    if (index >= words.size()) return 0.0f;
    // memcpy, not reinterpret_cast: strict aliasing.
    float value = 0.0f;
    const uint32_t bits = words[index];
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

std::string CmpNode::reference(const char* extension) const {
    for (const std::string& s : strings) {
        if (ends_with_ci(s, extension)) return s;
    }
    return {};
}

const CmpNode* CmpScene::find(const std::string& wanted) const {
    for (const CmpNode& node : nodes) {
        if (node.name == wanted) return &node;
    }
    return nullptr;
}

std::vector<const CmpNode*> CmpScene::children_of(int64_t id) const {
    std::vector<const CmpNode*> out;
    for (const CmpNode& node : nodes) {
        if (node.parent_id == id) out.push_back(&node);
    }
    return out;
}

float dynamic_value(const CmpField& field, float t, float fallback) {
    if (field.count() < 2) return fallback;
    const float kind = field.as_float(0);

    if (kind == cmp_field::dynamic_kind::kFixed) return field.as_float(1);

    if (kind == cmp_field::dynamic_kind::kRandomRange) {
        if (field.count() < 3) return field.as_float(1);
        return 0.5f * (field.as_float(1) + field.as_float(2));
    }

    // Curves: kinds 2/3 are (time, value) pairs, 4/5 (time, min, max) triples taken at the
    // midpoint, as kind 1. The two tags of a pair differ only in interpolation.
    const bool pairs =
        kind == cmp_field::dynamic_kind::kCurve || kind == cmp_field::dynamic_kind::kCurveAlternate;
    const bool triples = kind == cmp_field::dynamic_kind::kRandomCurve ||
                         kind == cmp_field::dynamic_kind::kRandomCurveAlternate;
    if (!pairs && !triples) return fallback;

    const size_t stride = pairs ? 2 : 3;
    const size_t keys = (field.count() - 1) / stride;
    if (keys == 0) return fallback;

    // The value of key `i`: the one float of a pair, or the midpoint of a
    // triple's two bounds.
    auto key_time = [&](size_t i) { return field.as_float(1 + stride * i); };
    auto key_value = [&](size_t i) {
        const size_t at = 1 + stride * i + 1;
        if (pairs) return field.as_float(at);
        return 0.5f * (field.as_float(at) + field.as_float(at + 1));
    };

    if (t <= key_time(0)) return key_value(0);
    for (size_t i = 1; i < keys; ++i) {
        const float t1 = key_time(i);
        if (t > t1) continue;
        const float t0 = key_time(i - 1);
        const float span = t1 - t0;
        if (span <= 0.0f) return key_value(i);
        return key_value(i - 1) + (key_value(i) - key_value(i - 1)) * ((t - t0) / span);
    }
    return key_value(keys - 1);
}

namespace {
/// Samples taken to average a track (shipped tracks have at most seven keys).
constexpr int kMeanSamples = 64;
}  // namespace

float dynamic_mean(const CmpField& field, float fallback) {
    if (field.count() < 2) return fallback;
    // A constant and a random range do not vary with time.
    const float kind = field.as_float(0);
    if (kind == cmp_field::dynamic_kind::kFixed || kind == cmp_field::dynamic_kind::kRandomRange) {
        return dynamic_value(field, 0.0f, fallback);
    }

    double total = 0.0;
    for (int i = 0; i < kMeanSamples; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kMeanSamples);
        total += dynamic_value(field, t, fallback);
    }
    return static_cast<float>(total / kMeanSamples);
}

CmpColorKey color_mean(const CmpField& field, const CmpColorKey& fallback) {
    const size_t keys = color_key_count(field);
    if (keys == 0) return fallback;
    if (keys == 1) return color_at(field, 0.0f, fallback);

    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
    for (int i = 0; i < kMeanSamples; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kMeanSamples);
        const CmpColorKey key = color_at(field, t, fallback);
        r += key.r;
        g += key.g;
        b += key.b;
        a += key.a;
    }
    CmpColorKey out;
    out.time = 0.5f;
    out.r = static_cast<float>(r / kMeanSamples);
    out.g = static_cast<float>(g / kMeanSamples);
    out.b = static_cast<float>(b / kMeanSamples);
    out.a = static_cast<float>(a / kMeanSamples);
    return out;
}

size_t color_key_count(const CmpField& field) {
    if (field.count() < 5 || field.count() % 5 != 0) return 0;
    return field.count() / 5;
}

CmpColorKey color_at(const CmpField& field, float t, const CmpColorKey& fallback) {
    const size_t keys = color_key_count(field);
    if (keys == 0) return fallback;

    auto key = [&](size_t i) {
        CmpColorKey k;
        k.time = field.as_float(5 * i);
        k.r = field.as_float(5 * i + 1);
        k.g = field.as_float(5 * i + 2);
        k.b = field.as_float(5 * i + 3);
        k.a = field.as_float(5 * i + 4);
        return k;
    };

    if (t <= key(0).time) return key(0);
    for (size_t i = 1; i < keys; ++i) {
        const CmpColorKey b = key(i);
        if (t > b.time) continue;
        const CmpColorKey a = key(i - 1);
        const float span = b.time - a.time;
        if (span <= 0.0f) return b;
        const float u = (t - a.time) / span;
        CmpColorKey out;
        out.time = t;
        out.r = a.r + (b.r - a.r) * u;
        out.g = a.g + (b.g - a.g) * u;
        out.b = a.b + (b.b - a.b) * u;
        out.a = a.a + (b.a - a.a) * u;
        return out;
    }
    return key(keys - 1);
}

namespace {

/// A byte cursor over the trailing record section. Byte-aligned (a track has a lone flag
/// byte); a bad read sets `bad` rather than throwing, so the section stays readable.
class ScriptReader {
public:
    ScriptReader(const uint8_t* data, size_t end, size_t offset)
        : d_(data), end_(end), off_(offset) {}

    uint8_t u8() {
        if (off_ + 1 > end_) return fail<uint8_t>();
        return d_[off_++];
    }

    uint32_t u32() {
        if (off_ + 4 > end_) return fail<uint32_t>();
        uint32_t v = 0;
        std::memcpy(&v, d_ + off_, 4);
        off_ += 4;
        return v;
    }

    int64_t i64() {
        if (off_ + 8 > end_) return fail<int64_t>();
        int64_t v = 0;
        std::memcpy(&v, d_ + off_, 8);
        off_ += 8;
        return v;
    }

    float f32() {
        const uint32_t bits = u32();
        float v = 0.0f;
        std::memcpy(&v, &bits, 4);
        return v;
    }

    /// A character-counted UTF-16LE string with no NUL (unlike a node field's).
    std::string counted_text() {
        const uint32_t chars = u32();
        if (bad_ || chars > kMaxNameChars || off_ + 2 * static_cast<size_t>(chars) > end_) {
            bad_ = true;
            return {};
        }
        std::string out;
        out.reserve(chars);
        for (uint32_t i = 0; i < chars; ++i) {
            const uint16_t unit =
                static_cast<uint16_t>(d_[off_ + 2 * i] | (d_[off_ + 2 * i + 1] << 8));
            out.push_back(unit < 0x80 ? static_cast<char>(unit) : '?');
        }
        off_ += 2 * static_cast<size_t>(chars);
        return out;
    }

    /// A loop count, refused above `limit` so a misread cannot exhaust the heap.
    uint32_t count(uint32_t limit) {
        const uint32_t n = u32();
        if (n > limit) {
            bad_ = true;
            return 0;
        }
        return n;
    }

    size_t offset() const { return off_; }
    bool ok() const { return !bad_; }

private:
    template <typename T>
    T fail() {
        bad_ = true;
        return T{};
    }

    const uint8_t* d_;
    size_t end_;
    size_t off_;
    bool bad_ = false;
};

/// A record's node id and the offset of the record after it.
struct ScriptRecordHeader {
    int64_t node_id = 0;
    size_t next = 0;
};

ScriptRecordHeader read_record_header(const uint8_t* d, size_t size, size_t off) {
    ScriptRecordHeader h;
    std::memcpy(&h.node_id, d + off, sizeof h.node_id);
    h.next = u32_at(d, size, off + 8);
    return h;
}

/// Follow the record chain from `off` and return where it stops.
size_t chain_end(const uint8_t* d, size_t size, size_t off, size_t stop) {
    while (off < stop) {
        if (off + 12 > stop) return off;
        const size_t next = u32_at(d, size, off + 8);
        if (next <= off || next > stop) return off;
        off = next;
    }
    return off;
}

/// Where the trailing record section begins and ends, or false if there is none.
/// Searched, not computed: a start opening with a known node id whose record chain lands
/// exactly on the payload's end, which is either 12 bytes short of the file (the trailer) or
/// the file's end (no trailer). The window widens back through the last few nodes, since the
/// node scanner can mistake a record for a node.
bool find_script_section(const uint8_t* data, size_t size, const CmpScene& scene,
                         const std::vector<size_t>& name_ends, size_t& start, size_t& stop) {
    if (scene.nodes.empty() || name_ends.size() != scene.nodes.size()) return false;

    std::set<int64_t> known;
    for (const CmpNode& n : scene.nodes) known.insert(n.id);

    const size_t stops[2] = {size >= kTrailerSize ? size - kTrailerSize : 0, size};
    const size_t backs[] = {1, 2, 4, 8, 16, 32, name_ends.size()};

    for (size_t candidate : stops) {
        if (candidate == 0 || candidate > size) continue;
        for (size_t back : backs) {
            const size_t index = name_ends.size() - std::min(back, name_ends.size());
            size_t floor = name_ends[index];
            if (floor > candidate) floor = candidate;

            for (size_t off = floor; off + 8 <= candidate; ++off) {
                int64_t id = 0;
                std::memcpy(&id, data + off, sizeof id);
                if (known.find(id) == known.end()) continue;
                if (chain_end(data, size, off, candidate) == candidate) {
                    start = off;
                    stop = candidate;
                    return true;
                }
            }
        }
    }
    return false;
}

/// A `Logic Group`'s graph. See `CmpLogicGroup`.
bool read_logic_group(ScriptReader& r, CmpLogicGroup& group) {
    const uint32_t objects = r.count(4096);
    group.objects.resize(objects);
    for (uint32_t i = 0; i < objects && r.ok(); ++i) {
        CmpLogicObject& o = group.objects[i];
        o.local = r.u32();
        o.object_id = r.i64();
        o.editor_x = r.f32();
        o.editor_y = r.f32();
    }
    // The link lists follow the whole object table, not interleaved.
    for (uint32_t i = 0; i < objects && r.ok(); ++i) {
        const uint32_t links = r.count(4096);
        group.objects[i].links.resize(links);
        for (uint32_t k = 0; k < links && r.ok(); ++k) {
            CmpLogicLink& l = group.objects[i].links[k];
            l.to = r.u32();
            l.output = r.counted_text();
            l.input = r.counted_text();
        }
    }
    return r.ok();
}

/// A `Timeline`'s tracks. See `CmpTimelineTrack`.
bool read_timeline(ScriptReader& r, CmpTimeline& timeline) {
    const uint32_t targets = r.count(4096);
    timeline.targets.resize(targets);
    for (uint32_t i = 0; i < targets && r.ok(); ++i) {
        CmpTimelineTarget& target = timeline.targets[i];
        target.object_id = r.i64();
        const uint32_t tracks = r.count(4096);
        target.tracks.resize(tracks);
        for (uint32_t t = 0; t < tracks && r.ok(); ++t) {
            CmpTimelineTrack& track = target.tracks[t];
            track.tag = r.u32();
            track.is_event = r.u8() != 0;
            const uint32_t keys = r.count(65536);
            track.keys.resize(keys);
            for (uint32_t k = 0; k < keys && r.ok(); ++k) {
                CmpTimelineKey& key = track.keys[k];
                key.time = r.f32();
                key.interpolation = r.u32();
                // An event key carries no value.
                if (track.is_event) continue;
                const uint32_t words = r.count(1024);
                key.words.resize(words);
                for (uint32_t w = 0; w < words && r.ok(); ++w) key.words[w] = r.u32();
            }
        }
    }
    return r.ok();
}

/// Decode the trailing record section into the scene.
void parse_script_section(const uint8_t* data, size_t size, CmpScene& scene,
                          const std::vector<size_t>& name_ends) {
    size_t off = 0;
    size_t stop = 0;
    if (!find_script_section(data, size, scene, name_ends, off, stop)) return;

    while (off < stop) {
        if (off + 12 > stop) break;
        const ScriptRecordHeader header = read_record_header(data, size, off);
        if (header.next <= off || header.next > stop) break;
        ++scene.script_records;

        const CmpNode* node = scene.node_by_id(header.node_id);
        ScriptReader r(data, header.next, off + 12);
        bool decoded = false;

        if (node != nullptr && node->type == cmp_type::kLogicGroup) {
            CmpLogicGroup group;
            group.node_id = header.node_id;
            // The decode must land exactly on the next record, or it misread.
            if (read_logic_group(r, group) && r.offset() == header.next) {
                scene.logic_groups.push_back(std::move(group));
                decoded = true;
            }
        } else if (node != nullptr &&
                   (node->type == cmp_type::kTimeline || node->type == cmp_type::kTimelineLink)) {
            CmpTimeline timeline;
            timeline.node_id = header.node_id;
            if (read_timeline(r, timeline) && r.offset() == header.next) {
                scene.timelines.push_back(std::move(timeline));
                decoded = true;
            }
        }

        if (!decoded) ++scene.script_undecoded;
        off = header.next;
    }
}

}  // namespace

CmpScene parse_cmp(const uint8_t* data, size_t size) {
    CmpScene scene;

    if (size < 16 + kTrailerSize) {
        scene.error = "too small to be a .cmp";
        return scene;
    }
    if (std::memcmp(data, kMagic, 4) != 0) {
        scene.error = "bad magic, expected ZEHC";
        return scene;
    }
    if (std::memcmp(data + 4, kConstant, 4) != 0) {
        scene.error = "unexpected constant after the magic";
        return scene;
    }

    // Two header shapes. When the dword at 0x08 is zero, a node count and a
    // payload size follow before the first node; otherwise nodes start at 0x0c.
    size_t off = (u32_at(data, size, 8) == 0) ? 20 : 12;
    const size_t end = size - kTrailerSize;

    std::set<uint32_t> id_prefixes;

    // Where each node's name ended, in step with scene.nodes: the one sure point of a node's
    // extent, and the window `find_script_section()` searches back through.
    std::vector<size_t> name_ends;

    // Find the first node header. In a well-formed file it is exactly at `off`,
    // but scanning costs nothing and tolerates a header variant not yet seen.
    while (off < end && !looks_like_node(data, end, off, id_prefixes)) off += 4;

    while (off < end) {
        CmpNode node;
        node.type = u32_at(data, size, off);

        uint32_t prefix = 0;
        std::memcpy(&prefix, data + off + 4, 4);
        id_prefixes.insert(prefix);

        node.root = is_root_id(data + off + 12);

        // The 8-byte id at +4 and parent id at +12 are the source's `ID` and `PARENTID`.
        std::memcpy(&node.id, data + off + 4, sizeof node.id);
        std::memcpy(&node.parent_id, data + off + 12, sizeof node.parent_id);

        const uint32_t chars = u32_at(data, size, off + 20);
        ByteReader reader(data, end);
        if (!reader.seek(off + 20)) break;
        node.name = reader.counted_utf16le();
        if (!reader.ok()) {
            scene.error = "node name ran past the payload";
            break;
        }
        off = reader.offset();
        name_ends.push_back(off);
        (void)chars;

        // Where this node ends: the next node header, found before the fields are read, so a
        // container footer's overlong count cannot swallow the next node.
        size_t next = off;
        while (next < end && !looks_like_node(data, end, next, id_prefixes)) next += 4;

        // Fields, until one with a count of zero or the next node.
        bool clean_end = false;
        while (true) {
            if (off + 4 > next) break;
            const uint32_t count = u32_at(data, size, off);
            off += 4;
            if (count == 0) {
                clean_end = true;
                break;
            }
            if (count > kMaxFieldDwords || off + 4 * static_cast<size_t>(count) > next) {
                // The container-footer case: a count that would run past the
                // next node. Stop here and let the node boundary stand.
                off -= 4;
                break;
            }

            const size_t bytes = 4 * static_cast<size_t>(count);

            // The field stream in file order, kept whole: a fixed positional schema per
            // descriptor, read by index (`cmp_field`).
            CmpField field;
            field.words.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                field.words.push_back(u32_at(data, size, off + 4 * static_cast<size_t>(i)));
            }
            node.fields.push_back(std::move(field));

            if (count == 3) {
                CmpVec3 v;
                ByteReader f(data + off, bytes);
                v.x = f.f32();
                v.y = f.f32();
                v.z = f.f32();
                if (f.ok()) node.vec3s.push_back(v);
            } else {
                std::vector<uint8_t> payload(data + off, data + off + bytes);
                std::string text = decode_text_field(payload);
                if (!text.empty()) node.strings.push_back(std::move(text));
            }
            off += bytes;
        }

        // Undecoded bytes before the next node: counted, never silent.
        if (!clean_end || off != next) ++scene.resyncs;

        scene.nodes.push_back(std::move(node));
        off = next;
    }

    // The level's script, past the last node; last because it is keyed by node id.
    if (!scene.nodes.empty()) parse_script_section(data, size, scene, name_ends);

    scene.ok = !scene.nodes.empty();
    if (!scene.ok && scene.error.empty()) scene.error = "no nodes found";
    return scene;
}

const CmpNode* CmpScene::node_by_id(int64_t id) const {
    for (const CmpNode& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const CmpLogicGroup* CmpScene::logic_group(int64_t node_id) const {
    for (const CmpLogicGroup& group : logic_groups) {
        if (group.node_id == node_id) return &group;
    }
    return nullptr;
}

const CmpTimeline* CmpScene::timeline(int64_t node_id) const {
    for (const CmpTimeline& t : timelines) {
        if (t.node_id == node_id) return &t;
    }
    return nullptr;
}

const CmpLogicObject* CmpLogicGroup::object(uint32_t local) const {
    for (const CmpLogicObject& o : objects) {
        if (o.local == local) return &o;
    }
    return nullptr;
}

int64_t CmpLogicGroup::resolve(uint32_t local) const {
    const CmpLogicObject* o = object(local);
    return o != nullptr ? o->object_id : 0;
}

float CmpTimelineKey::as_float(size_t index) const {
    if (index >= words.size()) return 0.0f;
    float v = 0.0f;
    std::memcpy(&v, &words[index], sizeof v);
    return v;
}

CmpVec3 CmpTimelineKey::vec3() const {
    CmpVec3 v;
    if (words.size() < 3) return v;
    v.x = as_float(0);
    v.y = as_float(1);
    v.z = as_float(2);
    return v;
}

void CmpNode::rebuild_views() {
    vec3s.clear();
    strings.clear();
    for (const CmpField& field : fields) {
        if (!field.present) continue;
        if (field.count() == 3) {
            vec3s.push_back({field.as_float(0), field.as_float(1), field.as_float(2)});
        } else {
            std::string text = field.as_text();
            if (!text.empty()) strings.push_back(std::move(text));
        }
    }
}

std::string CmpField::as_text() const {
    std::vector<uint8_t> payload(words.size() * 4);
    for (size_t i = 0; i < words.size(); ++i) {
        payload[i * 4 + 0] = static_cast<uint8_t>(words[i] & 0xFF);
        payload[i * 4 + 1] = static_cast<uint8_t>((words[i] >> 8) & 0xFF);
        payload[i * 4 + 2] = static_cast<uint8_t>((words[i] >> 16) & 0xFF);
        payload[i * 4 + 3] = static_cast<uint8_t>((words[i] >> 24) & 0xFF);
    }
    return decode_text_field(payload);
}

const char* timeline_event_input(uint32_t descriptor, uint32_t tag) {
    // Every (descriptor, tag) the shipped event keys use, with the input it fires.
    struct Entry {
        uint32_t descriptor;
        uint32_t tag;
        const char* input;
    };
    static const Entry kTable[] = {
        {cmp_type::kAnimationController, 0, "Play"},
        {cmp_type::kAnimationController, 1, "Play Looping"},
        {cmp_type::kAnimationController, 2, "Stop"},
        {cmp_type::kCameraController, 0, "Start Camera"},
        {44, 0, "Activate"},  // Camera Shake
        {55, 0, "Enable"},    // Damage Shape
        {55, 1, "Disable"},   // Damage Shape
        {cmp_type::kModel, 1, "Hide"},
        {cmp_type::kGroup, 1, "Hide"},
        {cmp_type::kParticleLink, 6, "Start Particle"},
        {cmp_type::kParticleLink, 7, "Stop Particle"},
        {cmp_type::kTimelineLink, 6, "Play"},
        {cmp_type::kTimelineLink, 8, "Stop"},
        {cmp_type::kMonster, 10, "Show"},
        {cmp_type::kMonster, 11, "Hide"},
        {cmp_type::kParticle, 0, "Start Particle"},
        {cmp_type::kPlayerBoxTrigger, 1, "Disable"},
        {cmp_type::kPlayerSphereTrigger, 0, "Enable"},
        {cmp_type::kMarker, 2, "Enable"},   // Property Node
        {cmp_type::kMarker, 3, "Disable"},  // Property Node
        {39, 0, "Roll"},                    // Random Choice
        {cmp_type::kRoomPiece, 1, "Hide"},
        {cmp_type::kSound, 0, "Play"},
        {cmp_type::kSound, 1, "Stop"},
        {41, 0, "Activate Teleport"},  // Teleport
        {cmp_type::kTimeline, 2, "Play"},
        {cmp_type::kTimeline, 4, "Stop"},
        {cmp_type::kTimer, 0, "Enable"},
        {cmp_type::kUnitSpawner, 0, "Spawn Units"},
    };
    for (const Entry& entry : kTable) {
        if (entry.descriptor == descriptor && entry.tag == tag) return entry.input;
    }
    return nullptr;
}

}  // namespace tl::formats
