//! Formations (`src/Empire Earth/Formations`): where a group's units stand. Geometry in, slots out: the C++ classes
//! (`EEFLine`, `EEFWedge`, `EEFCircle` and their presets) keep their members, and their constructors and `Place`
//! call this crate through `ffi` with plain data.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): every operation is `f32` in the C++'s order, with the engine's
//! lookup-table trigonometry (`ee_rmap::math`, `UMCos`/`UMSin`/`UMArcTan2`), so the slots are bit-equal to the C++'s.
//! `tools/sim_capture.py` is the gate: the C++ Place calls of seeded games, replayed here (`capture`).

#![allow(clippy::approx_constant, clippy::excessive_precision)]

pub mod capture;
pub mod ffi;

use ee_rmap::math::{um_arctan2, um_cos, um_sin};

/// `GEVector` / `GE3DPoint`: three floats.
pub type Vec3 = [f32; 3];

/// The formation's frame: `mOrientation`'s rotation (rows of `GETransformation::mMatrix`; `Apply(GEVector)` reads
/// nothing else), `mCenter` and `mHeading`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeFormationFrame {
    pub orientation: [[f32; 3]; 3],
    pub center: Vec3,
    pub heading: f32,
}

/// One slot: the position, the heading and the index of its unit in the input.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeFormationSlot {
    pub position: Vec3,
    pub heading: f32,
    pub unit: u32,
}

/// An `EEFLine`'s members besides its offsets.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeFormationLine {
    pub count: i32,
    /// `inRows + 1`
    pub rows: i32,
    pub spacing: f32,
    pub depth: f32,
}

/// An `EEFWedge`'s members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeFormationWedge {
    pub left_arm: Vec3,
    pub right_arm: Vec3,
    pub spacing: f32,
    pub row_depth: f32,
    pub count: u32,
}

/// An `EEFCircle`'s members that `Place` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeFormationCircle {
    /// 0: the formation heading, 1: facing the centre, 2: facing out
    pub mode: i32,
    pub half_arc: f32,
    pub min_radius: f32,
}

/// `max(a, b)` of `<windows.h>`: `a > b ? a : b` (NaN and signed zeros as the C++).
fn max(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

const MINUS_HALF_PI: f32 = -1.5707964;

/// `UMTan`: the table sine over the table cosine; an infinity where the cosine is zero.
pub fn um_tan(angle: f32) -> f32 {
    let cosine = um_cos(angle);
    if cosine != 0.0 {
        return um_sin(angle) / cosine;
    }
    if (0.0..1.5707964).contains(&angle) || (-3.1415927..=MINUS_HALF_PI).contains(&angle) {
        return f32::INFINITY;
    }
    f32::NEG_INFINITY
}

/// `EEGetHeadingBetween`.
pub fn heading_between(from: Vec3, to: Vec3) -> f32 {
    um_arctan2(-(to[0] - from[0]), to[1] - from[1])
}

/// A `GETransformation`'s rotation part.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rotation(pub [[f32; 3]; 3]);

impl Rotation {
    pub const IDENTITY: Rotation = Rotation([[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]);

    /// `SetOrientationYPR`.
    pub fn yaw_pitch_roll(yaw: f32, pitch: f32, roll: f32) -> Self {
        let cos_yaw = um_cos(yaw);
        let sin_yaw = um_sin(yaw);
        let neg_sin_yaw = -sin_yaw;
        let cos_pitch = um_cos(pitch);
        let sin_pitch = um_sin(pitch);
        let neg_sin_pitch = -sin_pitch;
        let cos_roll = um_cos(roll);
        let sin_roll = um_sin(roll);
        let neg_sin_roll = -sin_roll;
        let yp01 = neg_sin_yaw * cos_pitch;
        let yp02 = neg_sin_yaw * neg_sin_pitch;
        let yp11 = cos_yaw * cos_pitch;
        let yp12 = cos_yaw * neg_sin_pitch;
        Rotation([
            [cos_yaw * cos_roll + yp02 * sin_roll, yp01, cos_yaw * neg_sin_roll + yp02 * cos_roll],
            [sin_yaw * cos_roll + yp12 * sin_roll, yp11, sin_yaw * neg_sin_roll + yp12 * cos_roll],
            [cos_pitch * sin_roll, sin_pitch, cos_pitch * cos_roll],
        ])
    }

    /// `Rotate(yaw, pitch, roll)`: the new rotation applied after this one (`Apply(const GETransformation&)`).
    pub fn rotate(&mut self, yaw: f32, pitch: f32, roll: f32) {
        let a = Rotation::yaw_pitch_roll(yaw, pitch, roll).0;
        let b = self.0;
        let mut out = [[0.0f32; 3]; 3];
        for (i, row) in out.iter_mut().enumerate() {
            for (j, value) in row.iter_mut().enumerate() {
                *value = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
            }
        }
        self.0 = out;
    }

    /// `Apply(const GEVector&, GEVector&)`: the sums in the C++'s order.
    pub fn apply(&self, v: Vec3) -> Vec3 {
        let m = &self.0;
        [
            v[2] * m[0][2] + v[1] * m[0][1] + v[0] * m[0][0],
            v[0] * m[1][0] + v[2] * m[1][2] + v[1] * m[1][1],
            v[0] * m[2][0] + v[2] * m[2][2] + v[1] * m[2][1],
        ]
    }
}

/// `GE3DPoint += GEVector`.
fn add(a: Vec3, b: Vec3) -> Vec3 {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

/// `EEFLine`'s constructor: the column offsets (in spacings), the centre column first, then out to the left and right
/// in turn. An even count starts half a spacing either side of the centre.
pub fn line_offsets(spacing: f32, count: i32) -> Vec<f32> {
    let mut offsets = vec![0.0f32; count.max(0) as usize];
    let (mut offset, mut left, mut right, mut index);
    if count % 2 == 0 {
        offset = spacing * -0.5;
        left = count / 2 - 1;
        right = count / 2;
        index = 0usize;
    } else {
        offset = 0.0;
        offsets[0] = 0.0;
        left = count / 2 - 1;
        right = count / 2 + 1;
        index = 1;
    }
    while left >= 0 && right < count {
        offset += spacing;
        if left >= 0 {
            offsets[index] = -offset;
            left -= 1;
            index += 1;
        }
        offsets[index] = offset;
        right += 1;
        index += 1;
    }
    offsets
}

/// `EEFLine::Place`: rows of `count` behind the centre, spaced by the largest collision radius; every row but the
/// odd ones shifted by `depth`, an empty row after every `rows - 1`.
pub fn line_place(
    frame: &EeFormationFrame,
    line: &EeFormationLine,
    offsets: &[f32],
    radii: &[f32],
) -> Vec<EeFormationSlot> {
    let rotation = Rotation(frame.orientation);
    let mut radius = 0.0f32;
    for &r in radii {
        radius = max(r, radius);
    }
    let mut slots = Vec::with_capacity(radii.len());
    let mut row: i32 = 1;
    let mut depth: i32 = 0;
    for j in 0..radii.len() {
        let column = (j as i32) % line.count;
        let mut x = radius * offsets[column as usize];
        if row % 2 != 1 {
            x += line.depth;
        }
        let y = -(depth as f32 * line.spacing * radius);
        let moved = rotation.apply([x, y, 0.0]);
        slots.push(EeFormationSlot { position: add(frame.center, moved), heading: frame.heading, unit: j as u32 });
        if column != line.count - 1 {
            continue;
        }
        row += 1;
        depth += 1;
        if line.rows != 0 && row % line.rows == 0 {
            row += 1;
            depth += 1;
        }
    }
    slots
}

/// `EEFWedge`'s constructor: arms opening at `angle` with `spacing` between units; the depth to the next wedge is
/// half a wedge's units, at most what the arms reach.
pub fn wedge_geometry(spacing: f32, angle: f32, count: u32) -> EeFormationWedge {
    let mut row_depth = count as f32 * spacing * 0.5;
    if angle != 0.0 {
        let limit = spacing / um_tan(angle * 0.5);
        if limit < row_depth {
            row_depth = limit;
        }
    }
    let half = angle * 0.5;
    let x = um_cos(1.5707964 - half) * spacing;
    let y = um_cos(half) * spacing;
    EeFormationWedge { left_arm: [-x, -y, 0.0], right_arm: [x, -y, 0.0], spacing, row_depth, count }
}

/// `EEFWedge::Place`: a leader at the point, then pairs down the left and right arms; a further wedge behind after
/// every `count` units. The arms are scaled by the largest collision radius.
// `%` as the C++ (a zero count divides by zero there and panics here; `is_multiple_of` would hide it)
#[allow(clippy::manual_is_multiple_of)]
pub fn wedge_place(frame: &EeFormationFrame, wedge: &EeFormationWedge, radii: &[f32]) -> Vec<EeFormationSlot> {
    let rotation = Rotation(frame.orientation);
    let count = radii.len() as i32;
    let mut radius = 0.0f32;
    for &r in radii {
        radius = max(r, radius);
    }
    let left_arm = [wedge.left_arm[0] * radius, wedge.left_arm[1] * radius, wedge.left_arm[2]];
    let right_arm = [wedge.right_arm[0] * radius, wedge.right_arm[1] * radius, wedge.right_arm[2]];
    let left_step = rotation.apply(left_arm);
    let right_step = rotation.apply(right_arm);
    let mut center = frame.center;
    let mut left = center;
    let mut right = center;
    let mut slots = Vec::with_capacity(radii.len());
    let mut placed: i32 = 1;
    let mut j: u32 = 0;
    while (j as i32) < count {
        if j % wedge.count == 0 {
            if placed != 1 {
                let step = rotation.apply([0.0, -(radius * wedge.row_depth), 0.0]);
                center = add(center, step);
            }
            slots.push(EeFormationSlot { position: center, heading: frame.heading, unit: j });
            left = center;
            right = center;
        } else {
            left = add(left, left_step);
            right = add(right, right_step);
            slots.push(EeFormationSlot { position: left, heading: frame.heading, unit: j });
            if placed < count {
                j += 1;
                placed += 1;
                slots.push(EeFormationSlot { position: right, heading: frame.heading, unit: j });
            }
        }
        j += 1;
        placed += 1;
    }
    slots
}

/// `EEFCircle::Place`: the units evenly round a ring sized to their largest collision radius (at least
/// `min_radius`), the last unit first; a lone unit stands at the centre. No units, no slots.
pub fn circle_place(frame: &EeFormationFrame, circle: &EeFormationCircle, radii: &[f32]) -> Vec<EeFormationSlot> {
    let n = radii.len();
    if n == 0 {
        return Vec::new();
    }
    if n == 1 {
        return vec![EeFormationSlot { position: frame.center, heading: 0.0, unit: 0 }];
    }
    let mut radius = 0.5f32;
    for &r in radii.iter().rev() {
        radius = max(r, radius);
    }
    let mut distance = n as f32 * radius * 0.159154937;
    distance = max(distance, circle.min_radius);
    let mut angle = circle.half_arc / n as f32;
    angle += angle;
    let mut transformation = Rotation::yaw_pitch_roll(angle, 0.0, 0.0);
    let mut rotated = false;
    let mut slots = Vec::with_capacity(n);
    for j in (0..n).rev() {
        if rotated {
            transformation.rotate(angle, 0.0, 0.0);
        } else {
            rotated = true;
        }
        let position = add(frame.center, transformation.apply([0.0, distance, 0.0]));
        let heading = match circle.mode {
            0 => frame.heading,
            1 => heading_between(position, frame.center),
            2 => heading_between(frame.center, position),
            _ => 0.0,
        };
        slots.push(EeFormationSlot { position, heading, unit: j as u32 });
    }
    slots
}

/// The presets of `EEFormation::Create` (types 0-7) and their constructor arguments.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Preset {
    /// spacing, depth, count, rows (`inRows`, stored + 1)
    Line(f32, f32, i32, i32),
    /// spacing, angle, count
    Wedge(f32, f32, u32),
    /// mode
    Circle(i32),
}

/// The preset of a formation type, `None` for a type `Create` does not make.
pub fn preset(kind: i32) -> Option<Preset> {
    Some(match kind {
        0 => Preset::Line(1.0, 0.0, 5, 5),
        1 => Preset::Line(1.0, 0.0, 10, 5),
        2 => Preset::Line(2.0, 0.0, 5, 5),
        3 => Preset::Line(1.5, 0.75, 5, 5),
        4 => Preset::Wedge(1.0, 0.785398185, 0x15),
        5 => Preset::Wedge(1.0, 1.39626348, 0x15),
        6 => Preset::Circle(1),
        7 => Preset::Circle(2),
        _ => return None,
    })
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests;
