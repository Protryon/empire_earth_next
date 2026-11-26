//! `extern "C"` for the game's actions whole in Rust (`game_actions.rs`): the calls of `EEARust` (`ee/EEARust.h`),
//! its constructor, destructor and run, and its users' (the click's execute and selection, the lasso's selection, the
//! build placements, the calamity's cast and check, the camera's targets, the edge scroll's update, the improvement's
//! begin). The host is `ee_screenscpp_ga` (`Actions/EEARust.cpp`); records are gamecpp mrecord.rs's.

use core::ffi::c_void;

use crate::screens::game_actions::{EeScreensGaFields, EeScreensGaValue, Ga, GaEditorCall, GaHost};
use crate::screens::game_actions_host::{ee_screens_gah_run, GaCpp};

/// cbindgen:ignore
// (the record's editor parts are pointers to Rust boxes C++ never looks into)
#[allow(improper_ctypes)]
mod sys {
    use core::ffi::c_void;

    use crate::screens::game_actions::EeScreensGaFields;

    extern "C" {
        pub fn ee_screenscpp_ga(op: u32, r: *mut EeScreensGaFields, p: *mut c_void, i: *mut i64, f: *mut f32) -> i64;
    }
}

struct Host;

/// The C++ cases (`ee_screenscpp_ga`: one statement each).
struct Cpp;

impl GaCpp for Cpp {
    fn cpp(&mut self, key: u32, r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
        if let Some(answer) = record_case(key, p, i, f) {
            return answer;
        }
        unsafe { sys::ee_screenscpp_ga(key, r, p as *mut c_void, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
}

/// `IsDropSite`, `IsTransport`, `IsAirBase` of a class (ee_sim units uc_is.rs, which the inline members call).
fn uc_is(class: u64, which: i32) -> i64 {
    ee_sim::units::uc_is::ee_units_uc_is(class, which) as i64
}

/// A pointer member at `object + offset` (a tile's or a unit node's: the world layout's offsets).
///
/// # Safety
/// `object` is a live object whose member at `offset` is a pointer.
unsafe fn pointer_at(object: usize, offset: u64) -> i64 {
    // SAFETY: the caller's contract
    unsafe { *((object + offset as usize) as *const usize) as i64 }
}

/// The cases the world's records and Rust's objects answer (were `ee_screenscpp_ga`'s one-statement reads and
/// writes of the units', classes', groups', players', tiles' and world's plain members, the group calls and Rust's
/// globals): None for the others, which the C++ cases run. `p` is the case's object (a unit: an `EEUnit*` or, with
/// i1 set, an `EEComplexUnit*`, the same address; a tile; a class), the current player's selection the groups'.
fn record_case(key: u32, p: usize, i: &[i64; 8], f: &mut [f32; 8]) -> Option<i64> {
    use crate::gamecpp::camera_io as camera;
    use crate::gamecpp::globals::{get, have, set};
    use crate::gamecpp::message::{group_contains, group_do, group_get, group_len, group_unit, group_with, unit_id};
    use crate::gamecpp::{h, hd, hf, hp, O};
    use crate::screens::game_actions::*;
    use crate::screens::game_actions_host::*;
    use ee_sim::units::uc_is::{EE_UNITS_UC_IS_AIR_BASE, EE_UNITS_UC_IS_DROP_SITE, EE_UNITS_UC_IS_TRANSPORT};
    use ee_sim::world::layout;
    use ee_sim::world::view::{ClassView, TileView, UnitView, WorldRead, WorldView};
    use ee_sim::world_field;
    use std::ffi::{c_long, c_ulong};

    let l = layout::get()?;
    // SAFETY: the actions run on the UI thread over the world the C++ cases read in place, as they did
    let token = unsafe { WorldRead::begin() };
    let world = WorldView::global(&token)?;
    // SAFETY: a unit case's object is a live unit (its RTSUnit at its address)
    let unit = || unsafe { UnitView::of(&token, p as u64) };
    // AsComplex: the object itself (i1 set) or its unit's complex unit
    let complex = || if i[1] != 0 { unit() } else { unit().and_then(|u| u.complex_unit()) };
    let class = || unit().and_then(|u| u.class());
    let current = || world.current_player();
    let selection = || current().map_or(0, |pl| pl.selected_group_handle() as usize);
    let active = |pl: usize| hp(O::PlActiveGroup, pl, &[]);
    let (op, field) = (key >> 8, (key & 0xff) as i64);
    Some(match (op, field) {
        // the units (GetUnitID, GetPlayer()->GetPlayerID(), GetPlayer(), GetComplexUnit(), GetTileNode() != NULL, ...)
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_ID) => unit()?.id() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_PLAYER_ID) => unit()?.player()?.id() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_PLAYER) => unit()?.player().map_or(0, |pl| pl.handle() as i64),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_COMPLEX) => unit()?.complex_unit_handle() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_TILE_NODE) => unit()?.has_tile_node() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_TILE_X) => unit()?.tile_xy().0,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_TILE_Y) => unit()?.tile_xy().1,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_HIT_POINTS) => unit()?.hit_points(),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_MAX_HIT_POINTS) => unit()?.stats()?.max_hit_points(),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_RESOURCE_TYPE) => unit()?.resource_type() as c_ulong as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_RESOURCE_AMOUNT) => unit()?.resource_amount(),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_DISABLED) => {
            let disabled: bool = world_field!(complex()?.complex(), mIsDisabled);
            disabled as i64
        }
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_BUILT) => complex()?.is_built() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_TILE) => unit()?.tile().map_or(0, |t| t.handle() as i64),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_CLASS_PTR) => class().map_or(0, |c| c.handle() as i64),
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_CONTAINER) => complex()?.container_handle() as i64,
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_TILE_HEIGHT) => {
            f[0] = unit()?.tile()?.height();
            0
        }
        // the physical model's location x, y (GetPhysicalModel().GetLocation().GetX(), GetY()), its position
        _ if key == EE_SCREENS_GAH_U_LOC_X => {
            f[0] = complex()?.location()[0];
            0
        }
        _ if key == EE_SCREENS_GAH_U_LOC_Y => {
            f[0] = complex()?.location()[1];
            0
        }
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_POSITION) => {
            f[..3].copy_from_slice(&complex()?.location());
            0
        }
        // the unit classes (of the unit p): the getters' members
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_TYPE) => class()?.unit_type() as c_ulong as c_long as i64,
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_CATEGORY) => class()?.category() as c_ulong as i64,
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_FAMILY) => class()?.family() as c_ulong as i64,
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_ANIMAL_ID) => {
            let id: c_long = world_field!(class()?.fields(), mAnimalID);
            id as i64
        }
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_AIR_TRAFFIC) => {
            let kind: c_long = world_field!(class()?.fields(), mAirTrafficType);
            kind as i64
        }
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_CAN_RALLY) => {
            let b: bool = world_field!(class()?.fields(), mCanSetRallyPoint);
            b as i64
        }
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_TOWER) => {
            let b: bool = world_field!(class()?.fields(), mIsTower);
            b as i64
        }
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_TREE) => class()?.tree() as i64,
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_MEDIC) => {
            let b: bool = world_field!(class()?.fields(), mIsMedic);
            b as i64
        }
        // IsDropSite, IsTransport, IsAirBase (ee_sim units uc_is.rs, as the inline members call it)
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_DROP_SITE) => uc_is(class()?.handle(), EE_UNITS_UC_IS_DROP_SITE),
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_TRANSPORT) => uc_is(class()?.handle(), EE_UNITS_UC_IS_TRANSPORT),
        (EE_SCREENS_GA_OP_CLASS, EE_SCREENS_GA_C_AIR_BASE) => uc_is(class()?.handle(), EE_UNITS_UC_IS_AIR_BASE),
        _ if key == EE_SCREENS_GAH_K_CLASS_TILE_SIZE => {
            // SAFETY: the case's object is a live class
            unsafe { ClassView::of(&token, p as u64) }?.tile_size()
        }
        // the current player's selected group (the group's Rust calls, as EEGroup's inline members make them)
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_COUNT) => group_len(selection()) as i64,
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_KIND_FLAGS) => {
            let g = selection() as *const ee_sim::units::group_obj::EeUnitsGroup;
            // SAFETY: the current player's live selection
            unsafe { (*g).mKindFlags as u32 as i64 }
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_COUNT_KIND) => {
            group_get(selection(), 0, i[1] as c_long as i64) as c_long as i64
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_UNIT) => group_unit(selection(), i[1] as c_long) as i64,
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_TYPE_COUNT) => {
            group_get(selection(), 2, i[1] as c_long as i64) as c_long as i64
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_FIND_CATEGORY) => group_get(selection(), 3, i[1] as c_long as i64),
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_FIND_FLAGS) => group_get(selection(), 4, i[1] as c_ulong as i64),
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_CONTAINS) => group_contains(selection(), p) as i64,
        _ if key == EE_SCREENS_GAH_G_BOUNDS => {
            let g = selection() as *mut ee_sim::units::group_obj::EeUnitsGroup;
            // SAFETY: the current player's live selection; f holds the rectangle's four floats
            unsafe { ee_sim::units::group_host::ee_units_group_bounds(g, f.as_mut_ptr() as *mut [f32; 4]) };
            0
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_FIRST_ID) => unit_id(group_unit(selection(), 0)) as i64,
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_ACTIVE_COUNT) => {
            group_len(active(current()?.handle() as usize)) as i64
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_ACTIVE_UNIT) => {
            group_unit(active(current()?.handle() as usize), i[1] as c_long) as i64
        }
        // the current player
        (EE_SCREENS_GA_OP_PLAYER, EE_SCREENS_GA_P_PTR) => current().map_or(0, |pl| pl.handle() as i64),
        (EE_SCREENS_GA_OP_PLAYER, EE_SCREENS_GA_P_ID | EE_SCREENS_GA_P_CURRENT_ID) => current()?.id() as i64,
        (EE_SCREENS_GA_OP_PLAYER, EE_SCREENS_GA_P_ENEMY) => {
            // GetEnemy: mEnemies[i1] as a long
            let enemies: [c_ulong; 17] = world_field!(current()?.saved(), mEnemies);
            *enemies.get(i[1] as c_ulong as usize)? as c_long as i64
        }
        // the tile p: IsVisibleTo(the current player); its unit list's first node, a node's next and unit
        (EE_SCREENS_GA_OP_TILE, EE_SCREENS_GA_T_VISIBLE) => {
            let pl = current()?;
            // SAFETY: the case's object is a live tile
            let tile = unsafe { TileView::of(&token, p as u64) }?;
            (tile.visibility(pl.id() as usize) > 0 || pl.sees_all()) as i64
        }
        // SAFETY (these): p a live tile or node, the layout's offsets of its pointer members
        _ if key == EE_SCREENS_GAH_T_FIRST => unsafe { pointer_at(p, l.tile_units) },
        _ if key == EE_SCREENS_GAH_T_NEXT => unsafe { pointer_at(p, l.unit_node_next) },
        _ if key == EE_SCREENS_GAH_T_NODE_UNIT => unsafe { pointer_at(p, l.unit_node_unit) },
        // the map: its presence, tile (i1, i2) (0 off the map), the tile p's height
        (EE_SCREENS_GA_OP_MAP, EE_SCREENS_GA_M_EXISTS) => world.map().is_some() as i64,
        _ if key == EE_SCREENS_GAH_M_TILE_PTR => {
            world.map()?.tile(i[1] as c_ulong as i64, i[2] as c_ulong as i64).map_or(0, |t| t.handle() as i64)
        }
        _ if key == EE_SCREENS_GAH_M_TILE_HEIGHT => {
            // SAFETY: the case's object is a live tile
            f[0] = unsafe { TileView::of(&token, p as u64) }?.height();
            0
        }
        // the world's and its players' members (only a LAN game reaches most)
        (EE_SCREENS_GA_OP_NET, EE_SCREENS_GA_N_NUM_PLAYERS) => world.num_players() as c_ulong as i64,
        (EE_SCREENS_GA_OP_NET, EE_SCREENS_GA_N_LOADING_SAVED) => world.loading_saved_game() as i64,
        (EE_SCREENS_GA_OP_NET, EE_SCREENS_GA_N_USER_PAUSED) => world.user_paused() as i64,
        _ if key == EE_SCREENS_GAH_N_PLAYER_SLOT => world.player(i[1] as usize)?.slot() as i64,
        _ if key == EE_SCREENS_GAH_N_PLAYER_FIREWALL => {
            let behind: bool = world_field!(world.player(i[1] as usize)?.fields(), mBehindFirewall);
            behind as i64
        }
        _ if key == EE_SCREENS_GAH_N_PLAYER_CURRENT => {
            let of = |pl: Option<ee_sim::world::view::PlayerView<'_>>| pl.map_or(0, |pl| pl.handle());
            (of(current()) == of(world.player(i[1] as usize))) as i64
        }
        _ if key == EE_SCREENS_GAH_N_ACTIVE_CLEAR => {
            group_do(active(world.player(i[1] as usize)?.handle() as usize), 2, 0);
            0
        }
        _ if key == EE_SCREENS_GAH_N_COPY_SELECTION => {
            let pl = current()?.handle() as usize;
            group_with(crate::gamecpp::message::selected_group(pl), 2, active(pl));
            0
        }
        _ if key == EE_SCREENS_GAH_N_CURRENT_TICK => {
            let tick: c_ulong = world_field!(world.physics()?, mCurrentTick);
            tick as i64
        }
        // mUserPaused = i1; a player's mSlot = i2, the current player's 0xfe (their records written in place)
        _ if key == EE_SCREENS_GAH_N_SET_USER_PAUSED => {
            // SAFETY: the world's bool member (the layout's offset)
            unsafe { *((world.handle() as usize + l.world_user_paused as usize) as *mut bool) = i[1] != 0 };
            0
        }
        (EE_SCREENS_GA_OP_NET, EE_SCREENS_GA_N_SET_SLOT) => {
            let rts =
                world.player(i[1] as usize)?.rts().ptr() as *mut ee_sim::world::player_fields::EeUnitsRtsPlayerFields;
            // SAFETY: the live player's record
            unsafe { (*rts).mSlot = i[2] as u8 };
            0
        }
        _ if key == EE_SCREENS_GAH_N_SLOT_FE => {
            let rts = current()?.rts().ptr() as *mut ee_sim::world::player_fields::EeUnitsRtsPlayerFields;
            // SAFETY: as above
            unsafe { (*rts).mSlot = 0xfe };
            0
        }
        // Rust's globals: gWonNet set, gCommunications set, cleared; gCameraFollowing cleared
        _ if key == EE_SCREENS_GAH_N_WON => have!(gWonNet) as i64,
        (EE_SCREENS_GA_OP_NET, EE_SCREENS_GA_N_COMMS) => have!(gCommunications) as i64,
        _ if key == EE_SCREENS_GAH_N_CLEAR => {
            set!(gCommunications, std::ptr::null_mut());
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_FOLLOWING_OFF) => {
            set!(gCameraFollowing, false);
            0
        }
        // the camera's and the mini map's Rust calls (as the inline members make them)
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_SET_LOCATION) => {
            let at = ee_base::math::types::Ge3DPoint::new(f[0], f[1], f[2]);
            camera::set_location(&at, false, true);
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_KEEP_ZOOM) => {
            camera::set_zoom(camera::zoom());
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_CHANGED) => {
            camera::set_changed();
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_ZOOM) => {
            f[0] = camera::zoom();
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_SCROLL) => {
            camera::scroll(f[0], f[1]);
            0
        }
        _ if key == EE_SCREENS_GAH_CAM_SET_ZOOM => {
            camera::set_zoom(f[0]);
            0
        }
        _ if key == EE_SCREENS_GAH_CAM_SET_MODE => {
            camera::set_mode(i[1] as c_long);
            0
        }
        _ if key == EE_SCREENS_GAH_CAM_UPDATE => {
            camera::update(f[0]);
            0
        }
        (EE_SCREENS_GA_OP_MINI_MAP, EE_SCREENS_GA_MM_REFRESH) => {
            crate::gamecpp::minimap::ee_gamecpp_minimap_refresh(get!(gMiniMap));
            0
        }
        // the class table's size; the world's unit i0; a unit referenced, released (the game's shared cases)
        _ if key == EE_SCREENS_GA_OP_CLASS_COUNT => ClassView::count(&token) as i64,
        _ if key == EE_SCREENS_GA_OP_WORLD_UNIT => h(O::WorldUnit, 0, &[i[0] as c_ulong as i64]),
        _ if key == EE_SCREENS_GA_OP_UNIT_REFERENCE => h(O::PwUnitReference, p, &[]),
        _ if key == EE_SCREENS_GA_OP_UNIT_RELEASE => h(O::PwUnitDereference, p, &[]),
        // the network's and the editor's statements the game's cases already make word for word
        _ if key == EE_SCREENS_GAH_N_WON_CLEAR => h(O::WonNetClearGameList, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_WON_REJOIN => h(O::SsWonRejoinRoom, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_DELETE => h(O::EntryDeleteCommunicationsKeep, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_NEW_CLIENT => h(O::EntryNewClient, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_OPT_LOCK => h(O::EntryOptionsWriteLock, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_OPT_UNLOCK => h(O::EntryOptionsWriteUnlock, 0, &[]),
        _ if key == EE_SCREENS_GAH_N_EVENT_RELEASE => h(O::EntryDereferenceEvent, p, &[]),
        (EE_SCREENS_GA_OP_EDITOR, EE_SCREENS_GA_E_JIT) => h(O::UiUpdateJit, 0, &[]),
        (EE_SCREENS_GA_OP_EDITOR, EE_SCREENS_GA_E_CLOSE_FORM) => h(O::UiCloseForm, 0, &[]),
        (EE_SCREENS_GA_OP_EDITOR, EE_SCREENS_GA_E_SAVE_ERROR) => h(O::UiShowLoadSaveError, 0, &[]),
        (EE_SCREENS_GA_OP_EDITOR, EE_SCREENS_GA_E_DELETE_FILE) => h(O::PsMemFileDelete, p, &[]),
        _ if key == EE_SCREENS_GAH_E_NEW_SPEC => h(O::FileSpecNew, 0, &[]),
        _ if key == EE_SCREENS_GAH_E_DELETE_SPEC => h(O::PsSpecDelete, p, &[]),
        _ if key == EE_SCREENS_GAH_E_NEW_MEMORY => h(O::PsMemFileNew, 0, &[]),
        _ if key == EE_SCREENS_GAH_E_SPEC_VOLUME => h(O::PsSpecSetStartupVolume, p, &[]),
        _ if key == EE_SCREENS_GAH_E_CREATE => {
            h(O::PsSpecCreate, p, &[]);
            0
        }
        _ if key == EE_SCREENS_GAH_E_MASTER => {
            f[0] = hd(O::PsMasterVolume, 0, &[], &[]);
            0
        }
        _ if key == EE_SCREENS_GAH_E_MUSIC => {
            f[0] = hd(O::PsMusicVolume, 0, &[], &[]);
            0
        }
        _ if key == EE_SCREENS_GAH_E_SET_MASTER => hf(O::PsSetMasterVolume, 0, &[], &[f[0]]),
        _ if key == EE_SCREENS_GAH_E_SET_MUSIC => hf(O::PsSetMusicVolume, 0, &[], &[f[0]]),
        _ => return None,
    })
}

impl GaHost for Host {
    fn op(&mut self, op: u32, r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
        ee_screens_gah_run(&mut Cpp, op, r, p, i, f)
    }
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize {
        // SAFETY: the values' slice
        unsafe { crate::gamecpp::mrecord::ee_screenscpp_ga_record(kind, values.as_ptr(), values.len() as u32) as usize }
    }
    fn editor_call(&mut self, call: GaEditorCall) -> i64 {
        use crate::screens::editor_actions_ffi as sea;
        use crate::screens::editor_whole_ffi as sew;
        match call {
            GaEditorCall::Click(message) => sea::ee_screens_seu_click(message as *mut c_void),
            GaEditorCall::ClickExecute { action, rotating } => {
                sea::ee_screens_seu_click_execute(action as *mut c_void, u8::from(rotating))
            }
            GaEditorCall::Lasso(message) => sea::ee_screens_seu_lasso_message(message as *mut c_void),
            GaEditorCall::LassoInRectangle { action, rectangle } => {
                sea::ee_screens_seu_lasso_in_rectangle(action as *mut c_void, rectangle as *const c_void)
            }
            // SAFETY (these): the record's paint and action are the editor modules' boxes, live until their free
            GaEditorCall::PaintRun(paint) => unsafe { sea::ee_screens_sea_paint_run(paint) },
            GaEditorCall::PaintSetArea(paint, [l, t, r, b]) => {
                return i64::from(unsafe { sea::ee_screens_sea_paint_set_area(paint, l, t, r, b) })
            }
            GaEditorCall::PaintFree(paint) => unsafe { sea::ee_screens_sea_paint_free(paint) },
            GaEditorCall::WholeRun(action) => unsafe { sew::ee_screens_sew_run(action) },
            GaEditorCall::WholeFree(action) => unsafe { sew::ee_screens_sew_free(action) },
            GaEditorCall::UndoRedo(undo) => {
                use crate::screens::actions_ffi as actions;
                return (if undo { actions::ee_screens_actions_undo() } else { actions::ee_screens_actions_redo() })
                    as i64;
            }
        }
        0
    }
}

/// `code` over the action's record and the C++ host.
///
/// # Safety
/// `r` is a live `EEARust`'s record.
unsafe fn with<R>(r: *mut EeScreensGaFields, code: impl FnOnce(&mut Ga<'_, Host>) -> R) -> R {
    let mut host = Host;
    let mut ga = unsafe { Ga::new(r, &mut host) };
    code(&mut ga)
}

/// The constructor's members (the record value-initialised): its kind set, then the kind's defaults.
///
/// # Safety
/// `r` is the action's record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_init(r: *mut EeScreensGaFields, kind: u32) {
    unsafe { (*r).mKind = kind };
    unsafe { with(r, |g| g.init()) }
}

/// The destructor's releases.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_free(r: *mut EeScreensGaFields) {
    unsafe { with(r, |g| g.free()) }
}

/// `Virtual4`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_run(r: *mut EeScreensGaFields) {
    unsafe { with(r, |g| g.run()) }
}

/// The mouse click's `Execute`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_screens_ga_click_execute(
    r: *mut EeScreensGaFields,
    x: f32,
    y: f32,
    mv: bool,
    modifiers: u32,
    rotating: bool,
    facing: f32,
    force: bool,
) {
    unsafe { with(r, |g| g.click_execute(x, y, mv, modifiers, rotating, facing, force)) }
}

/// The mouse click's `SelectAtMouse`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_select_at_mouse(r: *mut EeScreensGaFields, modifiers: u32) {
    unsafe { with(r, |g| g.select_at_mouse(modifiers)) }
}

/// The lasso's `SelectUnits` over the rectangle `rect` (a `GERectangle*`, handed to the message).
///
/// # Safety
/// As `ee_screens_ga_init`; `rect` lives for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_select_units(
    r: *mut EeScreensGaFields,
    rect: *const c_void,
    flags: i32,
    unit_type: u32,
    patrol_group: i32,
) {
    unsafe { with(r, |g| g.lasso_select(rect as usize, flags, unit_type, patrol_group)) }
}

/// The build's `Build`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build(r: *mut EeScreensGaFields, x: f32, y: f32) {
    unsafe { with(r, |g| g.build(x, y)) }
}

/// The build's `BuildGate`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build_gate(r: *mut EeScreensGaFields, x: f32, y: f32, side: i32) {
    unsafe { with(r, |g| g.build_gate(x, y, side)) }
}

/// The build's `BuildWall`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build_wall(r: *mut EeScreensGaFields, x0: f32, y0: f32, x1: f32, y1: f32) {
    unsafe { with(r, |g| g.build_wall(x0, y0, x1, y1)) }
}

/// The calamity's `Cast`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_cast(r: *mut EeScreensGaFields, x: f32, y: f32) {
    unsafe { with(r, |g| g.cast((x, y))) }
}

/// The calamity's `Check`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_check(r: *mut EeScreensGaFields, x: f32, y: f32) -> i32 {
    unsafe { with(r, |g| g.check((x, y))) }
}

/// The camera's `SetLocation`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_set_location(r: *mut EeScreensGaFields, x: i32, y: i32) {
    unsafe { with(r, |g| g.set_location(x, y)) }
}

/// The camera's `SetUnit` (an `EEUnit*`).
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_set_unit(r: *mut EeScreensGaFields, unit: *mut c_void) {
    unsafe { with(r, |g| g.set_unit(unit as usize)) }
}

/// The edge scroll's `Update`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_scroll_update(r: *mut EeScreensGaFields, x: f32, y: f32) -> bool {
    unsafe { with(r, |g| g.scroll_update((x, y))) }
}

/// The lasso's `SelectInRectangle` (`rect` a `GERectangle*`; the editor's lassos).
///
/// # Safety
/// As `ee_screens_ga_init`; `rect` lives for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_select_in_rectangle(r: *mut EeScreensGaFields, rect: *const c_void) {
    unsafe { with(r, |g| g.select_in_rectangle(rect as usize)) }
}

/// The editor paint's `SetArea` (left, top, right, bottom, in tiles): whether it queued the area.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_paint_set_area(
    r: *mut EeScreensGaFields,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
) -> bool {
    unsafe { with(r, |g| g.paint_set_area([left, top, right, bottom])) }
}

/// The improvement's constructor: the improvement, raise (else lower) and in-game (else the scenario editor's)
/// flags set, the unit (an `EEUnit*`) referenced and held, the action posted.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_upgrade_begin(
    r: *mut EeScreensGaFields,
    unit: *mut c_void,
    improvement: i32,
    raise: bool,
    in_game: bool,
) {
    unsafe {
        (*r).mImprovement = improvement;
        (*r).mRaise = raise;
        (*r).mIsInGame = in_game;
    }
    unsafe { with(r, |g| g.upgrade_begin(unit as usize)) }
}
