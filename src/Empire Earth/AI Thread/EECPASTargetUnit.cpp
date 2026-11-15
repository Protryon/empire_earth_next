// C:\EEX\Empire Earth\AI Thread\EECPASTargetUnit.cpp (file name inferred: no __FILE__; RTTI class
// EECPASTargetUnit, vtable 0x43bf30). A target of an attack sub group: 0x51952..0x5198e (EECPASGTransport.cpp ends
// at 0x51952; EECPAttackCommander's ctor starts at 0x5198e). Clear 0x51952 and SetUnit 0x5196b (called from the sub
// group's pool code 0x5d08f); the ctor and dtor are inline (ee/EECPASTargetUnit.h).
#include <windows.h>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASTargetUnit.h"

// FUNCTION: EXE 0x451952
// Drops the target unit, releasing its reference and clearing its id
void EECPASTargetUnit::Clear()
{
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->mUnit = NULL;
    this->mUnitID = 0;
}

// FUNCTION: EXE 0x45196b
// Makes a unit the target, holding a reference to it and recording its id (NULL clears)
void EECPASTargetUnit::SetUnit(EEComplexUnit* inUnit)
{
    this->Clear();
    this->mUnit = inUnit;
    if (inUnit != NULL) {
        inUnit->Reference();
        this->mUnitID = this->mUnit->mUnitID;
    }
}

