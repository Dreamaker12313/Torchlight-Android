#include "core/asset_path.h"

#include <cctype>
#include <filesystem>
#include <system_error>

namespace tl::core {
namespace {

std::string basename_of(const std::string& normalised) {
    const size_t slash = normalised.find_last_of('/');
    return (slash == std::string::npos) ? normalised : normalised.substr(slash + 1);
}

std::string dirname_of(const std::string& normalised) {
    const size_t slash = normalised.find_last_of('/');
    return (slash == std::string::npos) ? std::string() : normalised.substr(0, slash);
}

}  // namespace

std::string AssetIndex::normalise(std::string s) {
    for (char& c : s) {
        if (c == '\\') c = '/';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool AssetIndex::build(const std::string& root) {
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::is_directory(root, ec) || ec) return false;

    by_path_.clear();
    by_basename_.clear();
    ambiguous_.clear();

    const fs::path base(root);
    fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec);
    if (ec) return false;

    for (const auto& entry : it) {
        std::error_code file_ec;
        if (!entry.is_regular_file(file_ec) || file_ec) continue;

        const std::string real = fs::relative(entry.path(), base, file_ec).generic_string();
        if (file_ec || real.empty()) continue;

        const std::string folded = normalise(real);
        by_path_.emplace(folded, real);

        const std::string base_name = basename_of(folded);
        auto [pos, inserted] = by_basename_.emplace(base_name, real);
        if (!inserted && pos->second != real) {
            // Same filename in more than one directory: resolving it without a
            // directory hint has to pick one. Recorded so callers can warn.
            ambiguous_.push_back(real);
        }
    }
    return true;
}

std::string AssetIndex::resolve(const std::string& requested) const {
    const std::string key = normalise(requested);

    if (auto it = by_path_.find(key); it != by_path_.end()) {
        return it->second;
    }
    if (auto it = by_basename_.find(basename_of(key)); it != by_basename_.end()) {
        return it->second;
    }
    return {};
}

std::string AssetIndex::resolve_near(const std::string& requested,
                                     const std::string& relative_to) const {
    const std::string key = normalise(requested);

    // A sibling of the referring asset wins over anything else.
    const std::string dir = dirname_of(normalise(relative_to));
    if (!dir.empty()) {
        const std::string sibling = dir + "/" + basename_of(key);
        if (auto it = by_path_.find(sibling); it != by_path_.end()) {
            return it->second;
        }
    }
    return resolve(requested);
}

std::string AssetIndex::resolve_relative(const std::string& dir, const std::string& name) {
    if (name.find('/') == std::string::npos && name.find('\\') == std::string::npos) return {};

    std::vector<std::string> parts;
    const auto push_all = [&parts](const std::string& text) {
        std::string part;
        const auto flush = [&]() {
            if (part == "..") {
                if (!parts.empty()) parts.pop_back();
            } else if (!part.empty() && part != ".") {
                parts.push_back(part);
            }
            part.clear();
        };
        for (const char c : text) {
            if (c == '/' || c == '\\') {
                flush();
            } else {
                part.push_back(c);
            }
        }
        flush();
    };

    push_all(dir);
    push_all(name);
    if (parts.empty()) return {};

    std::string out = parts.front();
    for (size_t i = 1; i < parts.size(); ++i) {
        out += '/';
        out += parts[i];
    }
    return out;
}

}  // namespace tl::core
