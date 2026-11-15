// C:\EEX\Low-Level Engine\Graphics Engine\GERectangle.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100246a7..0x10024ea0: the four static UString IDs (one shared guard byte, bits 1/2/4/8), And x2, Load,
// RemoveIntersectingRectangle, Save, then the vector<GERectangle> template COMDATs. The rest of GERectangle is
// header-inline (GEBitmap.cpp emits it first). No U2D keys: the unit includes no U2D header.
// And and RemoveIntersectingRectangle forward to Rust (rust/crates/ee_ge bbox.rs; GERustMath.h); the file I/O stays.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/FSFile.h"
#include "lle/GERectangle.h"
#include "GERustMath.h"
#include "GERustIO.h"

const UString GERectangle::sLeftID("GERectangle-Left");
const UString GERectangle::sTopID("GERectangle-Top");
const UString GERectangle::sRightID("GERectangle-Right");
const UString GERectangle::sBottomID("GERectangle-Bottom");

// Clips the rectangle to another one; returns false and leaves it unchanged when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle)
{
    return ee_ge_rect_and_in_place(GERust(*this), GERust(inRectangle));
}

// Writes the overlap of two rectangles to outRectangle; returns false (output untouched) when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle, GERectangle& outRectangle) const
{
    return ee_ge_rect_and(GERust(*this), GERust(inRectangle), GERust(outRectangle));
}

// Reads the left, top, right and bottom edges from a file; returns the first read error (or not-open)
long GERectangle::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mLeft, &this->mTop, &this->mRight, &this->mBottom};
    return GERustFields(inFile, theFields, 4, inFile.GetIsOpen(), false, false);
}

// Writes the left, top, right and bottom edges to a file; returns the first write error (or not-open)
long GERectangle::Save(FSFile& inFile) const
{
    const float* theFields[] = {&this->mLeft, &this->mTop, &this->mRight, &this->mBottom};
    return GERustFields(inFile, theFields, 4, inFile.GetIsOpen(), true, false);
}
