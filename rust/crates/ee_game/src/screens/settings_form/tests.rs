use super::*;

#[test]
fn the_control_table() {
    // EECreateGameSettingsEvent's cases, and nothing else
    let mine: Vec<c_ulong> = (0..300).filter(|&i| event_for_control(i) != 0).collect();
    let mut want: Vec<c_ulong> = vec![46, 47, 48, 49, 50, 72, 73, 74, 75, 76, 77, 78, 84, 214, 215, 216, 217, 218];
    want.extend(220..=239);
    want.extend([250, 251]);
    assert_eq!(mine, want);
    assert_eq!(event_for_control(45), 0); // the OK button's: the open events hang it
    assert_eq!(event_for_control(219), 0); // Custom
    for i in 0..300 {
        let e = event_for_control(i);
        assert!(e == 0 || is_settings_event(e), "{i}");
    }
    assert_eq!(event_for_control(77), EE_SCREENS_GS_MUSIC_QUALITY);
    assert_eq!(event_for_control(251), EE_SCREENS_GS_ADVANCED_CHANGED);
    assert!(!is_settings_event(0x0500) && !is_settings_event(0x0514) && !is_settings_event(0x0601));
}

#[test]
fn the_host_texts() {
    assert_eq!(split_list(b"A0\0\0dev 2\0"), [b"A0".to_vec(), Vec::new(), b"dev 2".to_vec()]);
    assert!(split_list(b"").is_empty());
    assert!(split_list(b"unended").is_empty());
    assert_eq!(c_text(b"dev\0rest"), b"dev");
    assert_eq!(c_text(b"dev"), b"dev");
}

#[test]
fn the_settings_size_as_a_mode() {
    let r = EeScreensSettingsRecord {
        game_window_width: 1600,
        game_window_height: 900,
        game_bit_depth: 32,
        ..Default::default()
    };
    assert_eq!(settings_mode(&r), EeScreensVideoMode { width: 1600, height: 900, refresh_rate: 0, bit_count: 32 });
}
