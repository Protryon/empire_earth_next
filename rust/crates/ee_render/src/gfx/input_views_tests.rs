use super::*;

#[repr(C)]
#[derive(Default)]
struct FakeMouse {
    position: Ge2DPoint,
    bounds: GeRectangle,
    wheel: c_long,
    pointer: c_long,
    overlay: c_long,
    frozen: bool,
    shift_mode: bool,
    sensitivity: f32,
}

// one test: the registrations are process-wide
#[test]
fn the_registered_mouse_and_viewport_are_read_and_written_in_place() {
    // SAFETY: none registered
    unsafe {
        ee_gfx_input_mouse_set(std::ptr::null());
        ee_gfx_main_viewport_set(std::ptr::null_mut());
    }
    assert_eq!(mouse_position(), None);
    assert!(main_viewport().is_none());
    let mut m = Box::new(FakeMouse {
        position: Ge2DPoint { x: 5.0, y: 6.0 },
        bounds: GeRectangle { bottom: 100.0, left: 0.0, right: 200.0, top: 0.0 },
        pointer: 3,
        overlay: 8,
        sensitivity: 1.0,
        ..Default::default()
    });
    let view = EeGfxMouseView {
        position: &mut m.position,
        bounds: &mut m.bounds,
        wheel: &mut m.wheel,
        pointer: &mut m.pointer,
        overlay: &mut m.overlay,
        frozen: &mut m.frozen,
        shift_mode: &mut m.shift_mode,
        sensitivity: &mut m.sensitivity,
    };
    // SAFETY: the fake lives through the test
    unsafe { ee_gfx_input_mouse_set(&view) };
    assert_eq!(mouse_position(), Some(Ge2DPoint { x: 5.0, y: 6.0 }));
    assert_eq!(
        (mouse_pointer(), mouse_overlay(), mouse_frozen(), mouse_sensitivity()),
        (Some(3), Some(8), Some(false), Some(1.0))
    );
    set_mouse_pointer(4);
    set_mouse_position(Ge2DPoint { x: 250.0, y: -3.0 });
    // SAFETY: the reads after the writes (no other user)
    let (pointer, position) =
        unsafe { (std::ptr::read_volatile(view.pointer), std::ptr::read_volatile(view.position)) };
    assert_eq!(pointer, 4);
    assert_eq!(position, mouse::clamp_position(Ge2DPoint { x: 250.0, y: -3.0 }, &m.bounds));
    let mut viewport = Box::new(crate::ge::viewport::GeViewport::default());
    viewport.view_rectangle.right = 1024.0;
    // SAFETY: the fake lives through the test
    unsafe { ee_gfx_main_viewport_set(&mut *viewport) };
    assert_eq!(main_viewport().map(|v| v.view_rectangle.right), Some(1024.0));
    // SAFETY: unregistered before the fakes go
    unsafe {
        ee_gfx_input_mouse_set(std::ptr::null());
        ee_gfx_main_viewport_set(std::ptr::null_mut());
    }
    assert_eq!(mouse_wheel(), None);
}
