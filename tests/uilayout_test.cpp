// The `[UI]` interface-layout reader, on files written out here in the shipped shape:
// UTF-16LE with a byte-order mark, as the Xbox release writes them.
#include "formats/uilayout.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::formats::parse_ui_layout;
using tl::formats::UiLayout;

/// The same text in the two encodings these files come in.
std::vector<uint8_t> as_utf16(const std::string& text) {
    std::vector<uint8_t> bytes{0xFF, 0xFE};
    for (char c : text) {
        bytes.push_back(static_cast<uint8_t>(c));
        bytes.push_back(0);
    }
    return bytes;
}

std::vector<uint8_t> as_utf8(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

/// The health orb, quoted: `HPBubble` holding `PlayerHealthBar`, which holds
/// `PlayerHealthBarSub`. Three deep, which is what the nesting has to survive.
const char* const kOrb =
    "[UI]\r\n"
    "\t<INTEGER>VERSION:3\r\n"
    "\t<UNSIGNED INT>COUNT:3\r\n"
    "\t[OBJECTS]\r\n"
    "\t\t[BASEOBJECT]\r\n"
    "\t\t\t[PROPERTIES]\r\n"
    "\t\t\t\t<STRING>DESCRIPTOR:Image\r\n"
    "\t\t\t\t<STRING>NAME:HPBubble\r\n"
    "\t\t\t\t<INTEGER64>ID:1\r\n"
    "\t\t\t\t<INTEGER64>PARENTID:-1\r\n"
    "\t\t\t\t<FLOAT>OFFSET X:65\r\n"
    "\t\t\t\t<FLOAT>OFFSET Y:48\r\n"
    "\t\t\t\t<FLOAT>WIDTH:108\r\n"
    "\t\t\t\t<FLOAT>HEIGHT:107\r\n"
    "\t\t\t\t<STRING>IMAGE:hpmanaball\r\n"
    "\t\t\t\t<STRING>WIDGET NAME:PlayerHealth\r\n"
    "\t\t\t[/PROPERTIES]\r\n"
    "\t\t\t[CHILDREN]\r\n"
    "\t\t\t\t[BASEOBJECT]\r\n"
    "\t\t\t\t\t[PROPERTIES]\r\n"
    "\t\t\t\t\t\t<STRING>DESCRIPTOR:Image\r\n"
    "\t\t\t\t\t\t<STRING>NAME:Image3_clone3\r\n"
    "\t\t\t\t\t\t<FLOAT>OFFSET X:11\r\n"
    "\t\t\t\t\t\t<FLOAT>OFFSET Y:11\r\n"
    "\t\t\t\t\t\t<FLOAT>WIDTH:86\r\n"
    "\t\t\t\t\t\t<FLOAT>HEIGHT:84\r\n"
    "\t\t\t\t\t\t<STRING>WIDGET NAME:PlayerHealthBar\r\n"
    "\t\t\t\t\t[/PROPERTIES]\r\n"
    "\t\t\t\t\t[CHILDREN]\r\n"
    "\t\t\t\t\t\t[BASEOBJECT]\r\n"
    "\t\t\t\t\t\t\t[PROPERTIES]\r\n"
    "\t\t\t\t\t\t\t\t<STRING>DESCRIPTOR:Image\r\n"
    "\t\t\t\t\t\t\t\t<STRING>NAME:Image2\r\n"
    "\t\t\t\t\t\t\t\t<FLOAT>WIDTH:86\r\n"
    "\t\t\t\t\t\t\t\t<FLOAT>HEIGHT:84\r\n"
    "\t\t\t\t\t\t\t\t<STRING>IMAGE:hpball\r\n"
    "\t\t\t\t\t\t\t\t<BOOL>ALWAYS ON TOP:true\r\n"
    "\t\t\t\t\t\t\t\t<STRING>WIDGET NAME:PlayerHealthBarSub\r\n"
    "\t\t\t\t\t\t\t[/PROPERTIES]\r\n"
    "\t\t\t\t\t\t\t[CHILDREN]\r\n"
    "\t\t\t\t\t\t\t[/CHILDREN]\r\n"
    "\t\t\t\t\t\t[/BASEOBJECT]\r\n"
    "\t\t\t\t\t[/CHILDREN]\r\n"
    "\t\t\t\t[/BASEOBJECT]\r\n"
    "\t\t\t[/CHILDREN]\r\n"
    "\t\t[/BASEOBJECT]\r\n"
    "\t[/OBJECTS]\r\n";

void check_orb(const UiLayout& layout) {
    TL_CHECK(layout.ok);
    TL_CHECK_EQ(layout.version, 3);
    TL_CHECK_EQ(layout.count, 3u);
    TL_CHECK_EQ(layout.objects.size(), size_t{1});

    const tl::formats::UiObject& bubble = layout.objects[0];
    TL_CHECK_EQ(bubble.descriptor(), std::string("Image"));
    TL_CHECK_EQ(bubble.name(), std::string("HPBubble"));
    TL_CHECK_EQ(bubble.float_or("OFFSET X", -1.0f), 65.0f);
    TL_CHECK_EQ(bubble.float_or("HEIGHT", -1.0f), 107.0f);
    // Absent, and the fallback rather than zero: half this format's meaning is
    // in which keys a window does *not* author.
    TL_CHECK(!bubble.has("SCALE WIDTH"));
    TL_CHECK_EQ(bubble.float_or("SCALE WIDTH", 7.0f), 7.0f);
    TL_CHECK_EQ(bubble.children.size(), size_t{1});

    const tl::formats::UiObject& bar = bubble.children[0];
    TL_CHECK_EQ(bar.string_or("WIDGET NAME", ""), std::string("PlayerHealthBar"));
    TL_CHECK_EQ(bar.children.size(), size_t{1});
    TL_CHECK_EQ(bar.children[0].string_or("IMAGE", ""), std::string("hpball"));
    TL_CHECK(bar.children[0].bool_or("ALWAYS ON TOP", false));

    const tl::formats::UiObject* found = layout.find_widget("PlayerHealthBarSub");
    TL_CHECK(found != nullptr);
    TL_CHECK_EQ(found->name(), std::string("Image2"));
    TL_CHECK(layout.find_widget("NoSuchWindow") == nullptr);
}

TL_TEST(ui_layout_reads_utf16) {
    const std::vector<uint8_t> bytes = as_utf16(kOrb);
    check_orb(parse_ui_layout(bytes.data(), bytes.size()));
}

TL_TEST(ui_layout_reads_utf8) {
    const std::vector<uint8_t> bytes = as_utf8(kOrb);
    check_orb(parse_ui_layout(bytes.data(), bytes.size()));
}

TL_TEST(ui_layout_reads_utf8_with_a_bom) {
    std::string text = "\xEF\xBB\xBF";
    text += kOrb;
    const std::vector<uint8_t> bytes = as_utf8(text);
    check_orb(parse_ui_layout(bytes.data(), bytes.size()));
}

/// Siblings, and the `[CHILDREN]` marker doing no work the nesting does not
/// already do: two objects at the top and one inside the first.
TL_TEST(ui_layout_keeps_file_order) {
    const std::string text =
        "[UI]\n"
        "\t<INTEGER>VERSION:3\n"
        "\t<UNSIGNED INT>COUNT:3\n"
        "\t[OBJECTS]\n"
        "\t\t[BASEOBJECT]\n"
        "\t\t\t[PROPERTIES]\n"
        "\t\t\t\t<STRING>NAME:first\n"
        "\t\t\t[/PROPERTIES]\n"
        "\t\t\t[CHILDREN]\n"
        "\t\t\t\t[BASEOBJECT]\n"
        "\t\t\t\t\t[PROPERTIES]\n"
        "\t\t\t\t\t\t<STRING>NAME:inner\n"
        "\t\t\t\t\t[/PROPERTIES]\n"
        "\t\t\t\t[/BASEOBJECT]\n"
        "\t\t\t[/CHILDREN]\n"
        "\t\t[/BASEOBJECT]\n"
        "\t\t[BASEOBJECT]\n"
        "\t\t\t[PROPERTIES]\n"
        "\t\t\t\t<STRING>NAME:second\n"
        "\t\t\t[/PROPERTIES]\n"
        "\t\t[/BASEOBJECT]\n"
        "\t[/OBJECTS]\n";
    const std::vector<uint8_t> bytes = as_utf8(text);
    const UiLayout layout = parse_ui_layout(bytes.data(), bytes.size());
    TL_CHECK(layout.ok);
    TL_CHECK_EQ(layout.objects.size(), size_t{2});
    TL_CHECK_EQ(layout.objects[0].name(), std::string("first"));
    TL_CHECK_EQ(layout.objects[1].name(), std::string("second"));
    TL_CHECK_EQ(layout.objects[0].children.size(), size_t{1});
    TL_CHECK_EQ(layout.objects[0].children[0].name(), std::string("inner"));
}

/// The header's own tally is checked rather than trusted, because misreading
/// the nesting produces a plausible tree rather than an obvious failure.
TL_TEST(ui_layout_rejects_a_wrong_count) {
    const std::string text =
        "[UI]\n"
        "\t<UNSIGNED INT>COUNT:9\n"
        "\t[OBJECTS]\n"
        "\t\t[BASEOBJECT]\n"
        "\t\t\t[PROPERTIES]\n"
        "\t\t\t\t<STRING>NAME:only\n"
        "\t\t\t[/PROPERTIES]\n"
        "\t\t[/BASEOBJECT]\n"
        "\t[/OBJECTS]\n";
    const std::vector<uint8_t> bytes = as_utf8(text);
    const UiLayout layout = parse_ui_layout(bytes.data(), bytes.size());
    TL_CHECK(!layout.ok);
    TL_CHECK(!layout.error.empty());
}

TL_TEST(ui_layout_rejects_other_files) {
    const std::string text = "<?xml version=\"1.0\" ?>\n<GUILayout>\n</GUILayout>\n";
    const std::vector<uint8_t> bytes = as_utf8(text);
    const UiLayout layout = parse_ui_layout(bytes.data(), bytes.size());
    TL_CHECK(!layout.ok);
    TL_CHECK(parse_ui_layout(nullptr, 0).ok == false);
}

TL_TEST(ui_layout_rejects_an_unterminated_object) {
    const std::string text =
        "[UI]\n"
        "\t[OBJECTS]\n"
        "\t\t[BASEOBJECT]\n"
        "\t\t\t[PROPERTIES]\n"
        "\t\t\t\t<STRING>NAME:dangling\n"
        "\t\t\t[/PROPERTIES]\n";
    const std::vector<uint8_t> bytes = as_utf8(text);
    TL_CHECK(!parse_ui_layout(bytes.data(), bytes.size()).ok);
}

}  // namespace
