//! A node's media and cost text (were EETechTreeNode.cpp's `LoadMedia`, `UnloadMedia` and `GetCostString` and
//! EETechTreeEpoch.cpp's `LoadMediaOfKind` and `UnloadMediaOfKind`).
//!
//! The media walk: a node that makes a unit loads (unloads) its unit class's media and buttons, then each effect of its
//! researched event the media that effect uses (a button, an area effect template, a media item, a sound, another
//! class's media), sleeping a millisecond after each effect while loading, then tells the player the node's media are
//! loaded (unloaded). The objects are C++ behind two host operations: the effect's kind and media id, and one media
//! call on the player's behalf.

use std::ffi::c_void;

use ee_base::util::strings::resource_string;

use crate::tech::nodes::{
    EeTechHostIo, EeTechNodeRef, TechHost, EE_TECH_HOST_EFFECT_COUNT,
};

/// Effect `v[0]` of the researched event of `node` -> `v[0]` its kind, `v[1]` the media id it uses (by kind: the
/// button, the area effect template, the media item, the new unit class, else the sound), `v[2]` 1 when its attribute
/// is an area effect's (0x28)
pub const EE_TECH_HOST_MEDIA_EFFECT: i32 = 44;
/// The player `unit` (an `EEPlayer*`) loads (`v[2]` 1) or unloads (0) media `v[1]` of kind `v[0]` (an
/// `EE_TECH_MEDIA_*`)
pub const EE_TECH_HOST_MEDIA: i32 = 45;

/// `EEButtonObject::Get(id)->Acquire()` / `Release()`
pub const EE_TECH_MEDIA_BUTTON: i32 = 1;
/// `gAEManager.LoadTemplateMedia(id)` / `UnloadTemplateMedia`
pub const EE_TECH_MEDIA_AREA_EFFECT: i32 = 2;
/// `EEMedia::Get(id)->LoadMedia()` / `UnloadMedia()`
pub const EE_TECH_MEDIA_ITEM: i32 = 6;
/// `EESoundObject::Get(id)->LoadMedia()` / `UnloadMedia()`
pub const EE_TECH_MEDIA_SOUND: i32 = 0xf;
/// `player->SetUnitClassMedia(id, 0x7f)` / `UnloadUnitClassMedia(id)`
pub const EE_TECH_MEDIA_CLASS: i32 = 0x13;
/// `player->LoadUnitClassButtons(id)` / `UnloadUnitClassButtons(id)`
pub const EE_TECH_MEDIA_CLASS_BUTTONS: i32 = 0x100;
/// `player->EpochMediaLoaded(id)` / `EpochMediaUnloaded(id)`
pub const EE_TECH_MEDIA_EPOCH: i32 = 0x101;
/// `PlatSleep(1)`
pub const EE_TECH_MEDIA_SLEEP: i32 = 0x102;

fn media<H: TechHost + ?Sized>(h: &mut H, player: *mut c_void, what: i32, id: i32, load: bool) {
    let mut io = EeTechHostIo { unit: player, ..Default::default() };
    io.v[0] = what;
    io.v[1] = id;
    io.v[2] = i32::from(load);
    h.call(EE_TECH_HOST_MEDIA, &mut io);
}

/// What an effect of kind `kind` loads: its media kind, or none.
pub fn effect_media(kind: i32, area_effect: bool) -> Option<i32> {
    match kind {
        1 => Some(EE_TECH_MEDIA_BUTTON),
        2 if area_effect => Some(EE_TECH_MEDIA_AREA_EFFECT),
        6 => Some(EE_TECH_MEDIA_ITEM),
        0xf | 0x10 | 0x11 | 0x12 | 0x14 | 0x15 => Some(EE_TECH_MEDIA_SOUND),
        0x13 => Some(EE_TECH_MEDIA_CLASS),
        _ => None,
    }
}

/// `EETechTreeNode::LoadMedia(player)` (`load`) or `UnloadMedia(player)`.
pub fn node_media<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, player: *mut c_void, load: bool) {
    // SAFETY: a node of a loaded tree has its data; copied out
    let d = unsafe { *n.data };
    if d.mMakesUnit {
        media(h, player, EE_TECH_MEDIA_CLASS, d.mUnitClass as i32, load);
        media(h, player, EE_TECH_MEDIA_CLASS_BUTTONS, d.mUnitClass as i32, load);
    }
    let effects = h.op_node(EE_TECH_HOST_EFFECT_COUNT, n, &[]);
    for i in 0..effects {
        let mut io = EeTechHostIo { node: n, ..Default::default() };
        io.v[0] = i;
        h.call(EE_TECH_HOST_MEDIA_EFFECT, &mut io);
        if let Some(what) = effect_media(io.v[0], io.v[2] != 0) {
            media(h, player, what, io.v[1], load);
        }
        if load {
            media(h, player, EE_TECH_MEDIA_SLEEP, 0, true);
        }
    }
    media(h, player, EE_TECH_MEDIA_EPOCH, d.mID as i32, load);
}

/// `EETechTreeEpoch::LoadMediaOfKind(kind)` (`load`) or `UnloadMediaOfKind`: the epoch's nodes of that kind, in id
/// order, for the tree's player.
pub fn epoch_media_of_kind<H: TechHost + ?Sized>(
    h: &mut H,
    e: EeTechNodeRef,
    kind: i32,
    load: bool,
    player: *mut c_void,
) {
    for n in h.epoch_nodes(e) {
        // SAFETY: as node_media
        if unsafe { (*n.data).mKind } as i32 == kind {
            node_media(h, n, player, load);
        }
    }
}

/// The resources of `GetCostString` in its order, with their names' string ids (`IDS_RESOURCE_NAME_*`).
const COST_RESOURCES: [(i32, u16); 5] = [(2, 30116), (5, 30119), (6, 30120), (4, 30118), (3, 30117)];
/// `IDS_CARBON`: wood's name while the game uses carbon
const IDS_CARBON: u16 = 30121;

/// `EETechTreeNode::GetCostString`: "Food: n Gold: n Iron: n Stone: n Wood: n " of the resources the node costs (at
/// 1.0, each shown at `scale`), wood named carbon when `carbon`. A missing string keeps the name before it, as the
/// C++'s failed `GetString` left it.
pub fn cost_string(cost: &mut dyn FnMut(i32, f32) -> i32, scale: f32, carbon: bool) -> String {
    let mut text = String::new();
    let mut name = String::new();
    for (resource, id) in COST_RESOURCES {
        if cost(resource, 1.0) <= 0 {
            continue;
        }
        let id = if resource == 3 && carbon { IDS_CARBON } else { id };
        if let Some(s) = resource_string(id) {
            name = s.to_string();
        }
        text.push_str(&name);
        text.push_str(": ");
        text.push_str(&cost(resource, scale).to_string());
        text.push(' ');
    }
    text
}

/// A node's cost callback for [`ee_tech_cost_string`]: `(ctx, resource, unit class, scale) -> the cost`.
pub type EeTechCostFn = unsafe extern "C" fn(*mut c_void, i32, i32, f32) -> i32;

/// [`cost_string`] of the node `ctx` for `unit_class` into `out` (NUL-terminated, cut at `size`); its byte length.
///
/// # Safety
/// `cost` is valid with `ctx`; `out` holds `size` bytes (`size` > 0).
#[no_mangle]
pub unsafe extern "C" fn ee_tech_cost_string(
    ctx: *mut c_void,
    cost: EeTechCostFn,
    unit_class: i32,
    scale: f32,
    carbon: bool,
    out: *mut u8,
    size: usize,
) -> usize {
    // SAFETY: the caller's callback
    let text = cost_string(&mut |r, s| unsafe { cost(ctx, r, unit_class, s) }, scale, carbon);
    let n = text.len().min(size.saturating_sub(1));
    // SAFETY: the caller's buffer
    unsafe {
        std::ptr::copy_nonoverlapping(text.as_ptr(), out, n);
        *out.add(n) = 0;
    }
    n
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tech::nodes::{EeTechNodeDataFields, EE_TECH_HOST_EPOCH_NODES};

    #[test]
    fn cost_text() {
        let costs = [0, 0, 50, 0, 20, 0, 7];
        let mut cost = |r: i32, s: f32| (costs[r as usize] as f32 * s) as i32;
        assert_eq!(cost_string(&mut cost, 2.0, false), "Food: 100 Iron: 14 Stone: 40 ");
        let mut wood = |r: i32, _s: f32| if r == 3 { 5 } else { 0 };
        assert_eq!(cost_string(&mut wood, 1.0, true), "Carbon: 5 ");
        assert_eq!(cost_string(&mut wood, 1.0, false), "Wood: 5 ");
        assert_eq!(cost_string(&mut |_, _| 0, 1.0, false), "");
    }

    #[test]
    fn the_buffer_is_cut_and_terminated() {
        unsafe extern "C" fn food(_: *mut c_void, r: i32, _: i32, _: f32) -> i32 {
            i32::from(r == 2) * 1234
        }
        let mut b = [0xffu8; 8];
        let n = unsafe { ee_tech_cost_string(std::ptr::null_mut(), food, 0, 1.0, false, b.as_mut_ptr(), b.len()) };
        assert_eq!(n, 7);
        assert_eq!(&b, b"Food: 1\0");
    }

    struct Host {
        effects: Vec<[i32; 3]>,
        nodes: Vec<EeTechNodeRef>,
        log: Vec<[i32; 3]>,
    }

    impl TechHost for Host {
        fn call(&mut self, op: i32, io: &mut EeTechHostIo) -> i32 {
            match op {
                EE_TECH_HOST_EFFECT_COUNT => return self.effects.len() as i32,
                EE_TECH_HOST_MEDIA_EFFECT => {
                    let e = self.effects[io.v[0] as usize];
                    io.v[..3].copy_from_slice(&e);
                }
                EE_TECH_HOST_MEDIA => self.log.push([io.v[0], io.v[1], io.v[2]]),
                EE_TECH_HOST_EPOCH_NODES => {
                    for (i, n) in self.nodes.iter().enumerate().take(io.count as usize) {
                        // SAFETY: the walk's buffer of `count`
                        unsafe { *io.nodes.add(i) = *n };
                    }
                    return self.nodes.len() as i32;
                }
                _ => {}
            }
            0
        }
    }

    #[test]
    fn the_media_walk() {
        let mut node = crate::tech::nodes::node_fields_new();
        let mut data = EeTechNodeDataFields { mID: 1074, mMakesUnit: true, mUnitClass: 9, mKind: 7, ..Default::default() };
        let n = EeTechNodeRef { obj: std::ptr::null_mut(), node: &mut node, data: &mut data };
        let effects = vec![[1, 40, 0], [2, 3, 0], [2, 4, 1], [6, 5, 0], [0x11, 6, 0], [0x13, 7, 0], [9, 8, 0]];
        let mut h = Host { effects, nodes: vec![n], log: Vec::new() };
        node_media(&mut h, n, std::ptr::null_mut(), true);
        let sleep = [EE_TECH_MEDIA_SLEEP, 0, 1];
        assert_eq!(
            h.log,
            vec![
                [EE_TECH_MEDIA_CLASS, 9, 1],
                [EE_TECH_MEDIA_CLASS_BUTTONS, 9, 1],
                [EE_TECH_MEDIA_BUTTON, 40, 1],
                sleep,
                sleep,
                [EE_TECH_MEDIA_AREA_EFFECT, 4, 1],
                sleep,
                [EE_TECH_MEDIA_ITEM, 5, 1],
                sleep,
                [EE_TECH_MEDIA_SOUND, 6, 1],
                sleep,
                [EE_TECH_MEDIA_CLASS, 7, 1],
                sleep,
                sleep,
                [EE_TECH_MEDIA_EPOCH, 1074, 1],
            ]
        );
        h.log.clear();
        epoch_media_of_kind(&mut h, n, 7, false, std::ptr::null_mut());
        assert_eq!(h.log.len(), 8); // no sleeps while unloading
        assert_eq!(h.log[7], [EE_TECH_MEDIA_EPOCH, 1074, 0]);
        h.log.clear();
        epoch_media_of_kind(&mut h, n, 3, false, std::ptr::null_mut());
        assert!(h.log.is_empty());
    }
}
