//! The state-to-key functions and the mappings from Direct3D 7 state to WebGPU state: the texture filter setting, the
//! sampler key, the pipeline key, blend factors and compare functions. wgpu's enums are plain data: none of this needs a
//! device.

use crate::abi::*;

/// How a blit quad draws. The values are the C++'s `GXBlitMode`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum BlitMode {
    Copy,
    Keyed,
    Fill,
    Premultiplied,
    Depth,
}

/// `GXSamplerKey`: address (2 bits) | min (2) | mag (2) | mip (2), bit 8 anisotropic; `filtering` is the setting
/// (`GXSetTextureFiltering`).
pub fn sampler_key(address: u32, min: u32, mag: u32, mip: u32, filtering: u32) -> u32 {
    let (mut mip, mut anisotropic) = (mip, 0);
    if mip != GXTFP_NONE && mip != 0 && min >= GXTFN_LINEAR {
        if filtering >= 1 {
            mip = GXTFP_LINEAR;
        }
        if filtering >= 2 {
            anisotropic = 1;
        }
    }
    (address & 3) | ((min & 3) << 2) | ((mag & 3) << 4) | ((mip & 3) << 6) | (anisotropic << 8)
}

/// What a sampler key asks of `wgpu::SamplerDescriptor`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SamplerDesc {
    pub address: wgpu::AddressMode,
    pub min_filter: wgpu::FilterMode,
    pub mag_filter: wgpu::FilterMode,
    pub mip_filter: wgpu::FilterMode,
    pub lod_min: f32,
    pub lod_max: f32,
    pub anisotropy: u16,
}

pub fn sampler_desc(key: u32) -> SamplerDesc {
    let address = match key & 3 {
        GXTADDRESS_WRAP => wgpu::AddressMode::Repeat,
        GXTADDRESS_MIRROR => wgpu::AddressMode::MirrorRepeat,
        _ => wgpu::AddressMode::ClampToEdge,
    };
    let filter = |linear: bool| if linear { wgpu::FilterMode::Linear } else { wgpu::FilterMode::Nearest };
    let mip = (key >> 6) & 3;
    let mut desc = SamplerDesc {
        address,
        min_filter: filter(((key >> 2) & 3) >= GXTFN_LINEAR),
        mag_filter: filter(((key >> 4) & 3) >= GXTFG_LINEAR),
        mip_filter: if mip == GXTFP_LINEAR { wgpu::FilterMode::Linear } else { wgpu::FilterMode::Nearest },
        lod_min: 0.0,
        lod_max: if mip == GXTFP_NONE || mip == 0 { 0.0 } else { 32.0 },
        anisotropy: 1,
    };
    if key & 0x100 != 0 {
        // anisotropic: every filter linear, as WebGPU requires
        desc.min_filter = wgpu::FilterMode::Linear;
        desc.mag_filter = wgpu::FilterMode::Linear;
        desc.mip_filter = wgpu::FilterMode::Linear;
        desc.anisotropy = 8;
    }
    desc
}

/// The inputs of the pipeline key (`Draw`).
#[derive(Clone, Copy, Debug, Default)]
pub struct PipelineState {
    pub topology: u32, // 0 points, 1 lines, 2 triangles
    pub cull_mode: u32,
    pub blend_enabled: bool,
    pub src_blend: u32,
    pub dst_blend: u32,
    pub has_depth_target: bool,
    pub z_enable: bool,
    pub z_func: u32,
    pub z_write: bool,
    pub flat: bool,
    pub target_has_alpha: bool,
}

/// Pipeline key: topology 2 | cull 2 | blend 1 | src 4 | dst 4 | depth 1 | zfunc 4 | zwrite 1 | flat 1 | target alpha 1.
pub fn pipeline_key(s: &PipelineState) -> u32 {
    let cull = if s.topology == 2 {
        match s.cull_mode {
            GXCULL_CW => 1,
            GXCULL_CCW => 2,
            _ => 0,
        }
    } else {
        0
    };
    let z = s.has_depth_target && s.z_enable;
    let blend_bits = if s.blend_enabled { ((s.src_blend & 15) << 5) | ((s.dst_blend & 15) << 9) } else { 0 };
    (s.topology & 3)
        | (cull << 2)
        | (u32::from(s.blend_enabled) << 4)
        | blend_bits
        | (u32::from(s.has_depth_target) << 13)
        | (if z { (s.z_func & 15) << 14 } else { 0 })
        | (u32::from(z && s.z_write) << 18)
        | (u32::from(s.flat) << 19)
        | (u32::from(s.target_has_alpha) << 20)
}

/// What a pipeline key asks of a render pipeline.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PipelineDesc {
    pub topology: wgpu::PrimitiveTopology,
    pub cull: Option<wgpu::Face>,
    pub blend: Option<wgpu::BlendState>,
    pub depth: bool,
    pub z_compare: wgpu::CompareFunction,
    pub z_write: bool,
    pub flat: bool,
}

pub fn blend_factor(blend: u32, target_alpha: bool) -> wgpu::BlendFactor {
    use wgpu::BlendFactor::*;
    match blend {
        GXBLEND_ZERO => Zero,
        GXBLEND_ONE => One,
        GXBLEND_SRCCOLOR => Src,
        GXBLEND_INVSRCCOLOR => OneMinusSrc,
        GXBLEND_SRCALPHA => SrcAlpha,
        GXBLEND_INVSRCALPHA => OneMinusSrcAlpha,
        GXBLEND_DESTALPHA => {
            if target_alpha {
                DstAlpha
            } else {
                One
            }
        }
        GXBLEND_INVDESTALPHA => {
            if target_alpha {
                OneMinusDstAlpha
            } else {
                Zero
            }
        }
        GXBLEND_DESTCOLOR => Dst,
        GXBLEND_INVDESTCOLOR => OneMinusDst,
        GXBLEND_SRCALPHASAT => SrcAlphaSaturated,
        _ => One,
    }
}

pub fn compare_function(function: u32) -> wgpu::CompareFunction {
    use wgpu::CompareFunction::*;
    match function {
        GXCMP_NEVER => Never,
        GXCMP_LESS => Less,
        GXCMP_EQUAL => Equal,
        GXCMP_LESSEQUAL => LessEqual,
        GXCMP_GREATER => Greater,
        GXCMP_NOTEQUAL => NotEqual,
        GXCMP_GREATEREQUAL => GreaterEqual,
        _ => Always,
    }
}

/// `Pipeline` decoded: the blend factors, including the two combined `BOTH*` modes and the saturated-source rule.
pub fn pipeline_desc(key: u32) -> PipelineDesc {
    let topology = match key & 3 {
        0 => wgpu::PrimitiveTopology::PointList,
        1 => wgpu::PrimitiveTopology::LineList,
        _ => wgpu::PrimitiveTopology::TriangleList,
    };
    let cull = match (key >> 2) & 3 {
        2 => Some(wgpu::Face::Front),
        1 => Some(wgpu::Face::Back),
        _ => None,
    };
    let blending = (key >> 4) & 1 != 0;
    let (source, destination) = ((key >> 5) & 15, (key >> 9) & 15);
    let depth = (key >> 13) & 1 != 0;
    let z_function = (key >> 14) & 15;
    let z_write = (key >> 18) & 1 != 0;
    let flat = (key >> 19) & 1 != 0;
    let target_alpha = (key >> 20) & 1 != 0;
    let mut blend = None;
    if blending {
        let (mut src, mut dst) = (blend_factor(source, target_alpha), blend_factor(destination, target_alpha));
        if source == GXBLEND_BOTHSRCALPHA {
            src = wgpu::BlendFactor::SrcAlpha;
            dst = wgpu::BlendFactor::OneMinusSrcAlpha;
        } else if source == GXBLEND_BOTHINVSRCALPHA {
            src = wgpu::BlendFactor::OneMinusSrcAlpha;
            dst = wgpu::BlendFactor::SrcAlpha;
        }
        let alpha_src = if src == wgpu::BlendFactor::SrcAlphaSaturated {
            wgpu::BlendFactor::One // not allowed on the alpha channel
        } else {
            src
        };
        blend = Some(wgpu::BlendState {
            color: wgpu::BlendComponent { src_factor: src, dst_factor: dst, operation: wgpu::BlendOperation::Add },
            alpha: wgpu::BlendComponent {
                src_factor: alpha_src,
                dst_factor: dst,
                operation: wgpu::BlendOperation::Add,
            },
        });
    }
    PipelineDesc {
        topology,
        cull,
        blend,
        depth,
        z_compare: if z_function != 0 { compare_function(z_function) } else { wgpu::CompareFunction::Always },
        z_write,
        flat,
    }
}

/// `BlitPipeline`'s blend: the premultiplied text blit's "over".
pub fn blit_blend(mode: BlitMode) -> Option<wgpu::BlendState> {
    if mode != BlitMode::Premultiplied {
        return None;
    }
    let component = wgpu::BlendComponent {
        src_factor: wgpu::BlendFactor::One,
        dst_factor: wgpu::BlendFactor::OneMinusSrcAlpha,
        operation: wgpu::BlendOperation::Add,
    };
    Some(wgpu::BlendState { color: component, alpha: component })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn golden_sampler_keys_from_the_cpp() {
        // golden_samplers.txt: GXSamplerKey over its inputs and the three filtering settings
        let mut count = 0;
        for line in include_str!("golden_samplers.txt").lines() {
            let v: Vec<u32> = line.split_whitespace().map(|f| f.parse().unwrap()).collect();
            if v.is_empty() {
                continue;
            }
            assert_eq!(sampler_key(v[0], v[1], v[2], v[3], v[4]), v[5], "{line}");
            count += 1;
        }
        assert!(count > 100);
    }

    #[test]
    fn sampler_key_upgrades_only_mipped_linear_sampling() {
        // point minification or no mips: the setting does nothing
        assert_eq!(sampler_key(1, 1, 2, 2, 2), sampler_key(1, 1, 2, 2, 0));
        assert_eq!(sampler_key(1, 2, 2, GXTFP_NONE, 2), sampler_key(1, 2, 2, GXTFP_NONE, 0));
        assert_ne!(sampler_key(1, 2, 2, 2, 2) & 0x100, 0);
        assert_eq!(sampler_key(1, 2, 2, 2, 1) & 0x100, 0);
    }

    #[test]
    fn sampler_desc_decodes_the_key() {
        let d = sampler_desc(sampler_key(GXTADDRESS_MIRROR, GXTFN_LINEAR, GXTFG_POINT, GXTFP_NONE, 0));
        assert_eq!(d.address, wgpu::AddressMode::MirrorRepeat);
        assert_eq!(d.min_filter, wgpu::FilterMode::Linear);
        assert_eq!(d.mag_filter, wgpu::FilterMode::Nearest);
        assert_eq!(d.lod_max, 0.0);
        let d = sampler_desc(sampler_key(GXTADDRESS_WRAP, GXTFN_LINEAR, GXTFG_LINEAR, 2, 2));
        assert_eq!(d.anisotropy, 8);
        assert_eq!(d.mip_filter, wgpu::FilterMode::Linear);
        assert_eq!(d.lod_max, 32.0);
        assert_eq!(
            sampler_desc(sampler_key(0, GXTFN_POINT, GXTFG_POINT, 0, 0)).address,
            wgpu::AddressMode::ClampToEdge
        );
        assert_eq!(sampler_desc(sampler_key(0, GXTFN_POINT, GXTFG_POINT, 0, 0)).lod_max, 0.0);
    }

    #[test]
    fn golden_pipeline_keys_from_the_cpp() {
        let mut count = 0;
        for line in include_str!("golden_pipeline_keys.txt").lines() {
            let f: Vec<u32> = line.split_whitespace().map(|f| f.parse().unwrap()).collect();
            if f.is_empty() {
                continue;
            }
            let key = pipeline_key(&PipelineState {
                topology: f[0],
                cull_mode: f[1],
                blend_enabled: f[2] != 0,
                src_blend: f[3],
                dst_blend: f[4],
                has_depth_target: f[5] != 0,
                z_enable: f[6] != 0,
                z_func: f[7],
                z_write: f[8] != 0,
                flat: f[9] != 0,
                target_has_alpha: f[10] != 0,
            });
            assert_eq!(key, f[11], "{line}");
            count += 1;
        }
        assert!(count > 50);
    }

    #[test]
    fn cull_applies_to_triangles_only() {
        let mut s = PipelineState { topology: 2, cull_mode: GXCULL_CCW, ..Default::default() };
        assert_eq!((pipeline_key(&s) >> 2) & 3, 2);
        s.topology = 0;
        assert_eq!((pipeline_key(&s) >> 2) & 3, 0);
    }

    #[test]
    fn pipeline_desc_decodes_blend_cases() {
        let key = |src, dst, alpha| {
            pipeline_key(&PipelineState {
                topology: 2,
                blend_enabled: true,
                src_blend: src,
                dst_blend: dst,
                target_has_alpha: alpha,
                ..Default::default()
            })
        };
        let b = pipeline_desc(key(GXBLEND_SRCALPHA, GXBLEND_INVSRCALPHA, true)).blend.unwrap();
        assert_eq!(b.color.src_factor, wgpu::BlendFactor::SrcAlpha);
        assert_eq!(b.alpha.dst_factor, wgpu::BlendFactor::OneMinusSrcAlpha);
        let b = pipeline_desc(key(GXBLEND_BOTHSRCALPHA, GXBLEND_ONE, false)).blend.unwrap();
        assert_eq!(
            (b.color.src_factor, b.color.dst_factor),
            (wgpu::BlendFactor::SrcAlpha, wgpu::BlendFactor::OneMinusSrcAlpha)
        );
        let b = pipeline_desc(key(GXBLEND_BOTHINVSRCALPHA, GXBLEND_ONE, false)).blend.unwrap();
        assert_eq!(
            (b.color.src_factor, b.color.dst_factor),
            (wgpu::BlendFactor::OneMinusSrcAlpha, wgpu::BlendFactor::SrcAlpha)
        );
        let b = pipeline_desc(key(GXBLEND_SRCALPHASAT, GXBLEND_ZERO, false)).blend.unwrap();
        assert_eq!(b.color.src_factor, wgpu::BlendFactor::SrcAlphaSaturated);
        assert_eq!(b.alpha.src_factor, wgpu::BlendFactor::One);
        let b = pipeline_desc(key(GXBLEND_DESTALPHA, GXBLEND_INVDESTALPHA, false)).blend.unwrap();
        assert_eq!((b.color.src_factor, b.color.dst_factor), (wgpu::BlendFactor::One, wgpu::BlendFactor::Zero));
        let b = pipeline_desc(key(GXBLEND_DESTALPHA, GXBLEND_INVDESTALPHA, true)).blend.unwrap();
        assert_eq!(
            (b.color.src_factor, b.color.dst_factor),
            (wgpu::BlendFactor::DstAlpha, wgpu::BlendFactor::OneMinusDstAlpha)
        );
        assert!(pipeline_desc(pipeline_key(&PipelineState { topology: 2, ..Default::default() })).blend.is_none());
    }

    #[test]
    fn pipeline_desc_decodes_depth_and_topology() {
        let s = PipelineState {
            topology: 1,
            has_depth_target: true,
            z_enable: true,
            z_func: GXCMP_GREATER,
            z_write: true,
            flat: true,
            ..Default::default()
        };
        let d = pipeline_desc(pipeline_key(&s));
        assert_eq!(d.topology, wgpu::PrimitiveTopology::LineList);
        assert!(d.depth && d.z_write && d.flat);
        assert_eq!(d.z_compare, wgpu::CompareFunction::Greater);
        // z disabled: a depth target compares always and writes nothing
        let s = PipelineState { has_depth_target: true, ..Default::default() };
        let d = pipeline_desc(pipeline_key(&s));
        assert!(d.depth && !d.z_write);
        assert_eq!(d.z_compare, wgpu::CompareFunction::Always);
        assert_eq!(
            pipeline_desc(pipeline_key(&PipelineState { topology: 0, ..Default::default() })).topology,
            wgpu::PrimitiveTopology::PointList
        );
    }

    #[test]
    fn compare_functions_map_one_to_one() {
        assert_eq!(compare_function(GXCMP_NEVER), wgpu::CompareFunction::Never);
        assert_eq!(compare_function(GXCMP_LESSEQUAL), wgpu::CompareFunction::LessEqual);
        assert_eq!(compare_function(GXCMP_ALWAYS), wgpu::CompareFunction::Always);
        assert_eq!(compare_function(99), wgpu::CompareFunction::Always);
    }

    #[test]
    fn blit_blend_is_premultiplied_over() {
        assert!(blit_blend(BlitMode::Copy).is_none());
        let b = blit_blend(BlitMode::Premultiplied).unwrap();
        assert_eq!(b.color.src_factor, wgpu::BlendFactor::One);
        assert_eq!(b.alpha.dst_factor, wgpu::BlendFactor::OneMinusSrcAlpha);
    }
}
