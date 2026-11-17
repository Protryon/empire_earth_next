//! A scripted `Effect2DHost` and an in-memory effect file for the twenty-first round's 2D effects' tests
//! (`effect_icons.rs`, `effect_glow.rs`, `effect_decal.rs`): every call logged, the answers set by the test.

use crate::ge::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::gfx::effect_glow::{EeGfxGlowArea, EeGfxGlowPlace, EeGfxGlowSetUp};
use crate::gfx::effect_icons::EeGfxIconUnit;
use crate::gfx::effect_sprite::EeGfx2DPoint;
use crate::gfx::effect_state::{EeGfxParticleTick, EffectFile};
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::raster::EeGfxModel;

#[derive(Default)]
pub struct Host {
    pub calls: Vec<String>,
    pub base: i32,
    pub media: i32,
    pub location: GeTransformation,
    pub unit: EeGfxIconUnit,
    /// the points under an area effect
    pub covered: Vec<(f32, f32)>,
    pub pending: i32,
    pub selected: bool,
    pub owner: bool,
    pub ghosts: u32,
    pub set_up: EeGfxGlowSetUp,
    pub names: Vec<&'static str>,
    pub place: EeGfxGlowPlace,
    pub area: EeGfxGlowArea,
    pub rows: u32,
    pub tick: EeGfxParticleTick,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        self.calls.push("base".into());
        self.base
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        panic!("not these kinds'")
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        EeGfxFrames::default()
    }
    fn move_to_point(&mut self) {
        self.calls.push("move".into());
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn selection_size(&mut self) -> f32 {
        0.0
    }
    fn confirm_enemy(&mut self) -> bool {
        false
    }
    fn confirm_goal(&mut self, _goal_index: u32, _tile_x: i32, _tile_y: i32) -> EeGfxConfirmGoal {
        EeGfxConfirmGoal::NoUnit
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        EeGfxConfirmUnit::default()
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.location
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn command_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn object_media(&mut self) -> i32 {
        self.calls.push("media".into());
        self.media
    }
    fn icon_unit(&mut self) -> EeGfxIconUnit {
        self.unit
    }
    fn area_effect(&mut self, kind: i32, x: f32, y: f32, player: u32) -> bool {
        self.calls.push(format!("area {kind} {x} {y} {player}"));
        self.covered.iter().any(|&(a, b)| (a - x).abs() < 1e-4 && (b - y).abs() < 1e-4)
    }
    fn area_effects(&mut self) -> i32 {
        self.pending
    }
    fn set_area_effects(&mut self, count: i32) {
        self.pending = count;
    }
    fn icons_selected(&mut self) -> bool {
        self.selected
    }
    fn icons_owner(&mut self) -> bool {
        self.owner
    }
    fn ghost_units(&mut self) -> u32 {
        self.ghosts
    }
    fn ground_point(&mut self, at: Ge3DPoint) -> EeGfx2DPoint {
        EeGfx2DPoint { has: at.x >= 0.0, point: Ge3DPoint { z: at.z - 1.0, ..at } }
    }
    fn glow_set_up(&mut self) -> EeGfxGlowSetUp {
        self.calls.push("glow setup".into());
        self.set_up
    }
    fn glow_tag_name(&mut self, index: u32) -> Vec<u8> {
        self.names[index as usize].as_bytes().to_vec()
    }
    fn glow_place(&mut self) -> EeGfxGlowPlace {
        self.place
    }
    fn glow_point(&mut self, index: u32) -> Ge3DPoint {
        Ge3DPoint { x: index as f32, y: 10.0, z: 20.0 }
    }
    fn glow_area(&mut self) -> EeGfxGlowArea {
        self.area
    }
    fn glow_rows(&mut self) -> u32 {
        self.rows
    }
    fn particle_tick(&mut self) -> EeGfxParticleTick {
        self.tick
    }
}

/// A file of tagged 32-bit values (a read of the wrong type or past the end fails with 3).
#[derive(Default)]
pub struct Mem {
    pub items: Vec<(char, u32)>,
    pub at: usize,
}

impl Mem {
    fn read(&mut self, t: char) -> Result<u32, i32> {
        match self.items.get(self.at) {
            Some(&(k, v)) if k == t => {
                self.at += 1;
                Ok(v)
            }
            _ => Err(3),
        }
    }
}

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        self.read('u').map(|x| *v = x).err().unwrap_or(0)
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        self.read('i').map(|x| *v = x as i32).err().unwrap_or(0)
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        self.read('f').map(|x| *v = f32::from_bits(x)).err().unwrap_or(0)
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        self.read('b').map(|x| *v = x != 0).err().unwrap_or(0)
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.items.push(('u', v));
        0
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        self.items.push(('i', v as u32));
        0
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.items.push(('f', v.to_bits()));
        0
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.items.push(('b', v as u32));
        0
    }
}
