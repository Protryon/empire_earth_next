use super::*;
use crate::ge::vertarr::{ee_ge_va_destroy, ee_ge_va_new};

#[test]
fn a_scroll_moves_one_coordinate_and_its_restore_puts_it_back() {
    let a = ee_ge_va_new(0, 3);
    for i in 0..3 {
        set(a, EeGeVaField::Uv, i, [i as f32, 10.0 + i as f32]);
    }
    let kept = Mutex::new(vec![99.0]);
    scroll_array(a, 1, 0.5, Some(&kept));
    assert_eq!(*kept.lock().unwrap(), vec![10.0, 11.0, 12.0]);
    assert_eq!(get::<2>(a, EeGeVaField::Uv, 2), [2.0, 12.5]);
    // a shift keeps nothing
    scroll_array(a, 0, 1.0, None);
    assert_eq!(get::<2>(a, EeGeVaField::Uv, 1), [2.0, 11.5]);
    restore_array(a, 1, &kept);
    assert_eq!(get::<2>(a, EeGeVaField::Uv, 1), [2.0, 11.0]);
    // SAFETY: made above, not used again
    unsafe { ee_ge_va_destroy(a) };
}
