//! `GXDevice` (IDirect3DDevice7): the fixed-function state and the draws, and `GXVertexBuffer`.

use crate::abi::*;
use crate::cformat;
use crate::gx::{
    Backend, DrawCall, Gx, Platform, RingKind, INDEX_RING_SIZE, UNIFORM_BINDING, UNIFORM_RING_SIZE, VERTEX_RING_SIZE,
};
use crate::keys::{self, BlitMode, PipelineState};
use crate::stats::{self, StatsInput, VERTEX_SIZE};
use crate::surface::SurfaceId;
use crate::uniform::{self, color, DeviceState};

pub struct VertexBuffer {
    pub vertices: Vec<u8>,
    pub stride: u32,
    pub count: u32,
    pub references: u32,
}

impl VertexBuffer {
    /// `CreateVertexBuffer`: the stride from the FVF, the vertices zeroed.
    pub fn new(fvf: u32, count: u32) -> VertexBuffer {
        let mut stride = 0;
        if fvf & GXFVF_XYZ != 0 {
            stride += 12;
        }
        if fvf & GXFVF_NORMAL != 0 {
            stride += 12;
        }
        if fvf & GXFVF_DIFFUSE != 0 {
            stride += 4;
        }
        if fvf & GXFVF_TEX1 != 0 {
            stride += 8;
        }
        VertexBuffer { vertices: vec![0; stride as usize * count as usize], stride, count, references: 1 }
    }
}

pub struct Device {
    pub references: u32,
    pub target: Option<SurfaceId>,
    pub viewport: GxViewport,
    pub transform: [GxMatrix; 4],
    pub render_state: [u64; GXRS_COUNT],
    pub stage_state: [[u64; GXTSS_COUNT]; 2],
    pub texture: [Option<SurfaceId>; 2],
    pub material: GxMaterial,
    pub lights: [GxLight; 8],
    pub light_on: [bool; 8],
    pub uniform_dirty: bool,
    pub uniform_offset: u64,
    pub uniform_serial: u64,
}

impl Default for Device {
    /// Direct3D 7's defaults for the states the renderer does not set itself.
    fn default() -> Self {
        let mut d = Device {
            references: 1,
            target: None,
            viewport: GxViewport::default(),
            transform: [GxMatrix::default(); 4],
            render_state: [0; GXRS_COUNT],
            stage_state: [[0; GXTSS_COUNT]; 2],
            texture: [None; 2],
            material: GxMaterial::default(),
            lights: [GxLight::default(); 8],
            light_on: [false; 8],
            uniform_dirty: true,
            uniform_offset: 0,
            uniform_serial: 0,
        };
        for i in 1..=3 {
            d.transform[i] = GxMatrix::identity();
        }
        let rs = &mut d.render_state;
        rs[GXRS_ZENABLE] = 1;
        rs[GXRS_ZWRITEENABLE] = 1;
        rs[GXRS_ZFUNC] = u64::from(GXCMP_LESSEQUAL);
        rs[GXRS_FILLMODE] = u64::from(GXFILL_SOLID);
        rs[GXRS_SHADEMODE] = u64::from(GXSHADE_GOURAUD);
        rs[GXRS_SRCBLEND] = u64::from(GXBLEND_ONE);
        rs[GXRS_DESTBLEND] = u64::from(GXBLEND_ZERO);
        rs[GXRS_CULLMODE] = u64::from(GXCULL_CCW);
        rs[GXRS_ALPHAFUNC] = u64::from(GXCMP_ALWAYS);
        rs[GXRS_LIGHTING] = 1;
        rs[GXRS_COLORVERTEX] = 1;
        rs[GXRS_DIFFUSEMATERIALSOURCE] = u64::from(GXMCS_COLOR1);
        rs[GXRS_AMBIENTMATERIALSOURCE] = u64::from(GXMCS_MATERIAL);
        rs[GXRS_FOGEND] = 0x3f80_0000; // 1.0f
        for (s, stage) in d.stage_state.iter_mut().enumerate() {
            stage[GXTSS_COLOROP] = u64::from(if s != 0 { GXTOP_DISABLE } else { GXTOP_MODULATE });
            stage[GXTSS_ALPHAOP] = u64::from(if s != 0 { GXTOP_DISABLE } else { GXTOP_SELECTARG1 });
            stage[GXTSS_COLORARG1] = u64::from(GXTA_TEXTURE);
            stage[GXTSS_COLORARG2] = u64::from(GXTA_CURRENT);
            stage[GXTSS_ALPHAARG1] = u64::from(GXTA_TEXTURE);
            stage[GXTSS_ALPHAARG2] = u64::from(GXTA_CURRENT);
            stage[GXTSS_ADDRESS] = u64::from(GXTADDRESS_WRAP);
            stage[GXTSS_MAGFILTER] = u64::from(GXTFG_POINT);
            stage[GXTSS_MINFILTER] = u64::from(GXTFN_POINT);
            stage[GXTSS_MIPFILTER] = u64::from(GXTFP_NONE);
        }
        d
    }
}

impl Device {
    fn state(&self, has_key: bool) -> DeviceState<'_> {
        DeviceState {
            world: &self.transform[GXTS_WORLD as usize],
            view: &self.transform[GXTS_VIEW as usize],
            projection: &self.transform[GXTS_PROJECTION as usize],
            viewport: &self.viewport,
            render_state: &self.render_state,
            stage_state: &self.stage_state[0],
            material: &self.material,
            lights: &self.lights,
            light_on: &self.light_on,
            texture_has_key: has_key,
        }
    }

    pub fn set_transform(&mut self, state: u32, matrix: &GxMatrix) -> GxResult {
        if !(GXTS_WORLD..=GXTS_PROJECTION).contains(&state) {
            return GXERR_INVALIDPARAMS;
        }
        self.transform[state as usize] = *matrix;
        self.uniform_dirty = true;
        GX_OK
    }

    pub fn get_transform(&self, state: u32) -> Result<GxMatrix, GxResult> {
        if !(GXTS_WORLD..=GXTS_PROJECTION).contains(&state) {
            return Err(GXERR_INVALIDPARAMS);
        }
        Ok(self.transform[state as usize])
    }

    pub fn set_render_state(&mut self, state: u32, value: u64) -> GxResult {
        let Some(slot) = self.render_state.get_mut(state as usize) else {
            return GXERR_INVALIDPARAMS;
        };
        if *slot != value {
            *slot = value;
            self.uniform_dirty = true;
        }
        GX_OK
    }

    pub fn get_render_state(&self, state: u32) -> Result<u64, GxResult> {
        self.render_state.get(state as usize).copied().ok_or(GXERR_INVALIDPARAMS)
    }

    pub fn set_stage_state(&mut self, stage: u32, state: u32, value: u64) -> GxResult {
        if stage > 1 || state as usize >= GXTSS_COUNT {
            return GXERR_INVALIDPARAMS;
        }
        self.stage_state[stage as usize][state as usize] = value;
        self.uniform_dirty = true;
        GX_OK
    }
}

/// `GXDevice::SetRenderTarget`.
pub fn set_render_target<B: Backend, P: Platform>(
    gx: &mut Gx<B, P>,
    device: &mut Device,
    target: SurfaceId,
) -> GxResult {
    gx.add_ref(target);
    if let Some(old) = device.target {
        gx.release(old);
    }
    device.target = Some(target);
    gx.m(target).is_render_target = true;
    GX_OK
}

/// `GXDevice::SetTexture`.
pub fn set_texture<B: Backend, P: Platform>(
    gx: &mut Gx<B, P>,
    device: &mut Device,
    stage: u32,
    texture: Option<SurfaceId>,
) -> GxResult {
    if stage > 1 {
        return GXERR_INVALIDPARAMS;
    }
    if let Some(texture) = texture {
        gx.add_ref(texture);
    }
    if let Some(old) = device.texture[stage as usize] {
        gx.release(old);
    }
    device.texture[stage as usize] = texture;
    device.uniform_dirty = true;
    GX_OK
}

/// A device reaching zero references.
pub fn destroy_device<B: Backend, P: Platform>(gx: &mut Gx<B, P>, device: &mut Device) {
    for texture in device.texture.into_iter().flatten() {
        gx.release(texture);
    }
    if let Some(target) = device.target {
        gx.release(target);
    }
}

/// `StatsOn`.
pub fn stats_on<B: Backend, P: Platform>(gx: &mut Gx<B, P>) -> bool {
    if gx.stats_every == -2 {
        gx.stats_every = match gx.plat.env("EE_GX_STATS") {
            Some(value) => cformat::strtol_10(&value).max(1),
            None => -1,
        };
    }
    gx.stats_every > 0
}

/// `GXDevice::BeginScene`.
pub fn begin_scene<B: Backend, P: Platform>(gx: &mut Gx<B, P>) -> GxResult {
    if stats_on(gx) {
        gx.stats.scenes += 1;
        if i64::from(gx.stats.scenes) >= gx.stats_every {
            let lines = stats::stats_lines(&gx.stats);
            for line in &lines {
                gx.log(line);
            }
            let live = stats::live_line(gx.live_surfaces(), gx.presents, gx.acquire_failures, &gx.backend.live());
            gx.log(&live);
            gx.stats = stats::DrawStats::default();
        }
    }
    if gx.null3d_ms == -2 {
        match gx.plat.env("EE_GX_NULL3D") {
            Some(value) => {
                gx.null3d_ms = cformat::strtol_10(&value);
                gx.log(&format!("GX: null 3D device, one scene every {} ms", gx.null3d_ms));
            }
            None => gx.null3d_ms = -1,
        }
    }
    if gx.null3d_ms >= 0 {
        let now = gx.plat.now_ms();
        gx.draw_scene = gx.null3d_ms > 0 && now >= gx.next_key_scene;
        if gx.draw_scene {
            gx.next_key_scene = now + gx.null3d_ms as u64;
        }
    }
    GX_OK
}

/// `GXDevice::Clear`.
#[allow(clippy::too_many_arguments)]
pub fn clear<B: Backend, P: Platform>(
    gx: &mut Gx<B, P>,
    device: &mut Device,
    rects: Option<&[GxRect]>,
    flags: u32,
    color_value: u32,
    z: f32,
) -> GxResult {
    let Some(target) = device.target else {
        return GXERR_GENERIC;
    };
    if !gx.draw_scene {
        return GX_OK;
    }
    if stats_on(gx) {
        gx.stats.clears += 1;
    }
    let v = device.viewport;
    let (target_width, target_height) = (gx.s(target).width as i32, gx.s(target).height as i32);
    let fill = color(color_value);
    let count = rects.map_or(1, <[GxRect]>::len);
    for i in 0..count {
        let mut rect = Rect::new(v.dwX as i32, v.dwY as i32, (v.dwX + v.dwWidth) as i32, (v.dwY + v.dwHeight) as i32);
        if let Some(rects) = rects {
            let r = rects[i];
            rect.left = rect.left.max(r.x1 as i32);
            rect.top = rect.top.max(r.y1 as i32);
            rect.right = rect.right.min(r.x2 as i32);
            rect.bottom = rect.bottom.min(r.y2 as i32);
        }
        rect.right = rect.right.min(target_width);
        rect.bottom = rect.bottom.min(target_height);
        if rect.right <= rect.left || rect.bottom <= rect.top {
            continue;
        }
        if flags & GXCLEAR_TARGET != 0 {
            gx.blit(target, &rect, None, None, BlitMode::Fill, Some(fill), 0.0, false);
        }
        if flags & GXCLEAR_ZBUFFER != 0 {
            gx.blit(target, &rect, None, None, BlitMode::Depth, Some(fill), z, false);
        }
    }
    GX_OK
}

/// `Draw`: one `DrawPrimitiveVB` or `DrawIndexedPrimitiveVB`.
#[allow(clippy::too_many_arguments)]
pub fn draw<B: Backend, P: Platform>(
    gx: &mut Gx<B, P>,
    device: &mut Device,
    primitive_type: u32,
    buffer: Option<&VertexBuffer>,
    start: u32,
    count: u32,
    indices: Option<&[u16]>,
) -> GxResult {
    if !gx.draw_scene {
        return GX_OK;
    }
    let invalid = if count != 0 { GXERR_INVALIDPARAMS } else { GX_OK };
    let (Some(target), Some(buffer)) = (device.target, buffer) else {
        return invalid;
    };
    if count == 0 || buffer.stride as usize != VERTEX_SIZE || start + count > buffer.count {
        return invalid;
    }
    if indices.is_some_and(<[u16]>::is_empty) {
        return GX_OK;
    }
    let texture = device.texture[0].filter(|_| device.stage_state[0][GXTSS_COLOROP] != u64::from(GXTOP_DISABLE));
    if let Some(texture) = texture {
        gx.for_gpu_read(texture);
    }
    gx.for_gpu_write(target);
    if !gx.create_texture(target) {
        return GXERR_GENERIC;
    }
    let mut depth = gx.s(target).depth;
    if let Some(d) = depth {
        if !gx.create_texture(d) {
            depth = None;
        }
    }

    // room in the rings first (a full ring submits, which closes the pass)
    let vertex_bytes = u64::from(count) * VERTEX_SIZE as u64;
    let index_bytes = indices.map_or(0, |i| (i.len() as u64 * 2 + 3) & !3);
    if gx.ring_room(RingKind::Vertex) + vertex_bytes + 64 > VERTEX_RING_SIZE
        || gx.ring_room(RingKind::Index) + index_bytes + 64 > INDEX_RING_SIZE
        || gx.ring_room(RingKind::Uniform) + 4096 > UNIFORM_RING_SIZE - UNIFORM_BINDING
    {
        gx.submit();
    }
    if device.uniform_dirty || device.uniform_serial != gx.serial {
        let has_key = texture.is_some_and(|t| {
            device.render_state[GXRS_COLORKEYENABLE] != 0 && {
                let top = gx.top(t);
                gx.s(top).has_key
            }
        });
        let u = uniform::build_fixed_function(&device.state(has_key));
        device.uniform_offset =
            gx.ring(RingKind::Uniform, UNIFORM_RING_SIZE - UNIFORM_BINDING, uniform::as_bytes(&u), 256);
        device.uniform_serial = gx.serial;
        device.uniform_dirty = false;
    }
    let vertices = &buffer.vertices[start as usize * VERTEX_SIZE..][..count as usize * VERTEX_SIZE];
    let vertex_offset = gx.ring(RingKind::Vertex, VERTEX_RING_SIZE, vertices, 4);
    let mut index_offset = 0;
    if let Some(indices) = indices {
        let bytes = indices.iter().flat_map(|i| i.to_le_bytes()).collect::<Vec<u8>>();
        index_offset = gx.ring(RingKind::Index, INDEX_RING_SIZE, &bytes, 4);
    }

    let rs = &device.render_state;
    let blend = rs[GXRS_ALPHABLENDENABLE] != 0;
    let key = keys::pipeline_key(&PipelineState {
        topology: stats::topology_of(primitive_type) as u32,
        cull_mode: rs[GXRS_CULLMODE] as u32,
        blend_enabled: blend,
        src_blend: rs[GXRS_SRCBLEND] as u32,
        dst_blend: rs[GXRS_DESTBLEND] as u32,
        has_depth_target: depth.is_some(),
        z_enable: rs[GXRS_ZENABLE] != 0,
        z_func: rs[GXRS_ZFUNC] as u32,
        z_write: rs[GXRS_ZWRITEENABLE] != 0,
        flat: rs[GXRS_SHADEMODE] as u32 == GXSHADE_FLAT,
        target_has_alpha: gx.s(target).format.mask[3] != 0,
    });

    gx.begin_pass(target, depth);
    let v = device.viewport;
    let (target_width, target_height) = (gx.s(target).width as f32, gx.s(target).height as f32);
    let mut x = (v.dwX as f32).min(target_width);
    let mut y = (v.dwY as f32).min(target_height);
    let mut w = (v.dwWidth as f32).min(target_width - x);
    let mut h = (v.dwHeight as f32).min(target_height - y);
    // a scaled target (GXRenderScale): the logical viewport on its larger texture
    let scale = gx.s(target).scale;
    x *= scale;
    y *= scale;
    let (tex_width, tex_height) = (gx.s(target).tex_width() as f32, gx.s(target).tex_height() as f32);
    w = (w * scale).min(tex_width - x);
    h = (h * scale).min(tex_height - y);
    if w <= 0.0 || h <= 0.0 {
        return GX_OK;
    }
    let min_z = cpp_max(0.0, cpp_min(1.0, v.dvMinZ));
    let max_z = cpp_max(min_z, cpp_min(1.0, v.dvMaxZ));
    let s = &device.stage_state[0];
    let textured_top = texture.and_then(|t| gx.sampled_texture(t));
    let mip = if textured_top.is_some_and(|t| gx.s(t).mip_levels > 1) { s[GXTSS_MIPFILTER] as u32 } else { GXTFP_NONE };
    let sampler_key =
        gx.sampler_key(s[GXTSS_ADDRESS] as u32, s[GXTSS_MINFILTER] as u32, s[GXTSS_MAGFILTER] as u32, mip);
    if stats_on(gx) {
        let wvp_input = StatsInput {
            world: &device.transform[GXTS_WORLD as usize].m,
            view: &device.transform[GXTS_VIEW as usize].m,
            projection: &device.transform[GXTS_PROJECTION as usize].m,
            render_state: &device.render_state,
        };
        stats::account_draw(
            &mut gx.stats,
            primitive_type,
            &wvp_input,
            vertices,
            count,
            indices.map(|i| i.len() as u32),
            texture.is_some(),
        );
    }
    let index = indices.map(|i| (index_offset, index_bytes, i.len() as u32));
    gx.backend.draw(&DrawCall {
        pipeline_key: key,
        viewport: [x, y, w, h, min_z, max_z],
        scissor: (gx.s(target).tex_width(), gx.s(target).tex_height()),
        texture: textured_top,
        sampler_key,
        uniform_offset: device.uniform_offset as u32,
        vertex_offset,
        vertex_bytes,
        index,
        vertex_count: count,
    });
    if let Some(top) = textured_top {
        let serial = gx.serial;
        gx.m(top).used_serial = serial;
    }
    let serial = gx.serial;
    gx.m(target).used_serial = serial;
    GX_OK
}

/// `std::min`: on NaN it keeps the left operand, unlike `f32::min`.
fn cpp_min(a: f32, b: f32) -> f32 {
    if b < a {
        b
    } else {
        a
    }
}

/// `std::max`: on NaN it keeps the left operand, unlike `f32::max`.
fn cpp_max(a: f32, b: f32) -> f32 {
    if a < b {
        b
    } else {
        a
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn device_defaults_match_direct3d7() {
        let d = Device::default();
        assert_eq!(d.transform[0], GxMatrix::default());
        assert_eq!(d.transform[1], GxMatrix::identity());
        assert_eq!(d.render_state[GXRS_ZENABLE], 1);
        assert_eq!(d.render_state[GXRS_CULLMODE], u64::from(GXCULL_CCW));
        assert_eq!(d.render_state[GXRS_FOGEND], 0x3f80_0000);
        assert_eq!(d.stage_state[0][GXTSS_COLOROP], u64::from(GXTOP_MODULATE));
        assert_eq!(d.stage_state[1][GXTSS_COLOROP], u64::from(GXTOP_DISABLE));
        assert_eq!(d.stage_state[1][GXTSS_ALPHAARG2], u64::from(GXTA_CURRENT));
        assert_eq!(d.stage_state[0][GXTSS_ADDRESS], u64::from(GXTADDRESS_WRAP));
    }

    #[test]
    fn transforms_reject_bad_indices() {
        let mut d = Device::default();
        assert_eq!(d.set_transform(0, &GxMatrix::identity()), GXERR_INVALIDPARAMS);
        assert_eq!(d.set_transform(4, &GxMatrix::identity()), GXERR_INVALIDPARAMS);
        let mut m = GxMatrix::identity();
        m.m[3] = 9.0;
        assert_eq!(d.set_transform(GXTS_VIEW, &m), GX_OK);
        assert_eq!(d.get_transform(GXTS_VIEW), Ok(m));
        assert_eq!(d.get_transform(0), Err(GXERR_INVALIDPARAMS));
    }

    #[test]
    fn a_render_state_marks_the_uniform_dirty_only_when_changed() {
        let mut d = Device { uniform_dirty: false, ..Default::default() };
        assert_eq!(d.set_render_state(GXRS_ZENABLE as u32, 1), GX_OK);
        assert!(!d.uniform_dirty);
        assert_eq!(d.set_render_state(GXRS_ZENABLE as u32, 0), GX_OK);
        assert!(d.uniform_dirty);
        assert_eq!(d.set_render_state(GXRS_COUNT as u32, 0), GXERR_INVALIDPARAMS);
        assert_eq!(d.get_render_state(GXRS_ZENABLE as u32), Ok(0));
        assert_eq!(d.get_render_state(1000), Err(GXERR_INVALIDPARAMS));
    }

    #[test]
    fn stage_states_reject_bad_stages() {
        let mut d = Device::default();
        assert_eq!(d.set_stage_state(2, 1, 0), GXERR_INVALIDPARAMS);
        assert_eq!(d.set_stage_state(0, GXTSS_COUNT as u32, 0), GXERR_INVALIDPARAMS);
        assert_eq!(d.set_stage_state(1, GXTSS_ADDRESS as u32, 9), GX_OK);
        assert_eq!(d.stage_state[1][GXTSS_ADDRESS], 9);
    }

    #[test]
    fn vertex_buffer_stride_comes_from_the_fvf() {
        let vb = VertexBuffer::new(GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1, 3);
        assert_eq!(vb.stride, 36);
        assert_eq!(vb.vertices.len(), 108);
        assert_eq!(VertexBuffer::new(GXFVF_XYZ, 2).vertices.len(), 24);
        assert_eq!(VertexBuffer::new(0, 5).stride, 0);
    }
}
