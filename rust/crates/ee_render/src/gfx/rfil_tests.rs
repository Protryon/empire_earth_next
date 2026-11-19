use super::*;

#[test]
fn fnv_and_names() {
    // FNV-1a's published values
    assert_eq!(fnv(b"", FNV_BASIS), 0xcbf2_9ce4_8422_2325);
    assert_eq!(fnv(b"a", FNV_BASIS), 0xaf63_dc4c_8601_ec8c);
    assert_eq!(text32(None), 0);
    assert_eq!(text32(Some(b"a")), 0x8601_ec8c);
}

#[test]
fn pointers_are_named_by_first_use_in_the_record() {
    let mut w = Writer::default();
    assert_eq!((w.order(0x50), w.order(0x40), w.order(0x50), w.order(0)), (1, 2, 1, 0));
    let mut d = Digest::new(&mut w);
    d.pointer(0x40, 7);
    d.pointer(0, 7);
    let mut expect = FNV_BASIS;
    for v in [2u32, 7, 0, 0] {
        expect = fnv(&v.to_le_bytes(), expect);
    }
    assert_eq!(d.hash, expect);
}

#[test]
fn pool_indices() {
    assert_eq!(pool_index(0, 0x1000, 0x10, 4), -1);
    assert_eq!(pool_index(0x1000, 0x1000, 0x10, 4), 0);
    assert_eq!(pool_index(0x1030, 0x1000, 0x10, 4), 3);
    assert_eq!(pool_index(0x1040, 0x1000, 0x10, 4), -2);
    assert_eq!(pool_index(0x0ff0, 0x1000, 0x10, 4), -2);
}

#[test]
fn texture_maps_name_keys_by_their_lowest_entry() {
    // key 0x9 (entries 3, 1) and key 0x5 (entry 2), in the map's (address) order: 0x5 first
    let entries = [(0x5, 0x1020), (0x9, 0x1030), (0x9, 0x1010)];
    let mut w = Writer::default();
    texture_map(&mut w, &entries, 0x1000, 0x10, 4);
    let words: Vec<i32> = w.data.chunks(4).map(|c| i32::from_le_bytes(c.try_into().unwrap())).collect();
    // key 0x9 is named 1 and goes first, its entries in the map's order; then key 0x5 (named 2)
    assert_eq!(words, vec![3, 1, 3, 1, 1, 2, 2]);
}

#[test]
fn a_frame_of_objects_writes_its_keys_pools_and_lists() {
    let shadow = EeGfxFillObject {
        location: [1.0, 2.0, 3.0],
        object: 0,
        model_scale: 1.0,
        selection_size: 0.0,
        scale: 0.5,
        f1c: 0.0,
        health: 1.0,
        opacity: 0.25,
        fraction: 0.75,
        model_index: 2,
        current_frame: 3,
        next_frame: 4,
        is_fogged: true,
        gap: [0; 3],
    };
    let shadows = [shadow];
    let map = [EeGfxRfilPair { key: 0x77 as *const c_void, value: &shadows[0] as *const _ as *const c_void }];
    let selected = [&shadows[0] as *const EeGfxFillObject];
    let ids: [c_ulong; 2] = [5, 9];
    let empty = EeGfxRfilSpan { data: std::ptr::null(), count: 0 };
    let none = EeGfxRfilPairs { data: std::ptr::null(), count: 0 };
    let layout = EeGfxRfilLayout {
        size: std::mem::size_of::<EeGfxRfilLayout>() as u64,
        unit_stride: 0x200,
        ..Default::default()
    };
    let frame = EeGfxRfilFrame {
        layout: &layout,
        frame: 11,
        ticks: 22,
        sway: 33,
        yaw: 1.5,
        world_transformation: GeTransformation::IDENTITY,
        flags: 5,
        picked: -1,
        world_key: 0x1234,
        fighting: 2.0,
        unit_count: 3.0,
        units: std::ptr::null(),
        units_used: 0,
        shadows: shadows.as_ptr(),
        shadows_used: 1,
        maps: [none, none, none, none, none, EeGfxRfilPairs { data: map.as_ptr(), count: 1 }],
        selected_units: empty,
        selected_objects: EeGfxRfilSpan { data: selected.as_ptr() as *const c_void, count: 1 },
        selected_unit: std::ptr::null(),
        selected_object: &shadows[0] as *const _ as *const c_void,
        label_unit: std::ptr::null(),
        label_name: std::ptr::null(),
        picked_ids: EeGfxRfilSpan { data: ids.as_ptr() as *const c_void, count: 2 },
        ghosts: empty,
        lines: empty,
    };
    // SAFETY: the frame's own arrays
    let data = unsafe { record(&frame) };
    let mut w = Writer::default();
    for v in [11u32, 22, 33] {
        w.u32(v);
    }
    w.f32(1.5);
    w.u64(fnv(&transformation_bytes(&GeTransformation::IDENTITY), FNV_BASIS));
    w.u32(5);
    w.i32(-1);
    w.u64(0x1234);
    w.f32(2.0);
    w.f32(3.0);
    w.u32(0);
    w.u32(1);
    w.f32(0.25);
    w.f32(0.75);
    let mut expect = Writer::default();
    let digest = shadow_digest(&mut expect, &shadow);
    w.u64(digest);
    for _ in 0..5 {
        w.u32(0);
    }
    for v in [1, 0, 0] {
        w.i32(v);
    }
    w.u32(0);
    w.u32(1);
    w.i32(0);
    for v in [-1, 0, -1] {
        w.i32(v);
    }
    w.u32(0);
    for v in [2u32, 5, 9, 0, 0] {
        w.u32(v);
    }
    assert_eq!(data, w.data);
}
