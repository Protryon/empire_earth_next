use std::sync::Mutex as StdMutex;

use super::*;
use crate::gfx::effect_reads::tests::{register, FGeffect, FGobj};

#[test]
fn media_are_filed_by_kind_in_table_order() {
    let filed = file_by_kind(&[
        (10, Some(3)),
        (0, None),
        (11, Some(0x48)),
        (12, Some(3)),
        (13, Some(0)),
        (14, Some(c_ulong::MAX)),
    ]);
    assert_eq!(filed.len(), EE_GFX_GEFFECT_KINDS);
    assert_eq!(filed[3], vec![10, 12]);
    assert_eq!(filed[0], vec![13]);
    assert_eq!(filed.iter().map(Vec::len).sum::<usize>(), 3);
}

static CALLS: StdMutex<Vec<(bool, usize)>> = StdMutex::new(Vec::new());

extern "C" fn fake_load(media: *mut c_void) {
    CALLS.lock().unwrap().push((true, media as usize));
}

extern "C" fn fake_unload(media: *mut c_void) {
    CALLS.lock().unwrap().push((false, media as usize));
}

const HOST: EeGfxGeffectHost = EeGfxGeffectHost { media_load: fake_load, media_unload: fake_unload };

fn effect(kind: c_long, id: c_ulong, object: usize, linked: [usize; 3]) -> Box<FGeffect> {
    let mut e = Box::new(FGeffect { object, linked, ..Default::default() });
    e.fields.mKind = kind;
    e.fields.mID = id;
    e
}

fn addr<T>(b: &T) -> usize {
    b as *const T as usize
}

// one test for the process-wide tables (the tests of a binary run in parallel)
#[test]
fn the_tables_and_the_loads() {
    register();
    // two graphics of kind 5 (one loaded), one of kind 6, an empty slot
    let loaded = Box::new(FGobj { media_use: EeDataMediaUse { mMediaCount: 1, mIsLoading: 0 }, ..Default::default() });
    let mut kind5 = Box::new(FGobj::default());
    kind5.fields.mKind = 5;
    let mut kind5b = Box::new(FGobj::default());
    kind5b.fields.mKind = 5;
    let mut kind6 = Box::new(FGobj::default());
    kind6.fields.mKind = 6;
    let media = [
        addr(&*kind5) as *mut c_void,
        std::ptr::null_mut(),
        addr(&*kind6) as *mut c_void,
        addr(&*kind5b) as *mut c_void,
    ];
    // SAFETY: four entries, each NULL or a fake graphic object
    unsafe { ee_gfx_geffect_sort_media(media.as_ptr(), media.len()) };
    assert_eq!(ee_gfx_geffect_kind_count(5), 2);
    assert_eq!(ee_gfx_geffect_kind_at(5, 1) as usize, addr(&*kind5b));
    assert_eq!(ee_gfx_geffect_kind_at(5, 2), std::ptr::null_mut());
    assert_eq!(ee_gfx_geffect_kind_at(5, -1), std::ptr::null_mut());
    assert_eq!(ee_gfx_geffect_kind_count(-3), 0);
    assert_eq!(ee_gfx_geffect_kind_count(0x48), 0);

    // a kind-only follow-on, a graphic not yet loaded, a loaded one
    let follow = effect(5, 30, 0, [0; 3]);
    let unloaded = Box::new(FGobj::default());
    let first = effect(1, 20, addr(&*unloaded), [addr(&*follow), 0, 0]);
    let second = effect(1, 21, addr(&*loaded), [0; 3]);
    let table = [addr(&*first) as *mut c_void, std::ptr::null_mut(), addr(&*second) as *mut c_void];
    // SAFETY: three entries
    unsafe { ee_gfx_geffect_table_set(table.as_ptr(), table.len()) };
    assert_eq!(ee_gfx_geffect_count(), 3);
    assert_eq!(ee_gfx_geffect_get(2) as usize, addr(&*second));
    assert_eq!(ee_gfx_geffect_get(3), std::ptr::null_mut());
    assert_eq!(ee_gfx_geffect_index_of(addr(&*second) as *const c_void), 2);
    assert_eq!(ee_gfx_geffect_index_of(addr(&*follow) as *const c_void), 3);

    CALLS.lock().unwrap().clear();
    // SAFETY: fake effects of the test layout
    unsafe {
        ee_gfx_geffect_load(addr(&*first) as *mut c_void, true, &HOST);
        ee_gfx_geffect_load(addr(&*second) as *mut c_void, true, &HOST);
        ee_gfx_geffect_unload(addr(&*first) as *mut c_void, &HOST);
    }
    assert_eq!(
        *CALLS.lock().unwrap(),
        vec![
            (true, addr(&*unloaded)),
            (true, addr(&*kind5)),
            (true, addr(&*kind5b)),
            (true, addr(&*loaded)),
            (false, addr(&*unloaded)),
            (false, addr(&*kind5)),
            (false, addr(&*kind5b)),
        ]
    );
    assert_eq!((first.num_loads, follow.num_loads, second.num_loads), (0, 0, 1));
    // only the first effect's graphic was not loaded when used on demand
    let mut ids = [0 as c_ulong; 4];
    // SAFETY: room for four
    let n = unsafe { ee_gfx_geffect_ids(ids.as_mut_ptr(), ids.len()) };
    assert_eq!((n, ids[0]), (1, 20));
    // SAFETY: the clear
    unsafe { ee_gfx_geffect_table_set(std::ptr::null(), 0) };
    assert_eq!(ee_gfx_geffect_count(), 0);
}
