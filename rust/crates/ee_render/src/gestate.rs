//! The LLE's named integer constants, as plain (unscoped) C enums for its C++ headers; each comment gives the
//! evidence for its name, "(provisional name)" marks an inferred one.
#![allow(non_camel_case_types, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// GETStage::mTextureAddressingMode (GETexture::SetStageTextureAddressingMode): DX7Rasterizer::SetTexture maps each to D3DTADDRESS_*.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEAddressingMode {
    /// DX7Rasterizer::SetTexture: 0 -> D3DTADDRESS_CLAMP
    kAddressingClamp = 0,
    /// DX7Rasterizer::SetTexture: 1 -> D3DTADDRESS_MIRROR
    kAddressingMirror = 1,
    /// DX7Rasterizer::SetTexture: 2 -> D3DTADDRESS_WRAP
    kAddressingWrap = 2,
}

/// GERasterizer::SetBlending source / destination factors: DX7Rasterizer::SetBlending 0x1000b4ea maps each to the
/// Direct3D 7 D3DBLEND_* value (the SDK names).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEBlendFactor {
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 0 -> D3DBLEND_ZERO
    kBlendZero = 0,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 1 -> D3DBLEND_ONE
    kBlendOne = 1,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 2 -> D3DBLEND_SRCCOLOR
    kBlendSrcColor = 2,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 3 -> D3DBLEND_INVSRCCOLOR
    kBlendInvSrcColor = 3,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 4 -> D3DBLEND_SRCALPHA
    kBlendSrcAlpha = 4,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 5 -> D3DBLEND_INVSRCALPHA
    kBlendInvSrcAlpha = 5,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 6 -> D3DBLEND_DESTALPHA
    kBlendDestAlpha = 6,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 7 -> D3DBLEND_INVDESTALPHA
    kBlendInvDestAlpha = 7,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 8 -> D3DBLEND_DESTCOLOR
    kBlendDestColor = 8,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 9 -> D3DBLEND_INVDESTCOLOR
    kBlendInvDestColor = 9,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 10 -> D3DBLEND_SRCALPHASAT
    kBlendSrcAlphaSat = 10,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 11 -> D3DBLEND_BOTHSRCALPHA
    kBlendBothSrcAlpha = 11,
    /// DX7Rasterizer::SetBlending 0x1000b4ea: 12 -> D3DBLEND_BOTHINVSRCALPHA
    kBlendBothInvSrcAlpha = 12,
}

/// GERasterizer::SetZCompare / SetAlphaTestingCompareState functions: DX7Rasterizer::SetZCompare 0x1000c910 and
/// SetAlphaTestingCompareState 0x1000b26c map each to D3DCMP_* (the SDK names).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GECompareFunction {
    /// DX7Rasterizer::SetZCompare 0x1000c910: 0 -> D3DCMP_NEVER
    kCompareNever = 0,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 1 -> D3DCMP_LESS
    kCompareLess = 1,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 2 -> D3DCMP_LESSEQUAL
    kCompareLessEqual = 2,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 3 -> D3DCMP_EQUAL
    kCompareEqual = 3,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 4 -> D3DCMP_NOTEQUAL
    kCompareNotEqual = 4,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 5 -> D3DCMP_GREATEREQUAL
    kCompareGreaterEqual = 5,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 6 -> D3DCMP_GREATER
    kCompareGreater = 6,
    /// DX7Rasterizer::SetZCompare 0x1000c910: 7 -> D3DCMP_ALWAYS
    kCompareAlways = 7,
}

/// GERasterizer::SetCulling / mCulling: DX7Rasterizer::SetCulling 0x1000b7e7 maps 1 -> D3DCULL_CW, 2 -> D3DCULL_CCW, anything else -> D3DCULL_NONE.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GECullMode {
    /// DX7Rasterizer::SetCulling 0x1000b7e7: default -> D3DCULL_NONE
    kCullNone = 0,
    /// DX7Rasterizer::SetCulling 0x1000b7e7: 1 -> D3DCULL_CW
    kCullCW = 1,
    /// DX7Rasterizer::SetCulling 0x1000b7e7: 2 -> D3DCULL_CCW
    kCullCCW = 2,
}

/// GERasterizer::SetDrawStyle: DX7Rasterizer::SetDrawStyle 0x1000b916 maps each to D3DFILL_*.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEDrawStyle {
    /// DX7Rasterizer::SetDrawStyle 0x1000b916: 0 -> D3DFILL_SOLID
    kDrawStyleSolid = 0,
    /// DX7Rasterizer::SetDrawStyle 0x1000b916: 1 -> D3DFILL_WIREFRAME
    kDrawStyleWireframe = 1,
    /// DX7Rasterizer::SetDrawStyle 0x1000b916: 2 -> D3DFILL_POINT
    kDrawStylePoint = 2,
}

/// GETStage::mMagFilterMode: DX7Rasterizer::SetTexture maps each to D3DTFG_*.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEMagFilter {
    /// DX7Rasterizer::SetTexture: 0 -> D3DTFG_POINT
    kMagFilterPoint = 0,
    /// DX7Rasterizer::SetTexture: 1 -> D3DTFG_LINEAR
    kMagFilterLinear = 1,
    /// DX7Rasterizer::SetTexture: 2 -> D3DTFG_FLATCUBIC
    kMagFilterFlatCubic = 2,
}

/// GETStage::mMipFilterMode (GETexture::SetStageMipFilterMode, GETextureManager::UpdateMIPFilterMode): DX7Rasterizer::SetTexture maps each to D3DTFP_*.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEMipMapFilter {
    /// DX7Rasterizer::SetTexture: 0 -> D3DTFP_NONE
    kMipFilterNone = 0,
    /// DX7Rasterizer::SetTexture: 1 -> D3DTFP_POINT
    kMipFilterPoint = 1,
    /// DX7Rasterizer::SetTexture: 2 -> D3DTFP_LINEAR
    kMipFilterLinear = 2,
}

/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEPixelFormatID {
    /// DX7Screen gPixelFormats[0] RGB|ALPHAPIXELS 32: 00ff0000/0000ff00/000000ff/ff000000 (provisional name)
    kPixelFormatA8R8G8B8 = 0,
    /// DX7Screen gPixelFormats[1] RGB 32: 00ff0000/0000ff00/000000ff (provisional name)
    kPixelFormatX8R8G8B8 = 1,
    /// DX7Screen gPixelFormats[2] RGB|ALPHAPIXELS 16: 0f00/00f0/000f/f000 (provisional name)
    kPixelFormatA4R4G4B4 = 2,
    /// DX7Screen gPixelFormats[3] RGB|ALPHAPIXELS 16: 7c00/03e0/001f/8000 (provisional name)
    kPixelFormatA1R5G5B5 = 3,
    /// DX7Screen gPixelFormats[4] RGB|ALPHAPIXELS 16: f800/07c0/003e/0001 (provisional name)
    kPixelFormatR5G5B5A1 = 4,
    /// DX7Screen gPixelFormats[5] RGB 16: 7c00/03e0/001f (provisional name)
    kPixelFormatX1R5G5B5 = 5,
    /// DX7Screen gPixelFormats[6] RGB 16: f800/07c0/003e (provisional name)
    kPixelFormatR5G5B5X1 = 6,
    /// DX7Screen gPixelFormats[7] RGB 16: f800/07e0/001f (provisional name)
    kPixelFormatR5G6B5 = 7,
    /// DX7Screen gPixelFormats[8] FOURCC DXT1 (provisional name)
    kPixelFormatDXT1 = 8,
    /// DX7Screen gPixelFormats[9] ZBUFFER 32 (provisional name)
    kPixelFormatZ32 = 9,
    /// DX7Screen gPixelFormats[10] ZBUFFER 24 (provisional name)
    kPixelFormatZ24 = 10,
    /// DX7Screen gPixelFormats[11] ZBUFFER 16 (provisional name)
    kPixelFormatZ16 = 11,
    /// CreateSurface type 3: tries 0xb (16), then 0xa (24), then 9 (32) (provisional name)
    kPixelFormatZPrefer16 = 12,
    /// CreateSurface type 3: tries 9 (32), then 0xa (24), then 0xb (16) (provisional name)
    kPixelFormatZPrefer32 = 13,
    /// EnumVideoModeCallback: default when no gPixelFormats entry matches the mode (provisional name)
    kPixelFormatUnknown = 14,
}

/// GETStage::mColorMode / mAlphaMode (GETexture::SetStageColorMode / SetStageAlphaMode): DX7Rasterizer::SetTexture
/// 0x1000bec0 sets the stage-0 D3DTSS_COLOROP/ALPHAOP and arguments per mode. Names are provisional from the D3D setup.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEStageOperation {
    /// DX7Rasterizer::SetTexture: 0 -> D3DTOP_SELECTARG2, arg2 D3DTA_DIFFUSE (provisional name)
    kStageOpDiffuse = 0,
    /// DX7Rasterizer::SetTexture: 1 -> D3DTOP_SELECTARG1, arg1 D3DTA_TEXTURE (provisional name)
    kStageOpTexture = 1,
    /// DX7Rasterizer::SetTexture: 2 -> D3DTOP_MODULATE texture x D3DTA_DIFFUSE (provisional name)
    kStageOpModulate = 2,
    /// DX7Rasterizer::SetTexture: 3 -> D3DTOP_MODULATE texture x D3DTA_CURRENT (provisional name)
    kStageOpModulateCurrent = 3,
}

/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GESurfaceType {
    /// DX7Screen::CreateSurface 0x1000d7e4: 0 -> DDSCAPS_PRIMARYSURFACE (provisional name)
    kSurfacePrimary = 0,
    /// DX7Screen::CreateSurface 0x1000d7e4: 1 -> DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY (provisional name)
    kSurfaceOffscreen = 1,
    /// DX7Screen::CreateSurface 0x1000d7e4: 2 -> DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX, back buffer count (provisional name)
    kSurfaceFlippingPrimary = 2,
    /// DX7Screen::CreateSurface 0x1000d7e4: 3 -> DDSCAPS_ZBUFFER (provisional name)
    kSurfaceZBuffer = 3,
    /// DX7Screen::CreateSurface 0x1000d7e4: 4 -> DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY (provisional name)
    kSurfaceSystemMemory = 4,
    /// DX7Screen::CreateSurface 0x1000d7e4: 5 -> DDSCAPS_TEXTURE (provisional name)
    kSurfaceTexture = 5,
    /// DX7Screen::CreateSurface 0x1000d7e4: 6 -> DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP (provisional name)
    kSurfaceMipMappedTexture = 6,
    /// DX7Screen::CreateSurface 0x1000d7e4: 7 -> the attached flip surfaces of a type-2 surface get type 7 (provisional name)
    kSurfaceBackBuffer = 7,
}

/// GETStage::mMinFilterMode: DX7Rasterizer::SetTexture maps each to D3DTFN_*.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GETextureMinFilter {
    /// DX7Rasterizer::SetTexture: 0 -> D3DTFN_POINT
    kMinFilterPoint = 0,
    /// DX7Rasterizer::SetTexture: 1 -> D3DTFN_LINEAR
    kMinFilterLinear = 1,
    /// DX7Rasterizer::SetTexture: 2 -> D3DTFN_ANISOTROPIC
    kMinFilterAnisotropic = 2,
}

/// GERasterizer::SetZBuffer state bits: DX7Rasterizer::SetZBuffer 0x1000c84a writes D3DRENDERSTATE_ZWRITEENABLE = bit 0,
/// D3DRENDERSTATE_ZENABLE = bit 1 or bit 0. Names are provisional.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEZBufferMode {
    /// DX7Rasterizer::SetZBuffer 0: ZENABLE 0, ZWRITEENABLE 0 (provisional name)
    kZBufferDisabled = 0,
    /// DX7Rasterizer::SetZBuffer bit 0: ZWRITEENABLE (and ZENABLE) (provisional name)
    kZBufferWriteOnly = 1,
    /// DX7Rasterizer::SetZBuffer bit 1: ZENABLE without ZWRITEENABLE (provisional name)
    kZBufferTestOnly = 2,
    /// DX7Rasterizer::SetZBuffer bits 0+1 (provisional name)
    kZBufferTestAndWrite = 3,
}

/// GEFont ctor inQuality: the CreateFont quality each case picks
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEFontQuality {
    /// GEFont::GEFont case -> DEFAULT_QUALITY
    kFontQualityDefault = 0,
    /// GEFont::GEFont case -> DRAFT_QUALITY
    kFontQualityDraft = 1,
    /// GEFont::GEFont case -> ANTIALIASED_QUALITY (the only antialiased one)
    kFontQualityAntialiased = 2,
}

/// GEFont's style bits: the GEFont ctor passes them to CreateFont; UIFontTable::Create builds them from the dbfonts flags
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEFontStyle {
    /// GEFont ctor: CreateFont weight FW_BOLD
    kFontStyleBold = 1,
    /// GEFont ctor: CreateFont fdwItalic = (inStyle >> 1) & 1
    kFontStyleItalic = 2,
    /// GEFont ctor: CreateFont fdwUnderline = (inStyle >> 2) & 1
    kFontStyleUnderline = 4,
    /// DX7Rasterizer::Draw (Draw2D.cpp:252) draws the text once more at +1,+1 in black or white (provisional name)
    kFontStyleShadow = 0x10,
}

/// GEModel::SetClipToPlane plane index: GERasterizer::IsOrthographic/IsPerspectiveModelVisible test the viewport plane named here before each call
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEClipPlane {
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetFrontClippingDistance
    kClipPlaneFront = 0,
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetRearClippingDistance
    kClipPlaneRear = 1,
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetTopPlane
    kClipPlaneTop = 2,
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetBottomPlane
    kClipPlaneBottom = 3,
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetLeftPlane
    kClipPlaneLeft = 4,
    /// GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetRightPlane
    kClipPlaneRight = 5,
}

/// GESurface::Load/Save/GetImageFileAttributes inType: the per-format exported member each case calls
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GEImageFileType {
    /// the original's Targa: no longer read or written (RUST_PLAN D9)
    kImageFileTGA = 0,
    /// saved (24-bit, bottom up); never loaded
    kImageFileBMP = 1,
    /// quality 80 (ee_base's jpeg decodes, the jpeg-encoder crate writes: openformats surface_file.rs)
    kImageFileJPG = 2,
    /// two JPEGs (colour, alpha) behind a two-word offset header
    kImageFileJPG2 = 3,
    /// port: loaded only
    kImageFilePNG = 4,
}

/// GETMTile::mWaterInfo (+0x03): bit 0 water, bits 1..4 the shoreline directions (ComputeShorelines 0x1002795f ORs sNeighbours[i].mShoreline), bit 7 space
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GETMWaterInfoFlag {
    /// SetIsWater sets it; ComputeShorelines starts a water tile at 1 (provisional name)
    kWaterInfoWater = 1,
    /// the shoreline bits (UpdateVisibleTerrain: code = (info >> 1) & 0xf) (provisional name)
    kWaterInfoShorelineMask = 0x1e,
    /// every bit but kWaterInfoSpace (GetIsWaterTile, SetIsSpace) (provisional name)
    kWaterInfoWaterMask = 0x7f,
    /// SetIsSpace sets it and counts mSpaceTileCount; GetIsSpaceTile tests it (provisional name)
    kWaterInfoSpace = 0x80,
}

/// GETerrainRenderVertex::mClipFlags (+0x20): DX7Rasterizer::TransformToCamera sets one bit per view-frustum plane the camera-space vertex is outside of
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GETerrainClipFlag {
    /// TransformToCamera: camera-space y <= the near distance (provisional name)
    kVertexClipNear = 1,
    /// TransformToCamera: y >= the far distance (provisional name)
    kVertexClipFar = 2,
    /// TransformToCamera: on or below the viewport's left plane (provisional name)
    kVertexClipLeft = 4,
    /// TransformToCamera: on or below the right plane (provisional name)
    kVertexClipRight = 8,
    /// TransformToCamera: on or below the top plane (provisional name)
    kVertexClipTop = 0x10,
    /// TransformToCamera: on or below the bottom plane (provisional name)
    kVertexClipBottom = 0x20,
}

/// GEText / UITextLabel horizontal text alignment
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GETextAlignment {
    /// UITextLabel::DrawTextA: x = left edge (provisional name)
    kTextAlignLeft = 0,
    /// UITextLabel: centred in the rectangle; DX7Rasterizer::Draw: x -= width / 2 (provisional name)
    kTextAlignCenter = 1,
    /// UITextLabel: x = right - width; DX7Rasterizer::Draw: x -= width (provisional name)
    kTextAlignRight = 2,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_values_are_the_originals() {
        for (value, original) in [
            (GEBlendFactor::kBlendBothInvSrcAlpha as u32, 12),
            (GECompareFunction::kCompareAlways as u32, 7),
            (GEPixelFormatID::kPixelFormatUnknown as u32, 14),
            (GESurfaceType::kSurfaceBackBuffer as u32, 7),
            (GEZBufferMode::kZBufferTestAndWrite as u32, 3),
            (GEFontStyle::kFontStyleShadow as u32, 0x10),
            (GEClipPlane::kClipPlaneRight as u32, 5),
            (GEImageFileType::kImageFilePNG as u32, 4),
            (GETMWaterInfoFlag::kWaterInfoSpace as u32, 0x80),
            (GETerrainClipFlag::kVertexClipBottom as u32, 0x20),
            (GETextAlignment::kTextAlignRight as u32, 2),
        ] {
            assert_eq!(value, original);
        }
        assert_eq!(std::mem::size_of::<GEBlendFactor>(), 4);
    }
}
