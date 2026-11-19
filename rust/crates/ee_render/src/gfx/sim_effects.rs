//! The effect manager as the simulation calls it: ee_sim's narrow effect interface (`ee_sim::world::effects`)
//! over the game's current manager (`gEffectManager`, through the manager's world, ffi_manager.rs). ee_game's
//! `ee_rust_init` registers [`RENDER_EFFECTS`]; a unit, goal or calamity body in ee_sim then makes its
//! `CreateUnitEffect` .. `DestroyEffect` calls through it, as the C++ does through `EEEffectManager`'s methods
//! (ee/EEEffectManager.h builds the same arguments).

use ee_sim::world::effects::{EffectCreated, EffectHandle, EffectPlace, EffectRequest, Effects};

use crate::gfx::effect_manager::EeGfxEffectEntry;
use crate::gfx::ffi_manager::{self as fm, EeGfxEffectArgs};

/// The entry point's arguments for a request, as `EEEffectManager`'s `Create*` method builds them.
pub fn args_of(request: &EffectRequest) -> EeGfxEffectArgs {
    let mut a = EeGfxEffectArgs { graphic_effect: request.graphic_effect, ..EeGfxEffectArgs::default() };
    match request.place {
        EffectPlace::Unit { unit, keeps_unit, tag_point, goal_index } => {
            a.entry = EeGfxEffectEntry::Unit;
            a.unit = unit;
            a.keeps_unit = keeps_unit;
            a.tag_point = tag_point;
            a.goal_index = goal_index;
        }
        EffectPlace::BetweenUnits { from, to } => {
            a.entry = EeGfxEffectEntry::BetweenUnits;
            a.unit = from;
            a.to_unit = to;
        }
        EffectPlace::UnitToPoint { unit, at, goal_index } => {
            a.entry = EeGfxEffectEntry::UnitToPoint;
            a.unit = unit;
            a.at = at;
            a.goal_index = goal_index;
        }
        EffectPlace::Placed { at } => {
            a.entry = EeGfxEffectEntry::Placed;
            a.at = at;
        }
        EffectPlace::Transformation(t) => {
            a.entry = EeGfxEffectEntry::Transformation;
            a.transformation = t;
        }
        EffectPlace::Line(l) => {
            a.entry = EeGfxEffectEntry::Line;
            a.line = l;
        }
        EffectPlace::Between { from, to } => {
            a.entry = EeGfxEffectEntry::Between;
            a.at = from;
            a.to = to;
        }
        EffectPlace::Weather { tiles } => {
            a.entry = EeGfxEffectEntry::Weather;
            a.tiles = tiles.as_ptr() as *const i32;
            a.tile_count = tiles.len() as u32;
        }
    }
    a
}

/// ee_sim's effects on the game's current manager.
pub struct RenderEffects;

/// What ee_game registers (`ee_sim::world::effects::register`).
pub static RENDER_EFFECTS: RenderEffects = RenderEffects;

impl RenderEffects {
    /// The living effect's record under the handle on the current manager, changed by `f`; `None` without one.
    fn with_record<R>(
        handle: EffectHandle,
        f: impl FnOnce(&mut crate::gfx::effect_state::EeGfxEffectRecord) -> R,
    ) -> Option<R> {
        let e = fm::find(fm::current(), handle.0);
        if e.is_null() {
            return None;
        }
        // SAFETY: a living effect's record (no other reference to it is held across this call)
        Some(f(unsafe { &mut *(*e).record }))
    }
}

impl Effects for RenderEffects {
    fn create(&self, request: &EffectRequest) -> EffectCreated {
        let args = args_of(request);
        let mut handle = 0u32;
        let result = fm::create(fm::current(), &args, &mut handle);
        EffectCreated { result, handle: EffectHandle(handle) }
    }

    fn destroy(&self, handle: EffectHandle) {
        fm::destroy(fm::current(), handle.0)
    }

    fn kind(&self, handle: EffectHandle) -> Option<i32> {
        Self::with_record(handle, |r| r.kind)
    }

    // EEEffect::SetLocalTo
    fn set_local_to(&self, handle: EffectHandle, player: i32) -> bool {
        Self::with_record(handle, |r| {
            r.saved = false;
            r.local_only = true;
            r.player = player;
        })
        .is_some()
    }

    fn finish_damaged(&self, handle: EffectHandle) -> bool {
        Self::with_record(handle, |r| r.damaged.finished = true).is_some()
    }
}
