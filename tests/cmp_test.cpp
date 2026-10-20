// The `.cmp` layout reader, on files built byte by byte.
#include "formats/cmp.h"
#include "test_support.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using tl::formats::parse_cmp;

struct Builder {
    std::vector<uint8_t> bytes;

    void u32(uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void f32(float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        u32(bits);
    }
    void raw(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); }
    void fill(uint8_t value, size_t n) { bytes.insert(bytes.end(), n, value); }

    /// A node name: a character count then UTF-16LE, with no terminator.
    void name(const std::string& text) {
        u32(static_cast<uint32_t>(text.size()));
        for (char c : text) {
            bytes.push_back(static_cast<uint8_t>(c));
            bytes.push_back(0);
        }
    }

    /// A string *field*: a dword count then UTF-16LE with a NUL, padded out to
    /// a whole number of dwords. Deliberately a different shape from name().
    void string_field(const std::string& text) {
        const size_t units = text.size() + 1;
        const size_t dwords = (units * 2 + 3) / 4;
        u32(static_cast<uint32_t>(dwords));
        std::vector<uint8_t> payload(dwords * 4, 0);
        for (size_t i = 0; i < text.size(); ++i) payload[2 * i] = static_cast<uint8_t>(text[i]);
        raw(payload.data(), payload.size());
    }

    void vec3_field(float x, float y, float z) {
        u32(3);
        f32(x);
        f32(y);
        f32(z);
    }

    void header(uint32_t x) {
        const uint8_t magic[4] = {'Z', 'E', 'H', 'C'};
        const uint8_t constant[4] = {0x0f, 0x3a, 0x1a, 0xb8};
        raw(magic, 4);
        raw(constant, 4);
        u32(x);
        if (x == 0) {
            u32(0);  // node count
            u32(0);  // payload size
        }
    }

    void i64(int64_t v) {
        uint8_t raw8[8];
        std::memcpy(raw8, &v, 8);
        raw(raw8, 8);
    }

    /// The 64-bit id node_header() writes for a prefix/tail pair, which is what
    /// a trailing record and a logic wire address a node by.
    static int64_t node_id(uint8_t id_prefix, uint8_t id_tail) {
        const uint8_t raw8[8] = {id_prefix, id_prefix, id_prefix, id_prefix,
                                 id_tail,   id_tail,   id_tail,   id_tail};
        int64_t id = 0;
        std::memcpy(&id, raw8, 8);
        return id;
    }

    /// Open a trailing record for `id`; returns the offset of its `next` dword.
    size_t begin_record(int64_t id) {
        i64(id);
        const size_t patch = bytes.size();
        u32(0);
        return patch;
    }

    /// Close a record, writing where the next one starts.
    void end_record(size_t patch) {
        const uint32_t here = static_cast<uint32_t>(bytes.size());
        for (int i = 0; i < 4; ++i) {
            bytes[patch + i] = static_cast<uint8_t>(here >> (8 * i));
        }
    }

    /// Node header. `id_prefix` is shared by every node in a real file.
    void node_header(uint32_t type, uint8_t id_prefix, uint8_t id_tail, bool root) {
        u32(type);
        bytes.push_back(id_prefix);
        fill(id_prefix, 3);
        fill(id_tail, 4);
        if (root) {
            fill(0xff, 8);
        } else {
            bytes.push_back(id_prefix);
            fill(id_prefix, 3);
            fill(0x01, 4);
        }
    }

    /// The trailer, which also fixes the final self-offset.
    void trailer() {
        fill(0xAA, 8);
        u32(static_cast<uint32_t>(bytes.size()));
    }
};

Builder camera_file() {
    Builder b;
    b.header(7);
    b.node_header(45, 0xDE, 0x01, /*root=*/true);
    b.name("Camera");
    b.u32(1);
    b.u32(1);
    b.vec3_field(73.346f, 2.5f, 10.848f);
    b.vec3_field(-0.940f, 0.0f, 0.342f);
    b.string_field("Camera Position");
    b.u32(0);  // field terminator
    b.trailer();
    return b;
}

}  // namespace

TL_TEST(cmp_rejects_a_file_without_the_magic) {
    std::vector<uint8_t> junk(64, 0x00);
    auto scene = parse_cmp(junk.data(), junk.size());
    TL_CHECK(!scene.ok);
    TL_CHECK(scene.error.find("magic") != std::string::npos);
}

TL_TEST(cmp_rejects_a_file_that_is_too_small) {
    std::vector<uint8_t> tiny{'Z', 'E', 'H', 'C'};
    auto scene = parse_cmp(tiny.data(), tiny.size());
    TL_CHECK(!scene.ok);
}

TL_TEST(cmp_reads_a_single_node_with_its_transform_and_reference) {
    Builder b = camera_file();
    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());

    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});
    TL_CHECK_EQ(scene.resyncs, size_t{0});

    const auto& node = scene.nodes.front();
    TL_CHECK_EQ(node.name, std::string("Camera"));
    TL_CHECK_EQ(node.type, tl::formats::cmp_type::kMarker);
    TL_CHECK(node.root);

    TL_CHECK(node.has_position());
    TL_CHECK(node.position().x > 73.3f && node.position().x < 73.4f);
    TL_CHECK(node.position().y > 2.49f && node.position().y < 2.51f);

    // Both three-float fields are kept, in order.
    TL_CHECK_EQ(node.vec3s.size(), size_t{2});

    TL_CHECK_EQ(node.strings.size(), size_t{1});
    TL_CHECK_EQ(node.strings.front(), std::string("Camera Position"));
}

TL_TEST(cmp_finds_a_node_by_name) {
    Builder b = camera_file();
    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.find("Camera") != nullptr);
    TL_CHECK(scene.find("Camera Target") == nullptr);
}

TL_TEST(cmp_matches_a_reference_by_extension_ignoring_case) {
    Builder b;
    b.header(7);
    b.node_header(11, 0xDE, 0x01, /*root=*/true);
    b.name("Light0");
    b.vec3_field(1.0f, 2.0f, 3.0f);
    b.string_field("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH");
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    const auto& node = scene.nodes.front();
    TL_CHECK_EQ(node.reference(".mesh"), std::string("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH"));
    TL_CHECK_EQ(node.reference(".MESH"), std::string("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH"));
    TL_CHECK(node.reference(".layout").empty());
}

TL_TEST(cmp_resynchronises_past_undecoded_bytes_and_counts_it) {
    // A container node with an overlong field count, then a second node: the reader must
    // recover the node after it.
    Builder b;
    b.header(7);
    b.node_header(0, 0xDE, 0x01, /*root=*/true);
    b.name("Properties");
    b.vec3_field(69.0f, 0.0f, 14.0f);
    b.u32(4);     // claims four dwords...
    b.u32(3018);  // ...but only three follow before the next node
    b.u32(3776);
    b.u32(0);

    b.node_header(11, 0xDE, 0x02, /*root=*/true);
    b.name("Light0");
    b.vec3_field(59.28f, 0.0f, 9.47f);
    b.string_field("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH");
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{2});

    // The second node is the point of the test: it must survive the container.
    TL_CHECK_EQ(scene.nodes[0].name, std::string("Properties"));
    TL_CHECK_EQ(scene.nodes[1].name, std::string("Light0"));
    TL_CHECK_EQ(scene.nodes[1].reference(".MESH"),
                std::string("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH"));

    // And the skip must be reported, not hidden.
    TL_CHECK(scene.resyncs >= 1);
}

TL_TEST(cmp_reads_the_header_variant_with_a_node_count) {
    // The other header shape: a zero at 0x08 means a node count and payload
    // size follow before the first node.
    Builder b;
    b.header(0);
    b.node_header(12, 0xDE, 0x01, /*root=*/true);
    b.name("Sound2");
    b.vec3_field(-1.78f, 0.0f, 1.79f);
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});
    TL_CHECK_EQ(scene.nodes.front().name, std::string("Sound2"));
    TL_CHECK_EQ(scene.resyncs, size_t{0});
}

// The field stream, positional per descriptor, including single dwords the vec3/string
// views cannot carry; their types are positional too, so both accessors are pinned.
TL_TEST(cmp_exposes_every_field_in_order_including_single_dwords) {
    Builder b;
    b.header(7);
    b.node_header(11, 0xDE, 0x01, /*root=*/true);
    b.name("Light0");
    b.u32(1);
    b.u32(1);  // VISIBLE
    b.vec3_field(59.28f, 0.0f, 9.47f);
    b.string_field("MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH");
    b.u32(1);
    b.u32(2);  // BRIGHTNESS, an unsigned int
    b.u32(1);
    b.f32(3.0f);  // SCALE X, a float
    b.u32(1);
    b.f32(3.0f);  // SCALE Z
    b.u32(1);
    b.f32(0.0f);  // ANGLE
    b.u32(0);     // field terminator
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK(scene.nodes.size() == 1);
    const tl::formats::CmpNode& node = scene.nodes[0];
    TL_CHECK(node.type == tl::formats::cmp_type::kLight);
    TL_CHECK(node.fields.size() == 7);

    using namespace tl::formats::cmp_field::light;
    TL_CHECK(node.field(kVisible) != nullptr);
    TL_CHECK(node.field(kVisible)->as_bool());
    TL_CHECK(node.field(kPosition)->count() == 3);
    TL_CHECK(node.field(kFile)->count() > 3);
    TL_CHECK(node.field(kBrightness)->as_uint() == 2);
    TL_CHECK(node.field(kScaleX)->as_float() == 3.0f);
    TL_CHECK(node.field(kScaleZ)->as_float() == 3.0f);
    TL_CHECK(node.field(kAngle)->as_float() == 0.0f);

    // A field index past the end is not an error: an older editor wrote shorter
    // nodes, and a missing field means "the default".
    TL_CHECK(node.field(99) == nullptr);

    // The two convenience views still work and still skip the single dwords.
    TL_CHECK(node.vec3s.size() == 1);
    TL_CHECK(node.strings.size() == 1);
    TL_CHECK(node.reference(".MESH") == "MEDIA/LIGHTS/LIGHT_CIRCLE_01.MESH");
}

// An out-of-range field read is defined, and a dword read as the wrong type gives the other
// reading.
TL_TEST(cmp_field_accessors_are_bounds_checked) {
    tl::formats::CmpField field;
    field.words = {0x40400000u};  // 3.0f
    TL_CHECK(field.count() == 1);
    TL_CHECK(field.as_float() == 3.0f);
    TL_CHECK(field.as_uint() == 0x40400000u);
    TL_CHECK(field.as_bool());
    TL_CHECK(field.as_float(1) == 0.0f);
    TL_CHECK(field.as_uint(1) == 0u);
    TL_CHECK(!field.as_bool(1));
}

// The node header's id (+4) and parent id (+12) are kept: a particle system's parts are
// separate nodes that name it as parent.
TL_TEST(cmp_reads_node_ids_and_parents) {
    Builder b;
    b.header(7);
    b.node_header(tl::formats::cmp_type::kParticle, 0xDE, 0x01, /*root=*/true);
    b.name("Glow camera");
    b.vec3_field(0.0f, 0.0f, 0.0f);
    b.u32(0);
    b.node_header(tl::formats::cmp_type::kEmitter, 0xDE, 0x02, /*root=*/false);
    b.name("Emitter3");
    b.vec3_field(0.0f, 0.0f, 0.0f);
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{2});

    const tl::formats::CmpNode& particle = scene.nodes[0];
    const tl::formats::CmpNode& emitter = scene.nodes[1];
    TL_CHECK(particle.root);
    TL_CHECK(!emitter.root);
    TL_CHECK(particle.parent_id == -1);
    TL_CHECK(particle.id != 0);
    TL_CHECK(emitter.id != particle.id);

    // node_header() gives a non-root node the shared prefix with a 0x01 tail as
    // its parent, which is what the first node's id is built from.
    TL_CHECK(emitter.parent_id == particle.id);

    const std::vector<const tl::formats::CmpNode*> children = scene.children_of(particle.id);
    TL_CHECK_EQ(children.size(), size_t{1});
    TL_CHECK_EQ(children.front()->name, std::string("Emitter3"));
    TL_CHECK(scene.children_of(emitter.id).empty());
}

// A node with no `NAME` (length 0) is read as a node; a header-like run with an unknown type
// in field bytes is not.
TL_TEST(cmp_reads_a_node_with_no_name) {
    Builder b;
    b.header(7);
    b.node_header(tl::formats::cmp_type::kParticle, 0xDE, 0x01, /*root=*/true);
    b.name("Flame1");
    b.vec3_field(0.0f, 0.0f, 0.0f);
    b.u32(0);
    b.node_header(46, 0xDE, 0x02, /*root=*/false);
    b.u32(0);  // no name
    b.u32(1);
    b.u32(1);  // ENABLED
    b.u32(1);
    b.u32(3);  // NUM INPUTS
    b.u32(0);
    b.node_header(tl::formats::cmp_type::kEmitter, 0xDE, 0x03, /*root=*/false);
    b.name("Emitter3");
    b.vec3_field(0.0f, 0.0f, 0.0f);
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{3});
    TL_CHECK_EQ(scene.nodes[1].type, uint32_t{46});
    TL_CHECK(scene.nodes[1].name.empty());
    TL_CHECK_EQ(scene.nodes[1].fields.size(), size_t{2});
    TL_CHECK_EQ(scene.nodes[2].name, std::string("Emitter3"));
    TL_CHECK_EQ(scene.resyncs, size_t{0});
}

// The "dynamic property" encoding shared by SCALE ON LAUNCH, PARTICLE LIFE, EMIT RATE and
// the Scale tracks.
TL_TEST(cmp_dynamic_property_kinds) {
    using tl::formats::CmpField;
    using tl::formats::dynamic_value;

    auto make = [](const std::vector<float>& values) {
        CmpField f;
        for (float v : values) {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, 4);
            f.words.push_back(bits);
        }
        return f;
    };

    // Kind 0: a constant, whatever the time.
    const CmpField fixed = make({0.0f, 4.0f});
    TL_CHECK(dynamic_value(fixed, 0.0f) == 4.0f);
    TL_CHECK(dynamic_value(fixed, 1.0f) == 4.0f);

    // Kind 1: a random range, collapsed to its midpoint so that a dumped frame
    // is reproducible between the desktop and the console.
    const CmpField range = make({1.0f, 1.0f, 5.0f});
    TL_CHECK(dynamic_value(range, 0.0f) == 3.0f);

    // Kinds 2 and 3: a curve of (time, value) pairs, read the same way.
    const CmpField curve = make({2.0f, 0.0f, 1.0f, 1.0f, 0.0f});
    TL_CHECK(dynamic_value(curve, 0.0f) == 1.0f);
    TL_CHECK(dynamic_value(curve, 0.5f) == 0.5f);
    TL_CHECK(dynamic_value(curve, 1.0f) == 0.0f);
    // Off the ends the track holds rather than extrapolating.
    TL_CHECK(dynamic_value(curve, -1.0f) == 1.0f);
    TL_CHECK(dynamic_value(curve, 2.0f) == 0.0f);

    // Kinds 4 and 5: (time, min, max) triples - a random range that itself
    // varies over the life. Each key collapses to its midpoint.
    const CmpField random_curve = make({4.0f, 0.0f, 3.0f, 3.0f, 1.0f, 2.0f, 4.0f});
    TL_CHECK(dynamic_value(random_curve, 0.0f) == 3.0f);
    TL_CHECK(dynamic_value(random_curve, 1.0f) == 3.0f);

    // A field too short to hold a kind and one value is the "use the default"
    // case, not an error.
    TL_CHECK(dynamic_value(make({0.0f}), 0.0f, 7.0f) == 7.0f);
    TL_CHECK(dynamic_value(CmpField{}, 0.0f, 7.0f) == 7.0f);

    // An unrecognised kind falls back rather than reading garbage as a curve.
    TL_CHECK(dynamic_value(make({11.0f, 1.0f}), 0.0f, 7.0f) == 7.0f);
}

// The mean over a particle's life, which is what a steady-state emitter draws.
TL_TEST(cmp_dynamic_mean_averages_a_track) {
    using tl::formats::CmpField;
    using tl::formats::dynamic_mean;

    auto make = [](const std::vector<float>& values) {
        CmpField f;
        for (float v : values) {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, 4);
            f.words.push_back(bits);
        }
        return f;
    };

    // A constant averages to itself, and takes the cheap path.
    TL_CHECK(dynamic_mean(make({0.0f, 4.0f})) == 4.0f);
    TL_CHECK(dynamic_mean(make({1.0f, 1.0f, 5.0f})) == 3.0f);

    // A straight ramp from 1 to 0 averages to a half.
    const float mean = dynamic_mean(make({2.0f, 0.0f, 1.0f, 1.0f, 0.0f}));
    TL_CHECK(mean > 0.49f && mean < 0.51f);
}

// COLOR OVER TIME has no kind tag: a flat run of (time, r, g, b, a) five-tuples.
TL_TEST(cmp_color_over_time_is_five_tuples) {
    using tl::formats::CmpField;
    using tl::formats::color_at;
    using tl::formats::color_key_count;
    using tl::formats::color_mean;

    auto make = [](const std::vector<float>& values) {
        CmpField f;
        for (float v : values) {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, 4);
            f.words.push_back(bits);
        }
        return f;
    };

    // One key: a constant colour. This is MOON.LAYOUT's halo tint.
    const CmpField one = make({0.0f, 0.129412f, 0.141176f, 0.247059f, 1.0f});
    TL_CHECK_EQ(color_key_count(one), size_t{1});
    TL_CHECK(color_at(one, 0.0f).r > 0.129f && color_at(one, 0.0f).r < 0.130f);
    TL_CHECK(color_at(one, 1.0f).b > 0.247f && color_at(one, 1.0f).b < 0.248f);

    // Three keys, black to white to black: the midpoint is white and the mean
    // is somewhere between.
    const CmpField three = make(
        {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f});
    TL_CHECK_EQ(color_key_count(three), size_t{3});
    TL_CHECK(color_at(three, 0.5f).g > 0.99f);
    TL_CHECK(color_at(three, 0.25f).g > 0.49f && color_at(three, 0.25f).g < 0.51f);
    const tl::formats::CmpColorKey mean = color_mean(three);
    TL_CHECK(mean.g > 0.45f && mean.g < 0.55f);

    // A count that is not a multiple of five is not a colour track, and saying
    // so is better than averaging four floats into a colour.
    TL_CHECK_EQ(color_key_count(make({0.0f, 1.0f, 1.0f, 1.0f})), size_t{0});
}

// A `Monster` node: 21 fields, starting on the transform rather than VISIBLE, naming its
// animation by stem.
TL_TEST(cmp_reads_a_monster_animation_speed_and_loops) {
    Builder b;
    b.header(7);
    b.node_header(tl::formats::cmp_type::kMonster, 0xB1, 0x02, /*root=*/true);
    b.name("Roach");
    b.vec3_field(20.96f, -0.41f, 57.42f);                      // 0  POSITION
    b.vec3_field(0.0f, 0.0f, 1.0f);                            // 1  FORWARD
    b.vec3_field(1.0f, 0.0f, 0.0f);                            // 2  RIGHT
    b.vec3_field(0.0f, 1.0f, 0.0f);                            // 3  UP
    b.string_field("Roach");                                   // 4  MONSTER
    b.string_field("MEDIA/MODELS/CRITTERS/ROACH/ROACH.MESH");  // 5 FILE
    for (int i = 6; i <= 13; ++i) {                            // 6-13 flags this test does not read
        b.u32(1);
        b.u32(0);
    }
    b.u32(1);
    b.f32(0.75f);                     // 14 SCALE, one uniform value
    for (int i = 15; i <= 17; ++i) {  // 15-17 CANNOT WANDER, NO GOLD, NO TREASURE
        b.u32(1);
        b.u32(0);
    }
    b.string_field("Idle");  // 18 ANIMATION
    b.u32(1);
    b.f32(1.5f);  // 19 SPEED
    b.u32(1);
    b.u32(1);  // 20 LOOPS
    b.u32(0);  // field terminator
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});

    const auto& node = scene.nodes.front();
    namespace monster = tl::formats::cmp_field::monster;
    TL_CHECK_EQ(node.type, tl::formats::cmp_type::kMonster);

    // Field 0 is the position, not VISIBLE - the trap this descriptor sets.
    TL_CHECK(node.has_position());
    TL_CHECK(node.position().z > 57.4f && node.position().z < 57.5f);

    const tl::formats::CmpField* unit = node.field(monster::kMonster);
    TL_CHECK(unit != nullptr);
    TL_CHECK_EQ(unit->as_text(), std::string("Roach"));

    const tl::formats::CmpField* animation = node.field(monster::kAnimation);
    TL_CHECK(animation != nullptr);
    TL_CHECK_EQ(animation->as_text(), std::string("Idle"));

    const tl::formats::CmpField* scale = node.field(monster::kScale);
    TL_CHECK(scale != nullptr);
    TL_CHECK_EQ(scale->count(), size_t{1});
    TL_CHECK(scale->as_float() > 0.74f && scale->as_float() < 0.76f);

    const tl::formats::CmpField* speed = node.field(monster::kSpeed);
    TL_CHECK(speed != nullptr);
    TL_CHECK(speed->as_float() > 1.49f && speed->as_float() < 1.51f);

    const tl::formats::CmpField* loops = node.field(monster::kLoops);
    TL_CHECK(loops != nullptr);
    TL_CHECK(loops->as_bool());
}

// An `Animation Controller` has no position yet answers `has_position()` true: a string of
// four or five characters pads to three dwords and is classified as a vec3. Read by index.
TL_TEST(cmp_animation_controller_reads_by_index_despite_a_three_dword_string) {
    Builder b;
    b.header(7);
    b.node_header(tl::formats::cmp_type::kAnimationController, 0xC3, 0x04, /*root=*/true);
    b.name("VarkolynDie1");
    b.u32(1);
    b.u32(1);  // 0 START ON LOAD
    b.u32(1);
    b.f32(0.2f);  // 1 BLEND TIME
    b.u32(1);
    b.f32(0.0f);  // 2 BLEND OUT TIME
    b.u32(1);
    b.f32(0.0f);  // 3 DURATION OVERRIDE
    b.u32(1);
    b.u32(0);                        // 4 PLAY IDLE
    b.string_field("Dead1");         // 5 ANIMATION
    b.string_field("MONSTERS");      // 6 GROUP
    b.string_field("VarkolynDead");  // 7 UNIT
    b.u32(0);                        // field terminator
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});

    const auto& node = scene.nodes.front();
    namespace controller = tl::formats::cmp_field::animation_controller;
    TL_CHECK_EQ(node.type, tl::formats::cmp_type::kAnimationController);

    // "Dead1" is five characters, so its field is three dwords and is taken for
    // a vector. The node has no position; this is the classifier speaking.
    TL_CHECK(node.has_position());
    TL_CHECK_EQ(node.vec3s.size(), size_t{1});
    // And that string is therefore absent from the collected list.
    for (const std::string& s : node.strings) TL_CHECK(s != std::string("Dead1"));

    const tl::formats::CmpField* start = node.field(controller::kStartOnLoad);
    TL_CHECK(start != nullptr);
    TL_CHECK(start->as_bool());

    TL_CHECK_EQ(node.field(controller::kAnimation)->as_text(), std::string("Dead1"));
    TL_CHECK_EQ(node.field(controller::kGroup)->as_text(), std::string("MONSTERS"));
    TL_CHECK_EQ(node.field(controller::kUnit)->as_text(), std::string("VarkolynDead"));
}

// `Texture Animation`: ANIMATION SPEED is a dynamic property in frames per
// second, and the random-start flag is field 4.
TL_TEST(cmp_reads_texture_animation_speed_and_random_start) {
    Builder b;
    b.header(7);
    // Root, though a real one hangs off an Emitter: a lone child's parent prefix is unseen.
    b.node_header(tl::formats::cmp_type::kTextureAnimation, 0xD7, 0x08, /*root=*/true);
    b.name("Texture Animation0");
    b.vec3_field(0.0f, 0.0f, 0.0f);  // 0 POSITION
    b.u32(1);
    b.u32(1);  // 1 ENABLE
    b.u32(2);
    b.f32(0.0f);   // 2 ANIMATION SPEED, kind 0 (fixed)
    b.f32(18.0f);  //   ... 18 frames per second
    b.u32(1);
    b.u32(1);  // 3 unsolved, a constant 1
    b.u32(1);
    b.u32(1);  // 4 USE RANDOM STARTING FRAME
    b.u32(0);  // field terminator
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.nodes.size(), size_t{1});

    const auto& node = scene.nodes.front();
    namespace sheet = tl::formats::cmp_field::texture_animation;
    TL_CHECK_EQ(node.type, tl::formats::cmp_type::kTextureAnimation);

    const tl::formats::CmpField* enable = node.field(sheet::kEnable);
    TL_CHECK(enable != nullptr);
    TL_CHECK(enable->as_bool());

    const tl::formats::CmpField* speed = node.field(sheet::kAnimationSpeed);
    TL_CHECK(speed != nullptr);
    const float fps = tl::formats::dynamic_value(*speed, 0.0f, 0.0f);
    TL_CHECK(fps > 17.9f && fps < 18.1f);

    const tl::formats::CmpField* random = node.field(sheet::kUseRandomStartingFrame);
    TL_CHECK(random != nullptr);
    TL_CHECK(random->as_bool());
}

// The trailing record section: a `Logic Group`'s wiring and a `Timeline`'s keys live past
// the last node, not inside it.
TL_TEST(cmp_reads_the_trailing_logic_and_timeline_records) {
    Builder b;
    b.header(7);

    const int64_t group_id = Builder::node_id(0xD7, 0x10);
    const int64_t timeline_id = Builder::node_id(0xD7, 0x11);

    b.node_header(tl::formats::cmp_type::kLogicGroup, 0xD7, 0x10, /*root=*/true);
    b.name("Logic Group1");
    b.u32(0);  // no fields at all, which is what a real one carries

    b.node_header(tl::formats::cmp_type::kTimeline, 0xD7, 0x11, /*root=*/true);
    b.name("Timeline2");
    b.u32(1);
    b.f32(10.0f);  // 0 DURATION
    b.u32(0);

    // The group wires its own `Level Activated` to the timeline's `Play`: a group is a member
    // of its own graph.
    const size_t logic = b.begin_record(group_id);
    b.u32(2);  // two objects
    b.u32(0);  //   local 0
    b.i64(timeline_id);
    b.f32(351.0f);
    b.f32(111.0f);
    b.u32(1);  //   local 1
    b.i64(group_id);
    b.f32(51.0f);
    b.f32(189.0f);
    b.u32(0);  // object 0 has no links
    b.u32(1);  // object 1 has one
    b.u32(0);  //   ... to local 0
    b.name("Level Activated");
    b.name("Play");
    b.end_record(logic);

    // The timeline record: one property track carrying a position, and one
    // event track carrying instants and no values at all.
    const size_t times = b.begin_record(timeline_id);
    b.u32(1);  // one target
    b.i64(group_id);
    b.u32(2);              //   two tracks
    b.u32(0);              //   track 0: field index 0 of the target
    b.bytes.push_back(0);  //   ... a property track
    b.u32(3);              //   ... three keys
    b.f32(0.0f);
    b.u32(tl::formats::cmp_interpolation::kLinear);
    b.u32(3);
    b.f32(24.59f);
    b.f32(0.0f);
    b.f32(87.8f);
    b.f32(0.25f);
    b.u32(tl::formats::cmp_interpolation::kLinear);
    b.u32(3);
    b.f32(24.59f);
    b.f32(0.0f);
    b.f32(87.8f);
    b.f32(1.0f);
    b.u32(tl::formats::cmp_interpolation::kLinear);
    b.u32(3);
    b.f32(-32.4f);
    b.f32(0.0f);
    b.f32(18.32f);
    b.u32(6);              //   track 1: event tag 6
    b.bytes.push_back(1);  //   ... an event track
    b.u32(1);              //   ... one key
    b.f32(0.5f);
    b.u32(tl::formats::cmp_interpolation::kLinear);
    b.end_record(times);

    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.script_records, size_t{2});
    TL_CHECK_EQ(scene.script_undecoded, size_t{0});

    const tl::formats::CmpLogicGroup* group = scene.logic_group(group_id);
    TL_CHECK(group != nullptr);
    TL_CHECK_EQ(group->objects.size(), size_t{2});
    TL_CHECK_EQ(group->objects[0].links.size(), size_t{0});
    TL_CHECK_EQ(group->objects[1].links.size(), size_t{1});

    const tl::formats::CmpLogicLink& link = group->objects[1].links.front();
    TL_CHECK_EQ(link.output, std::string("Level Activated"));
    TL_CHECK_EQ(link.input, std::string("Play"));
    // A wire names a *local* index, and resolving it is what reaches the node.
    TL_CHECK_EQ(group->resolve(link.to), timeline_id);

    const tl::formats::CmpTimeline* timeline = scene.timeline(timeline_id);
    TL_CHECK(timeline != nullptr);
    TL_CHECK_EQ(timeline->targets.size(), size_t{1});
    TL_CHECK_EQ(timeline->targets[0].tracks.size(), size_t{2});

    const tl::formats::CmpTimelineTrack& track = timeline->targets[0].tracks[0];
    TL_CHECK(!track.is_event);
    TL_CHECK_EQ(track.keys.size(), size_t{3});

    // Check parsed keys directly.
    TL_CHECK_EQ(track.keys[0].time, 0.0f);
    TL_CHECK_EQ(track.keys[0].interpolation, tl::formats::cmp_interpolation::kLinear);
    TL_CHECK_EQ(track.keys[0].vec3().x, 24.59f);
    TL_CHECK_EQ(track.keys[0].vec3().z, 87.8f);
    TL_CHECK_EQ(track.keys[1].time, 0.25f);
    TL_CHECK_EQ(track.keys[1].vec3().x, 24.59f);
    TL_CHECK_EQ(track.keys[1].vec3().z, 87.8f);
    TL_CHECK_EQ(track.keys[2].time, 1.0f);
    TL_CHECK_EQ(track.keys[2].vec3().x, -32.4f);
    TL_CHECK_EQ(track.keys[2].vec3().z, 18.32f);

    // An event track marks instants. It carries no value, and reading one as a
    // property track is what walks off the end of the record.
    const tl::formats::CmpTimelineTrack& events = timeline->targets[0].tracks[1];
    TL_CHECK(events.is_event);
    TL_CHECK_EQ(events.tag, uint32_t{6});
    TL_CHECK_EQ(events.keys.size(), size_t{1});
    TL_CHECK(events.keys.front().words.empty());
}

// A layout with no scripted content has no trailing section, and saying so is
// the difference between "this level does nothing" and "the reader lost it".
TL_TEST(cmp_reports_no_script_section_when_a_layout_has_none) {
    Builder b;
    b.header(7);
    b.node_header(tl::formats::cmp_type::kModel, 0xD7, 0x20, /*root=*/true);
    b.name("Generic Model1");
    b.u32(1);
    b.u32(1);  // 0 VISIBLE
    b.vec3_field(1.0f, 2.0f, 3.0f);
    b.u32(0);
    b.trailer();

    auto scene = parse_cmp(b.bytes.data(), b.bytes.size());
    TL_CHECK(scene.ok);
    TL_CHECK_EQ(scene.script_records, size_t{0});
    TL_CHECK(scene.logic_groups.empty());
    TL_CHECK(scene.timelines.empty());
}
