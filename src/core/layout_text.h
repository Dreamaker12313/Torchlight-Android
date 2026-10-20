// A layout read from its data-group form (text `.layout` or compiled `.adm`) into the scene the
// compiled reader (`formats::parse_cmp`) makes. `CEditorScene::loadScene(path, bool, ...)`;
// `CEditorScene::loadScene(path, CDataGroup&, ...)`;
// also `CFileSystem::getFileInfo`, `CDataGroup::LoadFile`.
// A property the text does not set is absent (`CmpField::present`), i.e. the object's default.
#pragma once

#include "formats/adm.h"
#include "formats/cmp.h"
#include "formats/layout_schema.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tl::core {

/// `CDescriptorProp::setPropertyValueByString`:
/// a property's value from its text. A string or word-interpreted property keeps the text as
/// UTF-16 words; any other is split at commas and each piece read by the property's type.
std::optional<std::vector<uint32_t>> property_words_from_string(
    const tl::formats::layout_schema::Property& property, const std::string& text);

/// The scene a layout's data group describes (`CEditorScene::loadScene(path,
/// CDataGroup&, ...)`). `layout` is the
/// file's root group.
tl::formats::CmpScene scene_from_layout_group(const tl::formats::AdmSection& layout);

/// Read a layout file in whichever form it is: compiled (`.cmp`), a text data
/// group, or a compiled data group (`.adm`).
tl::formats::CmpScene parse_layout_file(const uint8_t* data, size_t size);

}  // namespace tl::core
