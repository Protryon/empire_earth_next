// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.h (RECONSTRUCTION). A textured quad: four vertices in a
// rasterizer vertex array, one texture per screen. GEBitmap.cpp 0x1000f3a4..0x1000105c.
//
// port (R4.3, ninth part): the bitmap is a whole Rust object (ee_ge bmpobj.rs): its location, frame size, rotation,
// scales, vertex array and textures with every method. The class's only member is that object; every member below
// forwards to it (inline since the HUD 2D lane, round 1), and GEBitmap.cpp keeps the host (GEVertexHost: its calls on
// other objects).
// The original's exe view expanded GetHeight, GetWidth and SetRotation inline (reading +0x18..+0x28): one inline
// forwarder serves both views now.
#ifndef LLE_GEBITMAP_H
#define LLE_GEBITMAP_H

#include <stdint.h>
#include "rust/base/math.h"   // the bitmap (ee_ge bmpobj.rs), its host's EeGeBmpOp
#include "rust/render/ge.h"

class GETexture;
class GEVertexArray;
class GETextureCoordinate;

class LLE_API GEBitmap : public UReferenceCounter {   // sizeof 0x30 (vector deleting dtor 0xf923)
public:
    GEBitmap() : mRust(ee_ge_game_bmp_new(this)) {}   // white, unscaled, one texture slot per screen
    void ComputeStretchedCoordsAndScale(const GERectangle& inTextureRectangle, const GERectangle& inSourceRectangle,
                                        const GERectangle& inDestinationRectangle, unsigned long inScreen)
        { ee_ge_bmp_stretch(this->mRust, R(inTextureRectangle), R(inSourceRectangle), R(inDestinationRectangle), inScreen); }
    void GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                           GEColor& outBottomLeft) const
        { ee_ge_bmp_quad(this->mRust, EeGeBmpQuadCall::Colors, &outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft); }
    float GetHeight() const { return ee_ge_bmp_getf(this->mRust, EeGeBmpFloat::Height); }
    const GE2DPoint& GetLocation() const { return *(const GE2DPoint*)ee_ge_bmp_location(this->mRust); }  // exported, ICF-folded (0x38b41)
    void GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                             float& outBottomLeft) const
        { ee_ge_bmp_quad(this->mRust, EeGeBmpQuadCall::Opacities, &outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft); }
    GETexture* GetTexture(unsigned long inScreen) const { return (GETexture*)ee_ge_bmp_texture(this->mRust, inScreen); }
    // Provisional name (GELine/GEPoint/GERhombus have it); DX7Rasterizer::Draw(GEBitmap*) 0x10002c62 reads +0x10
    GEVertexArray* GetVertexArray() { return (GEVertexArray*)ee_ge_bmp_vertices(this->mRust); }
    void GetTextureCoordinatesPerVertex(GETextureCoordinate& outTopLeft, GETextureCoordinate& outTopRight,
                                        GETextureCoordinate& outBottomRight, GETextureCoordinate& outBottomLeft) const
        { ee_ge_bmp_quad(this->mRust, EeGeBmpQuadCall::Uvs, &outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft); }
    float GetWidth() const { return ee_ge_bmp_getf(this->mRust, EeGeBmpFloat::Width); }
    float GetXScale() const { return ee_ge_bmp_getf(this->mRust, EeGeBmpFloat::XScale); }  // exported (ICF-folded 0x100112ce)
    float GetYScale() const { return ee_ge_bmp_getf(this->mRust, EeGeBmpFloat::YScale); }
    void SetColor(const GEColor& inColor) { this->SetColorPerVertex(inColor, inColor, inColor, inColor); }
    void SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                           const GEColor& inBottomLeft)
        { ee_ge_bmp_quad(this->mRust, EeGeBmpQuadCall::SetColors, &inTopLeft, &inTopRight, &inBottomRight, &inBottomLeft); }
    void SetLocation(const GE2DPoint& inLocation) { ee_ge_bmp_set_location(this->mRust, (const EeGe2DPoint*)&inLocation); }
    void SetOpacity(float inOpacity) { this->SetOpacityPerVertex(inOpacity, inOpacity, inOpacity, inOpacity); }
    void SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
        { ee_ge_bmp_set_values(this->mRust, false, inTopLeft, inTopRight, inBottomRight, inBottomLeft); }
    void SetRotation(float inRotation) { ee_ge_bmp_set_rotation(this->mRust, inRotation); }
    void SetScale(float inScale) { this->SetXYScale(inScale, inScale); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_bmp_set_texture(this->mRust, inTexture, inScreen); }
    void SetTextureCoordinates(GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                               GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft)
        { ee_ge_bmp_quad(this->mRust, EeGeBmpQuadCall::SetUvs, &inTopLeft, &inTopRight, &inBottomRight, &inBottomLeft); }
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void SetTextureCoordinates(const GETextureCoordinate& inTopLeft, const GETextureCoordinate& inTopRight,
                               const GETextureCoordinate& inBottomRight, const GETextureCoordinate& inBottomLeft)
        { SetTextureCoordinates(const_cast<GETextureCoordinate&>(inTopLeft), const_cast<GETextureCoordinate&>(inTopRight), const_cast<GETextureCoordinate&>(inBottomRight), const_cast<GETextureCoordinate&>(inBottomLeft)); }
#endif
    void SetXYScale(float inXScale, float inYScale) { ee_ge_bmp_set_xy_scale(this->mRust, inXScale, inYScale); }
    void SetZ(float inZ) { this->SetZPerVertex(inZ, inZ, inZ, inZ); }
    void SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
        { ee_ge_bmp_set_values(this->mRust, true, inTopLeft, inTopRight, inBottomRight, inBottomLeft); }
    GEBitmap(const GEBitmap&) = delete;
    GEBitmap& operator=(const GEBitmap&) = delete;
    EeGeBitmap* GetRust() const { return this->mRust; }
private:
    virtual ~GEBitmap() { ee_ge_bmp_destroy(this->mRust); }   // the vertex array deleted, every screen's texture released
    static const EeGeRectangle* R(const GERectangle& inRectangle) { return reinterpret_cast<const EeGeRectangle*>(&inRectangle); }
    // (ComputeRotation 0x1000fbb9 is the Rust object's; the calls on other objects are GEVertexHost's, lle/GELine.h)

    EeGeBitmap* mRust;                      // the bitmap (ee_ge bmpobj.rs)
};

#endif
