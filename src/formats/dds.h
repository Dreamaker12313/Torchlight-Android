// What a `.dds` holds in its alpha channel, read from the texels: many DXT3/DXT5
// textures under `alpha_blend` materials are fully opaque despite their format.
#pragma once

#include <cstddef>
#include <cstdint>

namespace tl::formats {

/// What the top mip level's alpha says.
enum class DdsAlpha {
    /// Not a `.dds` this reads, or cut short: nothing can be said.
    Unknown,
    /// No alpha channel at all (DXT1 is taken as this: its one-bit alpha is a
    /// cut-out, which an alpha rejection handles with or without a blend).
    None,
    /// An alpha channel that is 255 at every texel of the top level.
    Opaque,
    /// An alpha channel with something else in it.
    Varies,
};

/// The top mip level's alpha, counted.
struct DdsAlphaCensus {
    DdsAlpha kind = DdsAlpha::Unknown;
    /// Texels of the top level; 0 unless `kind` is `Opaque` or `Varies`.
    uint64_t texels = 0;
    /// Alpha 255.
    uint64_t opaque = 0;
    /// Alpha at or below `clear_at` - what an `alpha_rejection greater` of
    /// that value throws away.
    uint64_t clear = 0;

    /// The share of texels an alpha rejection removes: the cut-out's holes.
    float clear_share() const {
        return texels == 0 ? 0.0f : static_cast<float>(clear) / static_cast<float>(texels);
    }
};

/// Reads the header and scans the top mip level: DXT1, DXT3, DXT5 and
/// uncompressed 32-bit with an alpha mask. Anything else is `Unknown`.
DdsAlphaCensus dds_alpha_census(const uint8_t* data, size_t size, uint8_t clear_at = 5);

/// The census's `kind` alone.
DdsAlpha dds_alpha(const uint8_t* data, size_t size);

}  // namespace tl::formats
