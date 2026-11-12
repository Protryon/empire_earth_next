use super::*;

fn controls(checks: [bool; 9], model: c_long, effect: c_long) -> EeScreensGraphics {
    let radio = |level: c_long| [level == 2, level == 1, level == 0];
    EeScreensGraphics { checks, model: radio(model), effect: radio(effect) }
}

#[test]
fn every_preset_matches_itself() {
    for q in 0..5 {
        let p = quality_preset(q).unwrap();
        let g = controls(p.checks, p.model, p.effect);
        // presets 1 and 2 share nothing, but 0 and 1 differ only in their detail levels
        assert_eq!(graphics_quality(&g), q, "preset {q}");
    }
}

#[test]
fn no_radio_checked_agrees_with_any_level() {
    let p = quality_preset(1).unwrap();
    let g = EeScreensGraphics { checks: p.checks, model: [false; 3], effect: [false; 3] };
    // presets 0 and 1 have the same check boxes: the first one wins
    assert_eq!(graphics_quality(&g), 0);
}

#[test]
fn two_checked_radios_of_different_levels_match_nothing() {
    let p = quality_preset(4).unwrap();
    let mut g = controls(p.checks, 2, 2);
    g.model = [true, true, false];
    assert_eq!(graphics_quality(&g), EE_SCREENS_QUALITY_CUSTOM);
}

#[test]
fn custom_when_a_check_box_differs() {
    let p = quality_preset(3).unwrap();
    let mut checks = p.checks;
    checks[7] = true; // weather effects: on in no preset
    assert_eq!(graphics_quality(&controls(checks, p.model, p.effect)), EE_SCREENS_QUALITY_CUSTOM);
}

#[test]
fn no_preset_past_four() {
    assert!(quality_preset(5).is_none());
    assert!(quality_preset(c_ulong::MAX).is_none());
    assert_eq!(quality_radio(9), EE_SCREENS_QUALITY_CUSTOM);
    assert_eq!(quality_radio(3), 3);
}

#[test]
fn detail_and_texture_lines() {
    assert_eq!([detail_level(0), detail_level(1), detail_level(2), detail_level(-1), detail_level(7)], [0, 1, 2, 2, 2]);
    assert_eq!([texture_resolution_line(0), texture_resolution_line(2), texture_resolution_line(4)], [0, 1, 2]);
    assert_eq!(texture_resolution_line(3), 0);
}

#[test]
fn graphics_show_follows_the_settings() {
    let s = EeScreensGraphicsSettings {
        trilinear_filter: true,
        wait_for_vsync: true,
        mip_levels_to_drop: 0,
        terrain_mip_levels_to_drop: 1,
        texture_resolution: 4,
        model_detail: 1,
        effect_detail: 9,
        ..Default::default()
    };
    let p = graphics_show(&s);
    assert_eq!(p.checks, [true, false, false, true, false, false, false, false, false]);
    assert_eq!((p.texture_line, p.model, p.effect), (2, 1, 2));
    let s = EeScreensGraphicsSettings { terrain_mip_levels_to_drop: 0, ..s };
    assert!(graphics_show(&s).checks[2]);
}

#[test]
fn nearest_line_takes_the_first_of_equals() {
    let t = table(EE_SCREENS_TABLE_UI_SCALE).unwrap();
    assert_eq!(nearest_line(t, 0), 0);
    assert_eq!(nearest_line(t, 112), 1); // 100 and 125: 12 against 13
    assert_eq!(nearest_line(t, 225), 5); // 200 and 250 equally near: the first
    assert_eq!(nearest_line(t, 1000), 7);
    let t = table(EE_SCREENS_TABLE_FRAME_RATE).unwrap();
    assert_eq!(nearest_line(t, -5), 3); // -1, unlimited
    assert_eq!(nearest_line(t, 90), 1); // 60 and 120 equally near
}

#[test]
fn nearest_line_wraps_as_the_cpp() {
    // labs(LONG_MIN) is LONG_MIN under -fwrapv: the most negative distance, so nearest
    let t = table(EE_SCREENS_TABLE_WINDOW_MODE).unwrap();
    assert_eq!(nearest_line(t, c_long::MIN + 1), 1); // 1 - (MIN + 1) wraps to MIN
    assert_eq!(nearest_line(t, c_long::MIN), 0); // 0 - MIN = MIN; 1 - MIN wraps to MIN + 1 (positive after abs)
}

#[test]
fn line_values_and_unknown_tables() {
    let t = table(EE_SCREENS_TABLE_BRIGHTNESS).unwrap();
    assert_eq!(line_value(t, 0), Some(70));
    assert_eq!(line_value(t, 6), Some(130));
    assert_eq!(line_value(t, 7), None);
    assert_eq!(line_value(t, c_ulong::MAX), None);
    assert!(table(7).is_none());
    for k in 0..7 {
        assert!(!table(k).unwrap().is_empty());
    }
}
