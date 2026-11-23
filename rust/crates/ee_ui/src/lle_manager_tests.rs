use super::*;
use crate::lle_cpp::Fake;

fn run(fake: &mut Fake, op: Op, abcd: [isize; 4], f: &mut [f32; 4]) -> Option<isize> {
    manager_call(fake, &HostArgs::new(0x100, op, abcd, f))
}

#[test]
fn window_absent_answers_zero_and_present_its_origin() {
    let mut fake = Fake::default();
    let mut f = [9.0; 4];
    assert_eq!(run(&mut fake, Op::MgrWindow, [0; 4], &mut f), Some(0));
    assert_eq!(fake.calls(), "mgr_window(256)");
    assert_eq!(f[0], 9.0);

    let mut fake = Fake::default();
    fake.answer("mgr_window", &[0x55]).answer("window_origin_x", &[3]).answer("window_origin_y", &[4]);
    assert_eq!(run(&mut fake, Op::MgrWindow, [0; 4], &mut f), Some(1));
    assert_eq!(f[..2], [3.0, 4.0]);
    assert_eq!(fake.calls(), "mgr_window(256)\nwindow_origin_x(85)\nwindow_origin_y(85)");
}

#[test]
fn window_size_and_blending_pack_two_numbers() {
    let mut fake = Fake::default();
    fake.answer("window_width", &[800]).answer("window_height", &[600]);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrWindowSize, [0; 4], &mut f), Some(800 | 600 << 32));

    let mut fake = Fake::default();
    fake.out("mgr_blending", 5).out("mgr_blending", 6);
    assert_eq!(run(&mut fake, Op::MgrBlending, [0; 4], &mut f), Some(5 | 6 << 32));
}

#[test]
fn texture_load_reads_the_screen_then_loads_references_and_sets_the_stages() {
    let mut fake = Fake::default();
    fake.answer("mgr_texture_manager", &[0x77]).answer("mgr_ui_depth", &[16]).answer("texture_load_ui", &[0x99]);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrTextureLoad, [0; 4], &mut f), Some(0x99));
    assert_eq!(
        fake.calls(),
        "mgr_rasterizer_screen(256)\nmgr_texture_manager(256)\nmgr_ui_depth(256)\n\
         texture_load_ui(119, \"textures\\\\UIControls\", 16, true)\ntexture_reference(153)\n\
         texture_stage_alpha(153, 2)\ntexture_stage_color(153, 2)\ntexture_stage_min(153, 1)\n\
         texture_stage_mag(153, 1)\ntexture_stage_mip(153, 1)\ntexture_stage_bias(153, -0.75)\n\
         texture_stage_address(153, 0)"
    );
}

#[test]
fn bitmap_coords_are_the_rectangle_corners() {
    let mut fake = Fake::default();
    let mut f = [0.1, 0.2, 0.3, 0.4];
    assert_eq!(run(&mut fake, Op::MgrBitmapCoords, [7, 0, 0, 0], &mut f), Some(0));
    assert_eq!(fake.calls(), "bitmap_set_opacity(7, 1.0)\nbitmap_set_uvs(7, 0.1, 0.2, 0.3, 0.2, 0.1, 0.4, 0.3, 0.4)");
}

#[test]
fn scaled_bitmap_copies_the_texture_and_coordinates() {
    let mut fake = Fake::default();
    fake.answer("bitmap_new", &[0x40]).answer("mgr_screen", &[1]).answer("bitmap_texture", &[0x50]);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrScaledBitmapNew, [7, 0, 0, 0], &mut f), Some(0x40));
    assert_eq!(
        fake.calls(),
        "bitmap_uvs(7, out)\nbitmap_new()\nmgr_screen(256)\nbitmap_texture(7, 1)\nbitmap_set_texture(64, 80, 1)\n\
         bitmap_set_uvs(64, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)"
    );
}

#[test]
fn events_callbacks_and_instances_pick_by_b() {
    let mut fake = Fake::default();
    let mut f = [0.0; 4];
    run(&mut fake, Op::FormEventCall, [3, 1, 0, 0], &mut f);
    run(&mut fake, Op::FormEventCall, [3, 0, 0, 0], &mut f);
    run(&mut fake, Op::MgrCallback, [9, 0, 12, 0], &mut f);
    run(&mut fake, Op::MgrCallback, [9, 1, 12, 5], &mut f);
    run(&mut fake, Op::MgrInstanceRef, [4, 0, 0, 0], &mut f);
    run(&mut fake, Op::MgrInstanceRef, [4, 1, 0, 0], &mut f);
    assert_eq!(
        fake.calls(),
        "event_dereference(3)\nevent_reference(3)\ncallback_manager(9, 256, 12)\ncallback_form(9, 5, 12)\n\
         instance_reference(4)\ninstance_dereference(4)"
    );
}

#[test]
fn translated_key_copies_one_character_then_asks_the_form() {
    let mut fake = Fake::default();
    fake.answer("form_translated_key", &[2]);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrTranslatedKey, [5, 6, 3, 0], &mut f), Some(2));
    assert_eq!(fake.calls(), "key_copy(6)\nform_translated_key(5, 3)");
}

#[test]
fn a_failed_lock_is_verified_at_its_line() {
    let mut fake = Fake::default();
    fake.out("lock_create", 0x30);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrLockNew, [1743, 0, 0, 0], &mut f), Some(0x30));
    assert_eq!(
        fake.calls(),
        "lock_create(out)\nes_verify(1743, \"TSScheduler::CreateCriticalSection(theLock) == keseOK\")"
    );
    let mut fake = Fake::default();
    fake.answer("lock_create", &[1]);
    run(&mut fake, Op::MgrLockNew, [1, 0, 0, 0], &mut f);
    assert_eq!(fake.calls(), "lock_create(out)");
}

#[test]
fn other_ops_are_the_cpp_hosts() {
    let mut fake = Fake::default();
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::MgrFormCall, [0; 4], &mut f), None);
    assert_eq!(run(&mut fake, Op::BitmapRelease, [0; 4], &mut f), None);
    assert!(fake.log.is_empty());
}

#[test]
fn event_host_counts_owners_and_adds() {
    let mut fake = Fake::default();
    fake.out("file_read_count", 12).answer("file_read_count", &[0]);
    let mut f = [0.0f32; 4];
    assert_eq!(event_call(&mut fake, &HostArgs::new(0x10, Op::BgtFileCount, [0; 4], &mut f)), Some(0));
    assert_eq!(f[0].to_bits(), 12);
    event_call(&mut fake, &HostArgs::new(0x10, Op::EvOwner, [0, 4, 0, 0], &mut f));
    event_call(&mut fake, &HostArgs::new(0x10, Op::EvOwner, [1, 4, 0x20, 0], &mut f));
    event_call(&mut fake, &HostArgs::new(0x10, Op::EvAdd, [0, 0x30, 7, 2], &mut f));
    event_call(&mut fake, &HostArgs::new(0x10, Op::EvAdd, [1, 0x30, 7, 2], &mut f));
    assert_eq!(
        fake.calls(),
        "file_read_count(16, out)\ntable_get(4)\nmgr_form_by_id(32, 4)\ntable_add_event(48, 7, 2)\nform_add_event(48, 7, 2)"
    );
    assert_eq!(event_call(&mut fake, &HostArgs::new(0x10, Op::EvInitialize, [0; 4], &mut f)), None);
}

#[test]
fn database_loads_seven_tables_in_order() {
    let mut fake = Fake::default();
    for _ in 0..7 {
        fake.out("db_find", 0x60);
    }
    load_database(&mut fake, 1, 2, 3);
    let calls = fake.calls();
    assert_eq!(
        calls,
        "db_find(2, 3, \"DB\\\\dbuifonts.dat\", out)\ndb_fonts(96, 1)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuiforms.dat\", out)\ndb_forms(96, 1)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuiback.dat\", out)\ndb_backgrounds(96)\ndb_create_backgrounds(1)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuicontrols.dat\", out)\ndb_controls(96, 1)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuicontrolevents.dat\", out)\ndb_control_events(96)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuiformevents.dat\", out)\ndb_form_events(96, 1)\nfile_delete(96)\n\
         db_find(2, 3, \"DB\\\\dbuihotkey.dat\", out)\ndb_hot_keys(96)\nfile_delete(96)"
    );
}

#[test]
fn database_failures_are_verified_at_the_original_lines() {
    let mut fake = Fake::default();
    fake.answer("db_find", &[5]).answer("db_fonts", &[1]);
    load_database(&mut fake, 1, 2, 3);
    let calls = fake.calls();
    assert!(calls.starts_with(
        "db_find(2, 3, \"DB\\\\dbuifonts.dat\", out)\nes_verify(1081, \"theResult == keseOK\")\nes_verify(1082, \"theDbFile != NULL\")\n\
         db_fonts(0, 1)\nes_verify(1084, \"theResult == keseOK\")\nfile_delete(0)\n"
    ));
}

#[test]
fn mouse_buttons_are_fetched_once_and_swapped_on_a_change() {
    let mut fake = Fake::default();
    fetch_mouse_buttons(&mut fake);
    assert_eq!(
        fake.calls(),
        "mouse_buttons_ready()\nmouse_button_fetch(0)\nmouse_button_fetch(1)\nmouse_button_fetch(2)"
    );
    let mut fake = Fake::default();
    fake.answer("mouse_buttons_ready", &[1]);
    fetch_mouse_buttons(&mut fake);
    assert_eq!(fake.calls(), "mouse_buttons_ready()");

    let mut fake = Fake::default();
    set_mouse_left_handed(&mut fake, false);
    set_mouse_left_handed(&mut fake, true);
    assert_eq!(
        fake.calls(),
        "mouse_left_handed()\nmouse_left_handed()\nmouse_swap_buttons()\nmouse_set_left_handed(true)"
    );
    remove_database(&mut fake);
}
