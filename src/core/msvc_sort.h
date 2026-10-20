// The console's `std::sort`: the Xbox 360 build's C++ library (Dinkumware, as
// in Visual Studio 2008), which orders equal elements differently from
// libstdc++'s. Where the port sorts what the console sorts with `std::sort`,
// and equal keys can meet, it sorts with this. `std::_Sort`,
// `std::_Unguarded_partition`, `std::_Med3`, the
// insertion sort.
#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

namespace tl::core {

namespace msvc_detail {

template <class It, class Pred>
void med3(It first, It mid, It last, Pred pred) {
    if (pred(*mid, *first)) std::iter_swap(mid, first);
    if (pred(*last, *mid)) std::iter_swap(last, mid);
    if (pred(*mid, *first)) std::iter_swap(mid, first);
}

template <class It, class Pred>
void median(It first, It mid, It last, Pred pred) {
    if (40 < last - first) {
        const auto step = (last - first + 1) / 8;
        med3(first, first + step, first + 2 * step, pred);
        med3(mid - step, mid, mid + step, pred);
        med3(last - 2 * step, last - step, last, pred);
        med3(first + step, mid, last - step, pred);
    } else {
        med3(first, mid, last, pred);
    }
}

template <class It, class Pred>
std::pair<It, It> unguarded_partition(It first, It last, Pred pred) {
    It mid = first + (last - first) / 2;
    median(first, mid, last - 1, pred);
    It pfirst = mid;
    It plast = pfirst + 1;

    while (first < pfirst && !pred(*(pfirst - 1), *pfirst) && !pred(*pfirst, *(pfirst - 1)))
        --pfirst;
    while (plast < last && !pred(*plast, *pfirst) && !pred(*pfirst, *plast)) ++plast;

    It gfirst = plast;
    It glast = pfirst;
    for (;;) {
        for (; gfirst < last; ++gfirst) {
            if (pred(*pfirst, *gfirst)) {
            } else if (pred(*gfirst, *pfirst)) {
                break;
            } else {
                std::iter_swap(plast++, gfirst);
            }
        }
        for (; first < glast; --glast) {
            if (pred(*(glast - 1), *pfirst)) {
            } else if (pred(*pfirst, *(glast - 1))) {
                break;
            } else {
                std::iter_swap(--pfirst, glast - 1);
            }
        }
        if (glast == first && gfirst == last) return {pfirst, plast};

        if (glast == first) {
            // No room at the bottom: the pivot run moves up.
            if (plast != gfirst) std::iter_swap(pfirst, plast);
            ++plast;
            std::iter_swap(pfirst++, gfirst++);
        } else if (gfirst == last) {
            // No room at the top: the pivot run moves down.
            if (--glast != --pfirst) std::iter_swap(glast, pfirst);
            std::iter_swap(pfirst, --plast);
        } else {
            std::iter_swap(gfirst++, --glast);
        }
    }
}

template <class It, class Pred>
void insertion_sort(It first, It last, Pred pred) {
    if (first == last) return;
    for (It next = first; ++next != last;) {
        It next1 = next;
        auto value = *next;
        if (pred(value, *first)) {
            std::move_backward(first, next, ++next1);
            *first = value;
        } else {
            for (It first1 = next1; pred(value, *--first1); next1 = first1) *next1 = *first1;
            *next1 = value;
        }
    }
}

template <class It, class Diff, class T, class Pred>
void push_heap(It first, Diff hole, Diff top, T value, Pred pred) {
    for (Diff idx = (hole - 1) / 2; top < hole && pred(*(first + idx), value);
         idx = (hole - 1) / 2) {
        *(first + hole) = *(first + idx);
        hole = idx;
    }
    *(first + hole) = value;
}

template <class It, class Diff, class T, class Pred>
void adjust_heap(It first, Diff hole, Diff bottom, T value, Pred pred) {
    const Diff top = hole;
    Diff idx = 2 * hole + 2;
    for (; idx < bottom; idx = 2 * idx + 2) {
        if (pred(*(first + idx), *(first + (idx - 1)))) --idx;
        *(first + hole) = *(first + idx);
        hole = idx;
    }
    if (idx == bottom) {
        *(first + hole) = *(first + (bottom - 1));
        hole = bottom - 1;
    }
    push_heap(first, hole, top, value, pred);
}

template <class It, class Pred>
void heap_sort(It first, It last, Pred pred) {
    using Diff = typename std::iterator_traits<It>::difference_type;
    const Diff bottom = last - first;
    for (Diff hole = bottom / 2; 0 < hole;) {
        --hole;
        adjust_heap(first, hole, bottom, *(first + hole), pred);
    }
    for (; 1 < last - first; --last) {
        auto value = *(last - 1);
        *(last - 1) = *first;
        adjust_heap(first, Diff(0), Diff(last - 1 - first), value, pred);
    }
}

}  // namespace msvc_detail

/// `std::_Sort(first, last, ideal, pred)`: what `std::sort` runs with `ideal`
/// the element count. Exposed with the budget so a test can reach the heap
/// sort the budget's end falls back on.
template <class It, class Pred>
void msvc_sort_budget(It first, It last, typename std::iterator_traits<It>::difference_type ideal,
                      Pred pred) {
    typename std::iterator_traits<It>::difference_type count;
    for (; 32 < (count = last - first) && 0 < ideal;) {
        const std::pair<It, It> mid = msvc_detail::unguarded_partition(first, last, pred);
        ideal /= 2;
        ideal += ideal / 2;
        if (mid.first - first < last - mid.second) {
            msvc_sort_budget(first, mid.first, ideal, pred);
            first = mid.second;
        } else {
            msvc_sort_budget(mid.second, last, ideal, pred);
            last = mid.first;
        }
    }
    if (32 < count) {
        msvc_detail::heap_sort(first, last, pred);
    } else if (1 < count) {
        msvc_detail::insertion_sort(first, last, pred);
    }
}

/// `std::sort(first, last, pred)` as the console's library does it.
template <class It, class Pred>
void msvc_sort(It first, It last, Pred pred) {
    msvc_sort_budget(first, last, last - first, pred);
}

}  // namespace tl::core
