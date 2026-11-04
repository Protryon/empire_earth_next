// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.h (RECONSTRUCTION). A textured quad: four vertices in a
// rasterizer vertex array, one texture per screen. GEBitmap.cpp 0x1000f3a4..0x1000105c.
#ifndef LLE_GEBITMAP_H
#define LLE_GEBITMAP_H

class GETexture;
class GEVertexArray;

class LLE_API GEBitmap : public UReferenceCounter {   // sizeof 0x30 (vector deleting dtor 0xf923)
public:
    GEBitmap();
    void ComputeStretchedCoordsAndScale(const GERectangle& inTextureRectangle, const GERectangle& inSourceRectangle,
                                        const GERectangle& inDestinationRectangle, unsigned long inScreen);
    void FlipHorizontally();
    void FlipVertically();
    void GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                           GEColor& outBottomLeft) const;
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports it (EESETriggerScreen ctor 0x57b0b7 expands it)
    float GetHeight() const { return this->mYScale * this->mHeight; }
#else
    float GetHeight() const;
#endif
    const GE2DPoint& GetLocation() const { return this->mLocation; }  // exported, ICF-folded (0x38b41)
    void GetLocationPerVertex(GE2DPoint& outTopLeft, GE2DPoint& outTopRight, GE2DPoint& outBottomRight,
                              GE2DPoint& outBottomLeft) const;
    float GetRotation() const { return this->mRotation; }  // exported, ICF-folded (0x4c369)
    void GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                             float& outBottomLeft) const;
    GETexture* GetTexture(unsigned long inScreen) const { return this->mTextures[inScreen]; }  // 0xf4eb; the UI expands it
    // Provisional name (GELine/GEPoint/GERhombus have it); DX7Rasterizer::Draw(GEBitmap*) 0x10002c62 reads +0x10
    GEVertexArray* GetVertexArray() { return this->mVertices; }
    void GetTextureCoordinatesPerVertex(GETextureCoordinate& outTopLeft, GETextureCoordinate& outTopRight,
                                        GETextureCoordinate& outBottomRight, GETextureCoordinate& outBottomLeft) const;
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports it and expands it (EEUIMiniMap::CopyData 0x275585)
    float GetWidth() const { return this->mXScale * this->mWidth; }
#else
    float GetWidth() const;
#endif
    float GetXScale() const { return this->mXScale; }  // exported (ICF-folded 0x100112ce); the UI reads +0x24 directly
    float GetYScale() const { return this->mYScale; }  // 0xf53e; the UI reads +0x28 directly
    void GetZPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight, float& outBottomLeft) const;
    void SetColor(const GEColor& inColor);
    void SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                           const GEColor& inBottomLeft);
    void SetLocation(const GE2DPoint& inLocation) { this->mLocation = inLocation; this->ComputeRotation(); }  // 0xf65c; the UI expands it
    void SetOpacity(float inOpacity);
    void SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft);
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports ?SetRotation@GEBitmap@@QAEXM@Z and expands it (store +0x20,
    // call ComputeRotation: EEUIFSEBRotation::ButtonUp 0x59a574)
    void SetRotation(float inRotation) { this->mRotation = inRotation; this->ComputeRotation(); }
#else
    void SetRotation(float inRotation);
#endif
    void SetScale(float inScale);
    void SetTexture(GETexture* inTexture, unsigned long inScreen);
    void SetTextureCoordinates(GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                               GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void SetTextureCoordinates(const GETextureCoordinate& inTopLeft, const GETextureCoordinate& inTopRight,
                               const GETextureCoordinate& inBottomRight, const GETextureCoordinate& inBottomLeft)
        { SetTextureCoordinates(const_cast<GETextureCoordinate&>(inTopLeft), const_cast<GETextureCoordinate&>(inTopRight), const_cast<GETextureCoordinate&>(inBottomRight), const_cast<GETextureCoordinate&>(inBottomLeft)); }
#endif
    void SetXYScale(float inXScale, float inYScale) { this->mXScale = inXScale; this->mYScale = inYScale; this->ComputeRotation(); }  // 0xf771; the UI expands it
    void SetZ(float inZ);
    void SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft);
private:
    virtual ~GEBitmap();
    void ComputeRotation();

    GE2DPoint mLocation;                    // +0x08
    GEVertexArray* mVertices;               // +0x10
    GETexture** mTextures;                  // +0x14 one per screen
    float mHeight;                          // +0x18 (texture frame height)
    float mWidth;                           // +0x1c
    float mRotation;                        // +0x20
    float mXScale;                          // +0x24
    float mYScale;                          // +0x28
    unsigned long mTextureCount;            // +0x2c (the active rasterizer's screen count)
};

#endif
