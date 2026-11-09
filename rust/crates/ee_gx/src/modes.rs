//! The display modes GX lists, the game's resolution for a UI scale, the render scale: pure functions of the display's
//! sizes (the platform layer's answers).

use crate::cformat;

/// What `PlatWindowGetDisplayLogicalSize` and `PlatWindowGetDisplaySize` answered; the bad values fall back as in the C++.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DisplayInfo {
    pub logical: Option<(i32, i32)>,
    pub pixels: Option<(i32, i32)>,
}

/// `LogicalDisplaySize`: the display's logical size or 1024x768.
pub fn logical_display_size(info: &DisplayInfo) -> (u32, u32) {
    match info.logical {
        Some((w, h)) if w > 0 && h > 0 => (w as u32, h as u32),
        _ => (1024, 768),
    }
}

/// `DesktopSize`: the display's pixels or 1024x768.
pub fn desktop_size(info: &DisplayInfo) -> (u32, u32) {
    let (w, h) = info.pixels.unwrap_or((0, 0));
    if w <= 0 || h <= 0 {
        (1024, 768)
    } else {
        (w as u32, h as u32)
    }
}

pub fn auto_ui_scale(info: &DisplayInfo) -> u32 {
    let (_, height) = logical_display_size(info);
    let percent = (height * 100 / 1080 + 12) / 25 * 25; // to the scale steps (25%)
    percent.clamp(100, 300)
}

/// `GXGameSize`.
pub fn game_size(info: &DisplayInfo, percent: u32, windowed: bool) -> (u32, u32) {
    let (mut width, mut height) = logical_display_size(info);
    if windowed {
        width = width * 4 / 5;
        height = height * 4 / 5;
    }
    let mut percent = if percent != 0 { percent } else { auto_ui_scale(info) };
    if percent < 50 {
        percent = 50;
    }
    let mut out_width = (width * 100 / percent) & !1;
    let mut out_height = (height * 100 / percent) & !1;
    if out_height < 600 {
        // the game's smallest layout
        out_width = (out_width * 600 / if out_height != 0 { out_height } else { 1 }) & !1;
        out_height = 600;
    }
    (out_width, out_height)
}

/// `GXRenderScale`, without the cached environment override (see `render_scale_override`).
pub fn render_scale(
    mode_height: u32,
    display_height: Option<i32>,
    render_percent: u32,
    override_value: Option<&str>,
) -> f32 {
    if let Some(text) = override_value {
        if !text.is_empty() {
            let scale = cformat::atof(text) as f32;
            return if (1.0..=4.0).contains(&scale) { scale } else { 1.0 };
        }
    }
    let display_height = match display_height {
        Some(h) if h > 0 && mode_height != 0 => h,
        _ => return 1.0,
    };
    let scale = display_height as f32 * render_percent as f32 / 100.0 / mode_height as f32;
    if scale < 1.05 {
        1.0
    } else if scale > 4.0 {
        4.0
    } else {
        scale
    }
}

/// `GXDirectDraw::EnumDisplayModes`' size list.
pub fn display_mode_sizes(info: &DisplayInfo) -> Vec<(u32, u32)> {
    const SIZES: [(u32, u32); 14] = [
        (640, 480),
        (800, 600),
        (1024, 768),
        (1152, 864),
        (1280, 720),
        (1280, 960),
        (1280, 1024),
        (1366, 768),
        (1600, 900),
        (1600, 1200),
        (1920, 1080),
        (1920, 1200),
        (2560, 1440),
        (3840, 2160),
    ];
    const UI_SCALES: [u32; 8] = [0, 100, 125, 150, 175, 200, 250, 300];
    let (desktop_width, desktop_height) = desktop_size(info);
    let mut sizes = Vec::new();
    for &(w, h) in &SIZES {
        // modes up to 1600x1200 always (a window can be larger than a small desktop), larger ones when they fit
        if w > 1600 && (w > desktop_width || h > desktop_height) {
            continue;
        }
        sizes.push((w, h));
    }
    // the desktop's own size is always a mode (the screen's current mode is looked up among them), and so is the
    // display's logical size (macOS: points, the default of a full screen game) when it differs from its pixels
    sizes.push((desktop_width, desktop_height));
    if let Some((w, h)) = info.logical {
        if w > 0 {
            sizes.push((w as u32, h as u32));
        }
    }
    // the game sizes of the UI scales, full screen and windowed (GXGameSize)
    for windowed in [false, true] {
        for &scale in &UI_SCALES {
            sizes.push(game_size(info, scale, windowed));
        }
    }
    // the menus' size for each of them (EEMenuWidth / EEMenuHeight: 768 lines at the game resolution's aspect ratio)
    for i in 0..sizes.len() {
        let (width, height) = sizes[i];
        if height <= 768 {
            continue;
        }
        let menu_width = ((width * 768 + height / 2) / height) & !1;
        sizes.push((menu_width.max(1024), 768));
    }
    let mut listed = Vec::new();
    for &size in &sizes {
        if !listed.contains(&size) {
            listed.push(size);
        }
    }
    listed
}

#[cfg(test)]
mod tests {
    use super::*;

    fn info(logical: Option<(i32, i32)>, pixels: Option<(i32, i32)>) -> DisplayInfo {
        DisplayInfo { logical, pixels }
    }

    #[test]
    fn golden_sizes_and_scales_from_the_cpp() {
        // golden_modes.txt: GXAutoUIScale, GXGameSize, GXRenderScale and the listed sizes for several displays
        let mut cases = 0;
        let mut d = DisplayInfo::default();
        for line in include_str!("golden_modes.txt").lines() {
            let f: Vec<&str> = line.split_whitespace().collect();
            match f.first().copied() {
                None => {}
                Some("display") => {
                    // logical w h, pixels w h; a size of -1 means the platform's call failed
                    let n = |i: usize| f[i].parse::<i32>().unwrap();
                    d.logical = (n(1) != -1).then(|| (n(1), n(2)));
                    d.pixels = (n(3) != -1).then(|| (n(3), n(4)));
                    cases += 1;
                }
                Some("auto") => assert_eq!(auto_ui_scale(&d), f[1].parse().unwrap(), "{line}"),
                Some("game") => {
                    let percent: u32 = f[1].parse().unwrap();
                    let windowed = f[2] == "1";
                    let expected = (f[3].parse().unwrap(), f[4].parse().unwrap());
                    assert_eq!(game_size(&d, percent, windowed), expected, "{line}");
                }
                Some("scale") => {
                    let mode_height: u32 = f[1].parse().unwrap();
                    let percent: u32 = f[2].parse().unwrap();
                    let expected: f32 = f[3].parse().unwrap();
                    let height = d.pixels.map(|p| p.1);
                    let got = render_scale(mode_height, height, percent, None);
                    assert_eq!(got.to_bits(), expected.to_bits(), "{line} {got} {expected}");
                }
                Some("modes") => {
                    let expected: Vec<(u32, u32)> = f[1..]
                        .iter()
                        .map(|s| {
                            let (w, h) = s.split_once('x').unwrap();
                            (w.parse().unwrap(), h.parse().unwrap())
                        })
                        .collect();
                    assert_eq!(display_mode_sizes(&d), expected);
                }
                _ => panic!("unknown line {line}"),
            }
        }
        assert!(cases >= 5);
    }

    #[test]
    fn fallbacks_for_a_failing_platform() {
        let none = info(None, None);
        assert_eq!(logical_display_size(&none), (1024, 768));
        assert_eq!(desktop_size(&none), (1024, 768));
        assert_eq!(logical_display_size(&info(Some((0, 5)), None)), (1024, 768));
        assert_eq!(desktop_size(&info(None, Some((0, 0)))), (1024, 768));
    }

    #[test]
    fn auto_ui_scale_steps_by_quarters_and_clamps() {
        let d = |h| info(Some((1, h)), None);
        assert_eq!(auto_ui_scale(&d(600)), 100);
        assert_eq!(auto_ui_scale(&d(1080)), 100);
        assert_eq!(auto_ui_scale(&d(1440)), 125);
        assert_eq!(auto_ui_scale(&d(2160)), 200);
        assert_eq!(auto_ui_scale(&d(9000)), 300);
    }

    #[test]
    fn game_size_keeps_the_smallest_layout() {
        let d = info(Some((800, 500)), None);
        let (w, h) = game_size(&d, 100, false);
        assert_eq!(h, 600);
        assert_eq!(w, 960);
        // the windowed size is 80% before the scale
        let (w, h) = game_size(&info(Some((1920, 1080)), None), 100, true);
        assert_eq!((w, h), (1536, 864));
        assert_eq!(
            game_size(&info(Some((1920, 1080)), None), 10, false),
            game_size(&info(Some((1920, 1080)), None), 50, false)
        );
    }

    #[test]
    fn render_scale_override_and_clamps() {
        assert_eq!(render_scale(768, Some(1080), 100, Some("2")), 2.0);
        assert_eq!(render_scale(768, Some(1080), 100, Some("9")), 1.0);
        assert_eq!(render_scale(768, Some(1080), 100, Some("0.5")), 1.0);
        assert_eq!(render_scale(768, Some(1080), 100, Some("")), 1080.0 * 100.0 / 100.0 / 768.0);
        assert_eq!(render_scale(0, Some(1080), 100, None), 1.0);
        assert_eq!(render_scale(768, None, 100, None), 1.0);
        assert_eq!(render_scale(1080, Some(1080), 100, None), 1.0);
        assert_eq!(render_scale(100, Some(4320), 100, None), 4.0);
        assert_eq!(render_scale(1080, Some(1080), 50, None), 1.0);
    }

    #[test]
    fn modes_list_the_desktop_and_never_repeat() {
        let d = info(Some((1920, 1080)), Some((1920, 1080)));
        let modes = display_mode_sizes(&d);
        assert!(modes.contains(&(1920, 1080)));
        let mut unique = modes.clone();
        unique.sort_unstable();
        unique.dedup();
        assert_eq!(unique.len(), modes.len());
        // above a small desktop: only up to 1600x1200 plus what the desktop supports
        let d = info(Some((1024, 768)), Some((1024, 768)));
        let modes = display_mode_sizes(&d);
        assert!(modes.contains(&(1600, 1200)));
        assert!(!modes.contains(&(1920, 1080)));
    }
}
