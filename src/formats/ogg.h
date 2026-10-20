// Ogg Vorbis, read only far enough to say how long the file plays (a voice line's
// length paces the level scripts' dialogs).
#pragma once

#include <cstddef>
#include <cstdint>

namespace tl::formats {

/// How long an Ogg Vorbis stream plays, in seconds; 0 when the bytes are not
/// one, or carry no sample count. The last page's granule position over the sample
/// rate; pages are walked forward because `OggS` also occurs inside audio data.
float ogg_duration(const uint8_t* data, size_t size);

}  // namespace tl::formats
