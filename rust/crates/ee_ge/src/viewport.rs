//! `GEViewport` (`GEViewport.cpp`): the camera-space clipping planes, the orthographic and perspective projections
//! with their screen-to-world matrices, and the world-space corners and planes of the frustum under a screen
//! rectangle. The C++ class stays the type callers use; `GeViewport` is its `#[repr(C)]` mirror (the layout is pinned
//! by `Graphics Engine/GERustViewport.h`), and the C++ members forward here.
//!
//! The graphics thread, the camera control and the UI call this (not the simulation), but the C++'s `f32` order is
//! kept throughout, so the frustum the visible terrain is cut with is the original's.

use crate::types::{Ge3DPlane, Ge3DPoint, GeRectangle, GeTransformation, GeVector};
use crate::um::{um_arctan, um_cos, um_sin, um_tan};

/// `GEViewport` (sizeof 0x288), in the C++'s member order.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct GeViewport {
    pub orthographic_rectangle: GeRectangle,
    pub view_rectangle: GeRectangle,
    pub front_plane: Ge3DPlane,
    pub rear_plane: Ge3DPlane,
    pub left_plane: Ge3DPlane,
    pub right_plane: Ge3DPlane,
    pub top_plane: Ge3DPlane,
    pub bottom_plane: Ge3DPlane,
    pub camera: GeTransformation,
    pub camera_inverse: GeTransformation,
    pub orthographic_denormalization: GeTransformation,
    pub perspective_denormalization: GeTransformation,
    pub orthographic_projection: GeTransformation,
    pub perspective_projection: GeTransformation,
    pub camera_look_at_vector: GeVector,
    pub orthographic_scale: f32,
    pub orthographic_depth_scale: f32,
    pub orthographic_depth_offset: f32,
    pub perspective_scale: f32,
    pub perspective_depth_scale: f32,
    pub perspective_depth_offset: f32,
    pub aspect_ratio: f32,
    pub aspect_ratio_multiplier: f32,
    pub field_of_view: f32,
    pub field_of_view_multiplier: f32,
    pub front_clipping_distance: f32,
    pub half_height: f32,
    pub half_width: f32,
    pub rear_clipping_distance: f32,
    pub x_center: f32,
    pub y_center: f32,
    pub y_clipping_plane: f32,
    pub near_depth: f32,
    pub far_depth: f32,
}

const _: () = assert!(core::mem::size_of::<GeViewport>() == 0x288);

/// `GEViewport::sLocalLookAtVector`: the camera looks along its local +y.
const LOCAL_LOOK_AT: GeVector = GeVector { x: 0.0, y: 1.0, z: 0.0 };
/// `kMinimumFrustrumExtent`: the smallest screen rectangle a frustum is cut from, in pixels each way.
const MINIMUM_FRUSTUM_EXTENT: f32 = 2.0;
/// `GE3DPlane()`: through the origin, facing +z.
const DEFAULT_PLANE: Ge3DPlane =
    Ge3DPlane { point: Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 }, normal: GeVector { x: 0.0, y: 0.0, z: 1.0 } };

impl Default for GeViewport {
    /// `GEViewport()`: an identity camera, a 90-degree field of view, clipping from 1 to 10 (the planes and the
    /// matrices are their classes' defaults, not computed).
    fn default() -> Self {
        GeViewport {
            orthographic_rectangle: GeRectangle::default(),
            view_rectangle: GeRectangle::default(),
            front_plane: DEFAULT_PLANE,
            rear_plane: DEFAULT_PLANE,
            left_plane: DEFAULT_PLANE,
            right_plane: DEFAULT_PLANE,
            top_plane: DEFAULT_PLANE,
            bottom_plane: DEFAULT_PLANE,
            camera: GeTransformation::IDENTITY,
            camera_inverse: GeTransformation::IDENTITY,
            orthographic_denormalization: GeTransformation::IDENTITY,
            perspective_denormalization: GeTransformation::IDENTITY,
            orthographic_projection: GeTransformation::IDENTITY,
            perspective_projection: GeTransformation::IDENTITY,
            camera_look_at_vector: GeTransformation::IDENTITY.apply_vector(LOCAL_LOOK_AT),
            orthographic_scale: 0.0,
            orthographic_depth_scale: 0.0,
            orthographic_depth_offset: 0.0,
            perspective_scale: 0.0,
            perspective_depth_scale: 0.0,
            perspective_depth_offset: 0.0,
            aspect_ratio: 1.0,
            aspect_ratio_multiplier: 1.0,
            field_of_view: 1.5707964,
            field_of_view_multiplier: 1.0,
            front_clipping_distance: 1.0,
            half_height: 0.0,
            half_width: 0.0,
            rear_clipping_distance: 10.0,
            x_center: 0.0,
            y_center: 0.0,
            y_clipping_plane: 1.0,
            near_depth: 0.0,
            far_depth: 1.0,
        }
    }
}

impl GeViewport {
    /// `GEViewport(view, orthographic, camera, front, rear, fov)`: the screen rectangle's centre, half sizes and
    /// aspect ratio, then the projections and the clipping planes.
    pub fn new(
        view: GeRectangle,
        orthographic: GeRectangle,
        camera: GeTransformation,
        front: f32,
        rear: f32,
        field_of_view: f32,
    ) -> GeViewport {
        let mut v = GeViewport { view_rectangle: view, camera, ..GeViewport::default() };
        v.camera_look_at_vector = camera.apply_vector(LOCAL_LOOK_AT);
        v.orthographic_rectangle = orthographic;
        v.front_clipping_distance = front;
        v.rear_clipping_distance = rear;
        v.field_of_view = field_of_view;
        v.field_of_view_multiplier = um_tan(field_of_view * 0.5);
        v.set_rectangle_values(&view);
        v.aspect_ratio_multiplier = v.aspect_ratio / v.field_of_view_multiplier;
        v.near_depth = 0.0;
        v.far_depth = 1.0;
        v.compute_orthographic_matrix();
        v.compute_perspective_matrix();
        v.compute_planes();
        v
    }

    /// The half sizes, the centre, the y clipping plane and the aspect ratio of a view rectangle (the constructor's
    /// and `SetViewRectangle`'s shared lines).
    fn set_rectangle_values(&mut self, view: &GeRectangle) {
        self.half_width = (view.right - view.left) * 0.5;
        self.half_height = (view.bottom - view.top) * 0.5;
        self.x_center = (view.right + view.left) * 0.5;
        self.y_center = (view.bottom + view.top) * 0.5;
        self.y_clipping_plane = self.half_height / self.half_width;
        self.aspect_ratio =
            if view.top != view.bottom { (view.right - view.left) / (view.bottom - view.top) } else { 1.0 };
    }

    /// `ComputeOrthographicMatrix`: the orthographic projection and its screen-to-world matrix (nothing with no
    /// rear clipping distance).
    pub fn compute_orthographic_matrix(&mut self) {
        if self.rear_clipping_distance == 0.0 {
            return;
        }
        let depth = self.rear_clipping_distance - self.front_clipping_distance;
        let depth_scale = 1.0 / depth;
        let r = self.orthographic_rectangle;
        self.orthographic_scale = if r.right != r.left { 2.0 / (r.right - r.left) } else { 2.0 };
        self.orthographic_depth_scale = depth_scale;
        self.orthographic_depth_offset = -(depth_scale * self.front_clipping_distance);
        let half_width = (r.right - r.left) * 0.5;
        let c = &self.camera.m;
        let d = &mut self.orthographic_denormalization.m;
        for i in 0..3 {
            d[i][0] = c[i][0] * half_width;
            d[i][1] = c[i][2] * half_width;
            d[i][2] = c[i][1] * depth;
            d[i][3] = c[i][1] * self.front_clipping_distance + c[i][3];
        }
        self.orthographic_projection.m = [
            [self.orthographic_scale, 0.0, 0.0, 0.0],
            [0.0, self.orthographic_scale / self.y_clipping_plane, 0.0, 0.0],
            [0.0, 0.0, self.orthographic_depth_scale, self.orthographic_depth_offset],
            [0.0, 0.0, 0.0, 1.0],
        ];
    }

    /// `ComputePerspectiveMatrix`: the perspective projection and its screen-to-world matrix (nothing with no rear
    /// clipping distance).
    pub fn compute_perspective_matrix(&mut self) {
        if self.rear_clipping_distance == 0.0 {
            return;
        }
        self.perspective_scale = 1.0 / um_tan(self.field_of_view * 0.5);
        let mut depth = self.rear_clipping_distance - self.front_clipping_distance;
        self.perspective_depth_scale = self.rear_clipping_distance / depth;
        self.perspective_depth_offset = -(self.perspective_depth_scale * self.front_clipping_distance);
        let scale = 1.0 / self.perspective_scale;
        depth = depth / self.rear_clipping_distance;
        let c = &self.camera.m;
        let d = &mut self.perspective_denormalization.m;
        for i in 0..3 {
            d[i][0] = c[i][0] * scale;
            d[i][1] = c[i][2] * scale;
            d[i][2] = c[i][1] * depth;
            d[i][3] = c[i][1] * self.front_clipping_distance + c[i][3];
        }
        self.perspective_projection.m = [
            [self.perspective_scale, 0.0, 0.0, 0.0],
            [0.0, self.aspect_ratio_multiplier, 0.0, 0.0],
            [0.0, 0.0, self.perspective_depth_scale, self.perspective_depth_offset],
            [0.0, 0.0, 1.0, 0.0],
        ];
    }

    /// `ComputePlanes`: the camera-space clipping planes (front, rear, left, right, bottom, top) from the field of
    /// view and the screen's half sizes.
    pub fn compute_planes(&mut self) {
        let mut point = Ge3DPoint::default();
        let mut normal = GeVector::default();
        let mut angle = self.field_of_view * 0.5;
        point.y = self.front_clipping_distance;
        normal.y = 1.0;
        self.front_plane = Ge3DPlane { point, normal };
        point.y = self.rear_clipping_distance;
        normal.y = -1.0;
        self.rear_plane = Ge3DPlane { point, normal };
        point.y = 0.0;
        normal.x = um_cos(angle);
        normal.y = um_sin(angle);
        normal.normalize();
        self.left_plane = Ge3DPlane { point, normal };
        normal.x = -normal.x;
        self.right_plane = Ge3DPlane { point, normal };
        angle = self.half_height;
        let half_width = self.half_width;
        angle = um_arctan(um_tan(self.field_of_view * 0.5) * (angle - 1.0) / (half_width - 1.0));
        normal.x = 0.0;
        normal.y = um_sin(angle);
        normal.z = um_cos(angle);
        normal.normalize();
        self.bottom_plane = Ge3DPlane { point, normal };
        normal.z = -normal.z;
        self.top_plane = Ge3DPlane { point, normal };
    }

    /// `DenormalizePerspectivePoint`: the point scaled by `w`, through the perspective screen-to-world matrix.
    pub fn denormalize_perspective_point(&self, p: Ge3DPoint, w: f32) -> Ge3DPoint {
        self.perspective_denormalization.apply_point(Ge3DPoint::new(p.x * w, p.y * w, p.z * w))
    }

    /// `DenormalizeOrthographicPoint`.
    pub fn denormalize_orthographic_point(&self, p: Ge3DPoint) -> Ge3DPoint {
        self.orthographic_denormalization.apply_point(p)
    }

    /// `GetFrustrumWorldPoints(rectangle, points, orthographic)`: the eight world-space corners (near then far, per
    /// screen corner: top left, top right, bottom left, bottom right) of the frustum under a screen rectangle widened
    /// to at least 2 pixels each way.
    pub fn frustum_world_points(&self, rectangle: &GeRectangle, orthographic: bool) -> [Ge3DPoint; 8] {
        let mut r = *rectangle;
        let scale = 1.0 / self.half_width;
        if r.right - r.left <= MINIMUM_FRUSTUM_EXTENT {
            r.right = r.left + MINIMUM_FRUSTUM_EXTENT;
        }
        if r.bottom - r.top <= MINIMUM_FRUSTUM_EXTENT {
            r.bottom = r.top + MINIMUM_FRUSTUM_EXTENT;
        }
        let mut out = [Ge3DPoint::default(); 8];
        let corners = [(r.left, r.top), (r.right, r.top), (r.left, r.bottom), (r.right, r.bottom)];
        for (k, (x, y)) in corners.into_iter().enumerate() {
            let mut p = Ge3DPoint::new((x - self.x_center) * scale, -((y - self.y_center) * scale), 0.0);
            if !orthographic {
                out[2 * k] = self.denormalize_perspective_point(p, self.front_clipping_distance);
                p.z = 1.0;
                out[2 * k + 1] = self.denormalize_perspective_point(p, self.rear_clipping_distance);
            } else {
                out[2 * k] = self.orthographic_denormalization.apply_point(p);
                p.z = 1.0;
                out[2 * k + 1] = self.orthographic_denormalization.apply_point(p);
            }
        }
        out
    }

    /// `GetFrustrumWorldPlanes(rectangle, planes, orthographic)`: the six world-space planes through the corners.
    pub fn frustum_world_planes(&self, rectangle: &GeRectangle, orthographic: bool) -> [Ge3DPlane; 6] {
        let p = self.frustum_world_points(rectangle, orthographic);
        [
            Ge3DPlane::through(p[4], p[2], p[0]),
            Ge3DPlane::through(p[1], p[3], p[5]),
            Ge3DPlane::through(p[0], p[2], p[3]),
            Ge3DPlane::through(p[4], p[5], p[7]),
            Ge3DPlane::through(p[0], p[1], p[4]),
            Ge3DPlane::through(p[6], p[7], p[2]),
        ]
    }

    /// `SetCamera`: the camera, its look-at vector and inverse, and the projections.
    pub fn set_camera(&mut self, camera: &GeTransformation) {
        self.camera = *camera;
        self.camera_look_at_vector = self.camera.apply_vector(LOCAL_LOOK_AT);
        self.camera_inverse = self.camera.inverse();
        self.compute_orthographic_matrix();
        self.compute_perspective_matrix();
    }

    /// `SetFieldOfView`: nothing when unchanged; else the multipliers, the perspective and the planes.
    pub fn set_field_of_view(&mut self, field_of_view: f32) {
        if field_of_view == self.field_of_view {
            return;
        }
        self.field_of_view = field_of_view;
        self.field_of_view_multiplier = um_tan(field_of_view * 0.5);
        self.aspect_ratio_multiplier = self.aspect_ratio / self.field_of_view_multiplier;
        self.compute_perspective_matrix();
        self.compute_planes();
    }

    /// `SetFrontClippingDistance` (the rear one pushed back past it if needed), then everything recomputed.
    pub fn set_front_clipping_distance(&mut self, distance: f32) {
        if distance >= self.rear_clipping_distance {
            self.rear_clipping_distance = distance + 1.0;
        }
        self.front_clipping_distance = distance;
        self.compute_orthographic_matrix();
        self.compute_perspective_matrix();
        self.compute_planes();
    }

    /// `SetRearClippingDistance` (the front one pulled in to half of it if needed), then everything recomputed.
    pub fn set_rear_clipping_distance(&mut self, distance: f32) {
        if distance <= self.front_clipping_distance {
            self.front_clipping_distance = distance * 0.5;
        }
        self.rear_clipping_distance = distance;
        self.compute_orthographic_matrix();
        self.compute_perspective_matrix();
        self.compute_planes();
    }

    /// `SetOrthographicRectangle`.
    pub fn set_orthographic_rectangle(&mut self, rectangle: &GeRectangle) {
        self.orthographic_rectangle = *rectangle;
        self.compute_orthographic_matrix();
    }

    /// `SetViewRectangle`: nothing when equal; else the rectangle's values, the aspect multiplier, the projections
    /// and the planes.
    pub fn set_view_rectangle(&mut self, rectangle: &GeRectangle) {
        if self.view_rectangle == *rectangle {
            return;
        }
        self.view_rectangle = *rectangle;
        self.set_rectangle_values(rectangle);
        self.aspect_ratio_multiplier = self.aspect_ratio / self.field_of_view_multiplier;
        self.compute_orthographic_matrix();
        self.compute_perspective_matrix();
        self.compute_planes();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::offset_of;

    #[test]
    fn offsets_match_the_cpp_class() {
        assert_eq!(offset_of!(GeViewport, view_rectangle), 0x10);
        assert_eq!(offset_of!(GeViewport, front_plane), 0x20);
        assert_eq!(offset_of!(GeViewport, bottom_plane), 0x98);
        assert_eq!(offset_of!(GeViewport, camera), 0xb0);
        assert_eq!(offset_of!(GeViewport, camera_inverse), 0xf0);
        assert_eq!(offset_of!(GeViewport, perspective_projection), 0x1f0);
        assert_eq!(offset_of!(GeViewport, camera_look_at_vector), 0x230);
        assert_eq!(offset_of!(GeViewport, orthographic_scale), 0x23c);
        assert_eq!(offset_of!(GeViewport, front_clipping_distance), 0x264);
        assert_eq!(offset_of!(GeViewport, rear_clipping_distance), 0x270);
        assert_eq!(offset_of!(GeViewport, far_depth), 0x284);
    }

    fn rect(l: f32, t: f32, r: f32, b: f32) -> GeRectangle {
        GeRectangle { bottom: b, left: l, right: r, top: t }
    }

    #[test]
    fn the_default_viewport_looks_along_y_from_1_to_10() {
        let v = GeViewport::default();
        assert_eq!(v.camera_look_at_vector, LOCAL_LOOK_AT);
        assert_eq!((v.front_clipping_distance, v.rear_clipping_distance), (1.0, 10.0));
        assert_eq!(v.front_plane, DEFAULT_PLANE);
    }

    #[test]
    fn the_constructor_centres_the_screen_and_cuts_the_planes() {
        let v = GeViewport::new(
            rect(0.0, 0.0, 1024.0, 768.0),
            rect(-400.0, -300.0, 400.0, 300.0),
            GeTransformation::IDENTITY,
            1.0,
            100.0,
            1.0,
        );
        assert_eq!((v.half_width, v.half_height, v.x_center, v.y_center), (512.0, 384.0, 512.0, 384.0));
        assert_eq!(v.aspect_ratio, 1024.0 / 768.0);
        assert_eq!(v.front_plane.point.y, 1.0);
        assert_eq!(v.rear_plane.normal.y, -1.0);
        // the side planes face inwards: the left plane's normal has a positive x, the right's a negative one
        assert!(v.left_plane.normal.x > 0.0 && v.right_plane.normal.x < 0.0);
        assert!(v.bottom_plane.normal.z > 0.0 && v.top_plane.normal.z < 0.0);
        // the orthographic scale is 2 over the rectangle's width
        assert_eq!(v.orthographic_scale, 2.0 / 800.0);
        assert_eq!(v.perspective_projection.m[3], [0.0, 0.0, 1.0, 0.0]);
        // a degenerate screen keeps an aspect ratio of 1
        let flat = GeViewport::new(
            rect(0.0, 5.0, 10.0, 5.0),
            rect(0.0, 0.0, 1.0, 1.0),
            GeTransformation::IDENTITY,
            1.0,
            10.0,
            1.0,
        );
        assert_eq!(flat.aspect_ratio, 1.0);
    }

    #[test]
    fn no_rear_distance_leaves_the_matrices_alone() {
        let mut v = GeViewport { rear_clipping_distance: 0.0, orthographic_scale: 7.0, ..GeViewport::default() };
        v.compute_orthographic_matrix();
        v.compute_perspective_matrix();
        assert_eq!(v.orthographic_scale, 7.0);
        assert_eq!(v.perspective_projection, GeTransformation::IDENTITY);
    }

    #[test]
    fn the_frustum_corners_are_near_then_far_per_screen_corner() {
        let v = GeViewport::new(
            rect(0.0, 0.0, 1024.0, 768.0),
            rect(-400.0, -300.0, 400.0, 300.0),
            GeTransformation::IDENTITY,
            1.0,
            100.0,
            1.0,
        );
        let p = v.frustum_world_points(&v.view_rectangle, false);
        // the near corners sit at the front distance, the far ones at the rear
        for k in 0..4 {
            assert!((p[2 * k].y - 1.0).abs() < 1e-5, "{:?}", p[2 * k]);
            assert!((p[2 * k + 1].y - 100.0).abs() < 1e-3, "{:?}", p[2 * k + 1]);
        }
        // top left is left (negative x) and up (positive z), bottom right the opposite
        assert!(p[0].x < 0.0 && p[0].z > 0.0 && p[6].x > 0.0 && p[6].z < 0.0);
        // a rectangle narrower than the minimum extent is widened
        let q = v.frustum_world_points(&rect(10.0, 10.0, 11.0, 11.0), true);
        assert!(q[2].x > q[0].x);
        let planes = v.frustum_world_planes(&v.view_rectangle, false);
        assert_eq!(planes.len(), 6);
    }

    #[test]
    fn the_setters_keep_the_clipping_order() {
        let mut v = GeViewport::new(
            rect(0.0, 0.0, 1024.0, 768.0),
            rect(-400.0, -300.0, 400.0, 300.0),
            GeTransformation::IDENTITY,
            1.0,
            100.0,
            1.0,
        );
        v.set_front_clipping_distance(200.0);
        assert_eq!(v.rear_clipping_distance, 201.0);
        v.set_rear_clipping_distance(100.0);
        assert_eq!(v.front_clipping_distance, 50.0);
        let before = v;
        v.set_field_of_view(1.0);
        assert_eq!(v, before);
        v.set_view_rectangle(&before.view_rectangle);
        assert_eq!(v, before);
        v.set_view_rectangle(&rect(0.0, 0.0, 640.0, 480.0));
        assert_eq!(v.half_width, 320.0);
        let mut camera = GeTransformation::IDENTITY;
        camera.m[0][3] = 5.0;
        v.set_camera(&camera);
        assert_eq!(v.camera_inverse.m[0][3], -5.0);
    }
}
