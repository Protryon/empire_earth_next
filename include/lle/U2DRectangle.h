// Low-Level Engine U2DRectangle header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DRECTANGLE_H
#define LLE_U2DRECTANGLE_H

#include "lle/U2DPoint.h"
#include <type_traits>
#include "rust/base/math.h"

// Engine template (U2DPolygon<float>'s bounds, GETerrainMesh.cpp). Members alphabetical like GERectangle (+0 bottom,
// +4 left, +8 right, +0xc top: U2DConvexHull<float>::SetPoints 0x1002f795, Load 0x1003057d). (provisional names)
template <class T> class U2DRectangle {
public:
    U2DRectangle() : mBottom(0), mLeft(0), mRight(0), mTop(0) {}
    U2DRectangle(T inLeft, T inTop, T inRight, T inBottom) : mBottom(inBottom), mLeft(inLeft), mRight(inRight), mTop(inTop) {}
#ifdef LLE_EXE_HEADERS
#endif
    void Clear() { *this = U2DRectangle(); }
    // left, top, right and bottom from (to) an open file, each through its typed call; kErrorNotOpen when it is not
    // open, else the first error (ee_base math u2d.rs; T is long or float)
    long Load(FSFile& inFile) { return ee_u2d_rect_io(&inFile, inFile.GetIsOpen(), &this->mBottom, std::is_floating_point<T>::value, false); }
    long Save(FSFile& inFile) const
    {
        return ee_u2d_rect_io(&inFile, inFile.GetIsOpen(), const_cast<T*>(&this->mBottom), std::is_floating_point<T>::value,
                              true);
    }

    T mBottom;                              // +0x00
    T mLeft;                                // +0x04
    T mRight;                               // +0x08
    T mTop;                                 // +0x0c
};


#endif
