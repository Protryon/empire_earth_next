//! `EEMap::mVisibilityWords` as a Rust-owned buffer (the object-storage plan's lane P): the tiles' visibility words,
//! `(players + 1) / 2` a tile, in tile order. The C++ keeps the pointer (`mVisibilityWords`, each tile's
//! `mVisibility` into it), so every reader and the save read them as before; Rust allocates, resizes and frees them.
//! A buffer carries its length in two words before the pointer the C++ holds.

/// The words before a buffer's first (its length, a u64 in two u32s)
const HEADER: usize = 2;

/// A zeroed buffer of `count` words (`new uint32_t[count]` and the memset).
pub fn alloc(count: usize) -> *mut u32 {
    let mut b = vec![0u32; count + HEADER].into_boxed_slice();
    let n = count as u64;
    b[0] = n as u32;
    b[1] = (n >> 32) as u32;
    let p = Box::into_raw(b) as *mut u32;
    // SAFETY: past the header, inside the allocation
    unsafe { p.add(HEADER) }
}

/// The length of a buffer [`alloc`] made.
///
/// # Safety
/// `words` is a live buffer of [`alloc`].
pub unsafe fn len(words: *const u32) -> usize {
    // SAFETY: the header before it
    let (lo, hi) = unsafe { (*words.sub(HEADER), *words.sub(HEADER - 1)) };
    (u64::from(lo) | (u64::from(hi) << 32)) as usize
}

/// Frees a buffer of [`alloc`] (null: nothing).
///
/// # Safety
/// `words` is null or a live buffer of [`alloc`], not used after.
pub unsafe fn free(words: *mut u32) {
    if words.is_null() {
        return;
    }
    // SAFETY: the caller's contract: the allocation starts at the header
    unsafe {
        let n = len(words) + HEADER;
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(words.sub(HEADER), n)));
    }
}

/// `SetPlayerCount`'s copy: a new buffer of `tiles * new_words` words; the n-th tile found (`found`, in the C++'s y
/// then x order) gets the n-th old slice's first `min(old, new)` words and its slice of the new buffer, which
/// `assign(tile, slice)` hands out. The old buffer is not freed here.
///
/// # Safety
/// `old` is null or a live buffer holding `found.len() * old_words` words or more.
pub unsafe fn resize(
    old: *const u32,
    found: &[u64],
    tiles: usize,
    old_words: usize,
    new_words: usize,
    mut assign: impl FnMut(u64, *mut u32),
) -> *mut u32 {
    let words = alloc(tiles * new_words);
    if old.is_null() {
        return words;
    }
    let copy = old_words.min(new_words);
    for (index, &tile) in found.iter().enumerate() {
        // SAFETY: inside both buffers (the caller's contract; the new one holds a slice for every tile counted)
        unsafe {
            std::ptr::copy_nonoverlapping(old.add(index * old_words), words.add(index * new_words), copy);
            assign(tile, words.add(index * new_words));
        }
    }
    words
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_buffer_is_zeroed_and_knows_its_length() {
        let p = alloc(5);
        // SAFETY: the buffer just made
        unsafe {
            assert_eq!(len(p), 5);
            assert!(std::slice::from_raw_parts(p, 5).iter().all(|&w| w == 0));
            free(p);
            free(std::ptr::null_mut());
        }
    }

    #[test]
    fn a_resize_copies_each_found_tiles_words_in_order() {
        // three tiles of two words
        let old = alloc(6);
        // SAFETY: the buffers made here
        unsafe {
            for i in 0..6 {
                *old.add(i) = 10 + i as u32;
            }
            let mut got = Vec::new();
            let new = resize(old, &[7, 8], 3, 2, 1, |t, p| got.push((t, *p)));
            assert_eq!(len(new), 3);
            assert_eq!(got, vec![(7, 10), (8, 12)]);
            let grown = resize(new, &[7, 8, 9], 3, 1, 3, |_, _| {});
            assert_eq!(std::slice::from_raw_parts(grown, 9), &[10, 0, 0, 12, 0, 0, 0, 0, 0]);
            free(old);
            free(new);
            free(grown);
        }
    }
}
