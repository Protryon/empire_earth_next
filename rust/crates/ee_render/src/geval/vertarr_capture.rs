//! TEST ONLY: the store gate's capture of the vertex and polygon arrays (tools/eemodel/storegate). While it is on,
//! every array made keeps a shadow of what was set on it, as the gate's memory arrays kept it before: every field
//! as floats (a standard array's colours and opacities too), the indices as 32 bits, and a count of the sets past
//! the end (which then change nothing). Reads, sizes and copies answer from the shadow, so a loader sees what it saw
//! through the gate's arrays; the arrays themselves are still written where the index is theirs. The gate reads the
//! shadows (`ge::store_capture`). The switch is the calling thread's (the gate loads on one thread; the tests run
//! side by side); off (the game) it costs one thread-local read per call.

use std::cell::Cell;
use std::collections::BTreeMap;
use std::sync::Mutex;

thread_local! {
    static ON: Cell<bool> = const { Cell::new(false) };
}

/// A vertex array's shadow: `count` vertices of three, three, two, three and one floats.
#[derive(Clone, Default)]
pub struct VaShadow {
    pub kind: u32,
    pub points: Vec<f32>,
    pub normals: Vec<f32>,
    pub uvs: Vec<f32>,
    pub colors: Vec<f32>,
    pub opacities: Vec<f32>,
    pub out_of_range: u32,
}

impl VaShadow {
    fn new(kind: u32, count: u32) -> Self {
        let n = count as usize;
        VaShadow {
            kind,
            points: vec![0.0; 3 * n],
            normals: vec![0.0; 3 * n],
            uvs: vec![0.0; 2 * n],
            colors: vec![0.0; 3 * n],
            opacities: vec![0.0; n],
            out_of_range: 0,
        }
    }

    pub fn count(&self) -> usize {
        self.opacities.len()
    }

    /// The field's floats of vertex `index` (`None` past the end).
    pub fn field(&mut self, field: u32, index: u32) -> Option<&mut [f32]> {
        let i = index as usize;
        if i >= self.count() {
            return None;
        }
        Some(match field {
            1 => &mut self.colors[3 * i..3 * i + 3],
            2 => &mut self.normals[3 * i..3 * i + 3],
            3 => &mut self.opacities[i..i + 1],
            4 => &mut self.points[3 * i..3 * i + 3],
            _ => &mut self.uvs[2 * i..2 * i + 2],
        })
    }
}

/// A polygon array's shadow: three indices per triangle.
#[derive(Clone, Default)]
pub struct PaShadow {
    pub indices: Vec<u32>,
    pub out_of_range: u32,
}

static VERTICES: Mutex<BTreeMap<usize, VaShadow>> = Mutex::new(BTreeMap::new());
static POLYGONS: Mutex<BTreeMap<usize, PaShadow>> = Mutex::new(BTreeMap::new());

/// Turns the capture on or off on this thread (the arrays it makes while it is on have shadows).
pub fn set_on(on: bool) {
    ON.with(|o| o.set(on));
}

#[inline]
pub fn on() -> bool {
    ON.with(|o| o.get())
}

pub fn va_new(array: usize, kind: u32, count: u32) {
    VERTICES.lock().unwrap_or_else(|e| e.into_inner()).insert(array, VaShadow::new(kind, count));
}

pub fn pa_new(array: usize, count: u32) {
    let shadow = PaShadow { indices: vec![0; 3 * count as usize], out_of_range: 0 };
    POLYGONS.lock().unwrap_or_else(|e| e.into_inner()).insert(array, shadow);
}

pub fn va_drop(array: usize) {
    VERTICES.lock().unwrap_or_else(|e| e.into_inner()).remove(&array);
}

pub fn pa_drop(array: usize) {
    POLYGONS.lock().unwrap_or_else(|e| e.into_inner()).remove(&array);
}

/// `f` on the vertex array's shadow, if it has one.
pub fn with_va<R>(array: usize, f: impl FnOnce(&mut VaShadow) -> R) -> Option<R> {
    VERTICES.lock().unwrap_or_else(|e| e.into_inner()).get_mut(&array).map(f)
}

/// `f` on the polygon array's shadow, if it has one.
pub fn with_pa<R>(array: usize, f: impl FnOnce(&mut PaShadow) -> R) -> Option<R> {
    POLYGONS.lock().unwrap_or_else(|e| e.into_inner()).get_mut(&array).map(f)
}

/// `CopyFrom`: the gate's arrays took the other's whole fields.
pub fn va_copy(to: usize, from: usize) -> bool {
    let mut map = VERTICES.lock().unwrap_or_else(|e| e.into_inner());
    let Some(source) = map.get(&from).cloned() else { return false };
    let Some(target) = map.get_mut(&to) else { return false };
    let kind = target.kind;
    let out_of_range = target.out_of_range;
    *target = VaShadow { kind, out_of_range, ..source };
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_shadow_keeps_every_field_and_counts_the_sets_past_the_end() {
        let mut s = VaShadow::new(0, 2);
        s.field(1, 1).unwrap().copy_from_slice(&[0.5, 0.25, 0.125]);
        s.field(5, 0).unwrap().copy_from_slice(&[1.0, 2.0]);
        assert!(s.field(4, 2).is_none());
        assert_eq!(s.colors, [0.0, 0.0, 0.0, 0.5, 0.25, 0.125]);
        assert_eq!(s.uvs, [1.0, 2.0, 0.0, 0.0]);
        assert_eq!(s.count(), 2);
    }

    #[test]
    fn a_copy_takes_the_other_arrays_fields_and_size() {
        va_new(0x1000, 0, 1);
        va_new(0x2000, 1, 3);
        with_va(0x2000, |s| s.field(4, 2).unwrap().copy_from_slice(&[7.0, 8.0, 9.0]));
        with_va(0x1000, |s| s.out_of_range = 4);
        assert!(va_copy(0x1000, 0x2000));
        let (kind, count, last, oor) = with_va(0x1000, |s| (s.kind, s.count(), s.points[8], s.out_of_range)).unwrap();
        assert_eq!((kind, count, last, oor), (0, 3, 9.0, 4));
        assert!(!va_copy(0x1000, 0x3000));
        va_drop(0x1000);
        va_drop(0x2000);
        assert!(with_va(0x1000, |_| ()).is_none());
    }
}
