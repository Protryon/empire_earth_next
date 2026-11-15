// C:\EEX\Empire Earth\Actions\EEALasso.cpp (file name inferred: no __FILE__; RTTI class EEALasso, vtable
// 0x438c74). The lasso selection action: 0x289f..0x2a84 (EEAJoinOrHostAGame's COMDATs end at 0x289f, EEAMiniMap's
// file-static chain starts at 0x2a84). dtor, Virtual4 (ICF: also EEABuild's and EEACalamity's), SelectUnits with the
// selection messages' ??_G and EEMSelectPatrolGroup's inline ctor after it (their first user), then the ??_G.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEALasso.h"
#include "ee/EEUnit.h"   // EEUnitClassCount
#include "lle/ISModifierState.h"

// Destroys the lasso action, releasing the message it has not sent
EEALasso::~EEALasso()
{
    if (this->mMessage)
        this->mMessage->Dereference();
}

// Sends the prepared selection message as local user input and forgets it
void EEALasso::Virtual4()
{
    if (this->mMessage) {
        gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
        this->mMessage = NULL;
    }
}

// Builds the selection message for a lasso rectangle and modifiers (select, add, toggle, patrol group or by type)
void EEALasso::SelectUnits(const GERectangle& inRectangle, long inFlags, unsigned long inUnitType, long inPatrolGroup)
{
    if (inUnitType < EEUnitClassCount()) {   // else any type (kAnyUnitType; the original: 0x366)
        if (inUnitType == 0xdf)
            this->mMessage = new EEMRecord(kMessageSelectPatrolGroup, {inPatrolGroup});
        else if (inFlags == 0)
            this->mMessage = EENewSelectUnits(inRectangle, inUnitType, 0x4e);
        else if (inFlags & kModifierControl)
            this->mMessage = EENewSelectUnits(inRectangle, inUnitType, 0x50);
        else if (inFlags & kModifierShift)
            this->mMessage = EENewSelectUnits(inRectangle, inUnitType, 0x4f);
        else
            this->mMessage = EENewSelectUnits(inRectangle, inUnitType, 0x4e);
    } else {
        if (inFlags == 0)
            this->mMessage = EENewSelectUnits(inRectangle, 0, 0x4e);
        else if (inFlags & kModifierControl)
            this->mMessage = EENewSelectUnits(inRectangle, 0, 0x50);
        else if (inFlags & kModifierShift)
            this->mMessage = EENewSelectUnits(inRectangle, 0, 0x4f);
        else
            this->mMessage = EENewSelectUnits(inRectangle, 0, 0x4e);
    }
    gUserInterface->PostAction(this);
}

void EEALasso::Virtual8()
{
}
