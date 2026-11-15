// C:\EEX\Empire Earth\Actions\EEABuild.cpp (file name inferred: no __FILE__; RTTI class EEABuild, vtable
// 0x438aa0). The build action: 0x173b..0x1a8d (EEABootClientFromCanceledGame's Virtual4 ends at 0x173b, EEACalamity's
// dtor starts at 0x1a8d). dtor, Build with EEMBuildUnit's inline ctor, EEMessage's inline +0x04 and EETechTree::Get
// after it (their first user in link order), BuildGate / BuildWall with their messages' inline ctors, then the ??_G.
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
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEABuild.h"

// Destroys the build action, releasing the message it has not sent
EEABuild::~EEABuild()
{
    if (this->mMessage)
        this->mMessage->Dereference();
}

// Sends the prepared build message as local user input and forgets it (ICF-folded into EEALasso::Virtual4 0x28c4:
// no address of its own)
void EEABuild::Virtual4()
{
    if (this->mMessage) {
        gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
        this->mMessage = NULL;
    }
}

// Places the current player's selected building at a screen point (centred for its tile size) and posts the action
void EEABuild::Build(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation;
    float theOffset = 0.0f;
    if (!::gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, 0x20, 1, true, 3, NULL))
        return;
    EEUnitClass* theClass =
        gUnitClassTable[::gWorld.GetCurrentPlayer()->mTechnologies->Get(this->mTechnology)->mData->mUnitClass];
    if (theClass) {
        long theSize = theClass->GetTileSize();
        if (theSize > 1) {
            if (theSize & 1)
                theOffset = theSize * 0.5f - 0.5f;
            else
                theOffset = theSize * 0.5f;
        }
    }
    theLocation.SetX(inLocation.GetX() - theOffset);
    theLocation.SetY(inLocation.GetY() - theOffset);
    this->mMessage = new EEMRecord(kMessageBuildUnit, {theLocation, this->mTechnology, this->mIsQueued});
    gUserInterface->PostAction(this);
}

// Posts a gate build at a location and wall side, if the current player can produce the technology
void EEABuild::BuildGate(const GE2DPoint& inLocation, long inSide)
{
    if (::gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, 0x20, 1, true, 3, NULL)) {
        this->mMessage = new EEMRecord(kMessageBuildGate, {inLocation, this->mTechnology, inSide, ::gWorld.mPlayers[::gWorld.mCurrentPlayer]->mPlayerID});
        gUserInterface->PostAction(this);
    }
}

// Posts a wall build between two points, if the current player can produce the technology
void EEABuild::BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd)
{
    if (::gWorld.GetCurrentPlayer()->CanProduce(this->mTechnology, 0x20, 1, true, 3, NULL)) {
        this->mMessage = new EEMRecord(kMessageBuildWall, {inStart, inEnd, this->mTechnology, ::gWorld.mPlayers[::gWorld.mCurrentPlayer]->mPlayerID});
        gUserInterface->PostAction(this);
    }
}
