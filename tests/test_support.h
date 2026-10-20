// A deliberately tiny test harness. No external dependency, so tests build and
// run anywhere the desktop engine does.
#pragma once

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tl::test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

/// Thrown by a failing check; caught by the runner so one failure does not
/// abort the whole suite.
struct Failure {
    std::string message;
};

/// Signals "the preconditions for this test are absent" (notably no game data): reported as
/// a skip, never a failure, so a build without the game stays green.
struct Skip {
    std::string reason;
};

/// The value of an optional a test expects to hold one, for use inside an
/// expression (`TL_VALUE`): a failure when it is empty, not undefined behaviour.
template <class T>
const T& checked_value(const std::optional<T>& value, const char* file, int line) {
    if (!value.has_value()) {
        throw Failure{std::string(file) + ":" + std::to_string(line) + ": expected a value"};
    }
    return *value;
}

/// Every registered test, or only those whose name contains `filter`; in
/// registration order, or shuffled from `shuffle_seed` when it is not 0.
int run_all(const char* filter = nullptr, unsigned shuffle_seed = 0);

}  // namespace tl::test

#define TL_TEST(name)                                     \
    static void name();                                   \
    static ::tl::test::Registrar reg_##name(#name, name); \
    static void name()

#define TL_CHECK(cond)                                                                         \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            throw ::tl::test::Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                                      ": expected " #cond};                                    \
        }                                                                                      \
    } while (0)

#define TL_CHECK_EQ(a, b)                                                                      \
    do {                                                                                       \
        auto va_ = (a);                                                                        \
        auto vb_ = (b);                                                                        \
        if (!(va_ == vb_)) {                                                                   \
            throw ::tl::test::Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                                      ": " #a " != " #b};                                      \
        }                                                                                      \
    } while (0)

#define TL_VALUE(opt) ::tl::test::checked_value((opt), __FILE__, __LINE__)

#define TL_SKIP(reason) \
    throw ::tl::test::Skip { (reason) }
