//! The calamities' shared timing (`src/Empire Earth/Calamities/EECalamityClass.cpp`): a calamity's age against its
//! class's lifetime, which every class's `Update` asks through `EECalamityClass::UpdateTimeAlive`.

/// `EECalamityClass::UpdateTimeAlive`'s arithmetic: the new age and whether the lifetime is over (the update's 7).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityAge {
    pub time_alive: f32,
    pub done: bool,
    pub pad: [u8; 3],
}

/// The calamity's age after `elapsed` more (ms, as the physics tick passes it) and whether it reached `lifetime`.
pub fn update_time_alive(time_alive: f32, elapsed: f32, lifetime: f32) -> EeCalamityAge {
    let time_alive = time_alive + elapsed;
    EeCalamityAge { time_alive, done: time_alive >= lifetime, pad: [0; 3] }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn calamities_age_until_their_lifetime() {
        let a = update_time_alive(900.0, 100.0, 1000.0);
        assert_eq!((a.time_alive, a.done), (1000.0, true));
        let b = update_time_alive(800.0, 100.0, 1000.0);
        assert_eq!((b.time_alive, b.done), (900.0, false));
        // f32 sums: 0.1 + 0.2 is not 0.3
        let c = update_time_alive(0.1, 0.2, 0.3);
        assert_eq!(c.time_alive.to_bits(), (0.1f32 + 0.2f32).to_bits());
        assert!(c.done);
        assert!(update_time_alive(0.0, 0.0, 0.0).done);
    }
}
