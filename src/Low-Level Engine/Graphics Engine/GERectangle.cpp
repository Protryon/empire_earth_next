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

const UString GERectangle::sLeftID("GERectangle-Left");
const UString GERectangle::sTopID("GERectangle-Top");
const UString GERectangle::sRightID("GERectangle-Right");
const UString GERectangle::sBottomID("GERectangle-Bottom");

// FUNCTION: LLE 0x100247ab
// Clips the rectangle to another one; returns false and leaves it unchanged when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle)
{
    return ee_ge_rect_and_in_place(GERust(*this), GERust(inRectangle));
}

// FUNCTION: LLE 0x10024826
// Writes the overlap of two rectangles to outRectangle; returns false (output untouched) when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle, GERectangle& outRectangle) const
{
    return ee_ge_rect_and(GERust(*this), GERust(inRectangle), GERust(outRectangle));
}

// FUNCTION: LLE 0x100248ba
// Reads the left, top, right and bottom edges from a file; returns the first read error (or not-open)
long GERectangle::Load(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(this->mLeft);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mTop);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mRight);
                if (theResult == kErrorOK)
                    theResult = inFile.Read(this->mBottom);
            }
        }
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x1002490c
// Splits the rectangle around another one into left/right/top/bottom pieces; the whole rectangle when no edge cuts
void GERectangle::RemoveIntersectingRectangle(const GERectangle& inRectangle,
                                              std::vector<GERectangle>& outRectangles) const
{
    // the whole rectangle is added only to an empty vector (the C++ tested its size, not its own pieces)
    GERectangle theThis(*this);
    GERectangle theOther(inRectangle);   // copies: either may be an element of outRectangles
    ee_ge_rect_remove_intersecting(GERust(theThis), GERust(theOther), outRectangles.size(), GERustPushRectangle,
                                   &outRectangles);
}

// FUNCTION: LLE 0x10024a82
// Writes the left, top, right and bottom edges to a file; returns the first write error (or not-open)
long GERectangle::Save(FSFile& inFile) const
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Write(this->mLeft);
        if (theResult == kErrorOK) {
            theResult = inFile.Write(this->mTop);
            if (theResult == kErrorOK) {
                theResult = inFile.Write(this->mRight);
                if (theResult == kErrorOK)
                    theResult = inFile.Write(this->mBottom);
            }
        }
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10024790
// TEMPLATE: LLE 0x10024e0b
// TEMPLATE: LLE 0x10024e3c
// TEMPLATE: LLE 0x10024e6a

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100246b1
// SYNTHETIC: LLE 0x100246cd
// SYNTHETIC: LLE 0x100246f2
// SYNTHETIC: LLE 0x1002470e
// SYNTHETIC: LLE 0x10024733
// SYNTHETIC: LLE 0x1002474f
// SYNTHETIC: LLE 0x10024774
// SYNTHETIC: LLE 0x10024784

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100246a7
// SYNTHETIC: LLE 0x100246c1
// SYNTHETIC: LLE 0x100246e8
// SYNTHETIC: LLE 0x10024702
// SYNTHETIC: LLE 0x10024729
// SYNTHETIC: LLE 0x10024743
// SYNTHETIC: LLE 0x1002476a

// Compiler-generated, continued:
// TEMPLATE: LLE 0x10024ad4
// TEMPLATE: LLE 0x10024b00
// TEMPLATE: LLE 0x10024b50
// TEMPLATE: LLE 0x10024ddf
