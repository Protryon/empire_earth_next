// C:\EEX\Low-Level Engine\Graphics Engine\GEText.h (RECONSTRUCTION; added for the User Interface units).
// A string drawn with a GEFont at a location, in a foreground and a background colour, with its measured extent.
// port (R4.3, sixteenth part): the text is a whole Rust object (ee_ge smallobj.rs; GEText.cpp deleted): its only
// member is the object, the methods forward, and GEObjectHost (GEBitmap.cpp) makes the calls on the string, the font
// and the platform's text functions. The original's implicit copy (which shared the font without a reference) is
// deleted; nothing used it.
#ifndef LLE_GETEXT_H
#define LLE_GETEXT_H

#include "lle/GEFont.h"
#include "lle/GETextAlignment.h"

class LLE_API GEText {
public:
    GEText(GEFont* inFont) : mRust(ee_ge_text_new(&GEObjectHost, this, inFont)) {}
    ~GEText() { ee_ge_text_destroy(this->mRust); }
    GEText(const GEText&) = delete;
    void operator=(const GEText& inText) { ee_ge_text_assign(this->mRust, inText.mRust); }
    const GEColor& GetBgColor() const { return *reinterpret_cast<const GEColor*>(ee_ge_text_color(this->mRust, false)); }
    const GEColor& GetFgColor() const { return *reinterpret_cast<const GEColor*>(ee_ge_text_color(this->mRust, true)); }
    GEFont* GetFont() const { return static_cast<GEFont*>(ee_ge_text_font(this->mRust)); }
    bool GetIsTransparent() const { return ee_ge_text_transparent(this->mRust); }
    const GE2DPoint& GetLocation() const { return *reinterpret_cast<const GE2DPoint*>(ee_ge_text_location(this->mRust)); }
    const UWideString& GetText() const { return *static_cast<const UWideString*>(ee_ge_text_string(this->mRust)); }
    const GERectangle& GetTextExtent() const { return *reinterpret_cast<const GERectangle*>(ee_ge_text_extent_of(this->mRust)); }
    void SetAlignment(unsigned long inAlignment) { ee_ge_text_set_alignment(this->mRust, inAlignment); }
    void SetBgColor(const GEColor& inColor) { *reinterpret_cast<GEColor*>(ee_ge_text_color(this->mRust, false)) = inColor; }
    void SetFgColor(const GEColor& inColor) { *reinterpret_cast<GEColor*>(ee_ge_text_color(this->mRust, true)) = inColor; }
    void SetFont(GEFont* inFont) { ee_ge_text_set_font(this->mRust, inFont); }
    void SetIsTransparent(bool inTransparent) { ee_ge_text_set_transparent(this->mRust, inTransparent); }
    void SetLocation(const GE2DPoint& inLocation) { *reinterpret_cast<GE2DPoint*>(ee_ge_text_location(this->mRust)) = inLocation; }
    void SetLocation(float inX, float inY) { reinterpret_cast<GE2DPoint*>(ee_ge_text_location(this->mRust))->SetXY(inX, inY); }
    void SetText(const UWideString& inText) { ee_ge_text_set_text(this->mRust, &inText); }
    EeGeText* GetRust() const { return this->mRust; }
private:
    EeGeText* mRust;                        // the text (ee_ge smallobj.rs)
};

#endif
