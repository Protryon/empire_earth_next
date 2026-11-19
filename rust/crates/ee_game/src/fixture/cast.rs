//! The calamity casts (TEST ONLY). "<seq> cast <form> <enum> <offset>": the local player casts the calamity class of
//! that EECalamityEnum (3 earthquake, 4 hurricane, 9 volcano, 24 meteor storm, ...) at the camera's focus tile moved
//! <offset> tiles east (a unit kind: at the other players' unit nearest it; a global kind: everywhere), straight into
//! the calamity manager under the world's write lock, as EE-SIM --calamities does, on a thread of its own. It prints
//! "TEST UI cast <enum> <result> at <x> <y>", then every second while a calamity of that class lives "TEST UI
//! calamity <enum> alive <ms>", and "TEST UI calamity <enum> ended" (or "... timeout" after 90 s).

use ee_sim::world::{PlayerView, UnitView, WorldRead, WorldView};

use super::host::{self, Call, EeFixtureOp as Op};

/// A player's units in its list order.
fn units<'t>(p: &PlayerView<'t>) -> impl Iterator<Item = UnitView<'t>> {
    std::iter::successors(p.first_unit(), |u| u.next())
}

/// The local player's first living unit, the caster.
fn caster(world: &WorldView) -> Option<u32> {
    let p = world.current_player()?;
    units(&p).find(|u| u.complex_unit_handle() != 0 && u.hit_points() > 0).map(|u| u.id())
}

/// The other players' living unit nearest a tile (a unit calamity's target).
fn target(world: &WorldView, x: i64, y: i64) -> Option<u32> {
    let current = world.current_player().map(|p| p.handle());
    let mut best: Option<(u32, i64)> = None;
    for k in 1..world.num_players() as usize {
        let Some(p) = world.player(k) else { continue };
        if Some(p.handle()) == current {
            continue;
        }
        for u in units(&p) {
            let (ux, uy) = u.tile_xy();
            let (dx, dy) = (ux - x, uy - y);
            let d = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy));
            if u.complex_unit_handle() != 0 && u.hit_points() > 0 && best.is_none_or(|(_, b)| d < b) {
                best = Some((u.id(), d));
            }
        }
    }
    best.map(|(id, _)| id)
}

pub fn run(class_enum: i64, offset: i64) {
    let mut result = "no-class";
    let (mut x, mut y) = (0i64, 0i64);
    host::with_write_lock(|| {
        let count = Call::new(Op::CalamityCount).go();
        for row in 0..count {
            let c = Call::new(Op::CalamityClass).i(&[row]).run();
            if c.result == 0 || c.i(1) != class_enum {
                continue;
            }
            // SAFETY: the world's write lock is held
            let t = unsafe { WorldRead::begin() };
            let Some(world) = WorldView::global(&t) else { break };
            let Some(caster) = caster(&world).filter(|_| world.map().is_some()) else {
                result = "no-caster";
                break;
            };
            let camera = Call::new(Op::Camera).run();
            x = camera.f(0) as i32 as i64 + offset;
            y = camera.f(1) as i32 as i64;
            let player = world.current_player().map_or(0, |p| i64::from(p.id()));
            let kind = c.i(2);
            let cast = if kind == 3 {
                target(&world, x, y).is_some_and(|id| {
                    Call::new(Op::CalamityCast).i(&[row, i64::from(caster), player, 0, i64::from(id)]).go() != 0
                })
            } else if kind == 2 {
                Call::new(Op::CalamityCast).i(&[row, i64::from(caster), player, 1, x, y]).go() != 0
            } else {
                Call::new(Op::CalamityCast).i(&[row, i64::from(caster), player, 2]).go() != 0
            };
            result = if cast { "cast" } else { "refused" };
            break;
        }
    });
    host::stderr(&format!("TEST UI cast {class_enum} {result} at {x} {y}\n"));
    if result != "cast" {
        return;
    }
    for _ in 0..90 {
        host::sleep_ms(1000);
        let alive = host::with_read_lock(|| Call::new(Op::CalamityAlive).i(&[class_enum]).go()).unwrap_or(-1);
        if alive < 0 {
            host::stderr(&format!("TEST UI calamity {class_enum} ended\n"));
            return;
        }
        host::stderr(&format!("TEST UI calamity {class_enum} alive {alive}\n"));
    }
    host::stderr(&format!("TEST UI calamity {class_enum} timeout\n"));
}
