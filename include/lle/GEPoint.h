// C:\EEX\Low-Level Engine\Graphics Engine\GEPoint.h (RECONSTRUCTION). A one-vertex point in a rasterizer vertex array.
// port (R4.3, fifteenth part): a whole Rust object (ee_ge prim.rs, as GELine; GEPoint.cpp deleted).
#ifndef LLE_GEPOINT_H
#define LLE_GEPOINT_H
#include "lle/LLEForward.h"

#include "lle/GELine.h"   // ee_ge prim.rs

class LLE_API GEPoint {
public:
    GEPoint() : mRust(ee_ge_game_prim_new(this, EeGePrimKind::Point)) {}
    GEPoint(const GE2DPoint& inPoint, const GEColor& inColor, float inOpacity)
        : mRust(ee_ge_game_prim_new_point(this, (const EeGe2DPoint*)&inPoint, &inColor, inOpacity)) {}
    ~GEPoint() { ee_ge_prim_destroy(this->mRust); }
    void GetColor(GEColor& outColor) const { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexGetColor, 0, &outColor); }
    float GetOpacity() const { return ee_ge_prim_opacity(this->mRust, 0); }
    GETexture* GetTexture(unsigned long inScreen) const { return (GETexture*)ee_ge_prim_texture(this->mRust, inScreen); }
    void SetColor(const GEColor& inColor) { ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexSetColor, 0, (void*)&inColor); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_prim_set_texture(this->mRust, inTexture, inScreen); }
    URust<EeGePrim> mRust;                        // the point (ee_ge prim.rs)
};

#endif
