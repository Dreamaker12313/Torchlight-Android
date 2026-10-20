// Runic's DAT text form (`[GROUP]`, `<TYPE>KEY:value`), the source `.adm` is compiled
// from, read into the same `AdmSection` tree as the compiled reader.
// `CDataGroup::LoadFile`, its `CPairStyle` half.
#pragma once

#include "formats/adm.h"

#include <cstddef>
#include <cstdint>

namespace tl::formats {

/// Whether `data` is a text DAT file: after an optional byte-order mark, its
/// first non-blank character is `[`. A compiled `.adm` starts with a binary
/// version word and never passes.
bool looks_like_dat_text(const uint8_t* data, size_t size);

/// Parse a text DAT file - UTF-16LE or UTF-16BE with a byte-order mark, or
/// UTF-8 with or without one. The file's first group becomes `root`.
///
/// `<INTEGER>` is `Integer`, `<UNSIGNED INT>` `Unsigned`, `<FLOAT>` and
/// `<DOUBLE>` `Float`, `<BOOL>` `Bool` (`true`/`1`), `<INTEGER64>` `Guid`,
/// `<STRING>` `String`, `<TRANSLATE>` `Translate`. Keys and group names keep the
/// file's spelling. A line that is none of these is skipped and counted in
/// `undecoded_bytes`; a closing tag with no group open is ignored. Never throws.
AdmFile parse_dat_text(const uint8_t* data, size_t size);

}  // namespace tl::formats
