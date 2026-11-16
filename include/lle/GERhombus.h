// C:\EEX\Low-Level Engine\Graphics Engine\GERhombus.h (RECONSTRUCTION). A rectangle drawn as a four-vertex
// rasterizer vertex array, optionally rotated about its centre.
// port (R4.3, fifteenth part): a whole Rust object (ee_ge prim.rs, as GELine; GERhombus.cpp deleted). The vertices
// are top left, top right, bottom left, bottom right; the corners are ee_ge bitmap.rs's.
#ifndef LLE_GERHOMBUS_H
#define LLE_GERHOMBUS_H

#include "lle/GELine.h"   // ee_ge prim.rs, GEVertexHost

class LLE_API GERhombus {
public:
    GERhombus() : mRust(ee_ge_prim_new(&GEVertexHost, this, EeGePrimKind::Rhombus)) {}
    ~GERhombus() { ee_ge_prim_destroy(this->mRust); }
    GERhombus(const GERhombus&) = delete;
    const GERectangle& GetRectangle() const { return *(const GERectangle*)ee_ge_prim_rectangle(this->mRust); }
    GEVertexArray* GetVertexArray() { return (GEVertexArray*)ee_ge_prim_vertices(this->mRust); }
    GERhombus& operator=(const GERhombus& inRhombus) { ee_ge_prim_assign(this->mRust, inRhombus.mRust); return *this; }
    void GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                           GEColor& outBottomLeft) const
    {
        GEColor* const theOut[4] = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
        for (unsigned long i = 0; i < 4; i++)
            ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexGetColor, i, theOut[i]);
    }
    void GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight, float& outBottomLeft) const
    {
        float* const theOut[4] = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
        for (unsigned long i = 0; i < 4; i++)
            *theOut[i] = ee_ge_prim_opacity(this->mRust, i);
    }
    void SetColor(const GEColor& inColor) { this->SetColorPerVertex(inColor, inColor, inColor, inColor); }
    void SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                           const GEColor& inBottomLeft)
    {
        const GEColor* const theColors[4] = {&inTopLeft, &inTopRight, &inBottomRight, &inBottomLeft};
        for (unsigned long i = 0; i < 4; i++)
            ee_ge_prim_vertex(this->mRust, EeGeBmpOp::VertexSetColor, i, (void*)theColors[i]);
    }
    void SetOpacity(float inOpacity) { this->SetOpacityPerVertex(inOpacity, inOpacity, inOpacity, inOpacity); }
    void SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
    {
        const float theOpacities[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
        for (unsigned long i = 0; i < 4; i++)
            ee_ge_prim_set_opacity(this->mRust, i, theOpacities[i]);
    }
    void SetRectangle(const GERectangle& inRectangle) { ee_ge_prim_place(this->mRust, (const EeGeRectangle*)&inRectangle, 0.0f); }
    void SetRotation(float inRotation) { ee_ge_prim_place(this->mRust, NULL, inRotation); }
    void SetZ(float inZ) { this->SetZPerVertex(inZ, inZ, inZ, inZ); }
    void SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
    {
        const float theDepths[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
        ee_ge_prim_set_depths(this->mRust, &theDepths);
    }
private:
    EeGePrim* mRust;                        // the rhombus (ee_ge prim.rs)
};

#endif
