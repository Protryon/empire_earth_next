// C:\EEX\Low-Level Engine\Graphics Engine\GELine.h (RECONSTRUCTION). A two-vertex line in a rasterizer vertex array.
// port (R4.3, fifteenth part): the line is a whole Rust object (ee_ge prim.rs; GELine.cpp deleted): its only member is
// the object, the methods forward, and the bitmap's host (GEVertexHost) makes the calls on the vertex array and the
// textures. Lines are drawn as arrays (GERasterizer::Draw(GELine*, count)): sizeof is one pointer.
#ifndef LLE_GELINE_H
#define LLE_GELINE_H

#include "rust/ee_ge.h"   // the line (ee_ge prim.rs), the host's EeGeBmpOp

class GE2DPoint;
class GEColor;
class GERectangle;
class GETexture;
class GETextureCoordinate;
class GEVertexArray;
LLE_API intptr_t GEVertexHost(void* inOwner, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b);   // GEBitmap.cpp

class LLE_API GELine {
public:
    GELine() : mRust(ee_ge_prim_new(&GEVertexHost, this, EeGePrimKind::Line)) {}
    GELine(const GE2DPoint& inStart, const GE2DPoint& inEnd, const GEColor& inStartColor, const GEColor& inEndColor,
           float inOpacity)
        : mRust(ee_ge_prim_new_line(&GEVertexHost, this, (const EeGe2DPoint*)&inStart, (const EeGe2DPoint*)&inEnd,
                                    &inStartColor, &inEndColor, inOpacity)) {}
    ~GELine() { ee_ge_prim_destroy(this->mRust); }
    GELine(const GELine&) = delete;
    GELine& operator=(const GELine&) = delete;
    void GetColorPerVertex(GEColor& outStart, GEColor& outEnd) const
        { this->Vertex(EeGeBmpOp::VertexGetColor, 0, &outStart); this->Vertex(EeGeBmpOp::VertexGetColor, 1, &outEnd); }
    void GetOpacityPerVertex(float& outStart, float& outEnd) const
        { outStart = ee_ge_prim_opacity(this->mRust, 0); outEnd = ee_ge_prim_opacity(this->mRust, 1); }
    GETexture* GetTexture(unsigned long inScreen) const { return (GETexture*)ee_ge_prim_texture(this->mRust, inScreen); }
    void GetTextureCoordinatesPerVertex(GETextureCoordinate& outStart, GETextureCoordinate& outEnd) const
        { this->Vertex(EeGeBmpOp::VertexGetUv, 0, &outStart); this->Vertex(EeGeBmpOp::VertexGetUv, 1, &outEnd); }
    GEVertexArray* GetVertexArray() { return (GEVertexArray*)ee_ge_prim_vertices(this->mRust); }
    void SetColor(const GEColor& inColor) { this->SetColorPerVertex(inColor, inColor); }
    void SetColorPerVertex(const GEColor& inStart, const GEColor& inEnd)
        { this->Vertex(EeGeBmpOp::VertexSetColor, 0, &inStart); this->Vertex(EeGeBmpOp::VertexSetColor, 1, &inEnd); }
    void SetEndPoint(const GE2DPoint& inPoint) { ee_ge_prim_set_xy(this->mRust, 1, (const EeGe2DPoint*)&inPoint); }
    void SetOpacity(float inOpacity) { this->SetOpacityPerVertex(inOpacity, inOpacity); }
    void SetOpacityPerVertex(float inStart, float inEnd)
        { ee_ge_prim_set_opacity(this->mRust, 0, inStart); ee_ge_prim_set_opacity(this->mRust, 1, inEnd); }
    void SetStartPoint(const GE2DPoint& inPoint) { ee_ge_prim_set_xy(this->mRust, 0, (const EeGe2DPoint*)&inPoint); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_prim_set_texture(this->mRust, inTexture, inScreen); }
    void SetTextureCoordinates(const GETextureCoordinate& inStart, const GETextureCoordinate& inEnd)
        { this->Vertex(EeGeBmpOp::VertexSetUv, 0, &inStart); this->Vertex(EeGeBmpOp::VertexSetUv, 1, &inEnd); }
private:
    void Vertex(EeGeBmpOp inOp, unsigned long inIndex, const void* inArg) const
        { ee_ge_prim_vertex(this->mRust, inOp, inIndex, const_cast<void*>(inArg)); }

    EeGePrim* mRust;                        // the line (ee_ge prim.rs)
};

#endif
