// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.cpp (RECONSTRUCTION). The combat manager's military class tests
// (ee/EECPCombatManager.h); stage 5 deleted the combat tree the rest of it ran (docs/AI_RESEARCH.md §8.90).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>

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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPCombatManager.h"
#include "rust/sim/ai.h"   // ee_ai_military_*

EECPCombatManager::EECPCombatManager(EEComputerPlayer* inPlayer) : mPlayer(inPlayer)
{
}

EECPCombatManager::~EECPCombatManager()
{
}

long EECPCombatManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    (void)inMajorVersion;
    (void)inMinorVersion;
    (void)inFile;
    return 0;
}

long EECPCombatManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    (void)inMajorVersion;
    (void)inMinorVersion;
    (void)outFile;
    return 0;
}

// Returns true for a mobile military class (not a citizen, building, projectile or naval mine): Rust (ee_ai
// military.rs)
bool EECPCombatManager::IsMilitaryClass(EEUnitClass* inClass)
{
    return ee_ai_military_class((uint64_t)(uintptr_t)inClass);
}

// Returns true when the unit type's class is in a military category (ee_ai military.rs)
bool EECPCombatManager::IsMilitaryType(unsigned long inUnitType)
{
    return ee_ai_military_type(inUnitType);
}
