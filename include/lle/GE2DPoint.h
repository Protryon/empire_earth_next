// C:\EEX\Low-Level Engine\Graphics Engine\GE2DPoint.h (RECONSTRUCTION). Requires <vector>.
//
// Inline status follows the plug-ins (they must keep inlining what they inline and importing what they import).
// The engine emits every member below except InterpolateTo/Load/RotatePoint/Save in the block that opens
// GE2DPoint.cpp (0x1000c129..0x1000c74b, in header include order UMFastMath, GE2DPoint, GEVector, GE3DPoint,
// GE3DLine), i.e. they look header-inline; but the plug-ins import DistanceTo/DistanceToSquared (Default.dll), and
// an inline DistanceToSquared costs Default.dll unit 9 two exact functions. Open: docs/CALIBRATION.md "lle".
#ifndef LLE_GE2DPOINT_H
#define LLE_GE2DPOINT_H

class UString;
class FSFile;

class LLE_API GE2DPoint {                   // proven sizeof 8 (Default.dll vector<GE2DPoint>: index * 8)
public:
    GE2DPoint() : mX(0.0f), mY(0.0f) {}    // inline: Default.dll EEClumps 0x8c19 stores two fldz, no call
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports ??0GE2DPoint@@QAE@JJ@Z and expands it (fild, fstp, fild,
    // fstp: EECPASGLand::Virtual20C 0x44a742)
    GE2DPoint(long inX, long inY) : mX((float)inX), mY((float)inY) {}
#else
    GE2DPoint(long inX, long inY);          // 0x1000c146 (fild, fild)
#endif
#ifdef LLE_EXE_HEADERS
    // calls __imp_??0GE2DPoint@@QAE@MM@Z from 10 functions, and 0x669d8d pins its inline cost at exactly 2 above the
    // free SetXY body. The two `return;` stand in for two compiled-away statements (assert/trace stubs); not original
    // text. docs/LOST_DEBUG.md.
    GE2DPoint(float inX, float inY) { this->SetXY(inX, inY); return; return; }
#else
    GE2DPoint(float inX, float inY) : mX(inX), mY(inY) {}   // inline: 0x8ae0 (no ??0GE2DPoint@@QAE@MM@Z import)
#endif
    // inline, but Default.dll imports both: a refused inline of a dllimport class goes through the import
#if (defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)) && !defined(LLE_EXE_HEADERS)
    // the newer header: one inline level deeper, so the budget refuses it first (GetTopLeft family, ~20 UI rows)
    GE2DPoint(const GE2DPoint& inPoint) { *this = inPoint; }
#else
    GE2DPoint(const GE2DPoint& inPoint) : mX(inPoint.mX), mY(inPoint.mY) {}
#endif
    ~GE2DPoint() {}                         // inline + empty: vector<GE2DPoint>::_Tidy keeps a dead store (0x5114)
    GE2DPoint& operator=(const GE2DPoint& inPoint) { this->mX = inPoint.mX; this->mY = inPoint.mY; return *this; }
    // inline (InterpolateTo 0xc80e expands the comparison; the plug-ins never compare points)
    bool operator==(const GE2DPoint& inPoint) const { return this->mX == inPoint.mX && this->mY == inPoint.mY; }
    bool operator!=(const GE2DPoint& inPoint) const { return this->mX != inPoint.mX || this->mY != inPoint.mY; }
#ifdef LOWLEVELENGINE_EXPORTS
    // inline in the engine: no engine code calls the exported 0x1000c18d/0x1000c1a5 (GE2DPoint.cpp emits them);
    // UIForm::GetToolTipShadowRectangle 0x10072574 adds in place. Neither the exe nor the plug-ins import them.
    GE2DPoint& operator+=(const GE2DPoint& inPoint) { this->mX += inPoint.mX; this->mY += inPoint.mY; return *this; }
    GE2DPoint& operator-=(const GE2DPoint& inPoint) { this->mX -= inPoint.mX; this->mY -= inPoint.mY; return *this; }
#else
    GE2DPoint& operator+=(const GE2DPoint& inPoint);
    GE2DPoint& operator-=(const GE2DPoint& inPoint);
#endif
    void Clear() { this->mX = 0.0f; this->mY = 0.0f; }
    // cross product of (inFrom - inTo) and (inTo - this): the side of the line inFrom-inTo this point lies on
    float GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const;
    // inline (exported ?GetX@GE2DPoint@@QBEMXZ, ICF-folded `fld [ecx]; ret`): a returned value, not a memory
    // operand: `a.GetX() - b.GetX()` gives `fld a; fld b; fsubp` (Default.dll 0xc29f)
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    // inline setters (exported bodies: SetX(float) `mov eax,[esp+4]; mov [ecx],eax` 0x54ae4, SetXY 0x1347f,
    // SetX(long) `fild; fstp` 0xe745, SetY(long) 0xc1ec)
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetX(long inX) { this->mX = (float)inX; }
    void SetY(long inY) { this->mY = (float)inY; }
    void SetXY(float inX, float inY) { this->mX = inX; this->mY = inY; }
    float DistanceTo(const GE2DPoint& inPoint) const;
    float DistanceToSquared(const GE2DPoint& inPoint) const;
    // proven import ?InterpolateTo@GE2DPoint@@QBEXABV1@MAAV?$vector@... (Default.dll CMUDrawShallows 0x3633)
    void InterpolateTo(const GE2DPoint& inPoint, float inStep, std::vector<GE2DPoint>& outPoints) const;
    long Load(FSFile& inFile);
    void RotatePoint(float inAngle, const GE2DPoint& inCenter);
    void RotatePoint(float inAngle, const GE2DPoint& inCenter, GE2DPoint& outPoint) const;
    long Save(FSFile& inFile);

    float mX;                               // +0x00 (fld [p]; GE2DPoint(float, float))
    float mY;                               // +0x04
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard: these are class members (exported ?s..@GE2DPoint@@0VUString@@B), but every static data
    // member a TU sees takes a VC6 $E number and the plug-ins' init-chain numbering shows they saw none
private:
    static const UString sPointCoordXStringID;   // GE2DPoint.cpp $E chain 0x1000c74b
    static const UString sPointCoordYStringID;
    static const UString sPointCoordZStringID;
#endif
};

#endif
