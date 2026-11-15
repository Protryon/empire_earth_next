// C:\EEX\Empire Earth\AI Thread\EECPAttackFormationNode.cpp (file name inferred: no __FILE__; RTTI class
// EECPAttackFormationNode, vtable 0x43bc88). One row of an attack formation: 0x54304..0x5473a (EECPAttackFormation.cpp's
// sort COMDATs end at 0x54304; EECPAttackGroup's ctor starts at 0x5473a). The ctor, ??_G, dtor, Create, then the
// U2DRectangle<float>::Extend COMDAT
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GETransformation.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEFormation.h"
#include "ee/EEFCircle.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPAttackFormation.h"
#include "ee/EEAIKindIDs.h"

static const float kAFNQuarters = 4.0f;     // 0x43bc80

// FUNCTION: EXE 0x454304
// Creates an empty formation row: no formation, no units, spacing 1, not placed
EECPAttackFormationNode::EECPAttackFormationNode()
    : mFormation(NULL), mType(0), mSpacing(1.0f), mPrevious(NULL), mL60(0), mFlags(0), mF70(0.0f), mIndex(0), mIsPlaced(false)
{
    this->mF74 = 0.0f;
    this->mDepth = 0.0f;
}

// SYNTHETIC: EXE 0x45439f
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4543bb
// Destroys the row, releasing its formation
EECPAttackFormationNode::~EECPAttackFormationNode()
{
    if (this->mFormation)
        this->mFormation->Dereference();
}

// FUNCTION: EXE 0x4543db
// Builds the row's formation of its type around its position, orders each unit to its slot (on water also
// setting a rank 0-3 from the slot order), grows the row's bounds over the slots and marks the row placed
void EECPAttackFormationNode::Create(EECPAttackFormation* inFormation)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEFormationSlot> theSlots;
    bool theSpread = inFormation->mSubGroup->mFlags.mFlags[kAITileWater];
    if (this->mUnits.size() <= 0)
        return;
    if (this->mFormation)
        this->mFormation->Dereference();
    EEFormation* theFormation = EEFormation::Create(this->mType);
    this->mFormation = theFormation;
    if (!theFormation)
        return;
    if (this->mType == 6 || this->mType == 7) {
        EEFCircle* theCircle = dynamic_cast<EEFCircle*>(theFormation);
        if (theCircle) {
            if ((this->mFlags >> 3) & 1)
                theCircle->mHalfArc = 1.5707964f;
            theCircle->mMinRadius = this->mDepth;
        }
    }
    this->mBounds.mTop = this->mPosition.GetY();
    this->mBounds.mLeft = this->mPosition.GetX();
    this->mBounds.mBottom = this->mPosition.GetY();
    this->mBounds.mRight = this->mPosition.GetX();
    theUnits.resize(this->mUnits.size());
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theUnits[i] = this->mUnits[i]->GetUnit();
    // NEAR: the original assigns +0x48 through the imported GE3DPoint::operator= (EEFormation.h has three
    // floats there)
    theFormation->mCenter.mX = this->mPosition.GetX();
    theFormation->mCenter.mY = this->mPosition.GetY();
    theFormation->mCenter.mZ = this->mPosition.mZ;
    theFormation->Place(theUnits, theSlots);
    inFormation->mSubGroup->mGroup->mCommander->mUpdatesThisTick += theSlots.size();
    long theCount;
    for (unsigned long j = 0; j < (theCount = theSlots.size()); j++) {
        if (theSpread)
            theSlots[j].mUnit->mFormationRank = (long)((float)j / theCount * kAFNQuarters);
        EEFormationSlot& theSlot = theSlots[j];
        EEUGRust* theGoal = EEUGRust::NewLocation(&theSlot.mPosition, true, !inFormation->mNoForcedMove, true,
            theSlot.mHeading, false, false);
        if (!inFormation->mSubGroup->GiveGoal(theSlots[j].mUnit, theGoal, 0, kActionPrepareToMove) && theGoal)
            delete theGoal;
        float theY = theSlots[j].mPosition.GetY();
        this->mBounds.Extend(U2DPoint<float>(theSlots[j].mPosition.GetX(), theY));
    }
    this->mIsPlaced = true;
}

// TEMPLATE: EXE 0x4546dc
// U2DRectangle<float>::Extend (inline, lle/U2DRectangle.h)
