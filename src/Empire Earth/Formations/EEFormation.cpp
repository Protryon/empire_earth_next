// C:\EEX\Empire Earth\Formations\EEFormation.cpp (file and folder names inferred: no __FILE__). The formation:
// 0xdd160..0xde331 (the base's Create and Arrange, and the eight classes the original had, EEFCircle.cpp to
// EEFWedge.cpp, folded into this one unit: the units' lane, round 2; ee/EEFormation.h).
// Unit flags: the exe's module set.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"

#include "ee/EEFormation.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESimCapture.h"
#include "rust/sim/formations.h"

// Creates the formation of a type (0-7; ee/EEFormation.h), NULL for any other type
EEFormation* EEFormation::Create(long inType)
{
    return inType >= 0 && inType < 8 ? new EEFormation(inType) : NULL;
}

// Collects the group's units and places them into formation slots
void EEFormation::Arrange(EEGroup* inGroup, std::vector<EEFormationSlot>& outSlots)
{
    std::vector<EEComplexUnit*> theUnits;
    theUnits.resize(inGroup->GetNumUnits());
    for (long i = inGroup->GetNumUnits() - 1; i >= 0; i--)
        theUnits[i] = inGroup->GetUnit(i);
    this->Place(theUnits, outSlots);
}

// TEST ONLY: a Place call's record for the simulation capture (tools/sim_capture.py; ee_sim formations/capture.rs
// decodes it): the type, the frame, the type's members (Rust's words), the units' radii, the slots from inFirst on
// (as unit indices)
static void CapturePlace(const EEFormation* inFormation, std::vector<EEComplexUnit*>& inUnits,
                         std::vector<EEFormationSlot>& inSlots, unsigned long inFirst)
{
    uint32_t theTag = 0;
    uint32_t theWords[EE_FORMATION_SHAPE_WORDS];
    uint32_t theCount = ee_formation_shape_words((int32_t)inFormation->mType, inFormation->mMinRadius, &theTag, theWords);
    EESimCaptureRecord theRecord(theTag);
    theRecord.I32(inFormation->mType);
    for (int i = 0; i < 3; i++)
        theRecord.F32s(inFormation->mOrientation.mMatrix[i], 3);
    theRecord.F32(inFormation->mCenter.GetX());
    theRecord.F32(inFormation->mCenter.GetY());
    theRecord.F32(inFormation->mCenter.GetZ());
    theRecord.F32(inFormation->mHeading);
    theRecord.Bytes(theWords, theCount * 4);
    theRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRecord.F32(inUnits[i]->mUnitClass->mOuterCollisionSphere);
    theRecord.U32((unsigned int)(inSlots.size() - inFirst));
    for (size_t i = inFirst; i < inSlots.size(); i++) {
        const EEFormationSlot& theSlot = inSlots[i];
        theRecord.F32(theSlot.mPosition.GetX());
        theRecord.F32(theSlot.mPosition.GetY());
        theRecord.F32(theSlot.mPosition.GetZ());
        theRecord.F32(theSlot.mHeading);
        unsigned int theIndex = 0xffffffff;
        for (size_t j = 0; j < inUnits.size(); j++)
            if (inUnits[j] == theSlot.mUnit) {
                theIndex = (unsigned int)j;
                break;
            }
        theRecord.U32(theIndex);
    }
    theRecord.Commit();
}

// Places the units by the type's shape (Rust: rows behind the centre, wedges from a point, or a ring), spaced by
// their outer collision radii. Lines and wedges replace outSlots; a circle appends, and does nothing for no units.
void EEFormation::Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots)
{
    unsigned long theFirst = 0;
    if (this->IsCircle()) {
        theFirst = outSlots.size();
        if (inUnits.size() == 0)
            return;
    } else {
        outSlots.clear();
    }
    EeFormationFrame theFrame;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            theFrame.orientation[i][j] = this->mOrientation.mMatrix[i][j];
    theFrame.center[0] = this->mCenter.GetX();
    theFrame.center[1] = this->mCenter.GetY();
    theFrame.center[2] = this->mCenter.GetZ();
    theFrame.heading = this->mHeading;
    std::vector<float> theRadii(inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRadii[i] = inUnits[i]->mUnitClass->mOuterCollisionSphere;
    std::vector<EeFormationSlot> theSlots(inUnits.size());
    unsigned int theCount = ee_formation_place((int32_t)this->mType, this->mMinRadius, &theFrame,
                                               theRadii.empty() ? NULL : &theRadii[0], (uint32_t)theRadii.size(),
                                               theSlots.empty() ? NULL : &theSlots[0]);
    EEFormationSlot theSlot;
    for (unsigned int i = 0; i < theCount; i++) {
        theSlot.mPosition = GE3DPoint(theSlots[i].position[0], theSlots[i].position[1], theSlots[i].position[2]);
        theSlot.mUnit = inUnits[theSlots[i].unit];
        theSlot.mHeading = theSlots[i].heading;
        outSlots.push_back(theSlot);
    }
    if (EESimCaptureRecord::Active())
        CapturePlace(this, inUnits, outSlots, theFirst);
}

void EEFormationCaptureSweep()
{
    static const unsigned long kSizes[] = {1, 2, 3, 4, 5, 6, 9, 10, 11, 16, 20, 21, 22, 23, 43, 64};
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theAll;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0)
                theAll.push_back(theUnit->mComplexUnit);
        for (unsigned long s = 0; s < sizeof(kSizes) / sizeof(kSizes[0]) && kSizes[s] <= theAll.size(); s++) {
            unsigned long theSize = kSizes[s];
            std::vector<EEComplexUnit*> theUnits(theAll.end() - theSize, theAll.end());
            GE3DPoint theFirst;
            GE3DPoint theLast;
            theUnits.front()->GetPosition(theFirst);
            theUnits.back()->GetPosition(theLast);
            float theYaw = EEGetHeadingBetween(&theFirst, &theLast);
            for (long theType = 0; theType < 8; theType++) {
                EEFormation* theFormation = EEFormation::Create(theType);
                theFormation->SetCenter(theFirst);
                theFormation->mHeading = theYaw;
                if (theSize % 2 == 0)   // odd sizes keep the identity orientation, as EEHerd's circles
                    theFormation->mOrientation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
                if (theFormation->IsCircle() && theSize % 3 == 0)
                    theFormation->SetMinRadius((float)theSize * 0.75f);
                std::vector<EEFormationSlot> theSlots;
                theFormation->Place(theUnits, theSlots);
                delete theFormation;
            }
        }
    }
}
