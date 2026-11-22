use super::*;
use crate::lle_cpp::Fake;

fn run(fake: &mut Fake, op: Op, abcd: [isize; 4], f: &mut [f32; 4]) -> Option<isize> {
    form_call(fake, &HostArgs::new(0x100, op, abcd, f))
}

#[test]
fn help_string_is_emptied_then_asked() {
    let mut fake = Fake::default();
    fake.answer("control_help_string", &[1]);
    let mut f = [0.0; 4];
    assert_eq!(run(&mut fake, Op::FormHelpGet, [5, 0, 0, 0], &mut f), Some(1));
    assert_eq!(fake.calls(), "form_help_clear()\ncontrol_help_string(5)");
}

#[test]
fn hot_key_events_events_and_mouse_asks_pick_by_b() {
    let mut fake = Fake::default();
    let mut f = [0.0; 4];
    for b in 0..4 {
        run(&mut fake, Op::FormHotKeySetEvent, [7, b, 9, 0], &mut f);
    }
    for b in 0..4 {
        run(&mut fake, Op::FormEventCall, [3, b, 0, 0], &mut f);
    }
    fake.answer("entity_value", &[2]);
    for b in 0..4 {
        run(&mut fake, Op::FormMouseAsk, [4, b, 0, 0], &mut f);
    }
    assert_eq!(
        fake.calls(),
        "hot_key_set_just_pressed(7, 9)\nhot_key_set_pressed(7, 9)\nhot_key_set_just_unpressed(7, 9)\n\
         hot_key_set_double_pressed(7, 9)\nevent_uninitialize(3)\nevent_dereference(3)\nevent_reference(3)\n\
         event_initialize(3)\nentity_value(4)\nentity_just_pressed(4)\nentity_just_unpressed(4)\n\
         entity_just_double_pressed(4)"
    );
}

#[test]
fn keys_lock_and_bitmap_draws() {
    let mut fake = Fake::default();
    let mut f = [0.25, 0.5, 0.75, 1.0];
    run(&mut fake, Op::FormKeysFetch, [0; 4], &mut f);
    run(&mut fake, Op::FormLockNew, [0; 4], &mut f);
    run(&mut fake, Op::FormBitmapSetTexCoords, [6, 0, 0, 0], &mut f);
    assert_eq!(
        fake.calls(),
        "form_fetch_tab_key()\nform_fetch_shift_key()\nlock_create(out)\n\
         es_verify(1743, \"TSScheduler::CreateCriticalSection(theLock) == keseOK\")\n\
         bitmap_set_uvs(6, 0.25, 0.5, 0.75, 0.5, 0.25, 1.0, 0.75, 1.0)"
    );
    let mut fake = Fake::default();
    fake.answer("bitmap_texture", &[8, 8]).answer("texture_width", &[64]).answer("texture_height", &[32]);
    run(&mut fake, Op::FormBitmapTexSize, [6, 0, 0, 0], &mut f);
    assert_eq!(f[..2], [64.0, 32.0]);
    assert_eq!(
        fake.calls(),
        "form_screen(256)\nbitmap_texture(6, 0)\ntexture_width(8)\nform_screen(256)\nbitmap_texture(6, 0)\ntexture_height(8)"
    );
    assert_eq!(run(&mut fake, Op::FormCtlType, [0; 4], &mut f), None);
}

#[test]
fn forms_file_makes_the_forms_with_an_id() {
    let mut fake = Fake::default();
    fake.out("file_read_count", 2).answer("mgr_create_form", &[0x40]);
    assert_eq!(create_forms(&mut fake, 1, 2), 0);
    // the fake's reads leave the record's id 0: no form
    assert_eq!(
        fake.calls(),
        "file_open(1)\nfile_read_count(1, out)\nfile_read(1, out, 268)\nfile_read(1, out, 268)\nfile_close(1)"
    );
    let mut fake = Fake::default();
    fake.answer("file_read_count", &[3]);
    assert_eq!(create_forms(&mut fake, 1, 2), 3);
    assert_eq!(fake.calls(), "file_open(1)\nfile_read_count(1, out)\nfile_close(1)");
}

#[test]
fn keyboard_passes_answer_ok_without_a_control_but_the_open() {
    let mut fake = Fake::default();
    assert_eq!(pass_to_keyboard(&mut fake, 1, 0, 2, 0, 0), 0);
    assert_eq!(pass_to_keyboard(&mut fake, 1, 3, 0, 0, 0), 9);
    let mut fake = Fake::default();
    fake.answer("form_keyboard_captured", &[5, 5, 5, 5, 5]).answer("control_ime_change", &[4]);
    pass_to_keyboard(&mut fake, 1, 0, 2, 0, 0);
    assert_eq!(pass_to_keyboard(&mut fake, 1, 1, 2, 3, 6), 4);
    pass_to_keyboard(&mut fake, 1, 2, 0, 0, 0);
    pass_to_keyboard(&mut fake, 1, 3, 0, 0, 0);
    pass_to_keyboard(&mut fake, 1, 4, 8, 0, 2);
    assert_eq!(
        fake.calls(),
        "form_keyboard_captured(1)\ncontrol_ime_result(5, 2)\nform_keyboard_captured(1)\ncontrol_ime_change(5, 2, 3, 6)\n\
         form_keyboard_captured(1)\ncontrol_ime_close(5)\nform_keyboard_captured(1)\ncontrol_ime_open(5)\n\
         form_keyboard_captured(1)\ncontrol_translated_key(5, 8, 2)"
    );
}

#[test]
fn packs_are_sixteen_bits_each() {
    assert_eq!(ee_ui_form_pack_keys(1, 2, 0x7fff, 0x10000), (1u64 | 2 << 16 | 0x7fff << 32 | 0xffff << 48) as isize);
    assert_eq!(ee_ui_form_pack_types(0x10001, 2, 3, 4), (1u64 | 2 << 16 | 3 << 32 | 4 << 48) as isize);
    assert_eq!(ee_ui_float_pair_bits(1.0, 2.0), (0x3f80_0000u64 | 0x4000_0000u64 << 32) as isize);
}
