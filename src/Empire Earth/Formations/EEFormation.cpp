// C:\EEX\Empire Earth\Formations\EEFormation.cpp (file and folder names inferred: no __FILE__). The formation base: 0xddcc6..0xdde43 (Create, Arrange).
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

#include "ee/EEFormation.h"
#include "ee/EEFCircle.h"
#include "ee/EEFInwardCircle.h"
#include "ee/EEFOutwardCircle.h"
#include "ee/EEFLine.h"
#include "ee/EEFLongLine.h"
#include "ee/EEFLooseLine.h"
#include "ee/EEFShortLine.h"
#include "ee/EEFStaggeredLine.h"
#include "ee/EEFWedge.h"
#include "ee/EEFDeepWedge.h"
#include "ee/EEFShallowWedge.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESimCapture.h"
#include "rust/ee_formations.h"
#include "ee/EEFormationRust.h"

// FUNCTION: EXE 0x4ddcc6
// Creates the formation of a type: 0-3 short, long, loose, staggered line; 4-5 deep, shallow wedge;
// 6-7 inward, outward circle; NULL for any other type
EEFormation* EEFormation::Create(long inType)
{
    EEFormation* theFormation = NULL;
    switch (inType) {
    case 0:
        theFormation = new EEFShortLine;
        break;
    case 1:
        theFormation = new EEFLongLine;
        break;
    case 2:
        theFormation = new EEFLooseLine;
        break;
    case 3:
        theFormation = new EEFStaggeredLine;
        break;
    case 4:
        theFormation = new EEFDeepWedge;
        break;
    case 5:
        theFormation = new EEFShallowWedge;
        break;
    case 6:
        theFormation = new EEFInwardCircle;
        break;
    case 7:
        theFormation = new EEFOutwardCircle;
        break;
    }
    return theFormation;
}

// FUNCTION: EXE 0x4dddd5
// Collects the group's units and places them into formation slots
void EEFormation::Arrange(EEGroup* inGroup, std::vector<EEFormationSlot>& outSlots)
{
    std::vector<EEComplexUnit*> theUnits;
    theUnits.resize(inGroup->GetNumUnits());
    for (long i = inGroup->GetNumUnits() - 1; i >= 0; i--)
        theUnits[i] = inGroup->GetUnit(i);
    this->Place(theUnits, outSlots);
}

EEFormationRadii::EEFormationRadii(const EEFormation* inFormation, std::vector<EEComplexUnit*>& inUnits)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            this->mFrame.orientation[i][j] = inFormation->mOrientation.mMatrix[i][j];
    this->mFrame.center[0] = inFormation->mCenter.GetX();
    this->mFrame.center[1] = inFormation->mCenter.GetY();
    this->mFrame.center[2] = inFormation->mCenter.GetZ();
    this->mFrame.heading = inFormation->mHeading;
    this->mRadii.resize(inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        this->mRadii[i] = inUnits[i]->mUnitClass->mOuterCollisionSphere;
    this->mSlots.resize(inUnits.size());
}

void EEFormationRadii::Append(unsigned int inCount, std::vector<EEComplexUnit*>& inUnits,
                              std::vector<EEFormationSlot>& outSlots)
{
    EEFormationSlot theSlot;
    for (unsigned int i = 0; i < inCount; i++) {
        const EeFormationSlot& theOut = this->mSlots[i];
        theSlot.mPosition = GE3DPoint(theOut.position[0], theOut.position[1], theOut.position[2]);
        theSlot.mUnit = inUnits[theOut.unit];
        theSlot.mHeading = theOut.heading;
        outSlots.push_back(theSlot);
    }
}

void EEFormationCaptureBegin(EESimCaptureRecord& ioRecord, const EEFormation* inFormation)
{
    ioRecord.I32(inFormation->mType);
    for (int i = 0; i < 3; i++)
        ioRecord.F32s(inFormation->mOrientation.mMatrix[i], 3);
    ioRecord.F32(inFormation->mCenter.GetX());
    ioRecord.F32(inFormation->mCenter.GetY());
    ioRecord.F32(inFormation->mCenter.GetZ());
    ioRecord.F32(inFormation->mHeading);
}

void EEFormationCaptureEnd(EESimCaptureRecord& ioRecord, std::vector<EEComplexUnit*>& inUnits,
                           std::vector<EEFormationSlot>& inSlots, unsigned long inFirst)
{
    ioRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        ioRecord.F32(inUnits[i]->mUnitClass->mOuterCollisionSphere);
    ioRecord.U32((unsigned int)(inSlots.size() - inFirst));
    for (size_t i = inFirst; i < inSlots.size(); i++) {
        const EEFormationSlot& theSlot = inSlots[i];
        ioRecord.F32(theSlot.mPosition.GetX());
        ioRecord.F32(theSlot.mPosition.GetY());
        ioRecord.F32(theSlot.mPosition.GetZ());
        ioRecord.F32(theSlot.mHeading);
        unsigned int theIndex = 0xffffffff;
        for (size_t j = 0; j < inUnits.size(); j++)
            if (inUnits[j] == theSlot.mUnit) {
                theIndex = (unsigned int)j;
                break;
            }
        ioRecord.U32(theIndex);
    }
    ioRecord.Commit();
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
                EEFCircle* theCircle = dynamic_cast<EEFCircle*>(theFormation);
                if (theCircle != NULL && theSize % 3 == 0)
                    theCircle->SetMinRadius((float)theSize * 0.75f);
                std::vector<EEFormationSlot> theSlots;
                theFormation->Place(theUnits, theSlots);
                delete theFormation;
            }
        }
    }
}
