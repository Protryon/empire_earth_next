//! The Graphics Engine's value types as `#[repr(C)]` mirrors of the C++ classes, which stay the types callers use
//! (`include/lle/GE*.h`; the forwarding units `static_assert` each size and public offset). Rust owns the algorithms
//! over them; the trivial members (constructors, accessors, component-wise `+=`, file I/O) stay C++.

/// `GE2DPoint` (sizeof 8).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Ge2DPoint {
    pub x: f32,
    pub y: f32,
}

/// `GE3DPoint` (sizeof 0xc).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Ge3DPoint {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// `GEVector` (sizeof 0xc).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeVector {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// `GE3DLine` (sizeof 0x18): start, direction.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Ge3DLine {
    pub start: Ge3DPoint,
    pub direction: GeVector,
}

/// `GE3DPlane` (sizeof 0x18): a point on the plane, the normal.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Ge3DPlane {
    pub point: Ge3DPoint,
    pub normal: GeVector,
}

/// `GETransformation` (sizeof 0x40): row-major 4x4, the translation in column 3.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeTransformation {
    pub m: [[f32; 4]; 4],
}

/// `GEOrientation` (sizeof 0xc).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeOrientation {
    pub yaw: f32,
    pub pitch: f32,
    pub roll: f32,
}

/// `GEQuaternion` (sizeof 0x10).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeQuaternion {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// `GEBoundingBox` (sizeof 0x24): the two corners, then width (x), height (z) and depth (y).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeBoundingBox {
    pub bottom_left_front: Ge3DPoint,
    pub top_right_back: Ge3DPoint,
    pub width: f32,
    pub height: f32,
    pub depth: f32,
}

/// `GERectangle` (sizeof 0x10), in the C++'s member order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeRectangle {
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
}

const _: () = assert!(core::mem::size_of::<Ge2DPoint>() == 8);
const _: () = assert!(core::mem::size_of::<Ge3DPoint>() == 0xc);
const _: () = assert!(core::mem::size_of::<GeVector>() == 0xc);
const _: () = assert!(core::mem::size_of::<Ge3DLine>() == 0x18);
const _: () = assert!(core::mem::size_of::<Ge3DPlane>() == 0x18);
const _: () = assert!(core::mem::size_of::<GeTransformation>() == 0x40);
const _: () = assert!(core::mem::size_of::<GeOrientation>() == 0xc);
const _: () = assert!(core::mem::size_of::<GeQuaternion>() == 0x10);
const _: () = assert!(core::mem::size_of::<GeBoundingBox>() == 0x24);
const _: () = assert!(core::mem::size_of::<GeRectangle>() == 0x10);

impl Ge3DPoint {
    pub const fn new(x: f32, y: f32, z: f32) -> Self {
        Ge3DPoint { x, y, z }
    }
}

impl GeVector {
    pub const fn new(x: f32, y: f32, z: f32) -> Self {
        GeVector { x, y, z }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::offset_of;

    #[test]
    fn offsets_match_the_cpp_classes() {
        assert_eq!(offset_of!(Ge3DLine, direction), 0xc);
        assert_eq!(offset_of!(Ge3DPlane, normal), 0xc);
        assert_eq!(offset_of!(GeBoundingBox, top_right_back), 0xc);
        assert_eq!(offset_of!(GeBoundingBox, width), 0x18);
        assert_eq!(offset_of!(GeBoundingBox, height), 0x1c);
        assert_eq!(offset_of!(GeBoundingBox, depth), 0x20);
        assert_eq!(offset_of!(GeRectangle, left), 4);
        assert_eq!(offset_of!(GeRectangle, top), 0xc);
        assert_eq!(offset_of!(GeQuaternion, w), 0xc);
    }

    #[test]
    fn identity_is_the_default_constructor() {
        let t = GeTransformation::IDENTITY;
        for (i, row) in t.m.iter().enumerate() {
            for (j, v) in row.iter().enumerate() {
                assert_eq!(*v, if i == j { 1.0 } else { 0.0 });
            }
        }
    }
}
