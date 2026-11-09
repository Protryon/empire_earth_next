// C:\EEX\Empire Earth\Formations\EEFCircle.cpp (file and folder names inferred: no __FILE__). The ring formation: 0xdd160..0xdd861
// (ctor, ??_G, the implicit dtor, Place, the vector<EEFormationSlot> COMDATs, then EEFormation's inline ctor and ??_G:
// this is their first user).
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"

#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEFormation.h"
#include "ee/EEFCircle.h"
#include "ee/EESimCapture.h"
#include "rust/ee_rust.h"
#include "ee/EEFormationRust.h"

// .rdata 0x43f818.. (unit-local named constants, then the vtable 0x43f828)
const float kEEFCircleF64 = 0.5f;
const float kCircleMinRadius = 0.5f;

// FUNCTION: EXE 0x4dd160
// Creates a ring formation with the given facing mode, spanning a full circle with a minimum radius of 0.5
EEFCircle::EEFCircle(long inMode, long inType)
    : EEFormation(inType), mMode(inMode)
{
    this->mF64 = kEEFCircleF64;
    this->mF68 = kCircleMinRadius;
    this->mB74 = false;
    this->mMinRadius = kCircleMinRadius;
    this->mHalfArc = 3.14159265f;
}

// SYNTHETIC: EXE 0x4dd1a2
// (the scalar deleting destructor; ICF-shared with EEFInwardCircle/EEFOutwardCircle/EEFWedge)

// FUNCTION: EXE 0x4dd1be
// Destroys the ring formation
// (~EEFormation inline; ICF-shared with ~EEFWedge)
EEFCircle::~EEFCircle()
{
}

// FUNCTION: EXE 0x4dd1cc
// Places the units evenly round a ring sized to their largest collision radius, each facing the formation
// heading, the centre or outward by mode; a lone unit stands at the centre
void EEFCircle::Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots)
{
    unsigned long theFirst = outSlots.size();   // appends: no clear
    if (inUnits.size() == 0)
        return;
    EEFormationRadii theRadii(this, inUnits);
    EeFormationCircle theCircle = {this->mMode, this->mHalfArc, this->mMinRadius};
    unsigned int theCount = ee_formation_circle_place(&theRadii.mFrame, &theCircle, theRadii.Radii(), theRadii.Count(),
                                                      theRadii.Slots());
    theRadii.Append(theCount, inUnits, outSlots);
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(0x49434d46);   // "FMCI"
        EEFormationCaptureBegin(theRecord, this);
        theRecord.I32(this->mMode);
        theRecord.F32(this->mHalfArc);
        theRecord.F32(this->mMinRadius);
        EEFormationCaptureEnd(theRecord, inUnits, outSlots, theFirst);
    }
}

// The template COMDATs of Place (vector<EEFormationSlot>; names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4dd45c
// TEMPLATE: EXE 0x4dd488
// TEMPLATE: EXE 0x4dd4e2
// TEMPLATE: EXE 0x4dd799
// TEMPLATE: EXE 0x4dd7b6

// TEMPLATE: EXE 0x4dd7e2
// (compiler-generated; name in analysis/exe/names.txt)

// The inline COMDATs this unit emits as their first user (after the template COMDATs of Place):
// NEAR: scheduling only (the original stores mL58 between the last two fldz stores; +0x48..+0x50 may be a GE3DPoint)
// TEMPLATE: EXE 0x4dd7f8
// (EEFormation::EEFormation(long), inline in EEFormation.h)
// TEMPLATE: EXE 0x4dd83d
// (EEFormation's scalar deleting destructor)
