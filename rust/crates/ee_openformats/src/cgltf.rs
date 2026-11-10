//! cgltf 1.15's reader as far as the game's loader sees it: `cgltf_parse` (the jsmn token stream walked key by key:
//! duplicates, wrong token types and bad references refuse the file exactly where cgltf refuses it),
//! `cgltf_load_buffers`, `cgltf_validate`, and the accessor readers. Everything cgltf parses only to throw away here
//! (cameras, lights, samplers, material extensions) is still walked, because a malformed one refuses the file.
//!
//! References are kept as cgltf's "pointer index": `0` none, else index + 1 (an index of -1 from `atoi` of a non-number
//! is none, by the C arithmetic); `fixup` refuses the ones out of range. A failing out-of-bounds read of a buffer, which
//! the C reader only gets past validation with wrapped sizes (undefined there), is an error here.
#![allow(clippy::needless_range_loop)]

use std::borrow::Cow;

use crate::jsmn::{self, Token, TokenType};

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

#[derive(Clone, Copy, Default)]
pub(crate) struct Meshopt {
    pub buffer: Ref,
    pub offset: u64,
    pub size: u64,
    pub stride: u64,
    pub count: u64,
    pub mode: u8,   // 0 invalid, 1 attributes, 2 triangles, 3 indices
    pub filter: u8, // 0 none, 1 octahedral, 2 quaternion, 3 exponential
}

#[derive(Default)]
pub(crate) struct BufferView {
    pub buffer: Ref,
    pub offset: u64,
    pub size: u64,
    pub stride: u64,
    pub meshopt: Option<Meshopt>,
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
    pub index: i32,
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
    pub draco: Option<(Ref, Vec<Attribute>)>,
    pub mappings: Vec<(Ref, i32)>,
}

impl Default for Primitive {
    fn default() -> Self {
        Primitive {
            kind: PrimitiveKind::Triangles,
            indices: 0,
            material: 0,
            attributes: Vec::new(),
            targets: Vec::new(),
            draco: None,
            mappings: Vec::new(),
        }
    }
}

#[derive(Default)]
pub(crate) struct Mesh {
    pub primitives: Vec<Primitive>,
    pub weights: Option<u64>,
    pub target_names: Option<u64>,
}

#[derive(Default)]
pub(crate) struct Material {
    pub name: Option<Vec<u8>>,
    pub has_pbr: bool,
    pub base_color_texture: Ref,
    pub extras: Option<(i32, i32)>,
    pub texture_refs: Vec<Ref>,
}

#[derive(Default)]
pub(crate) struct Texture {
    pub image: Ref,
    pub sampler: Ref,
    pub basisu_image: Ref,
    pub webp_image: Ref,
}

#[derive(Default)]
pub(crate) struct Image {
    pub name: Option<Vec<u8>>,
    pub uri: Option<Vec<u8>>,
    pub view: Ref,
}

/// An extension object cgltf keeps raw (the name, the JSON text's token range).
pub(crate) struct Extension {
    pub name: Vec<u8>,
    pub data: (i32, i32),
}

pub(crate) struct Node {
    pub name: Option<Vec<u8>>,
    pub children: Vec<Ref>,
    pub mesh: Ref,
    pub skin: Ref,
    pub camera: Ref,
    pub light: Ref,
    pub weights: Option<u64>,
    pub has_matrix: bool,
    pub matrix: [f32; 16],
    pub has_translation: bool,
    pub translation: [f32; 3],
    pub has_rotation: bool,
    pub rotation: [f32; 4],
    pub has_scale: bool,
    pub scale: [f32; 3],
    pub extensions: Vec<Extension>,
    pub gpu_instancing: Option<Vec<Attribute>>,
    /// set by `fixup`
    pub parent: Option<usize>,
}

impl Default for Node {
    fn default() -> Self {
        let mut matrix = [0.0; 16];
        matrix[0] = 1.0;
        matrix[5] = 1.0;
        matrix[10] = 1.0;
        matrix[15] = 1.0;
        Node {
            name: None,
            children: Vec::new(),
            mesh: 0,
            skin: 0,
            camera: 0,
            light: 0,
            weights: None,
            has_matrix: false,
            matrix,
            has_translation: false,
            translation: [0.0; 3],
            has_rotation: false,
            rotation: [0.0, 0.0, 0.0, 1.0],
            has_scale: false,
            scale: [1.0; 3],
            extensions: Vec::new(),
            gpu_instancing: None,
            parent: None,
        }
    }
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

/// A parsed glTF. `json` is the JSON chunk the token ranges refer to.
#[derive(Default)]
pub(crate) struct Data<'a> {
    pub json: &'a [u8],
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
    pub lights_count: u64,
    pub variants_named: Vec<bool>,
}

// ------------------------------------------------------------------------------------------------- C runtime --

/// `atoi`/`atoll` as the Windows CRT does them (saturating): the digits after optional white space and a sign.
fn c_strtol(text: &[u8]) -> i64 {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let mut negative = false;
    if i < text.len() && matches!(text[i], b'+' | b'-') {
        negative = text[i] == b'-';
        i += 1;
    }
    let mut v: i64 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        let d = (text[i] - b'0') as i64;
        v = v.saturating_mul(10).saturating_add(d);
        i += 1;
    }
    if negative {
        v.saturating_neg()
    } else {
        v
    }
}

fn atoi(text: &[u8]) -> i32 {
    c_strtol(text).clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

fn hex_value(c: u8) -> Option<u32> {
    match c {
        b'0'..=b'9' => Some((c - b'0') as u32),
        b'a'..=b'f' => Some((c - b'a') as u32 + 10),
        b'A'..=b'F' => Some((c - b'A') as u32 + 10),
        _ => None,
    }
}

fn atof_hex(text: &[u8], mut i: usize) -> f64 {
    // a mantissa of up to 13 hex digits (exact in a double); more digits only keep the magnitude, like the exponent
    let mut mantissa: u64 = 0;
    let mut exp2: i64 = 0;
    let mut digits = 0;
    let mut seen_point = false;
    while i < text.len() {
        if text[i] == b'.' && !seen_point {
            seen_point = true;
            i += 1;
            continue;
        }
        let Some(v) = hex_value(text[i]) else { break };
        if digits < 13 {
            mantissa = mantissa * 16 + v as u64;
            digits += 1;
            if seen_point {
                exp2 -= 4;
            }
        } else if !seen_point {
            exp2 += 4;
        }
        i += 1;
    }
    if i < text.len() && matches!(text[i], b'p' | b'P') {
        let mut j = i + 1;
        let mut neg = false;
        if j < text.len() && matches!(text[j], b'+' | b'-') {
            neg = text[j] == b'-';
            j += 1;
        }
        let mut e: i64 = 0;
        let start = j;
        while j < text.len() && text[j].is_ascii_digit() {
            e = (e * 10 + (text[j] - b'0') as i64).min(1_000_000);
            j += 1;
        }
        if j > start {
            exp2 += if neg { -e } else { e };
        }
    }
    (mantissa as f64) * 2f64.powi(exp2.clamp(-100_000, 100_000) as i32)
}

/// `atof` as the Windows CRT does it: optional white space and sign, INF/INFINITY/NAN or a decimal or hexadecimal
/// number; the longest valid prefix counts, none is 0. Decimal parsing is correctly rounded on both sides.
fn atof(text: &[u8]) -> f64 {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let mut sign = 1.0;
    if i < text.len() && matches!(text[i], b'+' | b'-') {
        if text[i] == b'-' {
            sign = -1.0;
        }
        i += 1;
    }
    let rest = &text[i..];
    let starts_ci = |t: &[u8], word: &[u8]| t.len() >= word.len() && t[..word.len()].eq_ignore_ascii_case(word);
    if starts_ci(rest, b"infinity") || starts_ci(rest, b"inf") {
        return sign * f64::INFINITY;
    }
    if starts_ci(rest, b"nan") {
        return sign * f64::NAN;
    }
    if starts_ci(rest, b"0x") {
        return sign * atof_hex(text, i + 2);
    }
    let start = i;
    let mut j = i;
    while j < text.len() && text[j].is_ascii_digit() {
        j += 1;
    }
    let mut digits = j - i;
    if j < text.len() && text[j] == b'.' {
        let k = j + 1;
        let mut m = k;
        while m < text.len() && text[m].is_ascii_digit() {
            m += 1;
        }
        digits += m - k;
        if m > k || digits > 0 {
            j = m;
        }
    }
    if digits == 0 {
        return 0.0;
    }
    if j < text.len() && matches!(text[j], b'e' | b'E') {
        let mut k = j + 1;
        if k < text.len() && matches!(text[k], b'+' | b'-') {
            k += 1;
        }
        let mut m = k;
        while m < text.len() && text[m].is_ascii_digit() {
            m += 1;
        }
        if m > k {
            j = m;
        }
    }
    let token = std::str::from_utf8(&text[start..j]).expect("only ASCII digits and punctuation were kept");
    sign * token.parse::<f64>().expect("a syntactically valid float")
}

// ------------------------------------------------------------------------------------------------ the JSON walk --

struct Cur<'t, 'a> {
    toks: &'t [Token],
    js: &'a [u8],
    i: usize,
}

const UNDEF: Token = Token { ty: TokenType::Undefined, start: 0, end: 0, size: 0, parent: -1 };

impl<'t, 'a> Cur<'t, 'a> {
    fn tok(&self, i: usize) -> Token {
        self.toks.get(i).copied().unwrap_or(UNDEF)
    }

    fn cur(&self) -> Token {
        self.tok(self.i)
    }

    fn text(&self, i: usize) -> &'a [u8] {
        let t = self.tok(i);
        &self.js[t.start.max(0) as usize..t.end.max(0) as usize]
    }

    fn check(&self, ty: TokenType) -> R<()> {
        if self.cur().ty == ty {
            Ok(())
        } else {
            Err(())
        }
    }

    // a key: a string followed by a value
    fn check_key(&self) -> R<()> {
        let t = self.cur();
        if t.ty != TokenType::String || t.size == 0 {
            Err(())
        } else {
            Ok(())
        }
    }

    fn to_int_at(&self, i: usize) -> i32 {
        let t = self.tok(i);
        if t.ty != TokenType::Primitive {
            return -1; // CGLTF_ERROR_JSON, used as the value
        }
        let n = (t.end - t.start) as usize;
        atoi(&self.js[t.start as usize..t.start as usize + n.min(127)])
    }

    fn to_size_at(&self, i: usize) -> u64 {
        let t = self.tok(i);
        if t.ty != TokenType::Primitive {
            return 0;
        }
        let n = (t.end - t.start) as usize;
        c_strtol(&self.js[t.start as usize..t.start as usize + n.min(127)]).max(0) as u64
    }

    fn to_float_at(&self, i: usize) -> f32 {
        let t = self.tok(i);
        if t.ty != TokenType::Primitive {
            return -1.0;
        }
        let n = (t.end - t.start) as usize;
        atof(&self.js[t.start as usize..t.start as usize + n.min(127)]) as f32
    }

    fn to_bool_at(&self, i: usize) -> bool {
        let t = self.tok(i);
        (t.end - t.start) == 4 && self.js.get(t.start as usize..t.start as usize + 4) == Some(b"true")
    }

    // the cgltf_json_to_* helpers applied after the key: the key token, then the value token
    fn int(&mut self) -> i32 {
        self.i += 1;
        let v = self.to_int_at(self.i);
        self.i += 1;
        v
    }

    fn size(&mut self) -> u64 {
        self.i += 1;
        let v = self.to_size_at(self.i);
        self.i += 1;
        v
    }

    fn float(&mut self) -> f32 {
        self.i += 1;
        let v = self.to_float_at(self.i);
        self.i += 1;
        v
    }

    fn boolean(&mut self) -> bool {
        self.i += 1;
        let v = self.to_bool_at(self.i);
        self.i += 1;
        v
    }

    // a reference: the int's C arithmetic, (cgltf_size)idx + 1
    fn reference(&mut self) -> Ref {
        ptr_index(self.int())
    }

    fn reference_prim(&mut self) -> R<Ref> {
        self.i += 1;
        self.check(TokenType::Primitive)?;
        let v = ptr_index(self.to_int_at(self.i));
        self.i += 1;
        Ok(v)
    }

    // cgltf_skip_json
    fn skip(&mut self) -> R<()> {
        let mut end = self.i + 1;
        while self.i < end {
            let t = self.cur();
            match t.ty {
                TokenType::Object => end += t.size as usize * 2,
                TokenType::Array => end += t.size as usize,
                TokenType::Primitive | TokenType::String => {}
                TokenType::Undefined => return Err(()),
            }
            self.i += 1;
        }
        Ok(())
    }

    // an object value's members; f handles a key (returns true) or leaves it to be skipped
    fn members<F: FnMut(&mut Self, &'a [u8]) -> R<bool>>(&mut self, mut f: F) -> R<()> {
        self.check(TokenType::Object)?;
        let n = self.cur().size;
        self.i += 1;
        for _ in 0..n {
            self.check_key()?;
            let key = self.text(self.i);
            if !f(self, key)? {
                self.i += 1;
                self.skip()?;
            }
        }
        Ok(())
    }

    // cgltf_parse_json_array: positioned on the array; returns its size, positioned on its first element
    fn array(&mut self, seen: &mut bool) -> R<i32> {
        if self.cur().ty != TokenType::Array {
            return Err(());
        }
        if std::mem::replace(seen, true) {
            return Err(());
        }
        let n = self.cur().size;
        self.i += 1;
        Ok(n)
    }

    // cgltf_parse_json_string
    fn string(&mut self, slot: &mut Option<Vec<u8>>) -> R<()> {
        self.check(TokenType::String)?;
        if slot.is_some() {
            return Err(());
        }
        *slot = Some(self.text(self.i).to_vec());
        self.i += 1;
        Ok(())
    }

    fn string_value(&mut self, slot: &mut Option<Vec<u8>>) -> R<()> {
        self.i += 1;
        self.string(slot)
    }

    // cgltf_parse_json_float_array: positioned on the array
    fn float_array(&mut self, out: &mut [f32]) -> R<()> {
        self.check(TokenType::Array)?;
        if self.cur().size != out.len() as i32 {
            return Err(());
        }
        self.i += 1;
        for v in out.iter_mut() {
            self.check(TokenType::Primitive)?;
            *v = self.to_float_at(self.i);
            self.i += 1;
        }
        Ok(())
    }

    fn float_array_value(&mut self, out: &mut [f32]) -> R<()> {
        self.i += 1;
        self.float_array(out)
    }

    fn float_list_value(&mut self, n: usize) -> R<()> {
        let mut v = vec![0.0f32; n];
        self.float_array_value(&mut v)
    }

    // cgltf_parse_json_extras
    fn extras(&mut self, slot: &mut Option<(i32, i32)>) -> R<()> {
        self.i += 1;
        if slot.is_some() {
            return Err(());
        }
        let t = self.cur();
        *slot = Some((t.start, t.end));
        self.skip()
    }

    // cgltf_parse_json_unprocessed_extension: positioned on the key
    fn extension(&mut self, out: Option<&mut Vec<Extension>>) -> R<()> {
        self.check(TokenType::String)?;
        if self.tok(self.i + 1).ty != TokenType::Object {
            return Err(());
        }
        let name = self.text(self.i).to_vec();
        self.i += 1;
        let t = self.cur();
        let data = (t.start, t.end);
        self.skip()?;
        if let Some(out) = out {
            out.push(Extension { name, data });
        }
        Ok(())
    }

    // cgltf_parse_json_unprocessed_extensions: positioned on the "extensions" key
    fn unprocessed_extensions(&mut self, seen: &mut bool) -> R<()> {
        self.i += 1;
        self.check(TokenType::Object)?;
        if std::mem::replace(seen, true) {
            return Err(());
        }
        let n = self.cur().size;
        self.i += 1;
        for _ in 0..n {
            self.check_key()?;
            self.extension(None)?;
        }
        Ok(())
    }
}

fn ptr_index(v: i32) -> Ref {
    (v as i64 + 1) as u64
}

fn parse_attribute_type(name: &[u8]) -> (AttributeKind, i32) {
    if name.first() == Some(&b'_') {
        return (AttributeKind::Other, 0);
    }
    let us = name.iter().position(|&c| c == b'_');
    let head = &name[..us.unwrap_or(name.len())];
    let (mut kind, known) = match head {
        b"POSITION" => (AttributeKind::Position, true),
        b"NORMAL" => (AttributeKind::Normal, true),
        b"TEXCOORD" => (AttributeKind::Texcoord, true),
        b"TANGENT" | b"COLOR" | b"JOINTS" | b"WEIGHTS" => (AttributeKind::Other, true),
        _ => (AttributeKind::Other, false),
    };
    let mut index = 0;
    if let (Some(us), true) = (us, known) {
        index = atoi(&name[us + 1..]);
        if index < 0 {
            kind = AttributeKind::Other;
            index = 0;
        }
    }
    (kind, index)
}

// cgltf_parse_json_attribute_list: positioned on the object
fn attribute_list(c: &mut Cur, out: &mut Vec<Attribute>, seen: &mut bool) -> R<()> {
    c.check(TokenType::Object)?;
    if std::mem::replace(seen, true) {
        return Err(());
    }
    let n = c.cur().size;
    c.i += 1;
    out.reserve(n as usize);
    for _ in 0..n {
        c.check_key()?;
        let mut name = None;
        c.string(&mut name)?;
        let (kind, index) = parse_attribute_type(&name.unwrap());
        out.push(Attribute { kind, index, accessor: ptr_index(c.to_int_at(c.i)) });
        c.i += 1;
    }
    Ok(())
}

fn parse_draco(c: &mut Cur) -> R<(Ref, Vec<Attribute>)> {
    let mut attrs = Vec::new();
    let mut seen = false;
    let mut view = 0;
    c.members(|c, k| match k {
        b"attributes" => {
            c.i += 1;
            attribute_list(c, &mut attrs, &mut seen)?;
            Ok(true)
        }
        b"bufferView" => {
            view = c.reference();
            Ok(true)
        }
        _ => Ok(false),
    })?;
    Ok((view, attrs))
}

// cgltf_parse_json_material_mapping_data, both passes in one
fn mapping_data(c: &mut Cur, out: &mut Vec<(Ref, i32)>) -> R<()> {
    c.check(TokenType::Array)?;
    let n = c.cur().size;
    c.i += 1;
    for _ in 0..n {
        c.check(TokenType::Object)?;
        let m = c.cur().size;
        c.i += 1;
        let mut material = -1;
        let mut variants_tok: i64 = -1;
        let mut extras_tok: i64 = -1;
        for _ in 0..m {
            c.check_key()?;
            let k = c.text(c.i);
            if k == b"material" {
                material = c.int();
            } else if k == b"variants" {
                variants_tok = c.i as i64 + 1;
                if c.tok(c.i + 1).ty != TokenType::Array {
                    return Err(());
                }
                c.i += 1;
                c.skip()?;
            } else if k == b"extras" {
                extras_tok = c.i as i64 + 1;
                c.i += 1;
                c.skip()?;
            } else {
                c.i += 1;
                c.skip()?;
            }
        }
        if material < 0 || variants_tok < 0 {
            return Err(());
        }
        let vt = variants_tok as usize;
        let count = c.tok(vt).size;
        for k in 0..count {
            let variant = c.to_int_at(vt + 1 + k as usize);
            if variant < 0 {
                return Err(());
            }
            out.push((ptr_index(material), variant));
            if extras_tok >= 0 {
                // cgltf_parse_json_extras over the extras value: only its skip can fail
                let mut sub = Cur { toks: c.toks, js: c.js, i: extras_tok as usize };
                sub.skip()?;
            }
        }
    }
    Ok(())
}

fn material_mappings(c: &mut Cur, out: &mut Vec<(Ref, i32)>, seen: &mut bool) -> R<()> {
    c.members(|c, k| {
        if k != b"mappings" {
            return Ok(false);
        }
        if std::mem::replace(seen, true) {
            return Err(());
        }
        let start = c.i + 1;
        // the first pass only counts and validates: one combined pass checks the same things
        let mut sub = Cur { toks: c.toks, js: c.js, i: start };
        mapping_data(&mut sub, out)?;
        c.i = sub.i;
        Ok(true)
    })
}

fn parse_primitive(c: &mut Cur, out: &mut Primitive) -> R<()> {
    c.check(TokenType::Object)?;
    let mut seen_attrs = false;
    let mut seen_targets = false;
    let mut seen_exts = false;
    let mut seen_mappings = false;
    let mut extras = None;
    c.members(|c, k| {
        match k {
            b"mode" => {
                out.kind = match c.int() {
                    4 => PrimitiveKind::Triangles,
                    0 | 1 | 2 | 3 | 5 | 6 => PrimitiveKind::Other,
                    _ => PrimitiveKind::Invalid,
                };
            }
            b"indices" => out.indices = c.reference(),
            b"material" => out.material = c.reference(),
            b"attributes" => {
                c.i += 1;
                attribute_list(c, &mut out.attributes, &mut seen_attrs)?;
            }
            b"targets" => {
                c.i += 1;
                let n = c.array(&mut seen_targets)?;
                for _ in 0..n {
                    let mut target = Vec::new();
                    let mut seen = false;
                    attribute_list(c, &mut target, &mut seen)?;
                    out.targets.push(target);
                }
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    let kk = c.text(c.i);
                    if kk == b"KHR_draco_mesh_compression" {
                        c.i += 1;
                        out.draco = Some(parse_draco(c)?);
                    } else if kk == b"KHR_materials_variants" {
                        c.i += 1;
                        material_mappings(c, &mut out.mappings, &mut seen_mappings)?;
                    } else {
                        c.extension(None)?;
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_mesh(c: &mut Cur, out: &mut Mesh) -> R<()> {
    let mut name = None;
    let mut seen_prims = false;
    let mut seen_weights = false;
    let mut seen_exts = false;
    let mut seen_names = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"primitives" => {
                c.i += 1;
                let n = c.array(&mut seen_prims)?;
                for _ in 0..n {
                    let mut p = Primitive::default();
                    parse_primitive(c, &mut p)?;
                    out.primitives.push(p);
                }
            }
            b"weights" => {
                c.i += 1;
                let n = c.array(&mut seen_weights)?;
                out.weights = Some(n as u64);
                // cgltf_parse_json_float_array from the array token again
                c.i -= 1;
                let mut buf = vec![0.0f32; n.max(0) as usize];
                c.float_array(&mut buf)?;
            }
            b"extras" => {
                c.i += 1;
                if c.cur().ty == TokenType::Object {
                    let size = c.cur().size;
                    c.i += 1;
                    for _ in 0..size {
                        c.check_key()?;
                        if c.text(c.i) == b"targetNames" && c.tok(c.i + 1).ty == TokenType::Array {
                            c.i += 1;
                            c.check(TokenType::Array)?;
                            let n = c.array(&mut seen_names)?;
                            out.target_names = Some(n as u64);
                            for _ in 0..n {
                                let mut s = None;
                                c.string(&mut s)?;
                            }
                        } else {
                            c.i += 1;
                            c.skip()?;
                        }
                    }
                } else {
                    c.skip()?;
                }
            }
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn component_type(v: i32) -> ComponentType {
    match v {
        5120 => ComponentType::I8,
        5121 => ComponentType::U8,
        5122 => ComponentType::I16,
        5123 => ComponentType::U16,
        5125 => ComponentType::U32,
        5126 => ComponentType::F32,
        _ => ComponentType::Invalid,
    }
}

fn parse_sparse(c: &mut Cur, out: &mut Sparse) -> R<()> {
    c.members(|c, k| {
        match k {
            b"count" => out.count = c.size(),
            b"indices" => {
                c.i += 1;
                c.members(|c, k| {
                    match k {
                        b"bufferView" => out.indices_view = c.reference(),
                        b"byteOffset" => out.indices_offset = c.size(),
                        b"componentType" => out.indices_component = component_type(c.int()),
                        _ => return Ok(false),
                    }
                    Ok(true)
                })?;
            }
            b"values" => {
                c.i += 1;
                c.members(|c, k| {
                    match k {
                        b"bufferView" => out.values_view = c.reference(),
                        b"byteOffset" => out.values_offset = c.size(),
                        _ => return Ok(false),
                    }
                    Ok(true)
                })?;
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_accessor(c: &mut Cur, out: &mut Accessor) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"bufferView" => out.view = c.reference(),
            b"byteOffset" => out.offset = c.size(),
            b"componentType" => out.component = component_type(c.int()),
            b"normalized" => out.normalized = c.boolean(),
            b"count" => out.count = c.size(),
            b"type" => {
                c.i += 1;
                let t = c.text(c.i);
                out.ty = match (c.cur().ty, t) {
                    (TokenType::String, b"SCALAR") => ValueType::Scalar,
                    (TokenType::String, b"VEC2") => ValueType::Vec2,
                    (TokenType::String, b"VEC3") => ValueType::Vec3,
                    (TokenType::String, b"VEC4") => ValueType::Vec4,
                    (TokenType::String, b"MAT2") => ValueType::Mat2,
                    (TokenType::String, b"MAT3") => ValueType::Mat3,
                    (TokenType::String, b"MAT4") => ValueType::Mat4,
                    _ => out.ty,
                };
                c.i += 1;
            }
            b"min" | b"max" => {
                c.i += 1;
                let n = c.cur().size.min(16);
                let mut v = vec![0.0f32; n.max(0) as usize];
                c.float_array(&mut v)?;
            }
            b"sparse" => {
                out.is_sparse = true;
                c.i += 1;
                parse_sparse(c, &mut out.sparse)?;
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

// positioned on the object; returns the texture reference
fn parse_texture_transform(c: &mut Cur) -> R<()> {
    c.members(|c, k| {
        match k {
            b"offset" | b"scale" => c.float_list_value(2)?,
            b"rotation" => {
                c.float();
            }
            b"texCoord" => {
                c.int();
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

// cgltf_parse_json_texture_view: positioned on the object
fn parse_texture_view(c: &mut Cur) -> R<Ref> {
    let mut texture = 0;
    c.members(|c, k| {
        match k {
            b"index" => texture = c.reference(),
            b"texCoord" => {
                c.int();
            }
            b"scale" | b"strength" => {
                c.float();
            }
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    if c.text(c.i) == b"KHR_texture_transform" {
                        c.i += 1;
                        parse_texture_transform(c)?;
                    } else {
                        c.i += 1;
                        c.skip()?;
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })?;
    Ok(texture)
}

#[derive(Clone, Copy)]
enum Field {
    Float,
    Floats(usize),
    Texture,
}

// the material extensions' shape: each key a float, a float array or a texture view
fn parse_fields(c: &mut Cur, refs: &mut Vec<Ref>, fields: &[(&str, Field)]) -> R<()> {
    c.members(|c, k| {
        let Some((_, field)) = fields.iter().find(|(name, _)| name.as_bytes() == k) else { return Ok(false) };
        match field {
            Field::Float => {
                c.float();
            }
            Field::Floats(n) => c.float_list_value(*n)?,
            Field::Texture => {
                c.i += 1;
                let r = parse_texture_view(c)?;
                refs.push(r);
            }
        }
        Ok(true)
    })
}

const PBR_SG: &[(&str, Field)] = &[
    ("diffuseFactor", Field::Floats(4)),
    ("specularFactor", Field::Floats(3)),
    ("glossinessFactor", Field::Float),
    ("diffuseTexture", Field::Texture),
    ("specularGlossinessTexture", Field::Texture),
];
const CLEARCOAT: &[(&str, Field)] = &[
    ("clearcoatFactor", Field::Float),
    ("clearcoatRoughnessFactor", Field::Float),
    ("clearcoatTexture", Field::Texture),
    ("clearcoatRoughnessTexture", Field::Texture),
    ("clearcoatNormalTexture", Field::Texture),
];
const IOR: &[(&str, Field)] = &[("ior", Field::Float)];
const SPECULAR: &[(&str, Field)] = &[
    ("specularFactor", Field::Float),
    ("specularColorFactor", Field::Floats(3)),
    ("specularTexture", Field::Texture),
    ("specularColorTexture", Field::Texture),
];
const TRANSMISSION: &[(&str, Field)] = &[("transmissionFactor", Field::Float), ("transmissionTexture", Field::Texture)];
const VOLUME: &[(&str, Field)] = &[
    ("thicknessFactor", Field::Float),
    ("thicknessTexture", Field::Texture),
    ("attenuationColor", Field::Floats(3)),
    ("attenuationDistance", Field::Float),
];
const SHEEN: &[(&str, Field)] = &[
    ("sheenColorFactor", Field::Floats(3)),
    ("sheenColorTexture", Field::Texture),
    ("sheenRoughnessFactor", Field::Float),
    ("sheenRoughnessTexture", Field::Texture),
];
const EMISSIVE_STRENGTH: &[(&str, Field)] = &[("emissiveStrength", Field::Float)];
const IRIDESCENCE: &[(&str, Field)] = &[
    ("iridescenceFactor", Field::Float),
    ("iridescenceTexture", Field::Texture),
    ("iridescenceIor", Field::Float),
    ("iridescenceThicknessMinimum", Field::Float),
    ("iridescenceThicknessMaximum", Field::Float),
    ("iridescenceThicknessTexture", Field::Texture),
];
const DIFFUSE_TRANSMISSION: &[(&str, Field)] = &[
    ("diffuseTransmissionFactor", Field::Float),
    ("diffuseTransmissionTexture", Field::Texture),
    ("diffuseTransmissionColorFactor", Field::Floats(3)),
    ("diffuseTransmissionColorTexture", Field::Texture),
];
const ANISOTROPY: &[(&str, Field)] = &[
    ("anisotropyStrength", Field::Float),
    ("anisotropyRotation", Field::Float),
    ("anisotropyTexture", Field::Texture),
];
const DISPERSION: &[(&str, Field)] = &[("dispersion", Field::Float)];

fn parse_material(c: &mut Cur, out: &mut Material) -> R<()> {
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut out.name)?,
            b"pbrMetallicRoughness" => {
                out.has_pbr = true;
                c.i += 1;
                c.members(|c, k| {
                    match k {
                        b"metallicFactor" | b"roughnessFactor" => {
                            c.float();
                        }
                        b"baseColorFactor" => c.float_list_value(4)?,
                        b"baseColorTexture" => {
                            c.i += 1;
                            out.base_color_texture = parse_texture_view(c)?;
                        }
                        b"metallicRoughnessTexture" => {
                            c.i += 1;
                            let r = parse_texture_view(c)?;
                            out.texture_refs.push(r);
                        }
                        _ => return Ok(false),
                    }
                    Ok(true)
                })?;
            }
            b"emissiveFactor" => c.float_list_value(3)?,
            b"normalTexture" | b"occlusionTexture" | b"emissiveTexture" => {
                c.i += 1;
                let r = parse_texture_view(c)?;
                out.texture_refs.push(r);
            }
            b"alphaMode" => c.i += 2,
            b"alphaCutoff" => {
                c.float();
            }
            b"doubleSided" => {
                c.boolean();
            }
            b"extras" => c.extras(&mut out.extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    let k = c.text(c.i);
                    let fields = match k {
                        b"KHR_materials_pbrSpecularGlossiness" => Some(PBR_SG),
                        b"KHR_materials_clearcoat" => Some(CLEARCOAT),
                        b"KHR_materials_ior" => Some(IOR),
                        b"KHR_materials_specular" => Some(SPECULAR),
                        b"KHR_materials_transmission" => Some(TRANSMISSION),
                        b"KHR_materials_volume" => Some(VOLUME),
                        b"KHR_materials_sheen" => Some(SHEEN),
                        b"KHR_materials_emissive_strength" => Some(EMISSIVE_STRENGTH),
                        b"KHR_materials_iridescence" => Some(IRIDESCENCE),
                        b"KHR_materials_diffuse_transmission" => Some(DIFFUSE_TRANSMISSION),
                        b"KHR_materials_anisotropy" => Some(ANISOTROPY),
                        b"KHR_materials_dispersion" => Some(DISPERSION),
                        _ => None,
                    };
                    if let Some(fields) = fields {
                        c.i += 1;
                        parse_fields(c, &mut out.texture_refs, fields)?;
                    } else if k == b"KHR_materials_unlit" {
                        c.i += 1;
                        c.skip()?;
                    } else {
                        c.extension(None)?;
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_image(c: &mut Cur, out: &mut Image) -> R<()> {
    let mut mime = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"uri" => c.string_value(&mut out.uri)?,
            b"bufferView" => out.view = c.reference(),
            b"mimeType" => c.string_value(&mut mime)?,
            b"name" => c.string_value(&mut out.name)?,
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_sampler(c: &mut Cur) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"magFilter" | b"minFilter" | b"wrapS" | b"wrapT" => {
                c.int();
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_texture_source(c: &mut Cur, out: &mut Ref) -> R<()> {
    c.i += 1;
    c.members(|c, k| {
        if k == b"source" {
            *out = c.reference();
            Ok(true)
        } else {
            Ok(false)
        }
    })
}

fn parse_texture(c: &mut Cur, out: &mut Texture) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"sampler" => out.sampler = c.reference(),
            b"source" => out.image = c.reference(),
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    match c.text(c.i) {
                        b"KHR_texture_basisu" => parse_texture_source(c, &mut out.basisu_image)?,
                        b"EXT_texture_webp" => parse_texture_source(c, &mut out.webp_image)?,
                        _ => c.extension(None)?,
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_meshopt(c: &mut Cur) -> R<Meshopt> {
    let mut out = Meshopt::default();
    c.members(|c, k| {
        match k {
            b"buffer" => out.buffer = c.reference(),
            b"byteOffset" => out.offset = c.size(),
            b"byteLength" => out.size = c.size(),
            b"byteStride" => out.stride = c.size(),
            b"count" => out.count = c.size(),
            b"mode" => {
                c.i += 1;
                out.mode = match (c.cur().ty, c.text(c.i)) {
                    (TokenType::String, b"ATTRIBUTES") => 1,
                    (TokenType::String, b"TRIANGLES") => 2,
                    (TokenType::String, b"INDICES") => 3,
                    _ => out.mode,
                };
                c.i += 1;
            }
            b"filter" => {
                c.i += 1;
                out.filter = match (c.cur().ty, c.text(c.i)) {
                    (TokenType::String, b"NONE") => 0,
                    (TokenType::String, b"OCTAHEDRAL") => 1,
                    (TokenType::String, b"QUATERNION") => 2,
                    (TokenType::String, b"EXPONENTIAL") => 3,
                    _ => out.filter,
                };
                c.i += 1;
            }
            _ => return Ok(false),
        }
        Ok(true)
    })?;
    Ok(out)
}

fn parse_buffer_view(c: &mut Cur, out: &mut BufferView) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"buffer" => out.buffer = c.reference(),
            b"byteOffset" => out.offset = c.size(),
            b"byteLength" => out.size = c.size(),
            b"byteStride" => out.stride = c.size(),
            b"target" => {
                c.int();
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    if c.text(c.i) == b"EXT_meshopt_compression" {
                        c.i += 1;
                        out.meshopt = Some(parse_meshopt(c)?);
                    } else {
                        c.extension(None)?;
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_buffer(c: &mut Cur, out: &mut Buffer) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"byteLength" => out.size = c.size(),
            b"uri" => c.string_value(&mut out.uri)?,
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_skin(c: &mut Cur, out: &mut Skin) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_joints = false;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"joints" => {
                c.i += 1;
                let n = c.array(&mut seen_joints)?;
                for _ in 0..n {
                    out.joints.push(ptr_index(c.to_int_at(c.i)));
                    c.i += 1;
                }
            }
            b"skeleton" => out.skeleton = c.reference_prim()?,
            b"inverseBindMatrices" => out.inverse_bind_matrices = c.reference_prim()?,
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_camera(c: &mut Cur) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_type = false;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"perspective" | b"orthographic" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                let n = c.cur().size;
                c.i += 1;
                if std::mem::replace(&mut seen_type, true) {
                    return Err(());
                }
                let mut sub_extras = None;
                for _ in 0..n {
                    c.check_key()?;
                    match c.text(c.i) {
                        b"aspectRatio" | b"yfov" | b"zfar" | b"znear" | b"xmag" | b"ymag" => {
                            c.float();
                        }
                        b"extras" => c.extras(&mut sub_extras)?,
                        _ => {
                            c.i += 1;
                            c.skip()?;
                        }
                    }
                }
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_light(c: &mut Cur) -> R<()> {
    let mut name = None;
    let mut extras = None;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"color" => c.float_list_value(3)?,
            b"intensity" | b"range" => {
                c.float();
            }
            b"type" => c.i += 2,
            b"spot" => {
                c.i += 1;
                c.members(|c, k| {
                    if k == b"innerConeAngle" || k == b"outerConeAngle" {
                        c.float();
                        Ok(true)
                    } else {
                        Ok(false)
                    }
                })?;
            }
            b"extras" => c.extras(&mut extras)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_node(c: &mut Cur, out: &mut Node) -> R<()> {
    let mut extras = None;
    let mut seen_children = false;
    let mut seen_weights = false;
    let mut seen_exts = false;
    let name = &mut out.name;
    let (matrix, translation, rotation, scale) =
        (&mut out.matrix, &mut out.translation, &mut out.rotation, &mut out.scale);
    let (has_matrix, has_t, has_r, has_s) =
        (&mut out.has_matrix, &mut out.has_translation, &mut out.has_rotation, &mut out.has_scale);
    let (children, mesh, skin, camera, light, weights, extensions, gpu) = (
        &mut out.children,
        &mut out.mesh,
        &mut out.skin,
        &mut out.camera,
        &mut out.light,
        &mut out.weights,
        &mut out.extensions,
        &mut out.gpu_instancing,
    );
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(name)?,
            b"children" => {
                c.i += 1;
                let n = c.array(&mut seen_children)?;
                for _ in 0..n {
                    children.push(ptr_index(c.to_int_at(c.i)));
                    c.i += 1;
                }
            }
            b"mesh" => *mesh = c.reference_prim()?,
            b"skin" => *skin = c.reference_prim()?,
            b"camera" => *camera = c.reference_prim()?,
            b"translation" => {
                *has_t = true;
                c.float_array_value(translation)?;
            }
            b"rotation" => {
                *has_r = true;
                c.float_array_value(rotation)?;
            }
            b"scale" => {
                *has_s = true;
                c.float_array_value(scale)?;
            }
            b"matrix" => {
                *has_matrix = true;
                c.float_array_value(matrix)?;
            }
            b"weights" => {
                c.i += 1;
                let n = c.array(&mut seen_weights)?;
                *weights = Some(n as u64);
                c.i -= 1;
                let mut buf = vec![0.0f32; n.max(0) as usize];
                c.float_array(&mut buf)?;
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    let kk = c.text(c.i);
                    if kk == b"KHR_lights_punctual" {
                        c.i += 1;
                        c.members(|c, k| {
                            if k == b"light" {
                                *light = c.reference_prim()?;
                                Ok(true)
                            } else {
                                Ok(false)
                            }
                        })?;
                    } else if kk == b"EXT_mesh_gpu_instancing" {
                        c.i += 1;
                        let mut attrs = Vec::new();
                        let mut seen = false;
                        c.members(|c, k| {
                            if k == b"attributes" {
                                c.i += 1;
                                attribute_list(c, &mut attrs, &mut seen)?;
                                Ok(true)
                            } else {
                                Ok(false)
                            }
                        })?;
                        *gpu = Some(attrs);
                    } else {
                        c.extension(Some(&mut *extensions))?;
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_scene(c: &mut Cur, out: &mut Scene) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_nodes = false;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"nodes" => {
                c.i += 1;
                let n = c.array(&mut seen_nodes)?;
                for _ in 0..n {
                    out.nodes.push(ptr_index(c.to_int_at(c.i)));
                    c.i += 1;
                }
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_animation_sampler(c: &mut Cur, out: &mut AnimSampler) -> R<()> {
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"input" => out.input = c.reference(),
            b"output" => out.output = c.reference(),
            b"interpolation" => {
                c.i += 1;
                out.interpolation = match (c.cur().ty, c.text(c.i)) {
                    (TokenType::String, b"LINEAR") => Interpolation::Linear,
                    (TokenType::String, b"STEP") => Interpolation::Step,
                    (TokenType::String, b"CUBICSPLINE") => Interpolation::CubicSpline,
                    _ => out.interpolation,
                };
                c.i += 1;
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_animation_channel(c: &mut Cur, out: &mut AnimChannel) -> R<()> {
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"sampler" => out.sampler = c.reference(),
            b"target" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    match c.text(c.i) {
                        b"node" => out.target_node = c.reference(),
                        b"path" => {
                            c.i += 1;
                            out.target_path = match (c.cur().ty, c.text(c.i)) {
                                (TokenType::String, b"translation") => TargetPath::Translation,
                                (TokenType::String, b"rotation") => TargetPath::Rotation,
                                (TokenType::String, b"scale") => TargetPath::Scale,
                                (TokenType::String, b"weights") => TargetPath::Weights,
                                _ => out.target_path,
                            };
                            c.i += 1;
                        }
                        b"extras" => c.extras(&mut extras)?,
                        b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
                        _ => {
                            c.i += 1;
                            c.skip()?;
                        }
                    }
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_animation(c: &mut Cur, out: &mut Animation) -> R<()> {
    let mut name = None;
    let mut extras = None;
    let mut seen_samplers = false;
    let mut seen_channels = false;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"name" => c.string_value(&mut name)?,
            b"samplers" => {
                c.i += 1;
                let n = c.array(&mut seen_samplers)?;
                for _ in 0..n {
                    let mut s = AnimSampler::default();
                    parse_animation_sampler(c, &mut s)?;
                    out.samplers.push(s);
                }
            }
            b"channels" => {
                c.i += 1;
                let n = c.array(&mut seen_channels)?;
                for _ in 0..n {
                    let mut ch = AnimChannel::default();
                    parse_animation_channel(c, &mut ch)?;
                    out.channels.push(ch);
                }
            }
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })
}

fn parse_asset(c: &mut Cur) -> R<()> {
    let mut version = None;
    let mut min_version = None;
    let mut copyright = None;
    let mut generator = None;
    let mut extras = None;
    let mut seen_exts = false;
    c.members(|c, k| {
        match k {
            b"copyright" => c.string_value(&mut copyright)?,
            b"generator" => c.string_value(&mut generator)?,
            b"version" => c.string_value(&mut version)?,
            b"minVersion" => c.string_value(&mut min_version)?,
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => c.unprocessed_extensions(&mut seen_exts)?,
            _ => return Ok(false),
        }
        Ok(true)
    })?;
    if let Some(v) = version {
        if atof(&v) < 2.0 {
            return Err(());
        }
    }
    Ok(())
}

// a list of element parsers: "key": [ ... ], its size in the data
macro_rules! array_of {
    ($c:expr, $seen:expr, $vec:expr, $parse:expr) => {{
        $c.i += 1;
        let n = $c.array($seen)?;
        for _ in 0..n {
            let mut element = Default::default();
            $parse($c, &mut element)?;
            $vec.push(element);
        }
    }};
}

fn parse_root<'a>(c: &mut Cur<'_, 'a>, d: &mut Data<'a>) -> R<()> {
    let mut seen = [false; 16];
    let mut extras = None;
    let mut seen_exts = false;
    let mut used = false;
    let mut required = false;
    c.members(|c, k| {
        match k {
            b"asset" => {
                c.i += 1;
                parse_asset(c)?;
            }
            b"meshes" => array_of!(c, &mut seen[0], d.meshes, parse_mesh),
            b"accessors" => array_of!(c, &mut seen[1], d.accessors, parse_accessor),
            b"bufferViews" => array_of!(c, &mut seen[2], d.views, parse_buffer_view),
            b"buffers" => array_of!(c, &mut seen[3], d.buffers, parse_buffer),
            b"materials" => array_of!(c, &mut seen[4], d.materials, parse_material),
            b"images" => array_of!(c, &mut seen[5], d.images, parse_image),
            b"textures" => array_of!(c, &mut seen[6], d.textures, parse_texture),
            b"samplers" => {
                c.i += 1;
                let n = c.array(&mut seen[7])?;
                d.samplers_count = n as u64;
                for _ in 0..n {
                    parse_sampler(c)?;
                }
            }
            b"skins" => array_of!(c, &mut seen[8], d.skins, parse_skin),
            b"cameras" => {
                c.i += 1;
                let n = c.array(&mut seen[9])?;
                d.cameras_count = n as u64;
                for _ in 0..n {
                    parse_camera(c)?;
                }
            }
            b"nodes" => array_of!(c, &mut seen[10], d.nodes, parse_node),
            b"scenes" => array_of!(c, &mut seen[11], d.scenes, parse_scene),
            b"scene" => d.scene = c.reference(),
            b"animations" => array_of!(c, &mut seen[12], d.animations, parse_animation),
            b"extras" => c.extras(&mut extras)?,
            b"extensions" => {
                c.i += 1;
                c.check(TokenType::Object)?;
                if std::mem::replace(&mut seen_exts, true) {
                    return Err(());
                }
                let n = c.cur().size;
                c.i += 1;
                for _ in 0..n {
                    c.check_key()?;
                    let kk = c.text(c.i);
                    if kk == b"KHR_lights_punctual" {
                        c.i += 1;
                        c.members(|c, k| {
                            if k == b"lights" {
                                c.i += 1;
                                let n = c.array(&mut seen[13])?;
                                d.lights_count = n as u64;
                                for _ in 0..n {
                                    parse_light(c)?;
                                }
                                Ok(true)
                            } else {
                                Ok(false)
                            }
                        })?;
                    } else if kk == b"KHR_materials_variants" {
                        c.i += 1;
                        c.members(|c, k| {
                            if k == b"variants" {
                                c.i += 1;
                                let n = c.array(&mut seen[14])?;
                                for _ in 0..n {
                                    let mut name = None;
                                    let mut extras = None;
                                    c.members(|c, k| {
                                        match k {
                                            b"name" => c.string_value(&mut name)?,
                                            b"extras" => c.extras(&mut extras)?,
                                            _ => return Ok(false),
                                        }
                                        Ok(true)
                                    })?;
                                    d.variants_named.push(name.is_some());
                                }
                                Ok(true)
                            } else {
                                Ok(false)
                            }
                        })?;
                    } else {
                        c.extension(None)?;
                    }
                }
            }
            b"extensionsUsed" | b"extensionsRequired" => {
                c.i += 1;
                c.check(TokenType::Array)?;
                let flag = if k == b"extensionsUsed" { &mut used } else { &mut required };
                let n = c.array(flag)?;
                for _ in 0..n {
                    let mut s = None;
                    c.string(&mut s)?;
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    })
}

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

// cgltf_fixup_pointers: every reference in range; parents and strides set
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
            if let Some((view, attrs)) = &p.draco {
                fix_req(*view, nview)?;
                for a in attrs {
                    fix_req(a.accessor, nacc)?;
                }
            }
            for (material, _) in &p.mappings {
                fix_req(*material, nmat)?;
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
        fix(t.basisu_image, nimg)?;
        fix(t.webp_image, nimg)?;
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
        if let Some(mc) = &v.meshopt {
            fix_req(mc.buffer, nbuf)?;
        }
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
        fix(n.light, d.lights_count as usize)?;
        if let Some(attrs) = &n.gpu_instancing {
            for a in attrs {
                fix_req(a.accessor, nacc)?;
            }
        }
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

/// `cgltf_parse`, as the original `GltfToCem` called it (type autodetected).
pub(crate) fn parse(data: &[u8]) -> R<Data<'_>> {
    const GLB_MAGIC: u32 = 0x4654_6c67;
    if data.len() < 12 {
        return Err(());
    }
    let u32_at = |p: usize| u32::from_le_bytes(data[p..p + 4].try_into().unwrap());
    let (json, bin): (&[u8], Option<&[u8]>);
    if u32_at(0) != GLB_MAGIC {
        (json, bin) = (data, None);
    } else {
        if u32_at(4) != 2 {
            return Err(());
        }
        if u32_at(8) as usize > data.len() {
            return Err(());
        }
        if 12 + 8 > data.len() {
            return Err(());
        }
        let json_length = u32_at(12) as usize;
        let size = data.len();
        if json_length > size - 12 - 8 {
            return Err(());
        }
        if u32_at(16) != 0x4E4F_534A {
            return Err(());
        }
        let json_start = 20;
        let mut b = None;
        if size - 12 - 8 - json_length >= 8 {
            let bin_chunk = json_start + json_length;
            let bin_length = u32_at(bin_chunk) as usize;
            if bin_length > size - 12 - 8 - json_length - 8 {
                return Err(());
            }
            if u32_at(bin_chunk + 4) != 0x004E_4942 {
                return Err(());
            }
            b = Some(&data[bin_chunk + 8..bin_chunk + 8 + bin_length]);
        }
        (json, bin) = (&data[json_start..json_start + json_length], b);
    }
    let toks = jsmn::tokenize(json).map_err(|_| ())?;
    let mut d = Data { json, bin, ..Default::default() };
    let mut c = Cur { toks: &toks, js: json, i: 0 };
    parse_root(&mut c, &mut d)?;
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
        if let Some(mc) = &v.meshopt {
            if d.buffers[mc.buffer as usize - 1].size < mc.offset.wrapping_add(mc.size) {
                return Err(());
            }
            if v.stride != 0 && mc.stride != v.stride {
                return Err(());
            }
            if v.size != mc.stride.wrapping_mul(mc.count) {
                return Err(());
            }
            if mc.mode == 0 {
                return Err(());
            }
            if mc.mode == 1 && !(mc.stride % 4 == 0 && mc.stride <= 256) {
                return Err(());
            }
            if mc.mode == 2 && mc.count % 3 != 0 {
                return Err(());
            }
            if (mc.mode == 2 || mc.mode == 3) && mc.stride != 2 && mc.stride != 4 {
                return Err(());
            }
            if (mc.mode == 2 || mc.mode == 3) && mc.filter != 0 {
                return Err(());
            }
            if mc.filter == 1 && mc.stride != 4 && mc.stride != 8 {
                return Err(());
            }
            if mc.filter == 2 && mc.stride != 8 {
                return Err(());
            }
        }
    }
    for m in &d.meshes {
        if let Some(w) = m.weights {
            if !m.primitives.is_empty() && m.primitives[0].targets.len() as u64 != w {
                return Err(());
            }
        }
        if let Some(t) = m.target_names {
            if !m.primitives.is_empty() && m.primitives[0].targets.len() as u64 != t {
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
            for (_, variant) in &p.mappings {
                if (*variant as usize) >= d.variants_named.len() {
                    return Err(());
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
        if let Some(attrs) = &n.gpu_instancing {
            if n.mesh == 0 || attrs.is_empty() {
                return Err(());
            }
            let first = d.accessors[attrs[0].accessor as usize - 1].count;
            if attrs.iter().any(|a| d.accessors[a.accessor as usize - 1].count != first) {
                return Err(());
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
    if d.variants_named.iter().any(|named| !named) {
        return Err(());
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

    fn doc_with_bin(extra: &str) -> Vec<u8> {
        format!(
            concat!(
                r#"{{"asset":{{"version":"2.0"}},"buffers":[{{"byteLength":12}}],"#,
                r#""bufferViews":[{{"buffer":0,"byteLength":12}}],"accessors":[{{"bufferView":0,"componentType":5126,"count":3,"type":"SCALAR"}}]{}}}"#
            ),
            extra
        )
        .into_bytes()
    }

    fn parse_str(s: &[u8]) -> Result<(), ()> {
        parse(s).map(|_| ())
    }

    #[test]
    fn atof_matches_the_crt_prefix_rules() {
        assert_eq!(atof(b"  -1.5e2xyz"), -150.0);
        assert_eq!(atof(b".5"), 0.5);
        assert_eq!(atof(b"5."), 5.0);
        assert_eq!(atof(b"1e"), 1.0);
        assert_eq!(atof(b"1e+"), 1.0);
        assert_eq!(atof(b"abc"), 0.0);
        assert_eq!(atof(b"true"), 0.0);
        assert!(atof(b"inf").is_infinite());
        assert!(atof(b"NaN").is_nan());
        assert_eq!(atof(b"0x10"), 16.0);
        assert_eq!(atof(b"0x1p3"), 8.0);
        assert_eq!(atof(b"0.1") as f32, 0.1f32);
    }

    #[test]
    fn atoi_saturates_like_the_crt_and_reads_a_prefix() {
        assert_eq!(atoi(b"  12abc"), 12);
        assert_eq!(atoi(b"-7"), -7);
        assert_eq!(atoi(b"abc"), 0);
        assert_eq!(atoi(b"99999999999"), i32::MAX);
        assert_eq!(atoi(b"-99999999999"), i32::MIN);
    }

    #[test]
    fn minimal_document_parses_and_validates() {
        let doc = doc_with_bin("");
        let mut d = parse(&doc).unwrap();
        assert_eq!((d.accessors.len(), d.views.len(), d.buffers.len()), (1, 1, 1));
        // with no data the sparse/bound checks are skipped, the size checks are not
        assert_eq!(validate(&d), Ok(()));
        d.accessors[0].count = 4;
        assert_eq!(validate(&d), Err(()));
    }

    #[test]
    fn duplicates_and_type_errors_refuse_the_file() {
        assert_eq!(parse_str(br#"{"accessors":[],"accessors":[]}"#), Err(()));
        assert_eq!(parse_str(br#"{"meshes":[{"name":"a","name":"b"}]}"#), Err(()));
        assert_eq!(parse_str(br#"{"nodes":[{"translation":[1,2]}]}"#), Err(()));
        assert_eq!(parse_str(br#"{"nodes":[{"mesh":"x"}]}"#), Err(()));
        assert_eq!(parse_str(br#"{"meshes":[{"primitives":[{"attributes":[]}]}]}"#), Err(()));
    }

    #[test]
    fn legacy_version_refuses() {
        assert_eq!(parse_str(br#"{"asset":{"version":"1.0"}}"#), Err(()));
        assert_eq!(parse_str(br#"{"asset":{"version":"2.0"}}"#), Ok(()));
        assert_eq!(parse_str(br#"{"asset":{"generator":"x"}}"#), Ok(()));
    }

    #[test]
    fn references_are_range_checked_in_fixup() {
        assert_eq!(parse_str(br#"{"nodes":[{"mesh":0}]}"#), Err(()));
        assert_eq!(validate(&parse(br#"{"nodes":[{"children":[0]}]}"#).unwrap()), Err(())); // a parent cycle
        assert_eq!(parse_str(br#"{"nodes":[{"children":[1]},{"children":[2]},{}]}"#), Ok(()));
        assert_eq!(parse_str(br#"{"scene":0}"#), Err(()));
        assert_eq!(parse_str(br#"{"scenes":[{"nodes":[0]}],"nodes":[{}],"scene":1}"#), Err(()));
        assert_eq!(parse_str(br#"{"nodes":[{"name":"a"}],"scenes":[{"nodes":[0]}],"scene":0}"#), Ok(()));
    }

    #[test]
    fn a_non_primitive_index_becomes_none_by_c_arithmetic() {
        // cgltf_json_to_int of an object is -1 (CGLTF_ERROR_JSON); (cgltf_size)-1 + 1 == 0: absent
        let doc = br#"{"meshes":[{"primitives":[{"attributes":{"POSITION":{}}}]}]}"#;
        // ...but the attribute reference is required, so fixup refuses
        assert_eq!(parse_str(doc), Err(()));
        assert_eq!(ptr_index(-1), 0);
        assert_eq!(ptr_index(-2), u64::MAX);
    }

    #[test]
    fn attribute_names() {
        assert_eq!(parse_attribute_type(b"POSITION"), (AttributeKind::Position, 0));
        assert_eq!(parse_attribute_type(b"NORMAL_1"), (AttributeKind::Normal, 1));
        assert_eq!(parse_attribute_type(b"TEXCOORD_x"), (AttributeKind::Texcoord, 0));
        assert_eq!(parse_attribute_type(b"_CUSTOM"), (AttributeKind::Other, 0));
        assert_eq!(parse_attribute_type(b"WEIRD_3"), (AttributeKind::Other, 0));
        assert_eq!(parse_attribute_type(b"TEXCOORD_-1"), (AttributeKind::Other, 0));
    }

    #[test]
    fn glb_chunks() {
        let json = br#"{"asset":{"version":"2.0"}}"#;
        let mut glb = Vec::new();
        glb.extend_from_slice(b"glTF");
        glb.extend_from_slice(&2u32.to_le_bytes());
        glb.extend_from_slice(&((20 + json.len() + 8 + 4) as u32).to_le_bytes());
        glb.extend_from_slice(&(json.len() as u32).to_le_bytes());
        glb.extend_from_slice(b"JSON");
        glb.extend_from_slice(json);
        glb.extend_from_slice(&4u32.to_le_bytes());
        glb.extend_from_slice(b"BIN\0");
        glb.extend_from_slice(&[1, 2, 3, 4]);
        let d = parse(&glb).unwrap();
        assert_eq!(d.bin, Some(&[1u8, 2, 3, 4][..]));
        // a wrong chunk magic
        let mut bad = glb.clone();
        bad[16] = b'X';
        assert_eq!(parse_str(&bad), Err(()));
    }

    #[test]
    fn floats_read_and_normalize() {
        let doc = doc_with_bin("");
        let mut d = parse(&doc).unwrap();
        let bytes: Vec<u8> = [1.5f32, 2.0, -3.0].iter().flat_map(|v| v.to_le_bytes()).collect();
        d.buffers[0].data = Some(Cow::Owned(bytes));
        assert_eq!(d.floats(&d.accessors[0]), Some(vec![1.5, 2.0, -3.0]));
        d.accessors[0].component = ComponentType::U8;
        d.accessors[0].stride = 1;
        d.accessors[0].normalized = true;
        let v = d.floats(&d.accessors[0]).unwrap();
        assert_eq!(v[0], 0.0); // the first byte of 1.5f32 is 0
                               // no data: cannot be read
        d.buffers[0].data = None;
        assert_eq!(d.floats(&d.accessors[0]), None);
    }

    #[test]
    fn a_read_past_the_buffer_is_refused() {
        let doc = doc_with_bin("");
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
        let doc = br#"{"buffers":[{"byteLength":3,"uri":"data:application/octet-stream;base64,AQID"}]}"#;
        let mut d = parse(doc).unwrap();
        assert_eq!(load_buffers(&mut d), Ok(()));
        assert_eq!(d.buffers[0].data.as_deref(), Some(&[1u8, 2, 3][..]));
        let doc2 = br#"{"buffers":[{"byteLength":3,"uri":"file.bin"}]}"#;
        let mut d2 = parse(doc2).unwrap();
        assert_eq!(load_buffers(&mut d2), Err(()));
    }

    #[test]
    fn cycles_through_parents_cannot_be_built_but_validate_guards() {
        let doc = br#"{"nodes":[{"name":"a"}]}"#;
        let mut d = parse(doc).unwrap();
        d.nodes.push(Node::default());
        d.nodes[0].parent = Some(1);
        d.nodes[1].parent = Some(0);
        assert_eq!(validate(&d), Err(()));
    }

    #[test]
    fn stride_comes_from_the_view_or_the_element_size() {
        let doc = doc_with_bin("");
        let d = parse(&doc).unwrap();
        assert_eq!(d.accessors[0].stride, 4);
    }
}
