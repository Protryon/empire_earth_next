//! The buildings' host's statements answered over the world views (the units' lane, round 14):
//! EEUnitClassBuildings.cpp's reads of the goal queues and stores of the resource members. Asked by
//! [`kind_view`](super::uc_kind_view::kind_view) for the buildings' kind after its plain reads.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::goals::EeUnitsGoalQueue;
use crate::units::uc_view::Ctx;
use crate::units::ucb_release::*;
use crate::world::unit_fields::EeUnitsUnitFields;
use crate::world::view::UnitView;
use crate::world_field;

/// Unit `u`'s goal queue `i`'s goals (`mGoalQueues[i].mGoals`).
fn queue(u: &UnitView<'_>, i: i64) -> Vec<u64> {
    let q = u.goal_queues()[i as c_ulong as usize];
    // SAFETY: the unit's live Rust goal queue (every complex unit's queues have one)
    unsafe { (*(q as usize as *const EeUnitsGoalQueue)).goals.clone() }
}

/// The buildings' statement `op` when it is one of these.
pub(crate) fn buildings_view(x: &Ctx<'_>, op: i32, b: i64, c: i64) -> Option<i64> {
    Some(match op {
        EE_UNITS_UCB_UNIT_QUEUE_LEN => queue(&x.u()?, b).len() as i64,
        EE_UNITS_UCB_UNIT_QUEUE_GOAL => queue(&x.u()?, b).get(c as usize).copied().unwrap_or(0) as i64,
        EE_UNITS_UCB_QUEUE_LEN_OF => queue(&x.o()?, b).len() as i64,
        EE_UNITS_UCB_QUEUE_GOAL_OF => queue(&x.o()?, b).get(c as usize).copied().unwrap_or(0) as i64,
        // `theUnit->SetResourceType(theClass->mResourceType)`, `SetResourceAmount(theClass->mResourceAmount)`
        EE_UNITS_UCB_SET_RESOURCE_TYPE | EE_UNITS_UCB_SET_RESOURCE_AMOUNT => {
            let class = x.cls()?.fields();
            let (ty, amount): (c_long, c_long) =
                (world_field!(class, mResourceType), world_field!(class, mResourceAmount));
            let to = x.u()?.fields().ptr() as *mut EeUnitsUnitFields;
            // SAFETY: the unit's members
            unsafe {
                if op == EE_UNITS_UCB_SET_RESOURCE_TYPE {
                    (*to).mResourceType = ty as c_ulong;
                } else {
                    (*to).mResourceAmount = amount;
                }
            }
            0
        }
        _ => return None,
    })
}
