// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\Unit11.cpp (file name inferred: tile-grid and
// mask queries, square and line marking; used by the CMU unit, HMUFractal and unit 12)
// Unit flags (calibrated, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "Unit11.h"

// Compiler-generated for this unit (names in analysis/default/names.txt):
// TEMPLATE: DEFAULT 0x10015c73
// SYNTHETIC: DEFAULT 0x100154a5
// SYNTHETIC: DEFAULT 0x100154af
// SYNTHETIC: DEFAULT 0x100154c0
// SYNTHETIC: DEFAULT 0x100154cc
// SYNTHETIC: DEFAULT 0x100154d7
// SYNTHETIC: DEFAULT 0x100154e1
// SYNTHETIC: DEFAULT 0x100154f2
// SYNTHETIC: DEFAULT 0x100154fe
// SYNTHETIC: DEFAULT 0x10015509
// SYNTHETIC: DEFAULT 0x10015513
// SYNTHETIC: DEFAULT 0x10015524
// SYNTHETIC: DEFAULT 0x10015530
// SYNTHETIC: DEFAULT 0x1001553b
// SYNTHETIC: DEFAULT 0x10015545
// SYNTHETIC: DEFAULT 0x10015556
// SYNTHETIC: DEFAULT 0x10015562
// SYNTHETIC: DEFAULT 0x1001556d
// SYNTHETIC: DEFAULT 0x10015577
// SYNTHETIC: DEFAULT 0x10015588
// SYNTHETIC: DEFAULT 0x10015594
// SYNTHETIC: DEFAULT 0x1001559f
// SYNTHETIC: DEFAULT 0x100155a9
// SYNTHETIC: DEFAULT 0x100155ba
// SYNTHETIC: DEFAULT 0x100155c6
// SYNTHETIC: DEFAULT 0x100155d1
// SYNTHETIC: DEFAULT 0x100155db
// SYNTHETIC: DEFAULT 0x100155ec
// SYNTHETIC: DEFAULT 0x100155f8
// SYNTHETIC: DEFAULT 0x10015c82
// SYNTHETIC: DEFAULT 0x10015c8c

// FUNCTION: DEFAULT 0x10015603
// Returns whether any tile in the square around a point is in the mask and has no land owner
bool IsAnyMaskedTileClear(const std::vector<std::vector<unsigned char> >& inMask,
                          const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY,
                          unsigned long inRadius)
{
    unsigned long theSize = inMask.size();
    unsigned long theLeft = inX - inRadius;
    unsigned long theRight = inX + inRadius;
    unsigned long theTop = inY - inRadius;
    unsigned long theBottom = inY + inRadius;
    bool theResult = false;
    if (theLeft >= theSize)
        theLeft = 0;
    if (theTop >= theSize)
        theTop = 0;
    if (theRight >= theSize)
        theRight = theSize - 1;
    if (theBottom >= theSize)
        theBottom = theSize - 1;
    for (unsigned long i = theLeft; i <= theRight && !theResult; i++)
        for (unsigned long j = theTop; j <= theBottom && !theResult; j++)
            theResult = inMask[i][j] && (inTiles[i][j] & 0x7fff) == 0;
    return theResult;
}

// FUNCTION: DEFAULT 0x100156ad
// Returns whether every tile in the square around a point is in the mask and not set in the other grid
bool AreAllMasked(const std::vector<std::vector<unsigned char> >& inMask,
                  const std::vector<std::vector<unsigned char> >& inOther, unsigned long inX, unsigned long inY,
                  unsigned long inRadius)
{
    unsigned long theSize = inMask.size();
    unsigned long theLeft = inX - inRadius;
    unsigned long theRight = inX + inRadius;
    unsigned long theTop = inY - inRadius;
    unsigned long theBottom = inY + inRadius;
    bool theResult = true;
    if (theLeft >= theSize)
        theLeft = 0;
    if (theTop >= theSize)
        theTop = 0;
    if (theRight >= theSize)
        theRight = theSize - 1;
    if (theBottom >= theSize)
        theBottom = theSize - 1;
    for (unsigned long i = theLeft; i <= theRight && theResult; i++)
        for (unsigned long j = theTop; j <= theBottom && theResult; j++)
            theResult = inMask[i][j] && !inOther[i][j];
    return theResult;
}

// FUNCTION: DEFAULT 0x10015753
// Returns false when all four corners of the square around a point are on the map and in the mask,
// true otherwise (despite its name, true means the square is not fully usable)
bool IsSquareOpen(std::vector<std::vector<unsigned char> >& inMask, unsigned long inX, unsigned long inY,
                  unsigned long inRadius)
{
    unsigned long theSize = inMask.size();
    unsigned long theLeft = inX - inRadius;
    unsigned long theRight = inX + inRadius + 1;
    unsigned long theTop = inY - inRadius;
    unsigned long theBottom = inY + inRadius + 1;
    if (theLeft < theSize && theRight < theSize && theTop < theSize && theBottom < theSize &&
        inMask[theLeft][theTop] && inMask[theLeft][theBottom] && inMask[theRight][theTop] &&
        inMask[theRight][theBottom])
        return 0;
    return 1;
}

// FUNCTION: DEFAULT 0x1001581c
// Tells whether a land tile borders an empty one (8-neighbourhood, within the map)
bool Unit11_1581c(const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY)
{
    long theLast = inX + 1;
    unsigned long theSize = inTiles.size();
    bool theResult = false;
    if (inX < theSize && inY < theSize && (inTiles[inX][inY] & 0x7fff) != 0)
        for (long i = inX - 1; !theResult && i <= theLast; i++)
            for (long j = inY - 1; !theResult && j <= (long)(inY + 1); j++)
                if ((unsigned long)i < theSize && (unsigned long)j < theSize)
                    theResult = (inTiles[i][j] & 0x7fff) == 0;
    return theResult;
}

// FUNCTION: DEFAULT 0x100158bb
// Returns whether a tile's owner is the given player (neutral land: owner + 0x400, else owner + 1)
bool IsTileOfType(const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY,
                  unsigned long inType, bool inSpecial)
{
    bool theResult;
    if (inSpecial)
        theResult = (inTiles[inX][inY] & 0x7fff) == inType + 0x400;
    else
        theResult = (inTiles[inX][inY] & 0x7fff) == inType + 1;
    return theResult;
}

// FUNCTION: DEFAULT 0x1001590d
// Sets a size-by-size square of the grid from a corner, clipped to the map
void MarkSquare(std::vector<std::vector<unsigned char> >& ioMask, unsigned long inX, unsigned long inY,
                unsigned long inSize)
{
    unsigned long theSize = ioMask.size();
    for (unsigned long i = 0; i < inSize; i++)
        for (unsigned long j = 0; j < inSize; j++)
            if (inX + i < theSize && inY + j < theSize)
                ioMask[inX + i][inY + j] = 1;
}

// FUNCTION: DEFAULT 0x10015970
// Sets every grid tile along a line of the given width, clipped to the map
void MarkLine(std::vector<std::vector<unsigned char> >& ioMask, const GE2DPoint& inFrom, const GE2DPoint& inTo,
              float inWidth)
{
    GE2DPoint theLeft;
    GE2DPoint theRight;
    GEVector theDirection;
    GEVector theSide;
    std::vector<GE2DPoint> theLine;
    std::vector<GE2DPoint> theCross;
    inWidth *= 0.5f;
    unsigned long theSize = ioMask.size();
    GETransformation theRotation;
    theRotation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f);
    theDirection.mX = inTo.GetX() - inFrom.GetX();
    theDirection.mY = inTo.GetY() - inFrom.GetY();
    theRotation.Apply(theDirection, theSide);
    theSide.Normalize();
    theSide.mX *= inWidth;
    theSide.mY *= inWidth;
    inFrom.InterpolateTo(inTo, 1.0f, theLine);
    for (unsigned long i = 0; i < theLine.size(); i++) {
        theLeft.mX = theLine[i].mX + theSide.mX;
        theLeft.mY = theLine[i].mY + theSide.mY;
        theRight.mX = theLine[i].mX - theSide.mX;
        theRight.mY = theLine[i].mY - theSide.mY;
        theLeft.InterpolateTo(theRight, 1.0f, theCross);
        for (unsigned long j = 0; j < theCross.size(); j++) {
            unsigned long theX = UMFloorUnsigned(theCross[j].mX);
            unsigned long theY = UMFloorUnsigned(theCross[j].mY);
            if (theX < theSize && theY < theSize)
                ioMask[theX][theY] = 1;
        }
    }
}
