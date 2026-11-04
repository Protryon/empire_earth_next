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
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"

// FUNCTION: EXE 0x42bbd3
// Creates an empty path sector with id 0
EEAIPathSector::EEAIPathSector()
    : mID(0)
{
}

// FUNCTION: EXE 0x42bc4a
// Destroys the path sector, deleting its sub-sectors
EEAIPathSector::~EEAIPathSector()
{
    this->DeleteSubSectors();
}

// FUNCTION: EXE 0x42bc72
// Adds a continent touching the sector unless it is already listed
void EEAIPathSector::AddNeighbor(EEAIContinent* inContinent)
{
    std::vector<EEAIContinent*>::iterator theNeighbor = std::find(this->mNeighbors.begin(), this->mNeighbors.end(), inContinent);
    if (theNeighbor == this->mNeighbors.end())
        this->mNeighbors.push_back(inContinent);
}

// FUNCTION: EXE 0x42bccf
// Adds a path area id to the sector's sorted id list unless it is already there
void EEAIPathSector::AddPathAreaID(unsigned short inID)
{
    if (!std::binary_search(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end(), inID)) {
        this->mPathAreaIDs.push_back(inID);
        std::sort(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end());
    }
}

// FUNCTION: EXE 0x42bd4d
// Returns the sector's path area containing the point (last listed first), or NULL
EEAIPathArea* EEAIPathSector::GetPathAreaAt(const U2DPoint<long>& inPoint)
{
    EEAIPathArea* theResult = NULL;
    for (long i = this->mPathAreaIDs.size() - 1; i >= 0; i--) {
        EEAIPathArea* theArea = gContinentManager.GetPathArea(this->mPathAreaIDs[i]);
        if (theArea && theArea->Contains(inPoint)) {
            theResult = theArea;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42bda4
// Deletes every sub-sector of the sector and empties the sub-sector map
void EEAIPathSector::DeleteSubSectors()
{
    std::map<unsigned short, EEAIPathSubSector*>::iterator theSubSector;
    for (theSubSector = this->mSubSectors.begin(); theSubSector != this->mSubSectors.end(); ++theSubSector)
        delete theSubSector->second;
    this->mSubSectors.clear();
}

// FUNCTION: EXE 0x42bde6
// Removes a path area id from the sector's sorted id list when present
void EEAIPathSector::RemovePathAreaID(unsigned short inID)
{
    std::vector<unsigned short>::iterator theID = std::lower_bound(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end(), inID);
    if (theID != this->mPathAreaIDs.end() && *theID == inID)
        this->mPathAreaIDs.erase(theID);
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// SYNTHETIC: EXE 0x42bc2e
// TEMPLATE: EXE 0x42be51
// TEMPLATE: EXE 0x42be65
