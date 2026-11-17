//! The earthquake's crack (`src/Empire Earth/Graphics Thread/EEE3DEarthquake.cpp`): the strip's triangles (eight per
//! segment over five vertices across), its texture rows as `BuildModel` lays them, `Queue`'s rows (each segment's
//! front and texture row as the crack grows) and the hold time a crack stopped over space keeps. The C++ keeps the
//! model and the ground: it places the five vertices across each row on the terrain (`GetGroundPoint`) and asks
//! the middle one's tile about space.
//!
//! This is the renderer's, not the simulation's: nothing here is simulation state (the hold time and the growing
//! flag are the effect's, set by its `Queue` as the original did).

/// The texture's u across the five vertices of a row.
pub const EE_GFX_QUAKE_ROW_U: [f32; 5] = [0.0, 0.25, 0.5, 0.75, 1.0];

/// One of `Queue`'s rows: the row's distance along the crack (its local y) and the texture's v there.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxQuakeRow {
    pub y: f32,
    pub front_v: f32,
}

/// Segment `segment`'s eight triangles (vertex indices; row `segment` is vertices `5 segment` .. `5 segment + 4`).
pub fn segment_polygons(segment: u32) -> [u32; 24] {
    let v = segment.wrapping_mul(5);
    [
        v + 5,
        v,
        v + 6,
        v + 6,
        v,
        v + 1,
        v + 6,
        v + 1,
        v + 7,
        v + 7,
        v + 1,
        v + 2,
        v + 7,
        v + 2,
        v + 8,
        v + 8,
        v + 2,
        v + 3,
        v + 8,
        v + 3,
        v + 9,
        v + 9,
        v + 3,
        v + 4,
    ]
}

/// `BuildModel`'s texture v of each segment's far row (0, 0.25, 0.5, ...: summed as the original summed it).
pub fn build_rows(out: &mut [f32]) {
    let mut v = 0.0f32;
    for r in out {
        *r = v;
        v += 0.25;
    }
}

/// `Queue`'s rows for a crack whose front is `distance` along it: each row up to the front at the front, the
/// rest at their own end, the texture's v following.
pub fn queue_rows(distance: f32, out: &mut [EeGfxQuakeRow]) {
    let mut v = 0.25f32;
    let mut end = 1.0f32;
    for r in out {
        if distance < end {
            r.y = distance;
            r.front_v = (1.0 - (end - distance)) * 0.25 + v - 0.25;
        } else {
            r.y = end;
            r.front_v = v;
        }
        end += 1.0;
        v += 0.25;
    }
}

/// The hold time of a growing crack that reached space (`Queue`): twice the time it still had to grow, and 15 s.
pub fn hold_time(length: f32, distance: f32, speed: f32) -> f32 {
    (length - distance) / speed * 2.0 + 15000.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_strip_joins_consecutive_rows() {
        let p = segment_polygons(2);
        assert!(p.iter().all(|&i| (10..20).contains(&i)));
        assert_eq!(&p[..3], &[15, 10, 16]);
    }

    #[test]
    fn the_rows_stop_at_the_front() {
        let mut rows = [EeGfxQuakeRow::default(); 4];
        queue_rows(1.5, &mut rows);
        assert_eq!(rows[0], EeGfxQuakeRow { y: 1.0, front_v: 0.25 });
        assert_eq!(rows[1].y, 1.5);
        assert_eq!(rows[1].front_v, 0.375);
        assert_eq!(rows[3].y, 1.5);
        let mut v = [0.0; 3];
        build_rows(&mut v);
        assert_eq!(v, [0.0, 0.25, 0.5]);
        assert_eq!(hold_time(10.0, 4.0, 0.5), 15024.0);
    }
}
