// C:\EEX\Low-Level Engine\Graphics Engine\GELine.h (RECONSTRUCTION). A two-vertex line in a rasterizer vertex array.
// port (R4.3, fifteenth part): the line is a whole Rust object (ee_ge prim.rs; GELine.cpp deleted): its only member is
// the object, the methods forward, and the bitmap's host (GEVertexHost) makes the calls on the vertex array and the
// textures. Lines are drawn as arrays (GERasterizer::Draw(GELine*, count)): sizeof is one pointer.
#ifndef LLE_GELINE_H
#define LLE_GELINE_H

#include "rust/base/math.h"   // the line (ee_ge prim.rs), the host's EeGeBmpOp
#include "rust/render/ge.h"

#include "lle/LLEForward.h"

class LLE_API GELine {
public:
    GELine() : mRust(ee_ge_game_prim_new(this, EeGePrimKind::Line)) {}
    GELine(const GE2DPoint& inStart, const GE2DPoint& inEnd, const GEColor& inStartColor, const GEColor& inEndColor,
           float inOpacity)
        : mRust(ee_ge_game_prim_new_line(this, (const EeGe2DPoint*)&inStart, (const EeGe2DPoint*)&inEnd,
                                    &inStartColor, &inEndColor, inOpacity)) {}
    ~GELine() { ee_ge_prim_destroy(this->mRust); }
    void GetColorPerVertex(GEColor& outStart, GEColor& outEnd) const { this->Vertices(EeGeBmpOp::VertexGetColor, &outStart, &outEnd); }
    void GetOpacityPerVertex(float& outStart, float& outEnd) const
        { ee_ge_prim_opacities(this->mRust, &outStart, &outEnd, NULL, NULL); }
    GETexture* GetTexture(unsigned long inScreen) const { return (GETexture*)ee_ge_prim_texture(this->mRust, inScreen); }
    void GetTextureCoordinatesPerVertex(GETextureCoordinate& outStart, GETextureCoordinate& outEnd) const
        { this->Vertices(EeGeBmpOp::VertexGetUv, &outStart, &outEnd); }
    GEVertexArray* GetVertexArray() { return (GEVertexArray*)ee_ge_prim_vertices(this->mRust); }
    void SetColor(const GEColor& inColor) { this->SetColorPerVertex(inColor, inColor); }
    void SetColorPerVertex(const GEColor& inStart, const GEColor& inEnd) { this->Vertices(EeGeBmpOp::VertexSetColor, &inStart, &inEnd); }
    void SetEndPoint(const GE2DPoint& inPoint) { ee_ge_prim_set_xy(this->mRust, 1, (const EeGe2DPoint*)&inPoint); }
    void SetOpacity(float inOpacity) { this->SetOpacityPerVertex(inOpacity, inOpacity); }
    void SetOpacityPerVertex(float inStart, float inEnd) { ee_ge_prim_set_opacities(this->mRust, 2, inStart, inEnd, 0.0f, 0.0f); }
    void SetStartPoint(const GE2DPoint& inPoint) { ee_ge_prim_set_xy(this->mRust, 0, (const EeGe2DPoint*)&inPoint); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_prim_set_texture(this->mRust, inTexture, inScreen); }
    void SetTextureCoordinates(const GETextureCoordinate& inStart, const GETextureCoordinate& inEnd)
        { this->Vertices(EeGeBmpOp::VertexSetUv, &inStart, &inEnd); }
    void Vertices(EeGeBmpOp inOp, const void* inStart, const void* inEnd) const
        { ee_ge_prim_vertices4(this->mRust, inOp, inStart, inEnd, NULL, NULL); }

    URust<EeGePrim> mRust;                        // the line (ee_ge prim.rs)
};

#endif
