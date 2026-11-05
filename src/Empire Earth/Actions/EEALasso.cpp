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
#include "lle/ISModifierState.h"

// FUNCTION: EXE 0x40289f
// Destroys the lasso action, releasing the message it has not sent
EEALasso::~EEALasso()
{
    if (this->mMessage)
        this->mMessage->Dereference();
}

// FUNCTION: EXE 0x4028c4
// Sends the prepared selection message as local user input and forgets it
void EEALasso::Virtual4()
{
    if (this->mMessage) {
        gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
        this->mMessage = NULL;
    }
}

// FUNCTION: EXE 0x4028e5
// Builds the selection message for a lasso rectangle and modifiers (select, add, toggle, patrol group or by type)
void EEALasso::SelectUnits(const GERectangle& inRectangle, long inFlags, unsigned long inUnitType, long inPatrolGroup)
{
    if (inUnitType < 0x366) {
        if (inUnitType == 0xdf)
            this->mMessage = (EEMSelectUnits*)new EEMSelectPatrolGroup(inPatrolGroup);
        else if (inFlags == 0)
            this->mMessage = new EEMSelectUnits(inRectangle, inUnitType, 0x4e);
        else if (inFlags & kModifierControl)
            this->mMessage = new EEMAddAndRemoveUnitsFromSelectedGroup(inRectangle, inUnitType, 0x50);
        else if (inFlags & kModifierShift)
            this->mMessage = new EEMAddUnitsToSelectedGroup(inRectangle, inUnitType, 0x4f);
        else
            this->mMessage = new EEMSelectUnits(inRectangle, inUnitType, 0x4e);
    } else {
        if (inFlags == 0)
            this->mMessage = new EEMSelectUnits(inRectangle, 0, 0x4e);
        else if (inFlags & kModifierControl)
            this->mMessage = new EEMAddAndRemoveUnitsFromSelectedGroup(inRectangle, 0, 0x50);
        else if (inFlags & kModifierShift)
            this->mMessage = new EEMAddUnitsToSelectedGroup(inRectangle, 0, 0x4f);
        else
            this->mMessage = new EEMSelectUnits(inRectangle, 0, 0x4e);
    }
    gUserInterface->PostAction(this);
}

// SYNTHETIC: EXE 0x402a02
// (EEMSelectUnits' scalar deleting destructor: ICF, also its two derived messages'; their first user)

// FUNCTION: EXE 0x402a3b
// Creates the message that selects a patrol group
// (EEMSelectPatrolGroup's inline ctor, ee/EEMessage.h)

// SYNTHETIC: EXE 0x402a68
// (the scalar deleting destructor)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEALasso::Virtual8()
{
}
