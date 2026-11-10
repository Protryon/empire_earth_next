// C:\EEX\Empire Earth\AI Thread\EEAIPathSectorRegionInformation.cpp (file name inferred: no __FILE__; RTTI
// class EEAIPathSectorRegionInformation, vtable 0x43a218). The path-sector grid geometry: 0x2bb72..0x2bbd3 (the map
// put it at the end of EEAIFunctionObjects.cpp; EEAIPathSector's ctor starts at 0x2bbd3).
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>
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
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"


// FUNCTION: EXE 0x42bb72
// Creates the path-sector grid geometry with every size 0
EEAIPathSectorRegionInformation::EEAIPathSectorRegionInformation()
{
    this->Reset();
}

// FUNCTION: EXE 0x42bb84
// Clears the sector grid's counts, sector size and sector area to 0
void EEAIPathSectorRegionInformation::Reset()
{
    this->mSectorsX = 0;
    this->mSectorsY = 0;
    this->mSectorSize = 0;
    this->mSectorArea = 0;
}

// FUNCTION: EXE 0x42bb93
// Sizes the sector grid for a map: square sectors of the given size, enough per side to cover the map
void EEAIPathSectorRegionInformation::Init(EEMap* inMap, long inSectorSize)
{
    this->mSectorsX = ee_astar_sectors_per_side((int32_t)inMap->mSize, (int32_t)inSectorSize);   // Rust: ee_astar sectors.rs
    this->mSectorsY = this->mSectorsX;
    this->mSectorSize = inSectorSize;
    this->mSectorArea = inSectorSize * inSectorSize;
}
