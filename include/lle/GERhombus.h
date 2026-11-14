// C:\EEX\Low-Level Engine\Graphics Engine\GERhombus.h (RECONSTRUCTION). A rectangle drawn as a four-vertex
// rasterizer vertex array, optionally rotated about its centre. GERhombus.cpp (0x10024ea0..0x1002534b) holds the ctor
// and ComputeRotation; the other members are header-inline (GEBoundingBox.cpp emits them first, 0x10011fe3..; the
// exported list: GetRectangle, GetVertexArray, ~GERhombus, operator=, Get/Set{Color,Location,Opacity,Z}PerVertex,
// GetRotation, SetColor, SetOpacity, SetRectangle, SetRotation, SetZ). Only what GERhombus.cpp needs is declared so far.
#ifndef LLE_GERHOMBUS_H
#define LLE_GERHOMBUS_H

#include "lle/GEVertexArray.h"   // the inline per-vertex members call through mVertices

class LLE_API GERhombus {                   // sizeof 0x18
public:
    GERhombus();
    ~GERhombus() { delete this->mVertices; }  // 0x10011fe3 (ICF-folded with ~NEPlayer)
    const GERectangle& GetRectangle() const { return this->mRectangle; }
    GEVertexArray* GetVertexArray() { return this->mVertices; }
    // the members below are inline in the real header: GEBoundingBox.cpp emits them first (0x10011ff0..0x10012426,
    // declaration order); the User Interface units call them out of line
    GERhombus& operator=(const GERhombus& inRhombus)
    {
        this->mVertices->CopyFrom(*inRhombus.mVertices);
        this->mRectangle = inRhombus.mRectangle;
        this->mRotation = inRhombus.mRotation;
        return *this;
    }
    void GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                           GEColor& outBottomLeft) const
    {
        this->mVertices->GetColor(outTopLeft, 0);
        this->mVertices->GetColor(outTopRight, 1);
        this->mVertices->GetColor(outBottomRight, 2);
        this->mVertices->GetColor(outBottomLeft, 3);
    }
    void GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                             float& outBottomLeft) const
    {
        outTopLeft = this->mVertices->GetOpacity(0);
        outTopRight = this->mVertices->GetOpacity(1);
        outBottomRight = this->mVertices->GetOpacity(2);
        outBottomLeft = this->mVertices->GetOpacity(3);
    }
    void SetColor(const GEColor& inColor)
    {
        this->mVertices->SetColor(inColor, 0);
        this->mVertices->SetColor(inColor, 1);
        this->mVertices->SetColor(inColor, 2);
        this->mVertices->SetColor(inColor, 3);
    }
    void SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                           const GEColor& inBottomLeft)
    {
        this->mVertices->SetColor(inTopLeft, 0);
        this->mVertices->SetColor(inTopRight, 1);
        this->mVertices->SetColor(inBottomRight, 2);
        this->mVertices->SetColor(inBottomLeft, 3);
    }
    void SetOpacity(float inOpacity)
    {
        this->mVertices->SetOpacity(inOpacity, 0);
        this->mVertices->SetOpacity(inOpacity, 1);
        this->mVertices->SetOpacity(inOpacity, 2);
        this->mVertices->SetOpacity(inOpacity, 3);
    }
    void SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
    {
        this->mVertices->SetOpacity(inTopLeft, 0);
        this->mVertices->SetOpacity(inTopRight, 1);
        this->mVertices->SetOpacity(inBottomRight, 2);
        this->mVertices->SetOpacity(inBottomLeft, 3);
    }
    void SetRectangle(const GERectangle& inRectangle) { this->mRectangle = inRectangle; this->ComputeRotation(); }  // UIEditBox::DrawPolygons expands it
    void SetRotation(float inRotation) { this->mRotation = inRotation; this->ComputeRotation(); }
    void SetZ(float inZ) { this->SetZPerVertex(inZ, inZ, inZ, inZ); }
    void SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
    {
        GE3DPoint theTopLeft;
        GE3DPoint theTopRight;
        GE3DPoint theBottomRight;
        GE3DPoint theBottomLeft;
        this->mVertices->GetPoint(theTopLeft, 0);
        this->mVertices->GetPoint(theTopRight, 1);
        this->mVertices->GetPoint(theBottomRight, 2);
        this->mVertices->GetPoint(theBottomLeft, 3);
        theTopLeft.SetZ(inTopLeft);
        theTopRight.SetZ(inTopRight);
        theBottomRight.SetZ(inBottomRight);
        theBottomLeft.SetZ(inBottomLeft);
        this->mVertices->SetPoint(theTopLeft, 0);
        this->mVertices->SetPoint(theTopRight, 1);
        this->mVertices->SetPoint(theBottomRight, 2);
        this->mVertices->SetPoint(theBottomLeft, 3);
    }
private:
    void ComputeRotation();

    GEVertexArray* mVertices;               // +0x00 four vertices: top left, top right, bottom left, bottom right
    GERectangle mRectangle;                 // +0x04
    float mRotation;                        // +0x14 radians, about the rectangle's centre
};

#endif
