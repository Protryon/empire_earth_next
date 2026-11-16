// C:\EEX\Low-Level Engine\Graphics Engine\GEPoint.h (RECONSTRUCTION). A one-vertex point in a rasterizer vertex array.
// port (R4.3, fifteenth part): a whole Rust object (ee_ge prim.rs, as GELine; GEPoint.cpp deleted).
#ifndef LLE_GEPOINT_H
#define LLE_GEPOINT_H

#include "lle/GELine.h"   // ee_ge prim.rs, GEVertexHost

class LLE_API GEPoint {
public:
    GEPoint() : mRust(ee_ge_prim_new(&GEVertexHost, this, EeGePrimKind::Point)) {}
    GEPoint(const GE2DPoint& inPoint, const GEColor& inColor, float inOpacity)
        : mRust(ee_ge_prim_new_point(&GEVertexHost, this, (const EeGe2DPoint*)&inPoint, &inColor, inOpacity)) {}
    ~GEPoint() { ee_ge_prim_destroy(this->mRust); }
    GEPoint(const GEPoint&) = delete;
    GEPoint& operator=(const GEPoint&) = delete;
    void GetColor(GEColor& outColor) const { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexGetColor, 0, &outColor); }
    float GetOpacity() const { return ee_ge_prim_opacity(this->mRust, 0); }
    void GetPoint(GE2DPoint& outPoint) const { ee_ge_prim_xy(this->mRust, 0, (EeGe2DPoint*)&outPoint); }
    GETexture* GetTexture(unsigned long inScreen) const { return (GETexture*)ee_ge_prim_texture(this->mRust, inScreen); }
    void GetTextureCoordinate(GETextureCoordinate& outCoordinate)
        { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexGetUv, 0, &outCoordinate); }
    GEVertexArray* GetVertexArray() { return (GEVertexArray*)ee_ge_prim_vertices(this->mRust); }
    void SetColor(const GEColor& inColor) { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexSetColor, 0, (void*)&inColor); }
    void SetOpacity(float inOpacity) { ee_ge_prim_set_opacity(this->mRust, 0, inOpacity); }
    void SetPoint(const GE2DPoint& inPoint) { ee_ge_prim_set_xy(this->mRust, 0, (const EeGe2DPoint*)&inPoint); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_prim_set_texture(this->mRust, inTexture, inScreen); }
    void SetTextureCoordinate(const GETextureCoordinate& inCoordinate)
        { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexSetUv, 0, (void*)&inCoordinate); }
private:
    EeGePrim* mRust;                        // the point (ee_ge prim.rs)
};

#endif
