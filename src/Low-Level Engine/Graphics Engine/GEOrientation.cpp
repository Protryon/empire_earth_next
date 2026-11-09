// C:\EEX\Low-Level Engine\Graphics Engine\GEOrientation.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10020d20..0x10020e09: Normalize and Save (Save's body is ICF-folded with GEVector::Save and GE3DPoint::Save; this
// unit's copy is the one kept). Load is defined here too, but the fold keeps GE3DPoint.cpp's copy (0x1000dc23). The
// ctors and accessors are header-inline. No init chains.
// Normalize forwards to Rust (rust/crates/ee_ge transform.rs; GERustMath.h).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/FSFile.h"
#include "lle/GEOrientation.h"
#include "GERustMath.h"

// Not in this unit's range (ICF keeps 0x1000dc23).
long GEOrientation::Load(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(this->mYaw);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mPitch);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mRoll);
        }
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10020d20
// Wraps yaw, pitch and roll into the range -pi..pi
void GEOrientation::Normalize()
{
    ee_ge_orientation_normalize(GERust(*this));
}

// FUNCTION: LLE 0x10020dc6
// Writes yaw, pitch and roll to a file; kErrorNotOpen when the file is not open
long GEOrientation::Save(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Write(this->mYaw);
        if (theResult == kErrorOK) {
            theResult = inFile.Write(this->mPitch);
            if (theResult == kErrorOK)
                theResult = inFile.Write(this->mRoll);
        }
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// ---- Exported members with no address of their own: ICF-folded into GEVector's ctors (0xc240, 0xd56f), its
// operator= shape (0xd588), the empty dtor 0xc16a and the float setters 0xc56c/0xc576/0x54ae4
GEOrientation::GEOrientation()
    : mYaw(0.0f), mPitch(0.0f), mRoll(0.0f)
{
}

GEOrientation::GEOrientation(float inYaw, float inPitch, float inRoll)
    : mYaw(inYaw), mPitch(inPitch), mRoll(inRoll)
{
}

GEOrientation::GEOrientation(const GEOrientation& inOrientation)
{
    this->mYaw = inOrientation.mYaw;
    this->mPitch = inOrientation.mPitch;
    this->mRoll = inOrientation.mRoll;
}

GEOrientation::~GEOrientation()
{
}

void GEOrientation::SetPitch(float inPitch)
{
    this->mPitch = inPitch;
}

void GEOrientation::SetRoll(float inRoll)
{
    this->mRoll = inRoll;
}

void GEOrientation::SetYaw(float inYaw)
{
    this->mYaw = inYaw;
}
