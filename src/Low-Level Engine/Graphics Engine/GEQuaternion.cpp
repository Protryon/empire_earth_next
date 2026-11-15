// C:\EEX\Low-Level Engine\Graphics Engine\GEQuaternion.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10023219..0x10023655: matrix <-> quaternion conversion, Normalize and Slerp (Shoemake's algorithms). Every other
// GEQuaternion member is header-inline. No init chains.
// Every member forwards to Rust (rust/crates/ee_ge transform.rs; GERustMath.h).
#include <windows.h>
#include <math.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEQuaternion.h"
#include "lle/UMFastMath.h"
#include "GERustMath.h"

// FUNCTION: LLE 0x10023219
// Sets the quaternion from a transformation's rotation (vector part negated), then normalises it
void GEQuaternion::ConvertFrom(const GETransformation& inTransformation)
{
    ee_ge_quat_from(GERust(inTransformation), GERust(*this));
}

// FUNCTION: LLE 0x100233b7
// Writes the quaternion's rotation into a transformation (no translation), then normalises the matrix
void GEQuaternion::ConvertTo(GETransformation& outTransformation) const
{
    ee_ge_quat_to(GERust(*this), GERust(outTransformation));
}

// FUNCTION: LLE 0x100234ca
// Scales the quaternion to unit length
// NEAR: the inline UMFastMath::Sqrt gives the fstp st(4) parameter shape (plain sqrt is further off); the second
// push ecx of the double argument area is scheduled one load earlier than the original
void GEQuaternion::Normalize()
{
    ee_ge_quat_normalize(GERust(*this));
}

// FUNCTION: LLE 0x10023526
// Interpolates between this quaternion and a target along the shorter arc (linear when they are nearly equal)
void GEQuaternion::Slerp(const GEQuaternion& inTarget, float inT, GEQuaternion& outQuaternion) const
{
    ee_ge_quat_slerp(GERust(*this), GERust(inTarget), inT, GERust(outQuaternion));
}
