// The renderer's asset lookup: the archive index (every entry by basename, the
// material scripts by directory, the spawn classes by name) and the rules that
// qualify a bare or relative reference against it. The port's own (ADR 0002).
// Split from renderer.cpp; the shared declarations are in renderer_impl.h.
#include "render/renderer_impl.h"

namespace tl::render::renderer_detail {

/// Every archive entry, indexed by lower-cased basename.
///
/// This exists for one reason: `ZipArchive::findFileInfo()` is a **linear scan
/// of the whole archive** with a case-insensitive pattern match per entry, and
/// `ResourceGroupManager::findResourceFileInfo()` calls it once per lookup.
/// `Pak.zip` holds 22,426 entries, and loading the playable town performs
/// thousands of lookups - every mesh, every collision mesh, every texture named
/// by every material. That is tens of millions of string comparisons before a
/// single byte of geometry is read, and it is where the town's load time went:
/// 74 seconds on the console, 27 on the desktop.
///
/// Building the index costs one enumeration. It is safe to cache because the
/// archive is mounted read-only and never changes while it is mounted, and it
/// is dropped and rebuilt if resources are mounted again.
///
/// Keyed on basename rather than on full path because that is the question
/// every caller here actually asks - which is itself a consequence of building
/// OGRE with OGRE_RESOURCEMANAGER_STRICT=0. See ADR 0002.
std::unordered_map<std::string, std::vector<Ogre::FileInfo>> g_archive_index;

/// Material scripts, grouped by the directory they sit in.
///
/// Built from the same enumeration, for the same reason: locating a mesh's
/// material by scanning the archive for `*.material` is another full 22,426
/// entry pass, and parse_sibling_material() does it once per mesh whose stem
/// has no script of its own. On the town that is hundreds of scans.
///
/// Keyed by the directory lower-cased, as every lookup here is (ADR 0002): the
/// zip reader hands the Xbox 360 pak's names back lower-cased
/// (`media/levelsets/cave/`) while its level data names the meshes as the
/// archive spells them (`media/levelSets/cave/`), and an exact key left every
/// level set from the cave on without its materials - white geometry.
std::unordered_map<std::string, std::vector<std::string>> g_materials_by_dir;
bool g_archive_indexed = false;
// PORT: asset lookup. Every material any script declares, by name lower-cased:
// the first script in the archive's order that declares it (parse_material_declaring).
std::unordered_map<std::string, std::string> g_material_scripts;
bool g_material_scripts_indexed = false;

/// `spawn_class_path`'s index, built on first use and dropped with the rest.
std::unordered_map<std::string, std::string> g_spawn_class_paths;
bool g_spawn_classes_indexed = false;

bool ends_with_lower(const std::string& text, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (n > text.size()) return false;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[text.size() - n + i])) != suffix[i]) {
            return false;
        }
    }
    return true;
}

std::string lowered(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

void forget_archive_index() {
    g_archive_index.clear();
    g_materials_by_dir.clear();
    g_material_scripts.clear();
    g_material_scripts_indexed = false;
    g_archive_indexed = false;
    g_spawn_class_paths.clear();
    g_spawn_classes_indexed = false;
}

/// Build the index if it does not exist yet. Idempotent and cheap after the
/// first call.
void ensure_archive_index() {
    if (g_archive_indexed) return;
    g_archive_indexed = true;

    auto& groups = Ogre::ResourceGroupManager::getSingleton();
    Ogre::FileInfoListPtr all = groups.listResourceFileInfo(kGameGroup, false);
    if (all) {
        for (const Ogre::FileInfo& info : *all) {
            g_archive_index[lowered(info.basename)].push_back(info);
            if (ends_with_lower(info.basename, ".material")) {
                g_materials_by_dir[lowered(info.path)].push_back(info.path + info.basename);
            }
        }
    }
    TL_LOG_INFO(
        "archive index: %zu distinct basename(s), %zu directory(ies) with "
        "material scripts",
        g_archive_index.size(), g_materials_by_dir.size());
}

/// Where a material script stands among its directory's scripts in the
/// archive's own order - the order Runic's OGRE parses them in (see the rank
/// test in parse_material_script()). A script the index does not know sorts
/// last.
// PORT: a material script's place in its directory's archive order (OGRE loading).
size_t material_script_rank(const std::string& script) {
    ensure_archive_index();
    const std::string dir = dir_of_asset(script);
    const auto scripts = g_materials_by_dir.find(lowered(dir));
    if (scripts == g_materials_by_dir.end()) return SIZE_MAX;
    const std::string wanted = lowered(script);
    for (size_t i = 0; i < scripts->second.size(); ++i) {
        if (lowered(scripts->second[i]) == wanted) return i;
    }
    return SIZE_MAX;
}

const std::vector<Ogre::FileInfo>* archive_entries(const std::string& basename) {
    ensure_archive_index();
    const auto it = g_archive_index.find(lowered(basename));
    return it == g_archive_index.end() ? nullptr : &it->second;
}

/// Every archive path under `dir` whose basename ends in `suffix`.
///
/// Not a glob, deliberately. **OGRE's own `find()` cannot match a path here**:
/// with `OGRE_RESOURCEMANAGER_STRICT=0`, `ZipArchive::load()` overwrites each
/// entry's `filename` with its *basename* (OgreZip.cpp), so a pattern
/// containing a directory - `media/sounds/*.adm` - matches nothing at all and
/// says so by returning an empty list rather than by failing. The archive index
/// this file already builds keeps the real path, so the walk goes through that.
///
/// `dir` is matched case-insensitively and as a prefix, so it reaches
/// subdirectories: `media/units/monsters/` finds `.../QuestUnits/MercPet/`.
std::vector<std::string> archive_paths_under(const std::string& dir, const char* suffix) {
    ensure_archive_index();
    const std::string prefix = lowered(dir);
    std::vector<std::string> out;
    for (const auto& entry : g_archive_index) {
        for (const Ogre::FileInfo& info : entry.second) {
            if (!ends_with_lower(info.basename, suffix)) continue;
            const std::string path = lowered(info.path);
            if (path.size() < prefix.size()) continue;
            if (path.compare(0, prefix.size(), prefix) != 0) continue;
            out.push_back(info.path + info.basename);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// Every archive path under `dir` whose basename ends in `suffix`, in the
/// archive's own order: `CFileSystem::getFilesInDirectory`
/// in pak mode asks
/// `ResourceGroupManager::findResourceFileInfo`, which walks each archive's
/// entries as the zip lists them, and keeps that order.
std::vector<std::string> archive_paths_in_listing_order(const std::string& dir,
                                                        const char* suffix) {
    const std::string prefix = lowered(dir);
    std::vector<std::string> out;
    Ogre::FileInfoListPtr all =
        Ogre::ResourceGroupManager::getSingleton().listResourceFileInfo(kGameGroup, false);
    if (!all) return out;
    for (const Ogre::FileInfo& info : *all) {
        if (!ends_with_lower(info.basename, suffix)) continue;
        const std::string path = lowered(info.path);
        if (path.size() < prefix.size()) continue;
        if (path.compare(0, prefix.size(), prefix) != 0) continue;
        out.push_back(info.path + info.basename);
    }
    return out;
}

// `CSpawnClassParser::CSpawnClassParser`
// starts both its maps empty and runs `CSpawnClassParser::reloadFromDirectory`
// at once:
// every file of `media/SpawnClasses/` (`CFileSystem::getFileList`) through
// `CSpawnClassParser::createEmptySpawnClass`
// first, then `fillEmptySpawnClass` on each. The first pass files a class
// under its `NAME`, upper-cased - not under its file's name: 39 shipped files
// are spelt `...SPWAN...` and name themselves `...SPAWN...`, which is how the
// random dungeons' monster classes and the general merchant's stock ask for
// them. A file with an empty `NAME`, or `DONOTSPAWN`, makes none; a name
// already filed keeps its first file. `CSpawnClassParser::getSpawnClass` is
// then a lookup by the upper-cased name. The port parses a class's objects
// the first time it is asked for, from the file this finds.
std::string spawn_class_path(const std::string& name) {
    if (!g_spawn_classes_indexed) {
        g_spawn_classes_indexed = true;
        for (const std::string& path :
             archive_paths_in_listing_order("media/spawnclasses/", ".dat.adm")) {
            const std::vector<uint8_t> bytes = read_resource(path);
            if (bytes.empty()) continue;
            const tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
            if (!file.ok) continue;
            const std::string key = tl::core::upper_ascii(file.root.string_or("NAME", ""));
            if (key.empty() || file.root.bool_or("DONOTSPAWN", false)) continue;
            g_spawn_class_paths.emplace(key, path);
        }
        TL_LOG_INFO("spawn classes: %zu by name", g_spawn_class_paths.size());
    }
    const auto it = g_spawn_class_paths.find(tl::core::upper_ascii(name));
    return it == g_spawn_class_paths.end() ? std::string() : it->second;
}

// PORT: asset lookup. Fill g_material_scripts: every script in the archive
// read once, its declared names filed, and nothing kept of the text.
void index_material_scripts() {
    g_material_scripts_indexed = true;
    ensure_archive_index();
    Ogre::Timer timer;
    size_t scripts = 0;
    auto& groups = Ogre::ResourceGroupManager::getSingleton();
    for (const auto& [dir, list] : g_materials_by_dir) {
        for (const std::string& script : list) {
            Ogre::DataStreamPtr stream = groups.openResource(script, kGameGroup);
            if (!stream) continue;
            for (const std::string& name : declared_materials(stream->getAsString())) {
                g_material_scripts.emplace(lowered(name), script);
            }
            ++scripts;
        }
    }
    TL_LOG_INFO("material index: %zu material(s) in %zu script(s), %llu ms",
                g_material_scripts.size(), scripts,
                static_cast<unsigned long long>(timer.getMilliseconds()));
}

// PORT: asset lookup. The original parses every material script in the
// archive when it starts, so a material is found wherever it is declared; the
// port parses a mesh's own directory (parse_sibling_material), because
// parsing the whole archive aborts on its duplicate names. A name declared
// only elsewhere - the palace pillar's `palace_tileset/palace_stairs02_mat`,
// in `media/models/palace_secret_door/palace.material` in both releases -
// left the mesh without one. This parses the script that declares it. The
// scripts are read once, on the first such name, and not kept.
void parse_material_declaring(const std::string& material) {
    if (!g_material_scripts_indexed) index_material_scripts();
    const auto it = g_material_scripts.find(lowered(material));
    if (it == g_material_scripts.end()) return;
    TL_LOG_INFO("material '%s': declared only in %s", material.c_str(), it->second.c_str());
    parse_material_script(it->second);
}

/// The directory a resource sits in inside the archive, with a trailing slash.
/// Empty if it cannot be located or the basename is ambiguous.
std::string archive_dir_of(const std::string& resource_name) {
    const std::vector<Ogre::FileInfo>* found = archive_entries(resource_name);
    if (!found || found->empty()) return {};

    if (found->size() > 1) {
        TL_LOG_WARN("'%s' is ambiguous: %zu copies in the archive, using %s", resource_name.c_str(),
                    found->size(), found->front().path.c_str());
    }
    return found->front().path;
}

/// The archive path of `basename` inside `dir`, cased as the archive cases it.
///
/// The same qualification rule as qualify_texture_paths(), for a different kind
/// of reference: a bare basename that occurs in more than one directory cannot
/// be opened at all, and Idle.SKELETON occurs in 145 of them. Matching is
/// case-insensitive, and the returned path carries the archive's own spelling
/// so that ZipArchive::exists(), which compares the basename exactly, agrees.
///
/// The directory is matched case-insensitively too: the Xbox 360 pak's own data
/// spells its paths in lower case (`media/levelsets/town1/`) where its archive
/// keeps `media/levelSets/town1/`, so an exact match qualified nothing that a
/// lower-cased path referred to - the town's birches, the mine's cobwebs and
/// rails drew blank. The PC data spells them as its archive does.
std::string archive_path_in_dir(const std::string& basename, const std::string& dir) {
    const std::vector<Ogre::FileInfo>* found = archive_entries(basename);
    if (!found) return {};

    const auto same_dir = [&dir](const std::string& path) {
        return path.size() == dir.size() &&
               std::equal(dir.begin(), dir.end(), path.begin(), [](char a, char b) {
                   return std::tolower(static_cast<unsigned char>(a)) ==
                          std::tolower(static_cast<unsigned char>(b));
               });
    };
    for (const Ogre::FileInfo& info : *found) {
        if (info.path == dir) return info.path + info.basename;
    }
    for (const Ogre::FileInfo& info : *found) {
        if (same_dir(info.path)) return info.path + info.basename;
    }
    return {};
}

std::string archive_path_unique(const std::string& basename) {
    const std::vector<Ogre::FileInfo>* found = archive_entries(basename);
    if (!found || found->size() != 1) return {};
    return found->front().path + found->front().basename;
}

// PORT: asset lookup. The copy of a `basename` the archive holds more than once
// that is nearest the referring directory `dir`: one in a directory above it,
// the closest first; else the one whose path shares the most leading
// directories with it, two at least (`media/` alone says nothing); archive
// order breaks a tie. Empty when none qualifies. The Xbox 360 pak keeps some
// textures a material names only beside its parent (the sunken temple's
// `stemple_broadleaf_*/` water, `wave.dds` one directory up) or in another
// set's tree (`Destroyer_Chain_shoulder.dds` under `wardrobe/` and
// `models/armor/`, where the PC pak has one `.png`), and a bare basename that
// is not unique opens nothing (ADR 0002).
std::string archive_path_nearest(const std::string& basename, const std::string& dir) {
    const std::vector<Ogre::FileInfo>* found = archive_entries(basename);
    if (found == nullptr || found->empty()) return {};
    const std::string want = lowered(dir);
    const Ogre::FileInfo* best = nullptr;
    size_t best_score = 0;
    for (const Ogre::FileInfo& info : *found) {
        const std::string path = lowered(info.path);
        size_t shared = 0;  // whole leading directories in common
        for (size_t i = 0; i < path.size() && i < want.size() && path[i] == want[i]; ++i) {
            if (path[i] == '/') ++shared;
        }
        const bool above = want.compare(0, path.size(), path) == 0;
        const size_t score = above ? 1000 + shared : (shared >= 2 ? shared : 0);
        if (score > best_score) {
            best_score = score;
            best = &info;
        }
    }
    return best == nullptr ? std::string() : best->path + best->basename;
}

// PORT: asset lookup. resolve_texture()'s last resort: `name`'s basename, under
// its own extension or any other, at the copy nearest `dir`
// (archive_path_nearest).
std::string nearest_texture(const std::string& name, const std::string& dir) {
    const size_t slash = name.find_last_of("/\\");
    const std::string bare = slash == std::string::npos ? name : name.substr(slash + 1);
    const std::string stem = bare.substr(0, bare.find_last_of('.'));
    for (const std::string& candidate :
         {bare, stem + ".dds", stem + ".png", stem + ".tga", stem + ".jpg"}) {
        std::string path = archive_path_nearest(candidate, dir);
        if (!path.empty()) return path;
    }
    return {};
}

std::string resolve_texture(const std::string& name, const std::string& dir,
                            const std::string& fallback_dir) {
    auto try_name = [&](const std::string& candidate) -> std::string {
        std::string path = archive_path_in_dir(candidate, dir);
        if (path.empty() && !fallback_dir.empty() && fallback_dir != dir) {
            path = archive_path_in_dir(candidate, fallback_dir);
        }
        if (path.empty()) path = archive_path_unique(candidate);
        return path;
    };

    // A reference carrying path separators is a path, not a basename. Collapse
    // it against the referring material's directory and look it up whole; the
    // archive's own spelling is matched case-insensitively, which matters here
    // because the town's river asks for `WATER_Base.dds` and the file ships as
    // `WATER_base.dds`.
    if (const std::string relative = tl::core::AssetIndex::resolve_relative(dir, name);
        !relative.empty()) {
        std::string path = resolve_archive_path(relative);
        if (!path.empty()) return path;

        // The reference's own extension does not always match the archive's -
        // `mine_lantern01.png` ships as `mine_lantern01.dds`, which is exactly
        // this function's reason for existing - but until now that mismatch
        // was only ever tried against a bare basename, never against a
        // genuinely relative reference: `mine_elevator/mine_eievator01.mesh`
        // asks for `../mine_eievator01.png`, one directory up from where the
        // mesh sits, and the archive's copy is `.dds`. Collapsing already
        // found the right *directory*; only the extension is still a guess,
        // so the guess is retried here rather than only at the bare-basename
        // fallback below, which a `../`-prefixed name never reaches cleanly.
        const size_t dot = relative.find_last_of('.');
        if (dot != std::string::npos) {
            const std::string stem = relative.substr(0, dot);
            for (const char* ext : {".dds", ".png", ".tga", ".jpg"}) {
                path = resolve_archive_path(stem + ext);
                if (!path.empty()) return path;
            }
        }
    }

    std::string path = try_name(name);
    if (!path.empty()) return path;

    // Fall back to the basename alone. A reference with separators that did not
    // resolve as a path may still name a file that exists somewhere else, which
    // is the same forgiving behaviour every other reference here gets.
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
        path = try_name(name.substr(slash + 1));
        if (!path.empty()) return path;
    }

    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};
    const std::string stem = name.substr(0, dot);

    for (const char* ext : {".dds", ".png", ".tga", ".jpg"}) {
        const std::string candidate = stem + ext;
        if (candidate.size() == name.size() &&
            std::equal(candidate.begin(), candidate.end(), name.begin(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            })) {
            continue;  // already tried
        }
        path = try_name(candidate);
        if (!path.empty()) return path;
    }

    // Last, a name the archive holds more than once and nowhere beside the
    // material: the copy nearest it.
    return nearest_texture(name, dir);
}

}  // namespace tl::render::renderer_detail
