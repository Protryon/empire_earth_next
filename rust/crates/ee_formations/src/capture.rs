//! The simulation capture's formation records (`ee/EESimCapture.h`, written by the C++ `Place` wrappers with
//! `EE_SIM_CAPTURE`): a call's inputs and the slots it produced. `replay` recomputes the slots here; the gate
//! (`tools/sim_capture.py replay`) runs it over captures recorded from the C++ before it was deleted.

use crate::{
    circle_place, line_offsets, line_place, preset, wedge_geometry, wedge_place, EeFormationCircle, EeFormationFrame,
    EeFormationLine, EeFormationSlot, EeFormationWedge, Preset,
};

/// "FMLN", "FMWG", "FMCI" (little-endian u32 of the four letters).
pub const TAG_LINE: u32 = u32::from_le_bytes(*b"FMLN");
pub const TAG_WEDGE: u32 = u32::from_le_bytes(*b"FMWG");
pub const TAG_CIRCLE: u32 = u32::from_le_bytes(*b"FMCI");

/// One record of a capture file: its tag and payload.
#[derive(Clone, Debug, PartialEq)]
pub struct Record<'a> {
    pub tag: u32,
    pub payload: &'a [u8],
}

/// Splits a capture file into records; `Err` names the offset of a truncated one.
pub fn records(data: &[u8]) -> Result<Vec<Record<'_>>, String> {
    let mut out = Vec::new();
    let mut at = 0usize;
    while at < data.len() {
        if data.len() - at < 8 {
            return Err(format!("truncated record header at {at}"));
        }
        let tag = u32::from_le_bytes(data[at..at + 4].try_into().unwrap());
        let size = u32::from_le_bytes(data[at + 4..at + 8].try_into().unwrap()) as usize;
        at += 8;
        if data.len() - at < size {
            return Err(format!("truncated record payload at {at}"));
        }
        out.push(Record { tag, payload: &data[at..at + size] });
        at += size;
    }
    Ok(out)
}

struct Reader<'a> {
    data: &'a [u8],
    at: usize,
}

impl Reader<'_> {
    fn u32(&mut self) -> Result<u32, String> {
        let bytes = self.data.get(self.at..self.at + 4).ok_or_else(|| format!("short record at {}", self.at))?;
        self.at += 4;
        Ok(u32::from_le_bytes(bytes.try_into().unwrap()))
    }
    fn i32(&mut self) -> Result<i32, String> {
        Ok(self.u32()? as i32)
    }
    fn f32(&mut self) -> Result<f32, String> {
        Ok(f32::from_bits(self.u32()?))
    }
    fn vec3(&mut self) -> Result<[f32; 3], String> {
        Ok([self.f32()?, self.f32()?, self.f32()?])
    }
}

/// A formation's own members, by kind.
#[derive(Clone, Debug, PartialEq)]
pub enum Shape {
    Line(EeFormationLine, Vec<f32>),
    Wedge(EeFormationWedge),
    Circle(EeFormationCircle),
}

/// One captured `Place` call.
#[derive(Clone, Debug, PartialEq)]
pub struct FormationCall {
    /// `EEFormation::mType`
    pub kind: i32,
    pub frame: EeFormationFrame,
    pub shape: Shape,
    pub radii: Vec<f32>,
    pub slots: Vec<EeFormationSlot>,
}

/// Decodes a formation record; `None` for another tag.
pub fn decode(record: &Record) -> Option<Result<FormationCall, String>> {
    if ![TAG_LINE, TAG_WEDGE, TAG_CIRCLE].contains(&record.tag) {
        return None;
    }
    Some(decode_formation(record))
}

fn decode_formation(record: &Record) -> Result<FormationCall, String> {
    let mut r = Reader { data: record.payload, at: 0 };
    let kind = r.i32()?;
    let orientation = [r.vec3()?, r.vec3()?, r.vec3()?];
    let center = r.vec3()?;
    let heading = r.f32()?;
    let frame = EeFormationFrame { orientation, center, heading };
    let shape = match record.tag {
        TAG_LINE => {
            let line = EeFormationLine { count: r.i32()?, rows: r.i32()?, spacing: r.f32()?, depth: r.f32()? };
            let offsets = (0..line.count.max(0)).map(|_| r.f32()).collect::<Result<_, _>>()?;
            Shape::Line(line, offsets)
        }
        TAG_WEDGE => Shape::Wedge(EeFormationWedge {
            left_arm: r.vec3()?,
            right_arm: r.vec3()?,
            spacing: r.f32()?,
            row_depth: r.f32()?,
            count: r.u32()?,
        }),
        _ => Shape::Circle(EeFormationCircle { mode: r.i32()?, half_arc: r.f32()?, min_radius: r.f32()? }),
    };
    let n = r.u32()?;
    let radii = (0..n).map(|_| r.f32()).collect::<Result<_, _>>()?;
    let m = r.u32()?;
    let mut slots = Vec::with_capacity(m as usize);
    for _ in 0..m {
        let position = r.vec3()?;
        let heading = r.f32()?;
        let unit = r.u32()?;
        slots.push(EeFormationSlot { position, heading, unit });
    }
    if r.at != record.payload.len() {
        return Err(format!("{} bytes left over in a formation record", record.payload.len() - r.at));
    }
    Ok(FormationCall { kind, frame, shape, radii, slots })
}

/// The slots of a captured call recomputed by this crate.
pub fn replay(call: &FormationCall) -> Vec<EeFormationSlot> {
    match &call.shape {
        Shape::Line(line, offsets) => line_place(&call.frame, line, offsets, &call.radii),
        Shape::Wedge(wedge) => wedge_place(&call.frame, wedge, &call.radii),
        Shape::Circle(circle) => circle_place(&call.frame, circle, &call.radii),
    }
}

/// Whether the captured members are what this crate's constructor gives the preset of `kind` (the constructors are
/// gated through the captured state).
pub fn constructed_as_preset(call: &FormationCall) -> bool {
    match (preset(call.kind), &call.shape) {
        (Some(Preset::Line(spacing, depth, count, rows)), Shape::Line(line, offsets)) => {
            let expected = EeFormationLine { count, rows: rows + 1, spacing, depth };
            bits_eq(&[line.spacing, line.depth], &[expected.spacing, expected.depth])
                && line.count == expected.count
                && line.rows == expected.rows
                && bits_eq(offsets, &line_offsets(spacing, count))
        }
        (Some(Preset::Wedge(spacing, angle, count)), Shape::Wedge(wedge)) => {
            let expected = wedge_geometry(spacing, angle, count);
            bits_eq(&wedge_floats(wedge), &wedge_floats(&expected)) && wedge.count == expected.count
        }
        (Some(Preset::Circle(mode)), Shape::Circle(circle)) => {
            // the arc and the minimum radius may be the caller's (EECPAttackFormationNode sets a half arc of pi/2,
            // EEHerd a minimum radius): only the mode is the constructor's alone
            circle.mode == mode
        }
        _ => false,
    }
}

fn wedge_floats(w: &EeFormationWedge) -> Vec<f32> {
    let mut v = Vec::new();
    v.extend_from_slice(&w.left_arm);
    v.extend_from_slice(&w.right_arm);
    v.push(w.spacing);
    v.push(w.row_depth);
    v
}

/// Equal bit patterns (a NaN equals the same NaN; 0.0 and -0.0 differ).
pub fn bits_eq(a: &[f32], b: &[f32]) -> bool {
    a.len() == b.len() && a.iter().zip(b).all(|(x, y)| x.to_bits() == y.to_bits())
}

/// Equal slots, floats by bits.
pub fn slots_eq(a: &[EeFormationSlot], b: &[EeFormationSlot]) -> bool {
    a.len() == b.len()
        && a.iter().zip(b).all(|(x, y)| {
            bits_eq(&x.position, &y.position) && x.heading.to_bits() == y.heading.to_bits() && x.unit == y.unit
        })
}
