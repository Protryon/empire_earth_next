// C:\EEX\Empire Earth\Actions\EEACalamity.cpp (file name inferred: no __FILE__; RTTI class EEACalamity, vtable
// 0x438b88). The cast-calamity action: 0x1a8d..0x1de1 (EEABuild's ??_G ends at 0x1a8d, EEACenterCamera's code starts at
// 0x1de1). dtor, Cast with EEMCastCalamity's inline ctor and GetMessageLabel after it (their first user), the ICF
// representative of the one-member setters, Check with RTSUnit::Dereference (first user), then the ??_G.
#include <windows.h>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EECalamityClass.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEACalamity.h"

// Destroys the calamity action, releasing the message it has not sent
EEACalamity::~EEACalamity()
{
    if (this->mMessage)
        this->mMessage->Dereference();
}

// Sends the prepared cast message as local user input and forgets it (ICF-folded into EEALasso::Virtual4 0x28c4:
// no address of its own)
void EEACalamity::Virtual4()
{
    if (this->mMessage) {
        gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
        this->mMessage = NULL;
    }
}

// Builds the message that casts the selected calamity at a screen point (or at the caster for the Conquistador spell)
void EEACalamity::Cast(const GE2DPoint& inLocation)
{
    GETransformation theTransformation(::gWorld.mTransformation);
    GE3DPoint theWorldPoint;
    GE3DPoint thePoint;
    EEUnit* theUnit;
    long theX;
    long theY;
    if (this->mTechnology == kTechConquistadorsSpell) {
        theUnit = ::gWorld.GetUnit(this->mCasterID);
        if (theUnit) {
            theX = theUnit->mTileX;
            theY = theUnit->mTileY;
        } else {
            theX = this->mX;
            theY = this->mY;
        }
    } else {
        EETile* theFound;
        bool theOnUnit = EEPointUnitOrTileXY(inLocation, theFound, this->mX, this->mY, true);
        ::gWorld.ScreenToWorld(inLocation, theWorldPoint);
        theTransformation.Apply(theWorldPoint, thePoint);
        this->mTargetID = this->mCalamityClass->GetDefaultTarget(thePoint, this->mX, this->mY, theOnUnit);
        theUnit = ::gWorld.GetUnit(this->mTargetID);
        if (theUnit) {
            theX = theUnit->mTileX;
            theY = theUnit->mTileY;
        } else {
            theX = this->mX;
            theY = this->mY;
        }
    }
    this->mMessage = new EEMRecord(kMessageCastCalamity, {this->mCasterID, this->mTargetID, theX, theY, this->mTechnology});
    if (this->mTechnology != kTechConquistadorsSpell)
        EEOrderAtCursor(inLocation, theUnit);
    gUserInterface->PostAction(this);
}

// Sets the id of the unit that casts the calamity
// (out of line: EEUIFMainGame 0x2666d4 calls it; ICF representative of the one-member setters, EEContagionHelper's too)
void EEACalamity::SetCasterID(long inCasterID)
{
    this->mCasterID = inCasterID;
}

// Checks whether the calamity may be cast at a screen point by its caster; stores the target tile and the validity
long EEACalamity::Check(const GE2DPoint& inLocation)
{
    GETransformation theTransformation(::gWorld.mTransformation);
    GE3DPoint theWorldPoint;
    GE3DPoint thePoint;
    EEUnit* theCaster = ::gWorld.GetUnit(this->mCasterID);
    EESelectedGroup* theGroup = ::gWorld.GetCurrentPlayer()->mActiveGroup;
    this->mValid = 1;
    if (theCaster && theCaster->mComplexUnit) {
        long theY;
        long theX;
        EETile* theFound;
        bool theOnUnit = EEPointUnitOrTileXY(inLocation, theFound, this->mX, this->mY, true);
        ::gWorld.ScreenToWorld(inLocation, theWorldPoint);
        theTransformation.Apply(theWorldPoint, thePoint);
        EEUnit* theTarget = ::gWorld.GetUnit(this->mCalamityClass->GetDefaultTarget(thePoint, this->mX, this->mY, theOnUnit));
        if (theTarget == NULL) {
            theTarget = gUnitPicker.PickUnit(inLocation, 0);
            if (theTarget)
                theTarget->Dereference();
        }
        if (theTarget) {
            theX = theTarget->mTileX;
            theY = theTarget->mTileY;
        } else {
            theX = this->mX;
            theY = this->mY;
        }
        unsigned long theCount = theGroup->GetNumUnits();
        EETechTreeNode* theNode = ::gWorld.GetCurrentPlayer()->mTechnologies->Get(this->mTechnology);
        for (unsigned long i = 0; i < theCount && this->mValid; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theNode->Contains(theUnit->mUnitClass->mUnitType))
                this->mValid = this->mCalamityClass->CanCastAt(theUnit, thePoint, theX, theY, theOnUnit, theTarget);
        }
    }
    return this->mValid;
}

void EEACalamity::Virtual8()
{
}
