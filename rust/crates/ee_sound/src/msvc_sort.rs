//! `std::sort` as the MSVC STL (14.44, the toolchain the C++ game was built with) implements it: an introsort with
//! insertion sort up to 32 elements, Tukey's ninther above 40, a fat partition and a heap sort after 1.5 log2(N)
//! divisions. It is not stable, so the order of equivalent elements is part of its behaviour: the scavenger sorts its
//! paused queue with it, and two instances of equal priority and volume must come out as they did in C++
//! (`golden_sort` in `golden_sound.txt` holds the C++ permutations).

const ISORT_MAX: usize = 32;

/// Sorts `v` with `less` (a strict weak order) exactly as MSVC's `std::sort(first, last, pred)`.
pub fn sort_by<T: Copy, F: FnMut(&T, &T) -> bool>(v: &mut [T], mut less: F) {
    let n = v.len();
    sort_unchecked(v, 0, n, n as isize, &mut less);
}

/// `std::_Sort_unchecked(first, last, ideal, pred)`; `ideal` is the remaining division budget.
pub fn sort_unchecked<T: Copy, F: FnMut(&T, &T) -> bool>(
    v: &mut [T],
    mut first: usize,
    mut last: usize,
    mut ideal: isize,
    less: &mut F,
) {
    loop {
        if last - first <= ISORT_MAX {
            insertion_sort(v, first, last, less);
            return;
        }
        if ideal <= 0 {
            make_heap(&mut v[first..last], less);
            sort_heap(&mut v[first..last], less);
            return;
        }
        let (mid_first, mid_last) = partition_by_median_guess(v, first, last, less);
        ideal = (ideal >> 1) + (ideal >> 2);
        if mid_first - first < last - mid_last {
            sort_unchecked(v, first, mid_first, ideal, less);
            first = mid_last;
        } else {
            sort_unchecked(v, mid_last, last, ideal, less);
            last = mid_first;
        }
    }
}

fn insertion_sort<T: Copy, F: FnMut(&T, &T) -> bool>(v: &mut [T], first: usize, last: usize, less: &mut F) {
    if first == last {
        return;
    }
    let mut mid = first;
    loop {
        mid += 1;
        if mid == last {
            break;
        }
        let val = v[mid];
        if less(&val, &v[first]) {
            v.copy_within(first..mid, first + 1);
            v[first] = val;
        } else {
            let mut hole = mid;
            let mut prev = hole;
            loop {
                prev -= 1;
                if !less(&val, &v[prev]) {
                    break;
                }
                v[hole] = v[prev];
                hole = prev;
            }
            v[hole] = val;
        }
    }
}

fn med3<T: Copy, F: FnMut(&T, &T) -> bool>(v: &mut [T], first: usize, mid: usize, last: usize, less: &mut F) {
    if less(&v[mid], &v[first]) {
        v.swap(mid, first);
    }
    if less(&v[last], &v[mid]) {
        v.swap(last, mid);
        if less(&v[mid], &v[first]) {
            v.swap(mid, first);
        }
    }
}

fn guess_median<T: Copy, F: FnMut(&T, &T) -> bool>(v: &mut [T], first: usize, mid: usize, last: usize, less: &mut F) {
    let count = last - first;
    if 40 < count {
        let step = (count + 1) >> 3;
        let two_step = step << 1;
        med3(v, first, first + step, first + two_step, less);
        med3(v, mid - step, mid, mid + step, less);
        med3(v, last - two_step, last - step, last, less);
        med3(v, first + step, mid, last - step, less);
    } else {
        med3(v, first, mid, last, less);
    }
}

fn partition_by_median_guess<T: Copy, F: FnMut(&T, &T) -> bool>(
    v: &mut [T],
    first: usize,
    last: usize,
    less: &mut F,
) -> (usize, usize) {
    let mid = first + ((last - first) >> 1);
    guess_median(v, first, mid, last - 1, less);
    let mut pfirst = mid;
    let mut plast = pfirst + 1;
    while first < pfirst && !less(&v[pfirst - 1], &v[pfirst]) && !less(&v[pfirst], &v[pfirst - 1]) {
        pfirst -= 1;
    }
    while plast < last && !less(&v[plast], &v[pfirst]) && !less(&v[pfirst], &v[plast]) {
        plast += 1;
    }
    let mut gfirst = plast;
    let mut glast = pfirst;
    loop {
        while gfirst < last {
            if less(&v[pfirst], &v[gfirst]) {
            } else if less(&v[gfirst], &v[pfirst]) {
                break;
            } else if plast != gfirst {
                v.swap(plast, gfirst);
                plast += 1;
            } else {
                plast += 1;
            }
            gfirst += 1;
        }
        while first < glast {
            let glast_prev = glast - 1;
            if less(&v[glast_prev], &v[pfirst]) {
            } else if less(&v[pfirst], &v[glast_prev]) {
                break;
            } else {
                pfirst -= 1;
                if pfirst != glast_prev {
                    v.swap(pfirst, glast_prev);
                }
            }
            glast -= 1;
        }
        if glast == first && gfirst == last {
            return (pfirst, plast);
        }
        if glast == first {
            if plast != gfirst {
                v.swap(pfirst, plast);
            }
            plast += 1;
            v.swap(pfirst, gfirst);
            pfirst += 1;
            gfirst += 1;
        } else if gfirst == last {
            glast -= 1;
            pfirst -= 1;
            if glast != pfirst {
                v.swap(glast, pfirst);
            }
            plast -= 1;
            v.swap(pfirst, plast);
        } else {
            glast -= 1;
            v.swap(gfirst, glast);
            gfirst += 1;
        }
    }
}

fn push_heap_by_index<T: Copy, F: FnMut(&T, &T) -> bool>(
    s: &mut [T],
    mut hole: usize,
    top: usize,
    val: T,
    less: &mut F,
) {
    while top < hole {
        let idx = (hole - 1) >> 1;
        if !less(&s[idx], &val) {
            break;
        }
        s[hole] = s[idx];
        hole = idx;
    }
    s[hole] = val;
}

fn pop_heap_hole_by_index<T: Copy, F: FnMut(&T, &T) -> bool>(
    s: &mut [T],
    mut hole: usize,
    bottom: usize,
    val: T,
    less: &mut F,
) {
    let top = hole;
    let mut idx = hole;
    let max_non_leaf = (bottom - 1) >> 1;
    while idx < max_non_leaf {
        idx = 2 * idx + 2;
        if less(&s[idx], &s[idx - 1]) {
            idx -= 1;
        }
        s[hole] = s[idx];
        hole = idx;
    }
    if idx == max_non_leaf && bottom.is_multiple_of(2) {
        s[hole] = s[bottom - 1];
        hole = bottom - 1;
    }
    push_heap_by_index(s, hole, top, val, less);
}

fn make_heap<T: Copy, F: FnMut(&T, &T) -> bool>(s: &mut [T], less: &mut F) {
    let bottom = s.len();
    let mut hole = bottom >> 1;
    while hole > 0 {
        hole -= 1;
        let val = s[hole];
        pop_heap_hole_by_index(s, hole, bottom, val, less);
    }
}

fn sort_heap<T: Copy, F: FnMut(&T, &T) -> bool>(s: &mut [T], less: &mut F) {
    let mut last = s.len();
    while last >= 2 {
        last -= 1;
        let val = s[last];
        s[last] = s[0];
        pop_heap_hole_by_index(s, 0, last, val, less);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The golden inputs: `count` (key, tag) pairs, keys from the engine's LCG in `0..keys` (many ties), tags the
    /// original positions. The C++ driver builds the same pairs.
    pub(crate) fn input(count: usize, keys: u32, seed: u32) -> Vec<(u32, u32)> {
        let mut r = ee_random::URandom::new(seed);
        (0..count as u32).map(|i| (r.next_in_series() % keys, i)).collect()
    }

    fn run(line: &str) -> String {
        let f: Vec<&str> = line.split(' ').collect();
        let (count, keys, seed, ideal) = (f[0].parse().unwrap(), f[1].parse().unwrap(), f[2].parse().unwrap(), f[3]);
        let mut v = input(count, keys, seed);
        let mut less = |a: &(u32, u32), b: &(u32, u32)| a.0 < b.0;
        if ideal == "n" {
            sort_by(&mut v, less);
        } else {
            sort_unchecked(&mut v, 0, count, ideal.parse().unwrap(), &mut less);
        }
        v.iter().map(|p| p.1.to_string()).collect::<Vec<_>>().join(",")
    }

    #[test]
    fn permutations_equal_the_msvc_std_sort() {
        let golden = include_str!("golden_sound.txt");
        let mut n = 0;
        for l in golden.lines().filter(|l| l.starts_with("sort\t")) {
            let f: Vec<&str> = l.split('\t').collect();
            assert_eq!(run(f[1]), f[2], "sort {}", f[1]);
            n += 1;
        }
        assert!(n >= 20, "only {n} sort cases");
    }

    #[test]
    fn sorts_without_ties_like_any_sort() {
        let mut v: Vec<(u32, u32)> = input(500, u32::MAX, 7);
        let mut want = v.clone();
        want.sort();
        sort_by(&mut v, |a, b| a.0 < b.0);
        assert_eq!(v, want);
    }

    #[test]
    fn heap_path_sorts() {
        let mut v = input(300, 10, 3);
        sort_unchecked(&mut v, 0, 300, 0, &mut |a: &(u32, u32), b: &(u32, u32)| a.0 < b.0);
        assert!(v.windows(2).all(|w| w[0].0 <= w[1].0));
    }

    #[test]
    fn empty_and_single() {
        let mut e: [(u32, u32); 0] = [];
        sort_by(&mut e, |a, b| a.0 < b.0);
        let mut one = [(1u32, 0u32)];
        sort_by(&mut one, |a, b| a.0 < b.0);
        assert_eq!(one, [(1, 0)]);
    }
}
