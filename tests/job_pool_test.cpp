// The fork-join pool: every index of a range run exactly once, on any number of workers.
#include "core/job_pool.h"
#include "test_support.h"

#include <atomic>
#include <vector>

namespace {

/// Runs `count` indices in slices of `grain` on `pool` and checks each was run once.
bool runs_each_once(tl::core::JobPool& pool, size_t count, size_t grain) {
    std::vector<std::atomic<int>> seen(count);
    pool.parallel_for(count, grain, [&seen](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) seen[i].fetch_add(1);
    });
    for (const auto& n : seen) {
        if (n.load() != 1) return false;
    }
    return true;
}

}  // namespace

TL_TEST(a_pool_with_no_workers_runs_every_index_on_the_caller) {
    tl::core::JobPool pool(0);
    TL_CHECK_EQ(pool.workers(), 0u);
    TL_CHECK(runs_each_once(pool, 1000, 7));
    TL_CHECK(runs_each_once(pool, 0, 4));
}

TL_TEST(a_pool_runs_every_index_once_whatever_its_workers_and_grain) {
    for (unsigned workers : {1u, 2u, 3u}) {
        tl::core::JobPool pool(workers);
        for (size_t count :
             {size_t{1}, size_t{3}, size_t{4}, size_t{5}, size_t{150}, size_t{4097}}) {
            for (size_t grain : {size_t{1}, size_t{4}, size_t{64}}) {
                TL_CHECK(runs_each_once(pool, count, grain));
            }
        }
    }
}

TL_TEST(a_pool_takes_round_after_round_without_losing_one) {
    tl::core::JobPool pool(2);
    std::atomic<size_t> total{0};
    for (int round = 0; round < 2000; ++round) {
        pool.parallel_for(37, 2, [&total](size_t begin, size_t end) { total += end - begin; });
    }
    TL_CHECK_EQ(total.load(), size_t{37} * 2000);
}
