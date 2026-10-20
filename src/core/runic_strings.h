// Runic's `STRINGS::` helpers the game's text is built with: numbers as text,
// and tags replaced. One transcription each, verified against the console's.
#pragma once

#include <cstdint>
#include <string>

namespace tl::core {

/// `STRINGS::GetValueAsWString(int)`: a
/// `std::wostringstream`'s text of the value, no grouping.
std::string value_as_string(int32_t value);

/// `STRINGS::GetValueAsWString(unsigned int)`.
std::string value_as_string(uint32_t value);

/// `STRINGS::GetValueAsWString(long long)`.
std::string value_as_string(int64_t value);

/// `STRINGS::GetValueAsWString(float)`: the
/// stream's defaults, six significant digits as `%g`, in the console's C
/// runtime's spelling: `1e+006`, `1.#INF`, `1.#QNAN`, `-1.#IND`.
std::string value_as_string(float value);

/// `STRINGS::replaceWString(text, find, with)`: while
/// `find` is anywhere in the text (searched from the start), its first occurrence is
/// replaced with `with`.
std::string replace_wstring(std::string text, const std::string& find, const std::string& with);

}  // namespace tl::core
