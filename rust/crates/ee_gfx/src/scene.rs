//! The capture's per-frame state and arithmetic (`EERenderObject::Fn10C036` and the fades' starts,
//! `src/Empire Earth/Graphics Thread/EERenderObject.cpp`): the grid, water and cloud clocks, the light fade, the
//! letterbox's zoom with its view band and cleared bars, and the time-of-day light blend (the 17 key frames, the
//! three fill lights, the prelit colour). The C++ keeps the state's storage (one `EeGfxSceneFade`), the rasterizer's
//! clears and viewport, the UI's backgrounds and the render environment it writes these values into.
//!
//! Drawn only (the renderer's thread): EE-SIM never runs it. The fade's and letterbox's progress is `f64`, as the
//! C++'s (the x87 build kept it in an extended register: a fade "at once" is the rate `f32::MAX`, whose product with
//! the elapsed milliseconds overflows an `f32`).

use ee_ge::types::GeVector;
use ee_ge::um;

use crate::effects::EeGfxColor;

/// The grid's texture scroll per millisecond times 1000 (0x441348).
const GRID_SPEED: f32 = 0.8;
/// The water's phase per millisecond, in turns (0x4413c4).
const WATER_SPEED: f32 = 5.0e-05;
const TWO_PI: f32 = 6.2831855;
/// A fade or letterbox "at once" (0 frames).
const AT_ONCE: f32 = 3.4028235e+38;
/// The game view's band in the 1600 x 1200 layout (0x441334.., 0x441360) and its letterboxed top and bottom.
const VIEW_LEFT: f32 = 0.0;
const VIEW_RIGHT: f32 = 1600.0;
const VIEW_TOP: f32 = 35.0;
const VIEW_BOTTOM: f32 = 1085.0;
const VIEW_HEIGHT: f32 = 1200.0;
const LETTERBOX_ASPECT: f32 = 0.42553192;
const VIEW_SCALE_Y: f32 = 1.0 / 1200.0;
/// The moon lights the night between these times of day (0x4413b8, 0x4413b4).
const MOON_FROM: f32 = 0.5625;
const MOON_TO: f32 = 0.9375;
/// A fill light's share of the prelit colour (0x4413b0).
const FILL_SHARE: f32 = 0.333333;

/// The light fade's and the letterbox's state (the C++'s statics `sLightFade` 0x4be368, `sLightFadeStart`,
/// `sL529070`, `sF529074`, `sF529078`, `sLetterboxDirection` 0x4be370, `sLetterboxStart`, `sF52907C`, `sF529080`,
/// `sB529088`); the zoom itself is the C++'s `gF4BE374`, which the scenario manager reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxSceneFade {
    pub light_fade: f32,
    pub light_fade_start: f32,
    /// 1: fading down to black, 0: up to full
    pub fade_down: i32,
    pub fade_rate: f32,
    pub fade_time: f32,
    /// 1: the bars opening towards the full view (zoom to 1), 0: closing (zoom to 0)
    pub letterbox_direction: i32,
    pub letterbox_start: f32,
    pub letterbox_rate: f32,
    pub letterbox_time: f32,
    /// the bars are to be cleared again on the next frame
    pub bars_cleared: bool,
}

impl Default for EeGfxSceneFade {
    /// `EEInitialiseScene`'s values (and the statics' initial ones).
    fn default() -> EeGfxSceneFade {
        EeGfxSceneFade {
            light_fade: 1.0,
            light_fade_start: 1.0,
            fade_down: 0,
            fade_rate: 0.0,
            fade_time: 0.0,
            letterbox_direction: 1,
            letterbox_start: 1.0,
            letterbox_rate: 0.0,
            letterbox_time: 0.0,
            bars_cleared: false,
        }
    }
}

fn rate(frames: u32) -> f32 {
    if frames == 0 {
        AT_ONCE
    } else {
        1.0 / frames as f32
    }
}

/// `EEInitialiseScene`'s reset (the bars' flag is left as it was, as the original's).
pub fn reset(s: &mut EeGfxSceneFade) {
    let bars = s.bars_cleared;
    *s = EeGfxSceneFade { bars_cleared: bars, ..EeGfxSceneFade::default() };
}

/// `EEStartLightFadeIn` / `EEStartLightFadeOut`: from the current fade over `frames` (0: at once).
pub fn start_fade(s: &mut EeGfxSceneFade, down: bool, frames: u32) {
    s.fade_down = down as i32;
    s.fade_time = 0.0;
    s.light_fade_start = s.light_fade;
    s.fade_rate = rate(frames);
}

/// `EEStartLetterboxOut` (direction 1) / `EEStartLetterboxIn` (0): from the current zoom over `frames`.
pub fn start_letterbox(s: &mut EeGfxSceneFade, out: bool, frames: u32, zoom: f32) {
    s.letterbox_start = zoom;
    s.letterbox_time = 0.0;
    s.letterbox_direction = out as i32;
    s.letterbox_rate = rate(frames);
}

/// The render environment's clocks.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSceneClocks {
    /// `gU4E9330`: the grid's and the area rings' texture scroll, in [0, 1)
    pub grid_phase: f32,
    pub water_phase: f32,
    pub cloud_offset: f32,
}

/// The clocks after `elapsed` milliseconds in a wind of `wind`.
pub fn step_clocks(c: &mut EeGfxSceneClocks, elapsed: f32, wind: f32) {
    c.grid_phase += elapsed * GRID_SPEED / 1000.0;
    if c.grid_phase >= 1.0 {
        c.grid_phase -= 1.0;
    }
    c.water_phase += elapsed * WATER_SPEED * TWO_PI;
    c.cloud_offset += elapsed * wind;
}

/// The letterboxed view band at a zoom, and the screen rows of the bars at a window height.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxLetterbox {
    pub left: f32,
    pub right: f32,
    pub top: f32,
    pub bottom: f32,
    /// the top bar's end row (`height / 1200 * top`)
    pub top_bar_end: f32,
    /// the bottom bar's first row
    pub bottom_bar_start: f32,
}

/// The view band at `zoom` (0: the letterbox's 0.4255 band in the middle, 1: the full view's 35 to 1085).
pub fn letterbox(zoom: f32, height: f32) -> EeGfxLetterbox {
    let half = (VIEW_HEIGHT - (VIEW_RIGHT - VIEW_LEFT) * LETTERBOX_ASPECT) * 0.5;
    let inverse = 1.0 - zoom;
    let top = inverse * half + VIEW_TOP * zoom;
    let bottom = (VIEW_HEIGHT - half) * inverse + VIEW_BOTTOM * zoom;
    EeGfxLetterbox {
        left: VIEW_LEFT,
        right: VIEW_RIGHT,
        top,
        bottom,
        top_bar_end: height * VIEW_SCALE_Y * top,
        bottom_bar_start: height * VIEW_SCALE_Y * bottom,
    }
}

/// What a frame's fade and letterbox step asks the C++ to do.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSceneStep {
    /// the light fade moved (the terrain is lit again)
    pub fade_changed: bool,
    /// the new zoom (`gF4BE374`)
    pub zoom: f32,
    /// clear the bars (they were flagged, or the zoom is 0)
    pub clear_bars: bool,
    /// the zoom changed: set the view band and clear the bars (one row more at the top)
    pub set_view: bool,
    /// the full view again: show the UI's backgrounds
    pub show_backgrounds: bool,
    /// the full view left: hide them
    pub hide_backgrounds: bool,
}

// the C++'s comparisons (a NaN passes through)
#[allow(clippy::manual_clamp)]
fn clamp_unit(v: f32) -> f32 {
    if v > 1.0 {
        1.0
    } else if v < 0.0 {
        0.0
    } else {
        v
    }
}

/// `Fn10C036`'s fade and letterbox for `elapsed` milliseconds, from the zoom `zoom`.
pub fn step(s: &mut EeGfxSceneFade, elapsed: f32, zoom: f32) -> EeGfxSceneStep {
    let mut r = EeGfxSceneStep::default();
    s.fade_time += elapsed;
    let progress = s.fade_rate as f64 * s.fade_time as f64;
    let start = s.light_fade_start as f64;
    let fade =
        if s.fade_down == 0 { (start + (1.0 - start) * progress) as f32 } else { ((1.0 - progress) * start) as f32 };
    let fade = clamp_unit(fade);
    r.fade_changed = fade != s.light_fade;
    s.light_fade = fade;
    s.letterbox_time += elapsed;
    let progress = s.letterbox_rate as f64 * s.letterbox_time as f64;
    let start = s.letterbox_start as f64;
    let next = if s.letterbox_direction == 1 {
        (start + (1.0 - start) * progress) as f32
    } else {
        ((1.0 - progress) * start) as f32
    };
    let next = clamp_unit(next);
    if s.bars_cleared || next == 0.0 {
        r.clear_bars = true;
        s.bars_cleared = false;
    }
    if next != zoom {
        r.set_view = true;
        if next == 0.0 {
            s.bars_cleared = true;
        }
        r.show_backgrounds = next == 1.0;
        r.hide_backgrounds = zoom == 1.0;
    }
    r.zoom = next;
    r
}

/// The time-of-day light tables (`EEInitialiseScene`'s 17 key frames: `gU4E9348` the colours, `gU4E92EC` the light's
/// intensity, `gU4E92A8` the terrain's ambient intensity, `gU4E9264` the units'; `gU4E9338` the ambient colour,
/// `gU4E9240` the three fill lights) and the sun's and moon's directions.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxDaylightTables {
    pub colors: [EeGfxColor; 17],
    pub light: [f32; 17],
    pub ambient: [f32; 17],
    pub unit_ambient: [f32; 17],
    pub ambient_color: EeGfxColor,
    pub fill: [GeVector; 3],
    pub sun: GeVector,
    pub moon: GeVector,
}

/// The lights at a time of day.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxDaylight {
    /// the lit direction (`gU4E9210`): the moon's at night
    pub direction: GeVector,
    /// the terrain's and units' light colour
    pub light: EeGfxColor,
    pub light_intensity: f32,
    pub ambient_intensity: f32,
    pub unit_ambient_intensity: f32,
    /// the prelit colour (and `mColor54`), clamped; its half
    pub prelit: EeGfxColor,
    pub dim_prelit: EeGfxColor,
}

fn dot(a: GeVector, b: GeVector) -> f32 {
    // GEVector::DotProduct: a is `this`, b the argument
    b.z * a.z + b.y * a.y + b.x * a.x
}

// the C++'s comparisons (a NaN passes through)
#[allow(clippy::manual_clamp)]
fn clamp_color(c: EeGfxColor) -> EeGfxColor {
    // GEColor::Normalize
    let one = |v: f32| {
        if v > 1.0 {
            1.0
        } else if v < 0.0 {
            0.0
        } else {
            v
        }
    };
    EeGfxColor { red: one(c.red), green: one(c.green), blue: one(c.blue) }
}

/// `Fn10C036`'s blend of the key frames around `time_of_day` (in [0, 1]; past 17/16 the original read past its
/// tables: the index is held at 16 here).
pub fn daylight(t: &EeGfxDaylightTables, time_of_day: f32) -> EeGfxDaylight {
    let direction = if (MOON_FROM..=MOON_TO).contains(&time_of_day) { t.moon } else { t.sun };
    let time = time_of_day * 16.0;
    let fraction = time - um::floor(time) as f32;
    let index = (um::floor_unsigned(time) as usize).min(16);
    let next = (index + 1).min(16);
    let rest = 1.0 - fraction;
    let (a, b) = (t.colors[next], t.colors[index]);
    let blend = EeGfxColor {
        red: a.red * fraction + b.red * rest,
        green: a.green * fraction + b.green * rest,
        blue: a.blue * fraction + b.blue * rest,
    };
    let light_intensity = fraction * t.light[next] + rest * t.light[index];
    let ambient_intensity = fraction * t.ambient[next] + rest * t.ambient[index];
    let scale = fraction * t.unit_ambient[next] + rest * t.unit_ambient[index];
    let mut color = EeGfxColor {
        red: t.ambient_color.red * scale,
        green: t.ambient_color.green * scale,
        blue: t.ambient_color.blue * scale,
    };
    for fill in t.fill {
        let d = dot(fill, direction);
        if d > 0.0 {
            color.red = color.red + blend.red * scale * d * FILL_SHARE;
            color.green = color.green + blend.green * scale * d * FILL_SHARE;
            color.blue = color.blue + blend.blue * scale * d * FILL_SHARE;
        }
    }
    let prelit = clamp_color(color);
    EeGfxDaylight {
        direction,
        light: blend,
        light_intensity,
        ambient_intensity,
        unit_ambient_intensity: scale,
        prelit,
        dim_prelit: EeGfxColor { red: prelit.red * 0.5, green: prelit.green * 0.5, blue: prelit.blue * 0.5 },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fades_start_from_the_current_value() {
        let mut s = EeGfxSceneFade::default();
        start_fade(&mut s, true, 4);
        assert_eq!((s.fade_down, s.fade_rate, s.light_fade_start), (1, 0.25, 1.0));
        let r = step(&mut s, 2.0, 1.0);
        assert!(r.fade_changed);
        assert_eq!(s.light_fade, 0.5);
        // at once: the f32::MAX rate's product stays finite in f64
        start_fade(&mut s, false, 0);
        let r = step(&mut s, 16.0, 1.0);
        assert_eq!((s.light_fade, r.fade_changed), (1.0, true));
        let r = step(&mut s, 16.0, 1.0);
        assert!(!r.fade_changed);
        s.bars_cleared = true;
        reset(&mut s);
        assert_eq!(s, EeGfxSceneFade { bars_cleared: true, ..EeGfxSceneFade::default() });
    }

    #[test]
    fn the_letterbox_closes_clears_and_reopens() {
        let mut s = EeGfxSceneFade::default();
        start_letterbox(&mut s, false, 0, 1.0);
        let r = step(&mut s, 10.0, 1.0);
        assert_eq!(r, EeGfxSceneStep { zoom: 0.0, clear_bars: true, set_view: true, hide_backgrounds: true, ..r });
        assert!(s.bars_cleared && !r.show_backgrounds);
        // at zoom 0 the bars are cleared every frame
        let r = step(&mut s, 10.0, 0.0);
        assert!(r.clear_bars && !r.set_view && !s.bars_cleared);
        start_letterbox(&mut s, true, 2, 0.0);
        let r = step(&mut s, 1.0, 0.0);
        assert_eq!((r.zoom, r.clear_bars, r.set_view, r.show_backgrounds), (0.5, false, true, false));
        let r = step(&mut s, 1.0, 0.5);
        assert_eq!((r.zoom, r.show_backgrounds, r.hide_backgrounds), (1.0, true, false));
        let b = letterbox(0.0, 1200.0);
        assert_eq!((b.top, b.bottom), (259.57446, 940.42554));
        let b = letterbox(1.0, 600.0);
        assert_eq!((b.top, b.bottom, b.top_bar_end, b.bottom_bar_start), (35.0, 1085.0, 17.5, 542.5));
    }

    #[test]
    fn clocks_wrap_the_grid() {
        let mut c = EeGfxSceneClocks { grid_phase: 0.9, ..Default::default() };
        step_clocks(&mut c, 250.0, 2.0);
        assert!((c.grid_phase - 0.1).abs() < 1e-6);
        assert_eq!(c.cloud_offset, 500.0);
        assert_eq!(c.water_phase, 250.0 * 5.0e-05 * 6.2831855);
    }

    #[test]
    fn daylight_blends_neighbouring_key_frames() {
        let mut t = EeGfxDaylightTables::default();
        for i in 0..17 {
            t.colors[i] = EeGfxColor { red: i as f32 / 16.0, green: 0.5, blue: 1.0 };
            t.light[i] = i as f32;
            t.ambient[i] = 2.0 * i as f32;
            t.unit_ambient[i] = 1.0;
        }
        t.ambient_color = EeGfxColor { red: 0.25, green: 0.25, blue: 0.25 };
        t.fill = [GeVector { x: 0.0, y: 0.0, z: 1.0 }, GeVector { x: 0.0, y: 0.0, z: -1.0 }, GeVector::default()];
        t.sun = GeVector { x: 0.0, y: 0.0, z: 1.0 };
        t.moon = GeVector { x: 1.0, y: 0.0, z: 0.0 };
        let d = daylight(&t, 0.25 + 1.0 / 32.0);
        assert_eq!(d.direction, t.sun);
        assert_eq!((d.light_intensity, d.ambient_intensity), (4.5, 9.0));
        assert_eq!(d.light.red, 4.5 / 16.0);
        assert_eq!(d.prelit.green, 0.25 + 0.5 * 0.333333);
        assert_eq!(d.dim_prelit.green, d.prelit.green * 0.5);
        // the moon at night: no fill light faces it
        let d = daylight(&t, 0.75);
        assert_eq!((d.direction, d.prelit.red), (t.moon, 0.25));
        // the last key frame holds
        let d = daylight(&t, 1.0);
        assert_eq!(d.light_intensity, 16.0);
        // the blue clamps at 1
        let d = daylight(&t, 0.0);
        assert_eq!(d.prelit.blue, 0.25 + 0.333333);
    }
}
