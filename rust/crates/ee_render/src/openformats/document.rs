//! A glTF 2.0 document as the importer reads it (import.rs): the `gltf` crate parses the file (`gltf::Glb`, the GLB
//! container; `gltf::json`, the document as typed serde structures), and this module turns that into the plain
//! records below, with what the crate does not do for a loader that must not panic on an edited file:
//! - every reference range-checked and the nodes' parents set (`fixup`; the crate's own validation is not run: it
//!   also demands what the game never needed, e.g. POSITION's min/max);
//! - the buffers (`load_buffers`: the GLB chunk, base64 data URIs; external files come resolved from import.rs);
//! - `validate`: every accessor inside its view and buffer, the index bounds, the node tree, the animations;
//! - the accessor readers (`floats`, `index_values`): any component type, normalized, strided, sparse, with every
//!   read bounds-checked.
//!
//! These last three follow cgltf 1.15 (`cgltf_load_buffers`, `cgltf_validate`, `cgltf_accessor_unpack_floats`), the
//! reader the game had before; the document parse was a port of cgltf's own (over jsmn) until the dependencies lane
//! (docs/progress/deps.md has what a malformed file now meets differently).
//!
//! References are kept as "index + 1", 0 for none.
#![allow(clippy::needless_range_loop)]

use std::borrow::Cow;

use gj::validation::Checked;
use gltf::json as gj;

type R<T> = Result<T, ()>;

/// A reference: 0 none, else index + 1.
pub(crate) type Ref = u64;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub(crate) enum ComponentType {
    #[default]
    Invalid,
    I8,
    U8,
    I16,
    U16,
    U32,
    F32,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub(crate) enum ValueType {
    #[default]
    Invalid,
    Scalar,
    Vec2,
    Vec3,
    Vec4,
    Mat2,
    Mat3,
    Mat4,
}

fn component_size(c: ComponentType) -> u64 {
    match c {
        ComponentType::I8 | ComponentType::U8 => 1,
        ComponentType::I16 | ComponentType::U16 => 2,
        ComponentType::U32 | ComponentType::F32 => 4,
        ComponentType::Invalid => 0,
    }
}

fn num_components(t: ValueType) -> u64 {
    match t {
        ValueType::Vec2 => 2,
        ValueType::Vec3 => 3,
        ValueType::Vec4 => 4,
        ValueType::Mat2 => 4,
        ValueType::Mat3 => 9,
        ValueType::Mat4 => 16,
        ValueType::Invalid | ValueType::Scalar => 1,
    }
}

fn calc_size(t: ValueType, c: ComponentType) -> u64 {
    let s = component_size(c);
    if t == ValueType::Mat2 && s == 1 {
        8 * s
    } else if t == ValueType::Mat3 && (s == 1 || s == 2) {
        12 * s
    } else {
        s * num_components(t)
    }
}

#[derive(Clone, Copy, Default)]
pub(crate) struct Sparse {
    pub count: u64,
    pub indices_view: Ref,
    pub indices_offset: u64,
    pub indices_component: ComponentType,
    pub values_view: Ref,
    pub values_offset: u64,
}

#[derive(Default)]
pub(crate) struct Accessor {
    pub component: ComponentType,
    pub ty: ValueType,
    pub normalized: bool,
    pub offset: u64,
    pub count: u64,
    pub stride: u64,
    pub view: Ref,
    pub is_sparse: bool,
    pub sparse: Sparse,
}

#[derive(Default)]
pub(crate) struct BufferView {
    pub buffer: Ref,
    pub offset: u64,
    pub size: u64,
    pub stride: u64,
}

#[derive(Default)]
pub(crate) struct Buffer<'a> {
    pub size: u64,
    pub uri: Option<Vec<u8>>,
    pub data: Option<Cow<'a, [u8]>>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum AttributeKind {
    Position,
    Normal,
    Texcoord,
    Other,
}

pub(crate) struct Attribute {
    pub kind: AttributeKind,
    pub index: u32,
    pub accessor: Ref,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum PrimitiveKind {
    Triangles,
    Invalid,
    Other,
}

pub(crate) struct Primitive {
    pub kind: PrimitiveKind,
    pub indices: Ref,
    pub material: Ref,
    pub attributes: Vec<Attribute>,
    pub targets: Vec<Vec<Attribute>>,
}

#[derive(Default)]
pub(crate) struct Mesh {
    pub primitives: Vec<Primitive>,
    pub weights: Option<u64>,
}

#[derive(Default)]
pub(crate) struct Material {
    pub name: Option<Vec<u8>>,
    pub base_color_texture: Ref,
    /// the `extras` member's JSON text
    pub extras: Option<String>,
    /// the other textures it names (range-checked only)
    pub texture_refs: Vec<Ref>,
}

#[derive(Default)]
pub(crate) struct Texture {
    pub image: Ref,
    pub sampler: Ref,
}

#[derive(Default)]
pub(crate) struct Image {
    pub name: Option<Vec<u8>>,
    pub uri: Option<Vec<u8>>,
    pub view: Ref,
}

pub(crate) struct Node {
    pub name: Option<Vec<u8>>,
    pub children: Vec<Ref>,
    pub mesh: Ref,
    pub skin: Ref,
    pub camera: Ref,
    pub weights: Option<u64>,
    pub has_matrix: bool,
    pub matrix: [f32; 16],
    pub has_translation: bool,
    pub translation: [f32; 3],
    pub has_rotation: bool,
    pub rotation: [f32; 4],
    pub has_scale: bool,
    pub scale: [f32; 3],
    /// the node's `EE_model` extension
    pub ee_model: Option<serde_json::Value>,
    /// set by `fixup`
    pub parent: Option<usize>,
}

#[derive(Default)]
pub(crate) struct Scene {
    pub nodes: Vec<Ref>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub(crate) enum Interpolation {
    #[default]
    Linear,
    Step,
    CubicSpline,
}

#[derive(Default)]
pub(crate) struct AnimSampler {
    pub input: Ref,
    pub output: Ref,
    pub interpolation: Interpolation,
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
pub(crate) enum TargetPath {
    #[default]
    Invalid,
    Translation,
    Rotation,
    Scale,
    Weights,
}

#[derive(Default)]
pub(crate) struct AnimChannel {
    pub sampler: Ref,
    pub target_node: Ref,
    pub target_path: TargetPath,
}

#[derive(Default)]
pub(crate) struct Animation {
    pub samplers: Vec<AnimSampler>,
    pub channels: Vec<AnimChannel>,
}

#[derive(Default)]
pub(crate) struct Skin {
    pub joints: Vec<Ref>,
    pub skeleton: Ref,
    pub inverse_bind_matrices: Ref,
}

/// A parsed glTF.
#[derive(Default)]
pub(crate) struct Data<'a> {
    pub bin: Option<&'a [u8]>,
    pub accessors: Vec<Accessor>,
    pub views: Vec<BufferView>,
    pub buffers: Vec<Buffer<'a>>,
    pub meshes: Vec<Mesh>,
    pub materials: Vec<Material>,
    pub textures: Vec<Texture>,
    pub images: Vec<Image>,
    pub nodes: Vec<Node>,
    pub scenes: Vec<Scene>,
    pub scene: Ref,
    pub animations: Vec<Animation>,
    pub skins: Vec<Skin>,
    pub cameras_count: u64,
    pub samplers_count: u64,
}

// ------------------------------------------------------------------------------- the document -> the records --

fn index<T>(i: Option<gj::Index<T>>) -> Ref {
    i.map_or(0, |i| i.value() as u64 + 1)
}

fn bytes(s: Option<String>) -> Option<Vec<u8>> {
    s.map(String::into_bytes)
}

fn component(c: gj::accessor::ComponentType) -> ComponentType {
    use gj::accessor::ComponentType as C;
    match c {
        C::I8 => ComponentType::I8,
        C::U8 => ComponentType::U8,
        C::I16 => ComponentType::I16,
        C::U16 => ComponentType::U16,
        C::U32 => ComponentType::U32,
        C::F32 => ComponentType::F32,
    }
}

fn attribute(semantic: &Checked<gj::mesh::Semantic>, accessor: Ref) -> Attribute {
    use gj::mesh::Semantic as S;
    let (kind, index) = match semantic {
        Checked::Valid(S::Positions) => (AttributeKind::Position, 0),
        Checked::Valid(S::Normals) => (AttributeKind::Normal, 0),
        Checked::Valid(S::TexCoords(set)) => (AttributeKind::Texcoord, *set),
        _ => (AttributeKind::Other, 0),
    };
    Attribute { kind, index, accessor }
}

fn accessor(a: gj::Accessor) -> Accessor {
    use gj::accessor::Type as T;
    let sparse = a.sparse.as_ref().map(|s| Sparse {
        count: s.count.0,
        indices_view: index(Some(s.indices.buffer_view)),
        indices_offset: s.indices.byte_offset.0,
        indices_component: match s.indices.component_type {
            Checked::Valid(c) => component(c.0),
            Checked::Invalid => ComponentType::Invalid,
        },
        values_view: index(Some(s.values.buffer_view)),
        values_offset: s.values.byte_offset.0,
    });
    Accessor {
        component: match a.component_type {
            Checked::Valid(c) => component(c.0),
            Checked::Invalid => ComponentType::Invalid,
        },
        ty: match a.type_ {
            Checked::Valid(T::Scalar) => ValueType::Scalar,
            Checked::Valid(T::Vec2) => ValueType::Vec2,
            Checked::Valid(T::Vec3) => ValueType::Vec3,
            Checked::Valid(T::Vec4) => ValueType::Vec4,
            Checked::Valid(T::Mat2) => ValueType::Mat2,
            Checked::Valid(T::Mat3) => ValueType::Mat3,
            Checked::Valid(T::Mat4) => ValueType::Mat4,
            Checked::Invalid => ValueType::Invalid,
        },
        normalized: a.normalized,
        offset: a.byte_offset.map_or(0, |o| o.0),
        count: a.count.0,
        stride: 0,
        view: index(a.buffer_view),
        is_sparse: sparse.is_some(),
        sparse: sparse.unwrap_or_default(),
    }
}

fn primitive(p: gj::mesh::Primitive) -> Primitive {
    use gj::mesh::Mode;
    let target = |t: &gj::mesh::MorphTarget| {
        let mut out = Vec::new();
        if let Some(a) = t.positions {
            out.push(Attribute { kind: AttributeKind::Position, index: 0, accessor: index(Some(a)) });
        }
        if let Some(a) = t.normals {
            out.push(Attribute { kind: AttributeKind::Normal, index: 0, accessor: index(Some(a)) });
        }
        if let Some(a) = t.tangents {
            out.push(Attribute { kind: AttributeKind::Other, index: 0, accessor: index(Some(a)) });
        }
        out
    };
    Primitive {
        kind: match p.mode {
            Checked::Valid(Mode::Triangles) => PrimitiveKind::Triangles,
            Checked::Valid(_) => PrimitiveKind::Other,
            Checked::Invalid => PrimitiveKind::Invalid,
        },
        indices: index(p.indices),
        material: index(p.material),
        attributes: p.attributes.iter().map(|(semantic, a)| attribute(semantic, index(Some(*a)))).collect(),
        targets: p.targets.iter().flatten().map(target).collect(),
    }
}

fn material(m: gj::Material) -> Material {
    let pbr = m.pbr_metallic_roughness;
    Material {
        name: bytes(m.name),
        base_color_texture: index(pbr.base_color_texture.map(|t| t.index)),
        extras: m.extras.map(|raw| raw.get().to_string()),
        texture_refs: [
            pbr.metallic_roughness_texture.map(|t| t.index),
            m.normal_texture.map(|t| t.index),
            m.occlusion_texture.map(|t| t.index),
            m.emissive_texture.map(|t| t.index),
        ]
        .into_iter()
        .map(index)
        .collect(),
    }
}

fn node(n: gj::Node) -> Node {
    Node {
        name: bytes(n.name),
        children: n.children.iter().flatten().map(|c| index(Some(*c))).collect(),
        mesh: index(n.mesh),
        skin: index(n.skin),
        camera: index(n.camera),
        weights: n.weights.map(|w| w.len() as u64),
        has_matrix: n.matrix.is_some(),
        matrix: n.matrix.unwrap_or([1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]),
        has_translation: n.translation.is_some(),
        translation: n.translation.unwrap_or([0.0; 3]),
        has_rotation: n.rotation.is_some(),
        rotation: n.rotation.map_or([0.0, 0.0, 0.0, 1.0], |q| q.0),
        has_scale: n.scale.is_some(),
        scale: n.scale.unwrap_or([1.0; 3]),
        ee_model: n.extensions.and_then(|mut e| e.others.remove("EE_model")),
        parent: None,
    }
}

fn animation(a: gj::Animation) -> Animation {
    use gj::animation::{Interpolation as I, Property as P};
    Animation {
        samplers: a
            .samplers
            .iter()
            .map(|s| AnimSampler {
                input: index(Some(s.input)),
                output: index(Some(s.output)),
                interpolation: match s.interpolation {
                    Checked::Valid(I::Step) => Interpolation::Step,
                    Checked::Valid(I::CubicSpline) => Interpolation::CubicSpline,
                    _ => Interpolation::Linear,
                },
            })
            .collect(),
        channels: a
            .channels
            .iter()
            .map(|c| AnimChannel {
                sampler: index(Some(c.sampler)),
                target_node: index(Some(c.target.node)),
                target_path: match c.target.path {
                    Checked::Valid(P::Translation) => TargetPath::Translation,
                    Checked::Valid(P::Rotation) => TargetPath::Rotation,
                    Checked::Valid(P::Scale) => TargetPath::Scale,
                    Checked::Valid(P::MorphTargetWeights) => TargetPath::Weights,
                    Checked::Invalid => TargetPath::Invalid,
                },
            })
            .collect(),
    }
}

fn records(root: gj::Root, bin: Option<&[u8]>) -> Data<'_> {
    Data {
        bin,
        accessors: root.accessors.into_iter().map(accessor).collect(),
        views: root
            .buffer_views
            .iter()
            .map(|v| BufferView {
                buffer: index(Some(v.buffer)),
                offset: v.byte_offset.map_or(0, |o| o.0),
                size: v.byte_length.0,
                stride: v.byte_stride.map_or(0, |s| s.0 as u64),
            })
            .collect(),
        buffers: root
            .buffers
            .into_iter()
            .map(|b| Buffer { size: b.byte_length.0, uri: bytes(b.uri), data: None })
            .collect(),
        meshes: root
            .meshes
            .into_iter()
            .map(|m| Mesh {
                weights: m.weights.as_ref().map(|w| w.len() as u64),
                primitives: m.primitives.into_iter().map(primitive).collect(),
            })
            .collect(),
        materials: root.materials.into_iter().map(material).collect(),
        // (a texture without a source reads as index u32::MAX: none)
        textures: root
            .textures
            .iter()
            .map(|t| Texture {
                image: index(Some(t.source).filter(|s| s.value() != u32::MAX as usize)),
                sampler: index(t.sampler),
            })
            .collect(),
        images: root
            .images
            .into_iter()
            .map(|i| Image { name: bytes(i.name), uri: bytes(i.uri), view: index(i.buffer_view) })
            .collect(),
        nodes: root.nodes.into_iter().map(node).collect(),
        scenes: root
            .scenes
            .iter()
            .map(|s| Scene { nodes: s.nodes.iter().map(|n| index(Some(*n))).collect() })
            .collect(),
        scene: index(root.scene),
        animations: root.animations.into_iter().map(animation).collect(),
        skins: root
            .skins
            .iter()
            .map(|s| Skin {
                joints: s.joints.iter().map(|j| index(Some(*j))).collect(),
                skeleton: index(s.skeleton),
                inverse_bind_matrices: index(s.inverse_bind_matrices),
            })
            .collect(),
        cameras_count: root.cameras.len() as u64,
        samplers_count: root.samplers.len() as u64,
    }
}

// --------------------------------------------------------------------------------------------- the references --

fn fix(r: Ref, size: usize) -> R<()> {
    if r != 0 && r > size as u64 {
        Err(())
    } else {
        Ok(())
    }
}

fn fix_req(r: Ref, size: usize) -> R<()> {
    if r == 0 || r > size as u64 {
        Err(())
    } else {
        Ok(())
    }
}

fn fixup(d: &mut Data) -> R<()> {
    let (nacc, nmat, nview, ntex, nimg, nbuf, nnode, nscene, nskin, nmesh) = (
        d.accessors.len(),
        d.materials.len(),
        d.views.len(),
        d.textures.len(),
        d.images.len(),
        d.buffers.len(),
        d.nodes.len(),
        d.scenes.len(),
        d.skins.len(),
        d.meshes.len(),
    );
    for m in &d.meshes {
        for p in &m.primitives {
            fix(p.indices, nacc)?;
            fix(p.material, nmat)?;
            for a in &p.attributes {
                fix_req(a.accessor, nacc)?;
            }
            for t in &p.targets {
                for a in t {
                    fix_req(a.accessor, nacc)?;
                }
            }
        }
    }
    for i in 0..nacc {
        let a = &d.accessors[i];
        fix(a.view, nview)?;
        if a.is_sparse {
            fix_req(a.sparse.indices_view, nview)?;
            fix_req(a.sparse.values_view, nview)?;
        }
        if a.view != 0 {
            let stride = d.views[a.view as usize - 1].stride;
            d.accessors[i].stride = stride;
        }
        let a = &mut d.accessors[i];
        if a.stride == 0 {
            a.stride = calc_size(a.ty, a.component);
        }
    }
    for t in &d.textures {
        fix(t.image, nimg)?;
        fix(t.sampler, d.samplers_count as usize)?;
    }
    for im in &d.images {
        fix(im.view, nview)?;
    }
    for m in &d.materials {
        fix(m.base_color_texture, ntex)?;
        for r in &m.texture_refs {
            fix(*r, ntex)?;
        }
    }
    for v in &d.views {
        fix_req(v.buffer, nbuf)?;
    }
    for s in &d.skins {
        for j in &s.joints {
            fix_req(*j, nnode)?;
        }
        fix(s.skeleton, nnode)?;
        fix(s.inverse_bind_matrices, nacc)?;
    }
    for i in 0..nnode {
        for j in 0..d.nodes[i].children.len() {
            let child = d.nodes[i].children[j];
            fix_req(child, nnode)?;
            let c = child as usize - 1;
            if d.nodes[c].parent.is_some() {
                return Err(());
            }
            d.nodes[c].parent = Some(i);
        }
        let n = &d.nodes[i];
        fix(n.mesh, nmesh)?;
        fix(n.skin, nskin)?;
        fix(n.camera, d.cameras_count as usize)?;
    }
    for s in &d.scenes {
        for n in &s.nodes {
            fix_req(*n, nnode)?;
            if d.nodes[*n as usize - 1].parent.is_some() {
                return Err(());
            }
        }
    }
    fix(d.scene, nscene)?;
    for an in &d.animations {
        for s in &an.samplers {
            fix_req(s.input, nacc)?;
            fix_req(s.output, nacc)?;
        }
        for ch in &an.channels {
            fix_req(ch.sampler, an.samplers.len())?;
            fix(ch.target_node, nnode)?;
        }
    }
    Ok(())
}

/// A .gltf's JSON or a .glb (by its magic) -> the records, every reference in range. A glTF 1 file is refused.
pub(crate) fn parse(data: &[u8]) -> R<Data<'_>> {
    let (json, bin) = if data.starts_with(b"glTF") {
        // (the crate subtracts the header's size from the length it reads)
        if data.len() < 12 || u32::from_le_bytes([data[8], data[9], data[10], data[11]]) < 12 {
            return Err(());
        }
        match gltf::Glb::from_slice(data).map_err(|_| ())? {
            gltf::Glb { json: Cow::Borrowed(json), bin, .. } => (
                json,
                match bin {
                    Some(Cow::Borrowed(bin)) => Some(bin),
                    _ => None,
                },
            ),
            _ => return Err(()),
        }
    } else {
        (data, None)
    };
    let root = gj::Root::from_slice(json).map_err(|_| ())?;
    if root.asset.version.trim().parse::<f32>().is_ok_and(|v| v < 2.0) {
        return Err(());
    }
    let mut d = records(root, bin);
    fixup(&mut d)?;
    Ok(d)
}

// ------------------------------------------------------------------------------------------------ cgltf_load_buffers --

/// `cgltf_load_buffers` with the external files already resolved (no gltf_path).
pub(crate) fn load_buffers(d: &mut Data) -> R<()> {
    if !d.buffers.is_empty() && d.buffers[0].data.is_none() && d.buffers[0].uri.is_none() {
        if let Some(bin) = d.bin {
            if (bin.len() as u64) < d.buffers[0].size {
                return Err(());
            }
            d.buffers[0].data = Some(Cow::Borrowed(bin));
        }
    }
    for b in &mut d.buffers {
        if b.data.is_some() {
            continue;
        }
        let Some(uri) = b.uri.as_deref() else { continue };
        if uri.starts_with(b"data:") {
            let comma = uri.iter().position(|&c| c == b',').ok_or(())?;
            if comma >= 7 && &uri[comma - 7..comma] == b";base64" {
                b.data = Some(Cow::Owned(decode_base64(&uri[comma + 1..], b.size)?));
            } else {
                return Err(());
            }
        } else {
            return Err(()); // an external file that came up empty; no base path
        }
    }
    Ok(())
}

fn decode_base64(mut s: &[u8], size: u64) -> R<Vec<u8>> {
    let mut out = Vec::new();
    out.try_reserve_exact(size as usize).map_err(|_| ())?;
    let mut buffer = 0u32;
    let mut bits = 0u32;
    for _ in 0..size {
        while bits < 8 {
            let ch = *s.first().ok_or(())?;
            s = &s[1..];
            let index = match ch {
                b'A'..=b'Z' => (ch - b'A') as u32,
                b'a'..=b'z' => (ch - b'a') as u32 + 26,
                b'0'..=b'9' => (ch - b'0') as u32 + 52,
                b'+' => 62,
                b'/' => 63,
                _ => return Err(()),
            };
            buffer = (buffer << 6) | index;
            bits += 6;
        }
        out.push((buffer >> (bits - 8)) as u8);
        bits -= 8;
    }
    Ok(out)
}

fn hex_value(c: u8) -> Option<u32> {
    match c {
        b'0'..=b'9' => Some((c - b'0') as u32),
        b'a'..=b'f' => Some((c - b'a') as u32 + 10),
        b'A'..=b'F' => Some((c - b'A') as u32 + 10),
        _ => None,
    }
}

/// `cgltf_decode_uri` on a C string: percent escapes decoded; the result ends at the first NUL of either side.
pub(crate) fn decode_uri(uri: &[u8]) -> Vec<u8> {
    let end = uri.iter().position(|&c| c == 0).unwrap_or(uri.len());
    let s = &uri[..end];
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while i < s.len() {
        if s[i] == b'%' && i + 2 < s.len() {
            if let (Some(a), Some(b)) = (hex_value(s[i + 1]), hex_value(s[i + 2])) {
                out.push((a * 16 + b) as u8);
                i += 3;
                continue;
            }
        }
        out.push(s[i]);
        i += 1;
    }
    let n = out.iter().position(|&c| c == 0).unwrap_or(out.len());
    out.truncate(n);
    out
}

// ---------------------------------------------------------------------------------------------- cgltf_validate --

fn index_bound(d: &Data, view: &BufferView, offset: u64, component: ComponentType, count: u64) -> R<u64> {
    let buffer = &d.buffers[view.buffer as usize - 1];
    let data = buffer.data.as_deref().ok_or(())?;
    let start = view.offset.checked_add(offset).ok_or(())?;
    let size = component_size(component);
    let mut bound = 0;
    for i in 0..count {
        let p = start.checked_add(i.checked_mul(size).ok_or(())?).ok_or(())?;
        bound = bound.max(read_index_at(data, p, component).ok_or(())?);
    }
    Ok(bound)
}

fn read_index_at(data: &[u8], p: u64, component: ComponentType) -> Option<u64> {
    let p = usize::try_from(p).ok()?;
    match component {
        ComponentType::U8 => data.get(p).map(|&v| v as u64),
        ComponentType::U16 => data.get(p..p + 2).map(|b| u16::from_le_bytes([b[0], b[1]]) as u64),
        ComponentType::U32 => data.get(p..p + 4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]) as u64),
        _ => Some(0),
    }
}

/// `cgltf_validate`.
pub(crate) fn validate(d: &Data) -> R<()> {
    for a in &d.accessors {
        if a.component == ComponentType::Invalid || a.ty == ValueType::Invalid {
            return Err(());
        }
        let element_size = calc_size(a.ty, a.component);
        if a.view != 0 {
            // the C arithmetic wraps for count 0
            let req = a.offset.wrapping_add(a.stride.wrapping_mul(a.count.wrapping_sub(1))).wrapping_add(element_size);
            if d.views[a.view as usize - 1].size < req {
                return Err(());
            }
        }
        if a.is_sparse {
            let s = &a.sparse;
            let ind_size = component_size(s.indices_component);
            let ind_req = s.indices_offset.wrapping_add(ind_size.wrapping_mul(s.count));
            let val_req = s.values_offset.wrapping_add(element_size.wrapping_mul(s.count));
            let iv = &d.views[s.indices_view as usize - 1];
            let vv = &d.views[s.values_view as usize - 1];
            if iv.size < ind_req || vv.size < val_req {
                return Err(());
            }
            if !matches!(s.indices_component, ComponentType::U8 | ComponentType::U16 | ComponentType::U32) {
                return Err(());
            }
            if d.buffers[iv.buffer as usize - 1].data.is_some() {
                let bound = index_bound(d, iv, s.indices_offset, s.indices_component, s.count)?;
                if bound >= a.count {
                    return Err(());
                }
            }
        }
    }
    for v in &d.views {
        let req = v.offset.wrapping_add(v.size);
        if d.buffers[v.buffer as usize - 1].size < req {
            return Err(());
        }
    }
    for m in &d.meshes {
        if let Some(w) = m.weights {
            if !m.primitives.is_empty() && m.primitives[0].targets.len() as u64 != w {
                return Err(());
            }
        }
        for p in &m.primitives {
            if p.kind == PrimitiveKind::Invalid {
                return Err(());
            }
            if p.targets.len() != m.primitives[0].targets.len() {
                return Err(());
            }
            if p.attributes.is_empty() {
                return Err(());
            }
            let first = &d.accessors[p.attributes[0].accessor as usize - 1];
            if first.count == 0 {
                return Err(());
            }
            for a in &p.attributes {
                if d.accessors[a.accessor as usize - 1].count != first.count {
                    return Err(());
                }
            }
            for t in &p.targets {
                for a in t {
                    if d.accessors[a.accessor as usize - 1].count != first.count {
                        return Err(());
                    }
                }
            }
            if p.indices != 0 {
                let ind = &d.accessors[p.indices as usize - 1];
                if !matches!(ind.component, ComponentType::U8 | ComponentType::U16 | ComponentType::U32) {
                    return Err(());
                }
                if ind.ty != ValueType::Scalar {
                    return Err(());
                }
                if ind.stride != component_size(ind.component) {
                    return Err(());
                }
                if ind.view != 0 {
                    let v = &d.views[ind.view as usize - 1];
                    if d.buffers[v.buffer as usize - 1].data.is_some() {
                        let bound = index_bound(d, v, ind.offset, ind.component, ind.count)?;
                        if bound >= first.count {
                            return Err(());
                        }
                    }
                }
            }
        }
    }
    for n in &d.nodes {
        if let Some(w) = n.weights {
            if n.mesh != 0 {
                let m = &d.meshes[n.mesh as usize - 1];
                if !m.primitives.is_empty() && m.primitives[0].targets.len() as u64 != w {
                    return Err(());
                }
            }
        }
    }
    // no cycle through parents: a tortoise and a hare from every node
    for i in 0..d.nodes.len() {
        let mut p1 = d.nodes[i].parent;
        let mut p2 = p1.and_then(|p| d.nodes[p].parent);
        while let (Some(a), Some(b)) = (p1, p2) {
            if a == b {
                return Err(());
            }
            p1 = d.nodes[a].parent;
            p2 = d.nodes[b].parent.and_then(|q| d.nodes[q].parent);
        }
    }
    for s in &d.scenes {
        for n in &s.nodes {
            if d.nodes[*n as usize - 1].parent.is_some() {
                return Err(());
            }
        }
    }
    for an in &d.animations {
        for ch in &an.channels {
            if ch.target_node == 0 {
                continue;
            }
            let mut components = 1;
            if ch.target_path == TargetPath::Weights {
                let node = &d.nodes[ch.target_node as usize - 1];
                if node.mesh == 0 {
                    return Err(());
                }
                let mesh = &d.meshes[node.mesh as usize - 1];
                if mesh.primitives.is_empty() {
                    return Err(());
                }
                components = mesh.primitives[0].targets.len() as u64;
            }
            let sampler = &an.samplers[ch.sampler as usize - 1];
            let values = if sampler.interpolation == Interpolation::CubicSpline { 3 } else { 1 };
            let input = &d.accessors[sampler.input as usize - 1];
            let output = &d.accessors[sampler.output as usize - 1];
            if input.count.wrapping_mul(components).wrapping_mul(values) != output.count {
                return Err(());
            }
        }
    }
    Ok(())
}

// --------------------------------------------------------------------------------------------------- accessors --

fn read_float_component(p: &[u8], at: u64, c: ComponentType, normalized: bool) -> R<f32> {
    let p = usize::try_from(at).ok().and_then(|a| p.get(a..)).ok_or(())?;
    let get = |n: usize| -> R<&[u8]> { p.get(..n).ok_or(()) };
    if c == ComponentType::F32 {
        let b = get(4)?;
        return Ok(f32::from_le_bytes([b[0], b[1], b[2], b[3]]));
    }
    if normalized {
        return Ok(match c {
            ComponentType::I16 => {
                let b = get(2)?;
                i16::from_le_bytes([b[0], b[1]]) as f32 / 32767.0
            }
            ComponentType::U16 => {
                let b = get(2)?;
                u16::from_le_bytes([b[0], b[1]]) as f32 / 65535.0
            }
            ComponentType::I8 => get(1)?[0] as i8 as f32 / 127.0,
            ComponentType::U8 => get(1)?[0] as f32 / 255.0,
            _ => 0.0,
        });
    }
    Ok(match c {
        ComponentType::I16 => {
            let b = get(2)?;
            i16::from_le_bytes([b[0], b[1]]) as f32
        }
        ComponentType::U16 => {
            let b = get(2)?;
            u16::from_le_bytes([b[0], b[1]]) as f32
        }
        ComponentType::U32 => {
            let b = get(4)?;
            u32::from_le_bytes([b[0], b[1], b[2], b[3]]) as f32
        }
        ComponentType::I8 => get(1)?[0] as i8 as f32,
        ComponentType::U8 => get(1)?[0] as f32,
        _ => 0.0,
    })
}

// cgltf_element_read_float into out[..n]
fn read_element(data: &[u8], at: u64, a: &Accessor, out: &mut [f32]) -> R<()> {
    let n = num_components(a.ty) as usize;
    let cs = component_size(a.component);
    let rd = |o: u64| read_float_component(data, at.wrapping_add(o), a.component, a.normalized);
    if a.ty == ValueType::Mat2 && cs == 1 {
        out[0] = rd(0)?;
        out[1] = rd(1)?;
        out[2] = rd(4)?;
        out[3] = rd(5)?;
        return Ok(());
    }
    if a.ty == ValueType::Mat3 && cs == 1 {
        for (i, off) in [0, 1, 2, 4, 5, 6, 8, 9, 10].iter().enumerate() {
            out[i] = rd(*off)?;
        }
        return Ok(());
    }
    if a.ty == ValueType::Mat3 && cs == 2 {
        for (i, off) in [0, 2, 4, 8, 10, 12, 16, 18, 20].iter().enumerate() {
            out[i] = rd(*off)?;
        }
        return Ok(());
    }
    for i in 0..n {
        out[i] = rd(cs * i as u64)?;
    }
    Ok(())
}

impl<'a> Data<'a> {
    fn view_data(&self, view: &BufferView) -> Option<&[u8]> {
        self.buffers[view.buffer as usize - 1].data.as_deref()
    }

    /// `cgltf_accessor_unpack_floats` over the whole accessor: `None` when it cannot be read (the C function's 0).
    pub(crate) fn floats(&self, a: &Accessor) -> Option<Vec<f32>> {
        let per_element = num_components(a.ty);
        let count = a.count.checked_mul(per_element)?;
        let n = usize::try_from(count).ok()?;
        if n == 0 {
            return Some(Vec::new());
        }
        let mut out = Vec::new();
        out.try_reserve_exact(n).ok()?;
        out.resize(n, 0.0f32);
        if a.view != 0 {
            let view = &self.views[a.view as usize - 1];
            let data = self.view_data(view)?;
            let base = view.offset.checked_add(a.offset)?;
            let per = per_element as usize;
            if a.component == ComponentType::F32 && a.stride == per_element * 4 {
                let start = usize::try_from(base).ok()?;
                let end = start.checked_add(n * 4)?;
                let bytes = data.get(start..end)?;
                for (v, b) in out.iter_mut().zip(bytes.as_chunks::<4>().0.iter()) {
                    *v = f32::from_le_bytes([b[0], b[1], b[2], b[3]]);
                }
            } else {
                let mut element = base;
                for i in 0..(n / per) {
                    read_element(data, element, a, &mut out[i * per..i * per + per]).ok()?;
                    element = element.wrapping_add(a.stride);
                }
            }
        }
        if a.is_sparse {
            let s = &a.sparse;
            let iv = &self.views[s.indices_view as usize - 1];
            let vv = &self.views[s.values_view as usize - 1];
            let idata = self.view_data(iv)?;
            let vdata = self.view_data(vv)?;
            let mut index_at = iv.offset.wrapping_add(s.indices_offset);
            let mut reader = vv.offset.wrapping_add(s.values_offset);
            let istride = component_size(s.indices_component);
            for _ in 0..s.count {
                let writer = read_index_at(idata, index_at, s.indices_component)?;
                let start = writer.checked_mul(per_element)?;
                let start = usize::try_from(start).ok()?;
                let per = per_element as usize;
                if start.checked_add(per)? > out.len() {
                    return None;
                }
                read_element(vdata, reader, a, &mut out[start..start + per]).ok()?;
                index_at = index_at.wrapping_add(istride);
                reader = reader.wrapping_add(a.stride);
            }
        }
        Some(out)
    }

    /// `cgltf_accessor_read_index` over the whole accessor (a sparse or view-less accessor reads as zeros). A read
    /// past the buffer's end is reported as an error, not read.
    pub(crate) fn index_values(&self, a: &Accessor) -> Option<Vec<u64>> {
        let n = usize::try_from(a.count).ok()?;
        let mut out = Vec::new();
        out.try_reserve_exact(n).ok()?;
        if a.is_sparse || a.view == 0 {
            out.resize(n, 0);
            return Some(out);
        }
        let view = &self.views[a.view as usize - 1];
        let Some(data) = self.view_data(view) else {
            out.resize(n, 0);
            return Some(out);
        };
        let base = view.offset.wrapping_add(a.offset);
        for i in 0..a.count {
            let at = base.wrapping_add(a.stride.wrapping_mul(i));
            out.push(read_index_at(data, at, a.component)?);
        }
        Some(out)
    }

    /// `cgltf_accessor.sparse`'s indices as the frame fixes read them: (index component size, bytes base).
    pub(crate) fn sparse_index_bytes(&self, a: &Accessor) -> Option<(usize, &[u8])> {
        let s = &a.sparse;
        let v = &self.views[s.indices_view as usize - 1];
        let data = self.view_data(v)?;
        let start = usize::try_from(v.offset.checked_add(s.indices_offset)?).ok()?;
        let size = match s.indices_component {
            ComponentType::U8 => 1,
            ComponentType::U16 => 2,
            _ => 4,
        };
        Some((size, data.get(start..)?))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn doc(members: &str) -> Vec<u8> {
        format!(r#"{{"asset":{{"version":"2.0"}}{}{members}}}"#, if members.is_empty() { "" } else { "," }).into_bytes()
    }

    fn doc_with_bin() -> Vec<u8> {
        doc(concat!(
            r#""buffers":[{"byteLength":12}],"bufferViews":[{"buffer":0,"byteLength":12}],"#,
            r#""accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"SCALAR"}]"#
        ))
    }

    fn parse_str(s: &[u8]) -> Result<(), ()> {
        parse(s).map(|_| ())
    }

    #[test]
    fn minimal_document_parses_and_validates() {
        let doc = doc_with_bin();
        let mut d = parse(&doc).unwrap();
        assert_eq!((d.accessors.len(), d.views.len(), d.buffers.len()), (1, 1, 1));
        // with no data the sparse/bound checks are skipped, the size checks are not
        assert_eq!(validate(&d), Ok(()));
        d.accessors[0].count = 4;
        assert_eq!(validate(&d), Err(()));
    }

    #[test]
    fn duplicates_and_type_errors_refuse_the_file() {
        assert_eq!(parse_str(&doc(r#""accessors":[],"accessors":[]"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""meshes":[{"primitives":[],"name":"a","name":"b"}]"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""nodes":[{"translation":[1,2]}]"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""nodes":[{"mesh":"x"}]"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""meshes":[{"primitives":[{"attributes":[]}]}]"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""meshes":[{"primitives":[{"attributes":{"POSITION":{}}}]}]"#)), Err(()));
        assert_eq!(parse_str(br#"{"asset":{"version":"2.0"},}"#), Err(()));
        assert_eq!(parse_str(b"not JSON"), Err(()));
    }

    #[test]
    fn a_document_needs_its_asset_and_version_2() {
        assert_eq!(parse_str(br#"{"asset":{"version":"1.0"}}"#), Err(()));
        assert_eq!(parse_str(br#"{"asset":{"version":"2.0"}}"#), Ok(()));
        assert_eq!(parse_str(br#"{"nodes":[]}"#), Err(()));
    }

    #[test]
    fn references_are_range_checked_in_fixup() {
        assert_eq!(parse_str(&doc(r#""nodes":[{"mesh":0}]"#)), Err(()));
        let cycle = doc(r#""nodes":[{"children":[0]}]"#);
        assert_eq!(validate(&parse(&cycle).unwrap()), Err(())); // a parent cycle
        assert_eq!(parse_str(&doc(r#""nodes":[{"children":[1]},{"children":[2]},{}]"#)), Ok(()));
        assert_eq!(parse_str(&doc(r#""scene":0"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""scenes":[{"nodes":[0]}],"nodes":[{}],"scene":1"#)), Err(()));
        assert_eq!(parse_str(&doc(r#""nodes":[{"name":"a"}],"scenes":[{"nodes":[0]}],"scene":0"#)), Ok(()));
        assert_eq!(parse_str(&doc(r#""materials":[{"normalTexture":{"index":0}}]"#)), Err(()));
    }

    #[test]
    fn attributes_names_and_the_extension_are_read() {
        let text = doc(concat!(
            r#""meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_1":0,"_X":0,"WEIRD":0},"mode":1}]}],"#,
            r#""accessors":[{"componentType":5126,"count":3,"type":"VEC3"}],"#,
            r#""nodes":[{"name":"aé","extensions":{"EE_model":{"radii":[1.5]},"OTHER":{}}}],"#,
            r#""materials":[{"extras":{"ee_textureIndex":3}}]"#
        ));
        let d = parse(&text).unwrap();
        let p = &d.meshes[0].primitives[0];
        assert_eq!(p.kind, PrimitiveKind::Other);
        let kinds: Vec<_> = p.attributes.iter().map(|a| (a.kind, a.index)).collect();
        assert!(kinds.contains(&(AttributeKind::Position, 0)) && kinds.contains(&(AttributeKind::Texcoord, 1)));
        assert_eq!(kinds.iter().filter(|k| k.0 == AttributeKind::Other).count(), 2);
        assert_eq!(d.nodes[0].name.as_deref(), Some("a\u{e9}".as_bytes()));
        assert_eq!(d.nodes[0].ee_model, Some(serde_json::json!({"radii": [1.5]})));
        assert_eq!(d.materials[0].extras.as_deref(), Some(r#"{"ee_textureIndex":3}"#));
    }

    fn glb(json: &[u8], length: u32) -> Vec<u8> {
        let mut glb = Vec::new();
        glb.extend_from_slice(b"glTF");
        glb.extend_from_slice(&2u32.to_le_bytes());
        glb.extend_from_slice(&length.to_le_bytes());
        glb.extend_from_slice(&(json.len() as u32).to_le_bytes());
        glb.extend_from_slice(b"JSON");
        glb.extend_from_slice(json);
        glb.extend_from_slice(&4u32.to_le_bytes());
        glb.extend_from_slice(b"BIN\0");
        glb.extend_from_slice(&[1, 2, 3, 4]);
        glb
    }

    #[test]
    fn glb_chunks() {
        let json = br#"{"asset":{"version":"2.0"}} "#;
        let good = glb(json, (20 + json.len() + 8 + 4) as u32);
        let d = parse(&good).unwrap();
        assert_eq!(d.bin, Some(&[1u8, 2, 3, 4][..]));
        // a wrong chunk magic, a length past the file, a length under the header's
        let mut bad = good.clone();
        bad[16] = b'X';
        assert_eq!(parse_str(&bad), Err(()));
        assert_eq!(parse_str(&glb(json, 1000)), Err(()));
        assert_eq!(parse_str(&glb(json, 3)), Err(()));
        assert_eq!(parse_str(b"glTF\x02\0\0"), Err(()));
    }

    #[test]
    fn floats_read_and_normalize() {
        let doc = doc_with_bin();
        let mut d = parse(&doc).unwrap();
        let bytes: Vec<u8> = [1.5f32, 2.0, -3.0].iter().flat_map(|v| v.to_le_bytes()).collect();
        d.buffers[0].data = Some(Cow::Owned(bytes));
        assert_eq!(d.floats(&d.accessors[0]), Some(vec![1.5, 2.0, -3.0]));
        d.accessors[0].component = ComponentType::U8;
        d.accessors[0].stride = 1;
        d.accessors[0].normalized = true;
        let v = d.floats(&d.accessors[0]).unwrap();
        assert_eq!(v[0], 0.0); // the first byte of 1.5f32 is 0
        d.buffers[0].data = None; // no data: cannot be read
        assert_eq!(d.floats(&d.accessors[0]), None);
    }

    #[test]
    fn a_read_past_the_buffer_is_refused() {
        let doc = doc_with_bin();
        let mut d = parse(&doc).unwrap();
        d.buffers[0].data = Some(Cow::Owned(vec![0; 2]));
        assert_eq!(d.floats(&d.accessors[0]), None);
    }

    #[test]
    fn decode_uri_and_base64() {
        assert_eq!(decode_uri(b"a%20b%zz"), b"a b%zz".to_vec());
        assert_eq!(decode_uri(b"a%00b"), b"a".to_vec());
        assert_eq!(decode_base64(b"AQID", 3), Ok(vec![1, 2, 3]));
        assert_eq!(decode_base64(b"AQ", 3), Err(()));
    }

    #[test]
    fn load_buffers_data_uri_and_glb_bin() {
        let text = doc(r#""buffers":[{"byteLength":3,"uri":"data:application/octet-stream;base64,AQID"}]"#);
        let mut d = parse(&text).unwrap();
        assert_eq!(load_buffers(&mut d), Ok(()));
        assert_eq!(d.buffers[0].data.as_deref(), Some(&[1u8, 2, 3][..]));
        let text = doc(r#""buffers":[{"byteLength":3,"uri":"file.bin"}]"#);
        let mut d2 = parse(&text).unwrap();
        assert_eq!(load_buffers(&mut d2), Err(()));
    }

    #[test]
    fn cycles_through_parents_cannot_be_built_but_validate_guards() {
        let text = doc(r#""nodes":[{"name":"a"},{}]"#);
        let mut d = parse(&text).unwrap();
        d.nodes[0].parent = Some(1);
        d.nodes[1].parent = Some(0);
        assert_eq!(validate(&d), Err(()));
    }

    #[test]
    fn stride_comes_from_the_view_or_the_element_size() {
        let doc = doc_with_bin();
        let d = parse(&doc).unwrap();
        assert_eq!(d.accessors[0].stride, 4);
    }
}
