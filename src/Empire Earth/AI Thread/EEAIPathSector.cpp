// C:\EEX\Empire Earth\AI Thread\EEAIPathSector.cpp (file name inferred: no __FILE__; RTTI class EEAIPathSector,
// vtable 0x43a52c). A path sector of the continent map: 0x2bbd3..0x2be88 (EEAIFunctionObjects.cpp ends at 0x2bbd3;
// EEAIPathSubSector's ctor starts at 0x2be88). ctor, ??_G, dtor, the members in address order, then the map/find
// COMDATs.
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
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"

// Destroys the path sector, deleting its sub-sectors
EEAIPathSector::~EEAIPathSector()
{
    this->DeleteSubSectors();
    ee_astar_sector_free(this);
}

// Returns the sector's path area containing the point (last listed first), or NULL
EEAIPathArea* EEAIPathSector::GetPathAreaAt(const U2DPoint<long>& inPoint)
{
    return (EEAIPathArea*)(uintptr_t)ee_astar_cm_object_path_area_at(&gContinentManager.mRecord, (uint64_t)(uintptr_t)this, true,
                                                                     inPoint.mX, inPoint.mY);   // Rust: cm_query.rs
}

// Deletes every sub-sector of the sector and empties the sub-sector map
void EEAIPathSector::DeleteSubSectors()
{
    EEAIListView<EeAStarSectorSub> theSubSectors = this->GetSubSectors();
    for (size_t i = 0; i < theSubSectors.size(); i++)
        delete (EEAIPathSubSector*)(uintptr_t)theSubSectors[i].sub;
    ee_astar_sector_clear_subs(this);
}

