// Case-insensitive asset lookup. Torchlight's archive is inconsistently cased and
// internal references do not match the filenames on disk (Alchemist.MESH asks for
// "alchemist.skeleton"). Always resolve through here, never with a raw path.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace tl::core {

class AssetIndex {
public:
    /// Index every file under `root`, recursively. Returns false if `root` is
    /// not a readable directory.
    bool build(const std::string& root);

    /// Resolve `requested` (any casing, either slash direction) to the real
    /// path on disk, relative to the root. Empty if there is no match.
    ///
    /// References in game data are frequently bare filenames - a mesh names its
    /// skeleton, not the skeleton's directory - so a full-path miss falls back
    /// to matching on the basename alone.
    std::string resolve(const std::string& requested) const;

    /// As above, but prefer a file sitting beside `relative_to`.
    ///
    /// This is the accurate form for intra-asset references, which are
    /// same-directory in practice, and it avoids the ambiguity of bare basenames
    /// (many directories contain an "idle.skeleton"). Pass the referring asset's
    /// path, for example the mesh that named the skeleton.
    std::string resolve_near(const std::string& requested, const std::string& relative_to) const;

    bool contains(const std::string& requested) const { return !resolve(requested).empty(); }

    size_t size() const noexcept { return by_path_.size(); }

    /// Basenames that occur in more than one directory. Resolving one of these
    /// without a `relative_to` hint picks arbitrarily, so callers handling
    /// intra-asset references should prefer resolve_near().
    const std::vector<std::string>& ambiguous_basenames() const noexcept { return ambiguous_; }

    /// Lowercase and normalise separators to '/'. Exposed for tests.
    static std::string normalise(std::string s);

    /// Collapse a relative reference against the directory that names it,
    /// resolving "." and ".." and normalising separators, but preserving case
    /// (the lookup reports the archive's own spelling back). Material scripts
    /// refer this way (`../../sharedtextures/...`).
    ///
    /// Returns an empty string when `name` has no separator, so a caller can
    /// tell "nothing to do" from "resolved to nothing".
    static std::string resolve_relative(const std::string& dir, const std::string& name);

private:
    std::unordered_map<std::string, std::string> by_path_;      // folded path -> real
    std::unordered_map<std::string, std::string> by_basename_;  // folded name -> real
    std::vector<std::string> ambiguous_;
};

}  // namespace tl::core
