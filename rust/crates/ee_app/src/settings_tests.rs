//! settings.rs over a mock config.yaml and string members: the reads, the defaults written back in the rows' order,
//! the ranges and clamps, the percents, the writes.

use crate::game_options::{self, EeAppOptionsFields, EeAppOptionsLimits};
use crate::settings::*;
use std::collections::BTreeMap;

#[derive(Default)]
struct File {
    values: BTreeMap<(String, String), Vec<u8>>,
    writes: Vec<String>,
}

impl File {
    fn put(&mut self, block: &str, key: &str, v: &str) {
        self.values.insert((block.into(), key.into()), v.as_bytes().to_vec());
    }
    fn get(&self, block: &str, key: &str) -> Option<String> {
        self.values.get(&(block.into(), key.into())).map(|v| String::from_utf8(v.clone()).unwrap())
    }
}

impl Config for File {
    fn get_int(&mut self, block: &str, key: &str) -> Option<i32> {
        let v = self.get(block, key)?;
        match v.as_str() {
            "true" => Some(1),
            "false" => Some(0),
            t => t.parse().ok(),
        }
    }
    fn get_string(&mut self, block: &str, key: &str) -> Option<Vec<u8>> {
        self.values.get(&(block.into(), key.into())).cloned()
    }
    fn set_int(&mut self, block: &str, key: &str, value: i32) -> bool {
        self.writes.push(format!("{block}.{key}"));
        self.put(block, key, &value.to_string());
        true
    }
    fn set_bool(&mut self, block: &str, key: &str, value: bool) -> bool {
        self.writes.push(format!("{block}.{key}"));
        self.put(block, key, if value { "true" } else { "false" });
        true
    }
    fn set_string(&mut self, block: &str, key: &str, value: &[u8]) -> bool {
        self.writes.push(format!("{block}.{key}"));
        self.values.insert((block.into(), key.into()), value.to_vec());
        true
    }
}

#[derive(Default)]
struct Text {
    values: BTreeMap<u32, Vec<u8>>,
    defaults: BTreeMap<u32, Vec<u8>>,
}

impl Strings for Text {
    fn get(&mut self, id: u32) -> Vec<u8> {
        self.values.get(&id).cloned().unwrap_or_default()
    }
    fn set(&mut self, id: u32, value: &[u8]) {
        self.values.insert(id, value.to_vec());
    }
    fn set_default(&mut self, id: u32) {
        let d = self.defaults.get(&id).cloned().unwrap_or_default();
        self.values.insert(id, d);
    }
}

struct Pair {
    settings: EeAppSettingsFields,
    options: EeAppOptionsFields,
}

impl Pair {
    fn new() -> Self {
        let limits = EeAppOptionsLimits { epochs: 15, ..Default::default() };
        let mut options = game_options::options_default(&limits);
        last_options_default(&mut options, 15);
        Pair { settings: EeAppSettingsFields::default(), options }
    }
    fn records(&mut self) -> Records {
        Records { settings: &mut self.settings, options: &mut self.options }
    }
}

#[test]
fn an_empty_file_gets_every_default_in_the_rows_order() {
    let mut f = File::default();
    let mut t = Text::default();
    t.defaults.insert(EE_APP_SETTINGS_PLAYER_NAME, b"Unknown".to_vec());
    let mut s = Pair::new();
    let mut d = Pair::new();
    s.settings.mModelDetail = 0;
    read(&mut f, &mut t, s.records(), d.records(), false);
    read(&mut f, &mut t, s.records(), d.records(), true);
    assert_eq!(s.settings, EeAppSettingsFields::default());
    assert_eq!(f.writes.len(), 58 + 17);
    assert_eq!(&f.writes[..3], &["player.name", "player.computer_difficulty", "display.screen"]);
    assert_eq!(f.writes[58], "game_options.map.size");
    assert_eq!(f.get("player", "name").unwrap(), "Unknown");
    assert_eq!(f.get("audio", "sound_volume").unwrap(), "100");
    assert_eq!(f.get("audio.music", "volume").unwrap(), "65");
    assert_eq!(f.get("input", "mouse_sensitivity").unwrap(), "31");
    assert_eq!(f.get("debug", "game_log").unwrap(), "true");
    assert_eq!(f.get("game_options", "unit_limit").unwrap(), "500");
    assert_eq!(f.get("game_options.epochs", "end").unwrap(), "14");
}

#[test]
fn values_out_of_range_are_defaults_and_clamped_ones_the_limit() {
    let mut f = File::default();
    f.put("display", "window_mode", "5");
    f.put("graphics.mip_levels_to_drop", "textures", "20");
    f.put("graphics.mip_levels_to_drop", "terrain", "-4");
    f.put("graphics", "model_detail", "-5");
    f.put("graphics", "effect_detail", "9");
    f.put("audio", "sound_volume", "150");
    f.put("audio", "max_sounds", "-20");
    f.put("camera", "zoom_lock", "-12");
    f.put("display.resolution", "width", "abc");
    f.put("audio", "device", "Speakers (Foo)");
    f.put("graphics", "shadows", "0");
    let mut t = Text::default();
    let mut s = Pair::new();
    let mut d = Pair::new();
    read(&mut f, &mut t, s.records(), d.records(), false);
    assert_eq!(s.settings.mWindowMode, 0);
    assert_eq!((s.settings.mMIPLevelsToDrop, s.settings.mTerrainMIPLevelsToDrop), (8, 0));
    // a clamp's minimum "any" keeps a low value; the maximum clamps a high one
    assert_eq!((s.settings.mModelDetail, s.settings.mEffectDetail), (-5, 2));
    assert_eq!(s.settings.mSoundVolume, 1.0);
    assert_eq!(s.settings.mMaxNumberOfSounds, 0.0);
    assert_eq!(s.settings.mZoomLock, -12.0f32 * 0.01);
    assert_eq!(s.settings.mGameWindowWidth, 800);
    assert!(!s.settings.mDisplayShadows);
    let device: Vec<u8> = s.settings.mAudioDevice.iter().take_while(|&&c| c != 0).map(|&c| c as u8).collect();
    assert_eq!(device, b"Speakers (Foo)");
    // the clamped values were read (not written back), the out-of-range ones were
    assert!(!f.writes.contains(&"graphics.mip_levels_to_drop.textures".to_string()));
    assert!(f.writes.contains(&"display.window_mode".to_string()));
    assert_eq!(f.get("display", "window_mode").unwrap(), "0");
}

#[test]
fn writes_take_whole_percents_and_flags() {
    let mut f = File::default();
    let mut t = Text::default();
    t.values.insert(EE_APP_SETTINGS_LAST_GAME_FILE, b"x.ees".to_vec());
    let mut s = Pair::new();
    s.settings.mMouseSensitivity = 0.3125;
    s.settings.mSideScrollSpeed = 3.33;
    s.settings.mGameLog = 5;
    s.settings.mSoundVolume = 2.5;
    assert_eq!(write(&mut f, &mut t, s.records(), false), 0);
    assert_eq!(f.get("input", "mouse_sensitivity").unwrap(), "31");
    assert_eq!(f.get("input.scroll", "side_speed").unwrap(), "333");
    assert_eq!(f.get("debug", "game_log").unwrap(), "true");
    // the clamp applies to writes too
    assert_eq!(f.get("audio", "sound_volume").unwrap(), "100");
    assert_eq!(f.get("recent", "game_file").unwrap(), "x.ees");
    assert_eq!(f.writes.len(), 58);
}

#[test]
fn after_load_fixes_the_texture_resolution_and_the_switches() {
    let d = EeAppSettingsFields::default();
    let mut s = EeAppSettingsFields { mTextureResolution: 3, mWindowMode: 1, mFrameRateLimit: 60, ..d };
    s.mGameBitDepth = 16;
    s.mL540 = 7;
    assert!(after_load(&mut s, &d, true));
    assert_eq!((s.mTextureResolution, s.mGameBitDepth, s.mTextureBitDepth, s.mL540), (0, 32, 32, 0));
    assert!(!s.mShowBlood && !s.mFullScreen && !s.mWaitForVSync);
    let mut s = EeAppSettingsFields { mTextureResolution: 4, ..d };
    assert!(!after_load(&mut s, &d, false));
    assert!(s.mShowBlood && s.mFullScreen && s.mWaitForVSync);
}
