// C:\EEX\Low-Level Engine\Graphics Engine\GELine.h (RECONSTRUCTION). A two-vertex line in a rasterizer vertex array.
// GELine.cpp (~0x100167f3) holds the ctors, dtor and SetTexture; the rest is inline (GEBoundingBox.cpp emits it first,
// 0x10010af2..).
#ifndef LLE_GELINE_H
#define LLE_GELINE_H

class GETexture;

class LLE_API GELine {                      // sizeof 0xc (implicit operator=, ICF-folded at 0x12ad2: 3 x movsd)
public:
    GELine();
    GELine(GE2DPoint& inStart, GE2DPoint& inEnd, GEColor& inStartColor, GEColor& inEndColor, float inOpacity);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    GELine(const GE2DPoint& inStart, const GE2DPoint& inEnd, const GEColor& inStartColor, const GEColor& inEndColor,
           float inOpacity)
        : GELine(const_cast<GE2DPoint&>(inStart), const_cast<GE2DPoint&>(inEnd), const_cast<GEColor&>(inStartColor), const_cast<GEColor&>(inEndColor), inOpacity) {}
    void SetEndPoint(const GE2DPoint& inPoint) { SetEndPoint(const_cast<GE2DPoint&>(inPoint)); }
    void SetStartPoint(const GE2DPoint& inPoint) { SetStartPoint(const_cast<GE2DPoint&>(inPoint)); }
#endif
    ~GELine();
    void GetColorPerVertex(GEColor& outStart, GEColor& outEnd) const
    {
        this->mVertices->GetColor(outStart, 0);
        this->mVertices->GetColor(outEnd, 1);
    }
    void GetEndPoint(GE2DPoint& outPoint) const
    {
        GE3DPoint thePoint;
        this->mVertices->GetPoint(thePoint, 1);
        outPoint.SetX(thePoint.GetX());
        outPoint.SetY(thePoint.GetY());
    }
    void GetOpacityPerVertex(float& outStart, float& outEnd) const
    {
        outStart = this->mVertices->GetOpacity(0);
        outEnd = this->mVertices->GetOpacity(1);
    }
    void GetStartPoint(GE2DPoint& outPoint) const
    {
        GE3DPoint thePoint;
        this->mVertices->GetPoint(thePoint, 0);
        outPoint.SetX(thePoint.GetX());
        outPoint.SetY(thePoint.GetY());
    }
    GETexture* GetTexture(unsigned long inScreen) const { return this->mTextures[inScreen]; }
    void GetTextureCoordinatesPerVertex(GETextureCoordinate& outStart, GETextureCoordinate& outEnd) const
    {
        this->mVertices->GetTextureCoordinate(outStart, 0);
        this->mVertices->GetTextureCoordinate(outEnd, 1);
    }
    GEVertexArray* GetVertexArray() { return this->mVertices; }
    void SetColor(const GEColor& inColor)
    {
        this->mVertices->SetColor(inColor, 0);
        this->mVertices->SetColor(inColor, 1);
    }
    void SetColorPerVertex(const GEColor& inStart, const GEColor& inEnd)
    {
        this->mVertices->SetColor(inStart, 0);
        this->mVertices->SetColor(inEnd, 1);
    }
    void SetEndPoint(GE2DPoint& inPoint)
    {
        GE3DPoint thePoint;
        thePoint.SetX(inPoint.GetX());
        thePoint.SetY(inPoint.GetY());
        this->mVertices->SetPoint(thePoint, 1);
    }
    void SetOpacity(float inOpacity)
    {
        this->mVertices->SetOpacity(inOpacity, 0);
        this->mVertices->SetOpacity(inOpacity, 1);
    }
    void SetOpacityPerVertex(float inStart, float inEnd)
    {
        this->mVertices->SetOpacity(inStart, 0);
        this->mVertices->SetOpacity(inEnd, 1);
    }
    void SetStartPoint(GE2DPoint& inPoint)
    {
        GE3DPoint thePoint;
        thePoint.SetX(inPoint.GetX());
        thePoint.SetY(inPoint.GetY());
        this->mVertices->SetPoint(thePoint, 0);
    }
    void SetTexture(GETexture* inTexture, unsigned long inScreen);
    void SetTextureCoordinates(GETextureCoordinate& inStart, GETextureCoordinate& inEnd)
    {
        this->mVertices->SetTextureCoordinate(inStart, 0);
        this->mVertices->SetTextureCoordinate(inEnd, 1);
    }
private:
    GEVertexArray* mVertices;               // +0x00
    GETexture** mTextures;                  // +0x04 one per screen
    unsigned long mScreenCount;             // +0x08 proven by the ctors (GetScreenCount) and the dtor loop (0x16a11)
};

#endif
