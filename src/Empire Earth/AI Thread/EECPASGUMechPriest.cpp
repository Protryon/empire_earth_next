// C:\EEX\Empire Earth\AI Thread\EECPASGUMechPriest.cpp (file name inferred: no __FILE__). 0x6547..0x65b0 (the calamity
// analyzers 0x65b0.. are in EECPASGUMechProphet.cpp: the tail COMDATs 0x8023..0x8fd7 they instantiate sit in its range).
#include <windows.h>
#include <vector>
#include <algorithm>
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

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
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
#include "ee/EECPASGUPriest.h"
#include "ee/EECPASGUMechPriest.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"
#include "ee/EECPASGUPCTargetIsUnitSetScreenBase.h"
#include "ee/EEUnitSetManager.h"

#include "ee/EECPAITargetPriority.h"

// FUNCTION: EXE 0x406547
// Creates the mech priest entry for a unit in the sub-group
EECPASGUMechPriest::EECPASGUMechPriest(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPASGUPriest(inUnit, inSubGroup)
{
    this->mType = kASGUKindMechPriest;
}

// SYNTHETIC: EXE 0x40656a
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x406586
// Creates an empty mech priest entry for the sub-group (no unit yet)
EECPASGUMechPriest::EECPASGUMechPriest(EECPAttackSubGroup* inSubGroup)
    : EECPASGUPriest(inSubGroup)
{
    this->mType = kASGUKindMechPriest;
}

// FUNCTION: EXE 0x4065a5
// Destroys the mech priest entry (base members only)
EECPASGUMechPriest::~EECPASGUMechPriest()
{
}

// ICF-FOLDED: EXE 0x40d21f
// Updates the unit the plain sub-group unit way, skipping the priest's own update
// (no address of its own: the same body as EECPASGUSea::Update, which ICF kept; [sib 57])
void EECPASGUMechPriest::Update(unsigned long inTime)
{
    EECPAttackSubGroupUnit::Update(inTime);
}
