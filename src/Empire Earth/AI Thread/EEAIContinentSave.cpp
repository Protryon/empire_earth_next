// New (port): the continent manager's regions in a saved game (save version 0.263, docs/RUST_PLAN.md R5a.2 "load
// equals continue"; ee/EESaveGameFiles.h EESaveHasContinentRegions).
//
// The original rebuilt the continent manager at every load (EEWorld::StartGame: Clear, then BuildRegions over the
// map as it is then). The running game built it once at its start and repaired it since (RepairPathArea as tiles
// change, forests losing trees): its path areas, sectors, sub sectors and choke points are not those of a fresh
// build. Its area serials were not either: a load numbered the herds and towns before the regions, the running game
// after them. The computer players' tasks name areas by serial and tile (EECPILocation), and a load re-found them
// (Restore calls Update): a choke point task became a continent task (the load had no choke point yet when it
// restored the tasks, and its own choke points are others), a town task of a town not its player's lost its area.
//
// From 0.263 a save holds the manager as it is (EEPWorldHelper::Save, after the dead attackers): every area with its
// serial, tiles and hull, the sectors and sub sectors with their links, the grids, counters and free ids, the map
// tiles' AI values (EETile::mAIValue: the path searches' tile costs) and the serials of the herds and towns. A load makes the regions from it where it ran BuildRegions (EEWorld::StartGame,
// EERestoreContinentRegions): the continents and hills Clear made take their saved contents, the path areas, forests,
// choke points, sectors and sub sectors are the saved ones, the herds and towns the load made again take their saved
// serials. The computer players' locations keep their saved areas from the start of the load to its first tick
// (gLoadKeepsLocations, ee/EECPILocation.h). An older save loads as before.
//
// The block is this file's own byte format (little-endian, written as one raw block with its length), so the dump's
// digest (EEContinentRegionsDigest, TEST ONLY) is the same bytes a save writes.
#include <windows.h>
#include <string.h>
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
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EETown.h"
#include "ee/EEHerd.h"
#include "ee/EESaveGameFiles.h"

extern long& gEEAIRegionSize;

namespace {

const unsigned long kRegionsMagic = 0x47524d43;   // "CMRG"
const unsigned long kRegionsFormat = 1;

// the area kinds of the serial table (mAreas): what owns the area and its index there
enum {
    kOwnerNone = 0, kOwnerContinent, kOwnerHill, kOwnerPathArea, kOwnerForest, kOwnerChokePoint, kOwnerRectangle,
    kOwnerTown, kOwnerHerd
};

struct Writer {
    std::vector<unsigned char> mBytes;
    void Bytes(const void* inData, size_t inSize)
    {
        const unsigned char* theData = (const unsigned char*)inData;
        mBytes.insert(mBytes.end(), theData, theData + inSize);
    }
    void U32(unsigned long inValue)
    {
        unsigned char theBytes[4] = {(unsigned char)inValue, (unsigned char)(inValue >> 8), (unsigned char)(inValue >> 16),
                                     (unsigned char)(inValue >> 24)};
        Bytes(theBytes, 4);
    }
    void I32(long inValue) { U32((unsigned long)inValue); }
    void U16(unsigned short inValue) { U32(inValue); }
    void Flag(bool inValue) { unsigned char theByte = inValue ? 1 : 0; Bytes(&theByte, 1); }
    void Point(const U2DPoint<long>& inPoint) { I32(inPoint.mX); I32(inPoint.mY); }
    void Rect(const U2DRectangle<long>& inRect) { I32(inRect.mBottom); I32(inRect.mLeft); I32(inRect.mRight); I32(inRect.mTop); }
    void Points(const std::vector<U2DPoint<long> >& inPoints)
    {
        U32((unsigned long)inPoints.size());
        for (size_t i = 0; i < inPoints.size(); i++)
            Point(inPoints[i]);
    }
    void Shorts(const std::vector<unsigned short>& inValues)
    {
        U32((unsigned long)inValues.size());
        for (size_t i = 0; i < inValues.size(); i++)
            U16(inValues[i]);
    }
    void Longs(const std::vector<long>& inValues)
    {
        U32((unsigned long)inValues.size());
        for (size_t i = 0; i < inValues.size(); i++)
            I32(inValues[i]);
    }
    void ULongs(const std::vector<unsigned long>& inValues)
    {
        U32((unsigned long)inValues.size());
        for (size_t i = 0; i < inValues.size(); i++)
            U32(inValues[i]);
    }
};

struct Reader {
    const std::vector<unsigned char>& mBytes;
    size_t mAt;
    explicit Reader(const std::vector<unsigned char>& inBytes) : mBytes(inBytes), mAt(0) {}
    bool Bytes(void* outData, size_t inSize)
    {
        if (mBytes.size() - mAt < inSize)
            return false;
        memcpy(outData, &mBytes[mAt], inSize);
        mAt += inSize;
        return true;
    }
    bool U32(unsigned long& outValue)
    {
        unsigned char theBytes[4];
        if (!Bytes(theBytes, 4))
            return false;
        outValue = (unsigned long)theBytes[0] | ((unsigned long)theBytes[1] << 8) | ((unsigned long)theBytes[2] << 16)
            | ((unsigned long)theBytes[3] << 24);
        return true;
    }
    bool I32(long& outValue)
    {
        unsigned long theValue = 0;
        bool theResult = U32(theValue);
        outValue = (long)(int32_t)theValue;
        return theResult;
    }
    bool U16(unsigned short& outValue)
    {
        unsigned long theValue = 0;
        bool theResult = U32(theValue);
        outValue = (unsigned short)theValue;
        return theResult;
    }
    bool Flag(bool& outValue)
    {
        unsigned char theByte = 0;
        bool theResult = Bytes(&theByte, 1);
        outValue = theByte != 0;
        return theResult;
    }
    bool Point(U2DPoint<long>& outPoint) { return I32(outPoint.mX) && I32(outPoint.mY); }
    bool Rect(U2DRectangle<long>& outRect)
    {
        return I32(outRect.mBottom) && I32(outRect.mLeft) && I32(outRect.mRight) && I32(outRect.mTop);
    }
    bool Count(unsigned long& outCount)
    {
        return U32(outCount) && outCount <= mBytes.size() - mAt;   // every element takes at least a byte
    }
    bool Points(std::vector<U2DPoint<long> >& outPoints)
    {
        unsigned long theCount = 0;
        if (!Count(theCount))
            return false;
        outPoints.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++)
            if (!Point(outPoints[i]))
                return false;
        return true;
    }
    bool Shorts(std::vector<unsigned short>& outValues)
    {
        unsigned long theCount = 0;
        if (!Count(theCount))
            return false;
        outValues.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++)
            if (!U16(outValues[i]))
                return false;
        return true;
    }
    bool Longs(std::vector<long>& outValues)
    {
        unsigned long theCount = 0;
        if (!Count(theCount))
            return false;
        outValues.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++)
            if (!I32(outValues[i]))
                return false;
        return true;
    }
    bool ULongs(std::vector<unsigned long>& outValues)
    {
        unsigned long theCount = 0;
        if (!Count(theCount))
            return false;
        outValues.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++)
            if (!U32(outValues[i]))
                return false;
        return true;
    }
};

// ---- writing

// a sparse point set as it is (U2DSparseArrayPointContainer::GetRaw): the offset, the count, the columns (a flag,
// then the size, the run count and the runs)
void WriteSparse(Writer& ioWriter, const U2DSparseArrayPointContainer& inSet)
{
    std::vector<long> theRaw;
    inSet.GetRaw(theRaw);
    size_t at = 0;
    for (int i = 0; i < 2 && at < theRaw.size(); i++)
        ioWriter.I32(theRaw[at++]);
    unsigned long theColumns = at < theRaw.size() ? (unsigned long)theRaw[at++] : 0;
    ioWriter.U32(theColumns);
    for (unsigned long c = 0; c < theColumns && at < theRaw.size(); c++) {
        bool thePresent = theRaw[at++] != 0;
        ioWriter.Flag(thePresent);
        if (!thePresent || at + 1 >= theRaw.size())
            continue;
        ioWriter.I32(theRaw[at++]);
        unsigned long theRuns = (unsigned long)theRaw[at++];
        ioWriter.U32(theRuns);
        for (unsigned long r = 0; r < theRuns * 2 && at < theRaw.size(); r++)
            ioWriter.I32(theRaw[at++]);
    }
}

void WriteArea(Writer& ioWriter, const EEAIArea& inArea)
{
    ioWriter.U32(inArea.GetSerial());
    ioWriter.I32(inArea.GetAreaType());
    WriteSparse(ioWriter, inArea.GetTileSet());
    inArea.WriteHull(ioWriter);
    ioWriter.Rect(inArea.GetBounds());
}

// a continent by its place in mContinents (-1: none or not listed)
long ContinentIndex(const EEAIContinent* inContinent)
{
    EEAIHandleView<EEAIContinent> theContinents = gContinentManager.GetContinents();
    for (size_t i = 0; inContinent != NULL && i < theContinents.size(); i++)
        if (theContinents[i] == inContinent)
            return (long)i;
    return -1;
}

void WriteContinents(Writer& ioWriter, const std::vector<EEAIContinent*>& inContinents)
{
    ioWriter.U32((unsigned long)inContinents.size());
    for (size_t i = 0; i < inContinents.size(); i++)
        ioWriter.I32(ContinentIndex(inContinents[i]));
}

void WriteGrid(Writer& ioWriter, const void* inGrid, size_t inBytes)
{
    ioWriter.Flag(inGrid != NULL);
    if (inGrid != NULL) {
        ioWriter.U32((unsigned long)inBytes);
        ioWriter.Bytes(inGrid, inBytes);
    }
}

// what owns an area of the serial table, and its index (towns and herds: the player and the town or herd id)
void AreaOwner(const EEAIArea* inArea, long& outOwner, long& outIndex, long& outPlayer)
{
    EEAIContinentManager& theManager = gContinentManager;
    outOwner = kOwnerNone;
    outIndex = -1;
    outPlayer = -1;
    if (inArea == NULL)
        return;
    for (size_t i = 0; i < theManager.GetContinents().size(); i++)
        if (theManager.GetContinents()[i] == inArea) { outOwner = kOwnerContinent; outIndex = (long)i; return; }
    for (size_t i = 0; i < theManager.GetHills().size(); i++)
        if (theManager.GetHills()[i] == inArea) { outOwner = kOwnerHill; outIndex = (long)i; return; }
    for (size_t i = 0; i < theManager.GetPathAreas().size(); i++)
        if (theManager.GetPathAreas()[i] == inArea) { outOwner = kOwnerPathArea; outIndex = (long)i; return; }
    for (size_t i = 0; i < theManager.GetForests().size(); i++)
        if (theManager.GetForests()[i] == inArea) { outOwner = kOwnerForest; outIndex = (long)i; return; }
    for (size_t i = 0; i < theManager.GetChokePoints().size(); i++)
        if (theManager.GetChokePoints()[i] == inArea) { outOwner = kOwnerChokePoint; outIndex = (long)i; return; }
    for (size_t i = 0; i < theManager.GetRectangleAreas().size(); i++)
        if (theManager.GetRectangleAreas()[i] == inArea) { outOwner = kOwnerRectangle; outIndex = (long)i; return; }
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (size_t i = 0; i < thePlayer->mTowns.size(); i++)
            if (thePlayer->mTowns[i] == inArea) {
                outOwner = kOwnerTown; outIndex = thePlayer->mTowns[i]->mTownID; outPlayer = (long)p; return;
            }
        for (size_t i = 0; i < thePlayer->mHerds.size(); i++)
            if (thePlayer->mHerds[i] == inArea) {
                outOwner = kOwnerHerd; outIndex = thePlayer->mHerds[i]->mID; outPlayer = (long)p; return;
            }
    }
}

void WriteRegions(Writer& ioWriter)
{
    EEAIContinentManager& theManager = gContinentManager;
    size_t i;
    ioWriter.U32(kRegionsMagic);
    ioWriter.U32(kRegionsFormat);
    // the counters
    ioWriter.U32(theManager.mRecord.width);
    ioWriter.U32(theManager.mRecord.height);
    ioWriter.I32(theManager.mRecord.state);
    ioWriter.U32(theManager.mRecord.last_serial);
    ioWriter.I32(theManager.mRecord.l118);
    ioWriter.U16(theManager.mRecord.next_path_area_id);
    ioWriter.U16(theManager.mRecord.last_sub_sector_id);
    ioWriter.I32(theManager.mRecord.checksum);
    ioWriter.I32(theManager.mRecord.next_land_mass);
    ioWriter.I32(theManager.mRecord.next_water_body);
    ioWriter.I32(theManager.mRecord.next_space_region);
    ioWriter.I32(theManager.mRecord.next_surface_region);
    ioWriter.I32(theManager.mRecord.num_water_tiles);
    ioWriter.I32(theManager.mRecord.num_land_tiles);
    ioWriter.I32(theManager.mRecord.num_space_tiles);
    ioWriter.I32(theManager.mRecord.flags_width);
    ioWriter.Flag(theManager.mRecord.clearing);
    for (i = 0; i < 7; i++)
        ioWriter.U16(theManager.mRecord.next_ids[i]);
    ioWriter.I32(theManager.mRecord.sector_size);
    ioWriter.I32(theManager.mRecord.sector_area);
    ioWriter.I32(theManager.mRecord.sectors_x);
    ioWriter.I32(theManager.mRecord.sectors_y);
    ioWriter.I32(gEEAIRegionSize);
    // the grids
    size_t theTiles = (size_t)theManager.mRecord.width * theManager.mRecord.height;
    ioWriter.Flag(theManager.mRecord.tile_values != NULL);   // longs: written one by one (sizeof(long) differs by system)
    for (size_t t = 0; theManager.mRecord.tile_values != NULL && t < theTiles; t++)
        ioWriter.I32(theManager.mRecord.tile_values[t]);
    WriteGrid(ioWriter, theManager.mRecord.continent_ids, theTiles * sizeof(unsigned short));
    WriteGrid(ioWriter, theManager.mRecord.sub_sector_ids, theTiles * sizeof(unsigned short));
    WriteGrid(ioWriter, theManager.mRecord.tile_flags, (size_t)theManager.mRecord.flags_width * theManager.mRecord.flags_width * sizeof(bool));
    // the map tiles' AI values (UpdateTileValues set them, RemoveForestTile lowers them; the path searches read them)
    for (unsigned long x = 0; x < theManager.mRecord.width; x++)
        for (unsigned long y = 0; y < theManager.mRecord.height; y++) {
            const EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTile((long)x, (long)y) : NULL;
            signed char theValue = theTile != NULL ? theTile->mAIValue : 0;
            ioWriter.Bytes(&theValue, 1);
        }
    // the continents
    ioWriter.U32((unsigned long)theManager.GetContinents().size());
    for (i = 0; i < theManager.GetContinents().size(); i++) {
        const EEAIContinent* theContinent = theManager.GetContinents()[i];
        ioWriter.Flag(theContinent != NULL);
        if (theContinent == NULL)
            continue;
        WriteArea(ioWriter, *theContinent);
        theContinent->WriteLists(ioWriter);
        WriteContinents(ioWriter, theContinent->GetNeighbors());
        for (long c = 0; c < 7; c++)
            ioWriter.U16(theContinent->GetCount(c));
        ioWriter.U32(theContinent->GetCategory());
        ioWriter.U16(theContinent->GetID());
    }
    // the hills
    ioWriter.U32((unsigned long)theManager.GetHills().size());
    for (i = 0; i < theManager.GetHills().size(); i++) {
        const EEAIHill* theHill = theManager.GetHills()[i];
        ioWriter.Flag(theHill != NULL);
        if (theHill == NULL)
            continue;
        WriteArea(ioWriter, *theHill);
        theHill->WriteFields(ioWriter);
    }
    // the path areas
    ioWriter.U32((unsigned long)theManager.GetPathAreas().size());
    for (i = 0; i < theManager.GetPathAreas().size(); i++) {
        const EEAIPathArea* theArea = theManager.GetPathAreas()[i];
        ioWriter.Flag(theArea != NULL);
        if (theArea == NULL)
            continue;
        WriteArea(ioWriter, *theArea);
        theArea->WriteFields(ioWriter);
    }
    // the forests
    ioWriter.U32((unsigned long)theManager.GetForests().size());
    for (i = 0; i < theManager.GetForests().size(); i++) {
        const EEAIForest* theForest = theManager.GetForests()[i];
        ioWriter.Flag(theForest != NULL);
        if (theForest == NULL)
            continue;
        WriteArea(ioWriter, *theForest);
        theForest->WriteFields(ioWriter);
    }
    // the choke points
    ioWriter.U32((unsigned long)theManager.GetChokePoints().size());
    for (i = 0; i < theManager.GetChokePoints().size(); i++) {
        const EEAIChokePoint* theChokePoint = theManager.GetChokePoints()[i];
        ioWriter.Flag(theChokePoint != NULL);
        if (theChokePoint == NULL)
            continue;
        WriteArea(ioWriter, *theChokePoint);
        theChokePoint->WriteFields(ioWriter);
    }
    // the rectangle areas
    ioWriter.U32((unsigned long)theManager.GetRectangleAreas().size());
    for (i = 0; i < theManager.GetRectangleAreas().size(); i++) {
        const EEAIRectangleArea* theArea = theManager.GetRectangleAreas()[i];
        ioWriter.Flag(theArea != NULL);
        if (theArea == NULL)
            continue;
        WriteArea(ioWriter, *theArea);
        theArea->WriteFields(ioWriter);
    }
    // the sub sectors: every one listed by id or held by a sector, numbered from 1 in that order
    std::vector<const EEAIPathSubSector*> theSubSectors;
    std::map<const EEAIPathSubSector*, unsigned long> theNumbers;
    for (i = 0; i < theManager.GetPathSubSectors().size(); i++) {
        const EEAIPathSubSector* theSubSector = theManager.GetPathSubSectors()[i];
        if (theSubSector != NULL && theNumbers.find(theSubSector) == theNumbers.end()) {
            theSubSectors.push_back(theSubSector);
            theNumbers[theSubSector] = (unsigned long)theSubSectors.size();
        }
    }
    for (i = 0; i < theManager.GetPathSectors().size(); i++) {
        const EEAIPathSector* theSector = theManager.GetPathSectors()[i];
        if (theSector == NULL)
            continue;
        for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theIt = theSector->mSubSectors.begin();
             theIt != theSector->mSubSectors.end(); ++theIt)
            if (theIt->second != NULL && theNumbers.find(theIt->second) == theNumbers.end()) {
                theSubSectors.push_back(theIt->second);
                theNumbers[theIt->second] = (unsigned long)theSubSectors.size();
            }
    }
    // a sub sector by its number (0: none; one that is not listed: also 0)
    struct Numbers {
        const std::map<const EEAIPathSubSector*, unsigned long>& mNumbers;
        unsigned long Of(const EEAIPathSubSector* inSubSector) const
        {
            std::map<const EEAIPathSubSector*, unsigned long>::const_iterator theIt = mNumbers.find(inSubSector);
            return theIt != mNumbers.end() ? theIt->second : 0;
        }
    } theNumber = {theNumbers};
    // the sectors (the sub sector each holds by its number; its continents by index)
    ioWriter.U32((unsigned long)theManager.GetPathSectors().size());
    for (i = 0; i < theManager.GetPathSectors().size(); i++) {
        const EEAIPathSector* theSector = theManager.GetPathSectors()[i];
        ioWriter.Flag(theSector != NULL);
        if (theSector == NULL)
            continue;
        ioWriter.U16(theSector->mID);
        ioWriter.Rect(theSector->mBounds);
        ioWriter.Point(theSector->mCenter);
        WriteContinents(ioWriter, theSector->mNeighbors);
        ioWriter.Shorts(theSector->mPathAreaIDs);
        ioWriter.U32((unsigned long)theSector->mSubSectors.size());
        for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theIt = theSector->mSubSectors.begin();
             theIt != theSector->mSubSectors.end(); ++theIt) {
            ioWriter.U16(theIt->first);
            ioWriter.U32(theNumber.Of(theIt->second));
        }
    }
    ioWriter.U32((unsigned long)theSubSectors.size());
    for (i = 0; i < theSubSectors.size(); i++) {
        const EEAIPathSubSector* theSubSector = theSubSectors[i];
        long theSector = -1;
        for (size_t s = 0; theSubSector->mSector != NULL && s < theManager.GetPathSectors().size() && theSector < 0; s++)
            if (theManager.GetPathSectors()[s] == theSubSector->mSector)
                theSector = (long)s;
        ioWriter.I32(theSector);
        WriteSparse(ioWriter, theSubSector->mTiles);
        ioWriter.U32((unsigned long)theSubSector->mNeighbors.size());
        for (size_t n = 0; n < theSubSector->mNeighbors.size(); n++)
            ioWriter.U32(theNumber.Of(theSubSector->mNeighbors[n]));
        ioWriter.U32((unsigned long)theSubSector->mLinks.size());
        for (std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::const_iterator theIt =
                 theSubSector->mLinks.begin(); theIt != theSubSector->mLinks.end(); ++theIt) {
            ioWriter.U16(theIt->first);
            ioWriter.U32(theNumber.Of(theIt->second.first));
            ioWriter.U32(theNumber.Of(theIt->second.second));
        }
        ioWriter.Point(theSubSector->mLocation);
        ioWriter.I32(theSubSector->mNumTiles);
        ioWriter.U16(theSubSector->mContinentID);
        ioWriter.U16(theSubSector->mPathAreaID);
        ioWriter.U16(theSubSector->mID);
        ioWriter.I32(theSubSector->mKind);
        ioWriter.I32(theSubSector->mPlayerID);
        ioWriter.Flag(theSubSector->mIsStale);
    }
    ioWriter.U32((unsigned long)theManager.GetPathSubSectors().size());
    for (i = 0; i < theManager.GetPathSubSectors().size(); i++)
        ioWriter.U32(theNumber.Of(theManager.GetPathSubSectors()[i]));
    // the shore tiles, the free ids, the sectors' pending tiles
    ioWriter.Points(theManager.GetShoreTiles(EeAStarCmShore::Shore));
    ioWriter.Points(theManager.GetShoreTiles(EeAStarCmShore::IceShore));
    ioWriter.Shorts(theManager.GetFreeIDs(EeAStarCmFree::SubSectors));
    ioWriter.Shorts(theManager.GetFreeIDs(EeAStarCmFree::PathAreas));
    std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > > theSectorTiles = theManager.GetSectorTiles();
    ioWriter.U32((unsigned long)theSectorTiles.size());
    for (std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > >::const_iterator theIt = theSectorTiles.begin();
         theIt != theSectorTiles.end(); ++theIt) {
        ioWriter.U16(theIt->first);
        ioWriter.Points(theIt->second);
    }
    // the serial table: each serial's area by its owner
    EEAIListView<EeAStarCmArea> theAreas = theManager.GetAreas();
    ioWriter.U32((unsigned long)theAreas.size());
    for (const EeAStarCmArea* theIt = theAreas.begin(); theIt != theAreas.end(); ++theIt) {
        long theOwner, theIndex, thePlayer;
        EEAIArea* theArea = (EEAIArea*)(uintptr_t)theIt->area;
        AreaOwner(theArea, theOwner, theIndex, thePlayer);
        ioWriter.U32(theIt->serial);
        ioWriter.I32(theOwner);
        ioWriter.I32(theIndex);
        ioWriter.I32(thePlayer);
        ioWriter.U32(theArea != NULL ? theArea->GetSerial() : 0);
    }
}

// ---- reading

bool ReadSparse(Reader& ioReader, U2DSparseArrayPointContainer& outSet)
{
    std::vector<long> theRaw;
    long theValue = 0;
    unsigned long theColumns = 0;
    if (!ioReader.I32(theValue))
        return false;
    theRaw.push_back(theValue);
    if (!ioReader.I32(theValue) || !ioReader.Count(theColumns))
        return false;
    theRaw.push_back(theValue);
    theRaw.push_back((long)theColumns);
    for (unsigned long c = 0; c < theColumns; c++) {
        bool thePresent = false;
        if (!ioReader.Flag(thePresent))
            return false;
        theRaw.push_back(thePresent ? 1 : 0);
        if (!thePresent)
            continue;
        unsigned long theRuns = 0;
        if (!ioReader.I32(theValue) || !ioReader.Count(theRuns))
            return false;
        theRaw.push_back(theValue);
        theRaw.push_back((long)theRuns);
        for (unsigned long r = 0; r < theRuns * 2; r++) {
            if (!ioReader.I32(theValue))
                return false;
            theRaw.push_back(theValue);
        }
    }
    return outSet.SetRaw(theRaw);
}

bool ReadArea(Reader& ioReader, EEAIArea& outArea)
{
    unsigned long theSerial = 0;
    long theType = 0;
    if (!ioReader.U32(theSerial) || !ioReader.I32(theType) || !ReadSparse(ioReader, outArea.GetTileSet())
        || !outArea.ReadHull(ioReader) || !ioReader.Rect(outArea.GetBounds()))
        return false;
    outArea.SetSerial(theSerial);
    outArea.SetAreaType(theType);
    return true;
}

bool ReadContinents(Reader& ioReader, std::vector<EEAIContinent*>& outContinents)
{
    unsigned long theCount = 0;
    if (!ioReader.Count(theCount))
        return false;
    outContinents.clear();
    for (unsigned long i = 0; i < theCount; i++) {
        long theIndex = -1;
        if (!ioReader.I32(theIndex))
            return false;
        EEAIHandleView<EEAIContinent> theContinents = gContinentManager.GetContinents();
        outContinents.push_back(theIndex >= 0 && (size_t)theIndex < theContinents.size() ? theContinents[theIndex] : NULL);
    }
    return true;
}

template <class T> bool ReadGrid(Reader& ioReader, EeAStarCmGrid inGrid, T* const& ioGrid, size_t inBytes)
{
    ee_astar_cm_set_grid(&gContinentManager.mRecord, inGrid, false, 0);
    bool thePresent = false;
    if (!ioReader.Flag(thePresent))
        return false;
    if (!thePresent)
        return true;
    unsigned long theBytes = 0;
    if (!ioReader.U32(theBytes) || theBytes != inBytes)
        return false;
    ee_astar_cm_set_grid(&gContinentManager.mRecord, inGrid, true, inBytes / sizeof(T));
    return ioReader.Bytes(ioGrid, inBytes);
}

// a Rust table as a vector (the reader works on these and stores them back)
template <class T> std::vector<T*> TableCopy(EeAStarCmTable inTable)
{
    EEAIHandleView<T> theTable = gContinentManager.Table<T>(inTable);
    std::vector<T*> theResult;
    for (size_t i = 0; i < theTable.size(); i++)
        theResult.push_back(theTable[i]);
    return theResult;
}

template <class T> void TableStore(EeAStarCmTable inTable, const std::vector<T*>& inObjects)
{
    gContinentManager.ResizeTable(inTable, 0);
    gContinentManager.ResizeTable(inTable, inObjects.size());
    for (size_t i = 0; i < inObjects.size(); i++)
        gContinentManager.SetTable(inTable, i, inObjects[i]);
}

// a table resized to inCount, keeping the objects it has below that (those past it deleted)
template <class T> void KeepTable(std::vector<T*>& ioTable, unsigned long inCount)
{
    for (size_t i = inCount; i < ioTable.size(); i++)
        delete ioTable[i];
    ioTable.resize(inCount, (T*)NULL);
}

// a table of objects, each entry present or not (NULL): new ones, or with inKeep the table's own where it has one
template <class T, class F> bool ReadTable(Reader& ioReader, std::vector<T*>& outTable, F inRead, bool inKeep = false)
{
    unsigned long theCount = 0;
    if (!ioReader.Count(theCount))
        return false;
    if (inKeep)
        KeepTable(outTable, theCount);
    else
        outTable.assign(theCount, (T*)NULL);
    for (unsigned long i = 0; i < theCount; i++) {
        bool thePresent = false;
        if (!ioReader.Flag(thePresent))
            return false;
        if (!thePresent) {
            delete outTable[i];
            outTable[i] = NULL;
            continue;
        }
        if (outTable[i] == NULL)
            outTable[i] = new T;
        if (!inRead(*outTable[i]))
            return false;
    }
    return true;
}

std::vector<unsigned char> sLoadedRegions;   // the block of the save being loaded (EELoadContinentRegions)
unsigned long sSavedLastSerial = 0;
bool sSavedClearing = false;

// replaces the manager's contents with the saved ones; false (the manager then half made) on a bad block
bool ReadRegions(Reader& ioReader)
{
    EEAIContinentManager& theManager = gContinentManager;
    unsigned long theMagic = 0, theFormat = 0;
    if (!ioReader.U32(theMagic) || theMagic != kRegionsMagic || !ioReader.U32(theFormat) || theFormat != kRegionsFormat)
        return false;
    // the regions BuildRegions would make go; the continents and hills Clear made are kept as objects (what the load
    // repaired since may point at them) and take their saved contents; the grids are replaced below
    ee_astar_cm_clear_sector_tiles(&theManager.mRecord);
    theManager.DeleteChokePoints();
    theManager.DeleteForests();
    theManager.DeleteRectangleAreas();
    theManager.DeletePathAreas();
    theManager.DeletePathSectors();
    theManager.ResizeTable(EeAStarCmTable::PathSubSectors, 0);
    ee_astar_cm_clear_areas(&theManager.mRecord);
    unsigned short theShort;
    bool theOK = ioReader.U32(theManager.mRecord.width) && ioReader.U32(theManager.mRecord.height) && ioReader.I32(theManager.mRecord.state)
        && ioReader.U32(theManager.mRecord.last_serial) && ioReader.I32(theManager.mRecord.l118) && ioReader.U16(theManager.mRecord.next_path_area_id)
        && ioReader.U16(theManager.mRecord.last_sub_sector_id) && ioReader.I32(theManager.mRecord.checksum) && ioReader.I32(theManager.mRecord.next_land_mass)
        && ioReader.I32(theManager.mRecord.next_water_body) && ioReader.I32(theManager.mRecord.next_space_region) && ioReader.I32(theManager.mRecord.next_surface_region)
        && ioReader.I32(theManager.mRecord.num_water_tiles) && ioReader.I32(theManager.mRecord.num_land_tiles)
        && ioReader.I32(theManager.mRecord.num_space_tiles) && ioReader.I32(theManager.mRecord.flags_width) && ioReader.Flag(sSavedClearing);
    for (long i = 0; i < 7 && theOK; i++) {
        theOK = ioReader.U16(theShort);
        theManager.mRecord.next_ids[i] = theShort;
    }
    long theRegionSize = 0;
    theOK = theOK && ioReader.I32(theManager.mRecord.sector_size) && ioReader.I32(theManager.mRecord.sector_area)
        && ioReader.I32(theManager.mRecord.sectors_x) && ioReader.I32(theManager.mRecord.sectors_y)
        && ioReader.I32(theRegionSize);
    if (!theOK || gWorld.mMap == NULL || theManager.mRecord.width != (unsigned long)gWorld.mMap->mSize
        || theManager.mRecord.height != (unsigned long)gWorld.mMap->mSize)   // another map's block
        return false;
    gEEAIRegionSize = theRegionSize;
    sSavedLastSerial = theManager.mRecord.last_serial;
    size_t theTiles = (size_t)theManager.mRecord.width * theManager.mRecord.height;
    ee_astar_cm_set_grid(&theManager.mRecord, EeAStarCmGrid::TileValues, false, 0);
    bool theValues = false;
    if (!ioReader.Flag(theValues))
        return false;
    if (theValues) {
        ee_astar_cm_set_grid(&theManager.mRecord, EeAStarCmGrid::TileValues, true, theTiles);
        for (size_t t = 0; t < theTiles; t++)
            if (!ioReader.I32(theManager.mRecord.tile_values[t]))
                return false;
    }
    if (!ReadGrid(ioReader, EeAStarCmGrid::ContinentIds, theManager.mRecord.continent_ids, theTiles * sizeof(unsigned short))
        || !ReadGrid(ioReader, EeAStarCmGrid::SubSectorIds, theManager.mRecord.sub_sector_ids, theTiles * sizeof(unsigned short))
        || !ReadGrid(ioReader, EeAStarCmGrid::TileFlags, theManager.mRecord.tile_flags, (size_t)theManager.mRecord.flags_width * theManager.mRecord.flags_width * sizeof(bool)))
        return false;
    for (unsigned long x = 0; x < theManager.mRecord.width; x++)
        for (unsigned long y = 0; y < theManager.mRecord.height; y++) {
            signed char theValue = 0;
            if (!ioReader.Bytes(&theValue, 1))
                return false;
            EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTile((long)x, (long)y) : NULL;
            if (theTile != NULL)
                theTile->mAIValue = theValue;
        }
    // the continents first (the other parts name them by index); their neighbours once all are made
    std::vector<std::vector<long> > theNeighbors;
    unsigned long theCount = 0;
    if (!ioReader.Count(theCount))
        return false;
    std::vector<EEAIContinent*> theContinents = TableCopy<EEAIContinent>(EeAStarCmTable::Continents);
    std::vector<EEAIHill*> theHills = TableCopy<EEAIHill>(EeAStarCmTable::Hills);
    std::vector<EEAIPathArea*> thePathAreas;
    std::vector<EEAIForest*> theForests;
    std::vector<EEAIChokePoint*> theChokePoints;
    std::vector<EEAIRectangleArea*> theRectangleAreas;
    std::vector<EEAIPathSector*> thePathSectors;
    KeepTable(theContinents, theCount);
    theNeighbors.resize(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        bool thePresent = false;
        if (!ioReader.Flag(thePresent))
            return false;
        if (!thePresent) {
            delete theContinents[i];
            theContinents[i] = NULL;
            continue;
        }
        if (theContinents[i] == NULL)
            theContinents[i] = new EEAIContinent;
        EEAIContinent* theContinent = theContinents[i];
        theContinent->ClearNeighbors();
        unsigned long theNeighborCount = 0, theCategory = 0;
        if (!ReadArea(ioReader, *theContinent) || !theContinent->ReadLists(ioReader)
            || !ioReader.Count(theNeighborCount))
            return false;
        for (unsigned long n = 0; n < theNeighborCount; n++) {
            long theIndex = -1;
            if (!ioReader.I32(theIndex))
                return false;
            theNeighbors[i].push_back(theIndex);
        }
        for (long c = 0; c < 7; c++) {
            if (!ioReader.U16(theShort))
                return false;
            theContinent->SetCount(c, theShort);
        }
        unsigned short theID = 0;
        if (!ioReader.U32(theCategory) || !ioReader.U16(theID))
            return false;
        theContinent->SetCategory((unsigned char)theCategory);
        theContinent->SetID(theID);
    }
    for (unsigned long i = 0; i < theCount; i++)
        for (size_t n = 0; theContinents[i] != NULL && n < theNeighbors[i].size(); n++) {
            long theIndex = theNeighbors[i][n];
            theContinents[i]->AddNeighbor(theIndex >= 0 && (unsigned long)theIndex < theCount
                                                                ? theContinents[theIndex] : NULL);
        }
    TableStore(EeAStarCmTable::Continents, theContinents);   // ReadContinents below reads them from the table
    if (!ReadTable(ioReader, theHills, [&ioReader](EEAIHill& outHill) {
            return ReadArea(ioReader, outHill) && outHill.ReadFields(ioReader);
        }, true))
        return false;
    if (!ReadTable(ioReader, thePathAreas, [&ioReader](EEAIPathArea& outArea) {
            return ReadArea(ioReader, outArea) && outArea.ReadFields(ioReader);
        }))
        return false;
    if (!ReadTable(ioReader, theForests, [&ioReader](EEAIForest& outForest) {
            return ReadArea(ioReader, outForest) && outForest.ReadFields(ioReader);
        }))
        return false;
    if (!ReadTable(ioReader, theChokePoints, [&ioReader](EEAIChokePoint& outChokePoint) {
            return ReadArea(ioReader, outChokePoint) && outChokePoint.ReadFields(ioReader);
        }))
        return false;
    if (!ReadTable(ioReader, theRectangleAreas, [&ioReader](EEAIRectangleArea& outArea) {
            return ReadArea(ioReader, outArea) && outArea.ReadFields(ioReader);
        }))
        return false;
    // the sectors: made first, their sub sectors filed once those are made
    std::vector<std::vector<std::pair<unsigned short, unsigned long> > > theHeld;
    if (!ioReader.Count(theCount))
        return false;
    thePathSectors.assign(theCount, (EEAIPathSector*)NULL);
    theHeld.resize(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        bool thePresent = false;
        if (!ioReader.Flag(thePresent))
            return false;
        if (!thePresent)
            continue;
        EEAIPathSector* theSector = new EEAIPathSector;
        thePathSectors[i] = theSector;
        unsigned long theHeldCount = 0;
        if (!ioReader.U16(theSector->mID) || !ioReader.Rect(theSector->mBounds) || !ioReader.Point(theSector->mCenter)
            || !ReadContinents(ioReader, theSector->mNeighbors) || !ioReader.Shorts(theSector->mPathAreaIDs)
            || !ioReader.Count(theHeldCount))
            return false;
        for (unsigned long k = 0; k < theHeldCount; k++) {
            unsigned short theKey = 0;
            unsigned long theNumber = 0;
            if (!ioReader.U16(theKey) || !ioReader.U32(theNumber))
                return false;
            theHeld[i].push_back(std::make_pair(theKey, theNumber));
        }
    }
    // the sub sectors (numbered from 1), then their neighbours and links
    unsigned long theSubCount = 0;
    if (!ioReader.Count(theSubCount))
        return false;
    std::vector<EEAIPathSubSector*> theSubSectors(theSubCount + 1, (EEAIPathSubSector*)NULL);
    struct Links {
        std::vector<unsigned long> mNeighbors;
        std::vector<std::pair<unsigned short, std::pair<unsigned long, unsigned long> > > mLinks;
    };
    std::vector<Links> theLinks(theSubCount + 1);
    for (unsigned long i = 1; i <= theSubCount; i++) {
        EEAIPathSubSector* theSubSector = new EEAIPathSubSector;
        theSubSectors[i] = theSubSector;
        long theSector = -1;
        unsigned long theNeighborCount = 0, theLinkCount = 0;
        if (!ioReader.I32(theSector) || !ReadSparse(ioReader, theSubSector->mTiles) || !ioReader.Count(theNeighborCount))
            return false;
        theSubSector->mSector = theSector >= 0 && (size_t)theSector < thePathSectors.size()
            ? thePathSectors[theSector] : NULL;
        for (unsigned long n = 0; n < theNeighborCount; n++) {
            unsigned long theNumber = 0;
            if (!ioReader.U32(theNumber))
                return false;
            theLinks[i].mNeighbors.push_back(theNumber);
        }
        if (!ioReader.Count(theLinkCount))
            return false;
        for (unsigned long n = 0; n < theLinkCount; n++) {
            unsigned short theKey = 0;
            unsigned long theFirst = 0, theSecond = 0;
            if (!ioReader.U16(theKey) || !ioReader.U32(theFirst) || !ioReader.U32(theSecond))
                return false;
            theLinks[i].mLinks.push_back(std::make_pair(theKey, std::make_pair(theFirst, theSecond)));
        }
        if (!ioReader.Point(theSubSector->mLocation) || !ioReader.I32(theSubSector->mNumTiles)
            || !ioReader.U16(theSubSector->mContinentID) || !ioReader.U16(theSubSector->mPathAreaID)
            || !ioReader.U16(theSubSector->mID) || !ioReader.I32(theSubSector->mKind) || !ioReader.I32(theSubSector->mPlayerID)
            || !ioReader.Flag(theSubSector->mIsStale))
            return false;
    }
    struct ByNumber {
        const std::vector<EEAIPathSubSector*>& mSubSectors;
        EEAIPathSubSector* Of(unsigned long inNumber) const { return inNumber < mSubSectors.size() ? mSubSectors[inNumber] : NULL; }
    } theByNumber = {theSubSectors};
    std::vector<bool> theFiled(theSubCount + 1, false);
    for (size_t s = 0; s < theHeld.size(); s++)
        for (size_t k = 0; k < theHeld[s].size(); k++) {
            EEAIPathSubSector* theSubSector = theByNumber.Of(theHeld[s][k].second);
            thePathSectors[s]->mSubSectors.insert(std::make_pair(theHeld[s][k].first, theSubSector));
            if (theHeld[s][k].second <= theSubCount)
                theFiled[theHeld[s][k].second] = true;
        }
    for (unsigned long i = 1; i <= theSubCount; i++) {
        EEAIPathSubSector* theSubSector = theSubSectors[i];
        for (size_t n = 0; n < theLinks[i].mNeighbors.size(); n++)
            theSubSector->mNeighbors.push_back(theByNumber.Of(theLinks[i].mNeighbors[n]));
        for (size_t n = 0; n < theLinks[i].mLinks.size(); n++)
            theSubSector->mLinks.insert(std::make_pair(theLinks[i].mLinks[n].first,
                                                       std::make_pair(theByNumber.Of(theLinks[i].mLinks[n].second.first),
                                                                      theByNumber.Of(theLinks[i].mLinks[n].second.second))));
    }
    if (!ioReader.Count(theCount))
        return false;
    std::vector<EEAIPathSubSector*> theTable(theCount, (EEAIPathSubSector*)NULL);
    for (unsigned long i = 0; i < theCount; i++) {
        unsigned long theNumber = 0;
        if (!ioReader.U32(theNumber))
            return false;
        theTable[i] = theByNumber.Of(theNumber);
    }
    TableStore(EeAStarCmTable::Hills, theHills);
    TableStore(EeAStarCmTable::PathAreas, thePathAreas);
    TableStore(EeAStarCmTable::Forests, theForests);
    TableStore(EeAStarCmTable::ChokePoints, theChokePoints);
    TableStore(EeAStarCmTable::RectangleAreas, theRectangleAreas);
    TableStore(EeAStarCmTable::PathSectors, thePathSectors);
    TableStore(EeAStarCmTable::PathSubSectors, theTable);
    for (unsigned long i = 1; i <= theSubCount; i++)
        if (!theFiled[i])   // no sector held it in the saved game (not owned there either): kept, not freed
            ESDebugOut("EEAIContinentSave: a saved sub sector (id %u) is in no sector\n", (unsigned)theSubSectors[i]->mID);
    std::vector<U2DPoint<long> > theShore, theIceShore;
    std::vector<unsigned short> theFreeSubSectors, theFreePathAreas;
    if (!ioReader.Points(theShore) || !ioReader.Points(theIceShore) || !ioReader.Shorts(theFreeSubSectors)
        || !ioReader.Shorts(theFreePathAreas) || !ioReader.Count(theCount))
        return false;
    ee_astar_cm_set_shore(&theManager.mRecord, EeAStarCmShore::Shore, U2DHullPoints(theShore), theShore.size());
    ee_astar_cm_set_shore(&theManager.mRecord, EeAStarCmShore::IceShore, U2DHullPoints(theIceShore), theIceShore.size());
    ee_astar_cm_add_free_ids(&theManager.mRecord, EeAStarCmFree::SubSectors, theFreeSubSectors.data(), theFreeSubSectors.size(), true);
    ee_astar_cm_add_free_ids(&theManager.mRecord, EeAStarCmFree::PathAreas, theFreePathAreas.data(), theFreePathAreas.size(), true);
    for (unsigned long i = 0; i < theCount; i++) {
        unsigned short theKey = 0;
        std::vector<EETileCoordinate> theTilesOf;
        if (!ioReader.U16(theKey) || !ioReader.Points(theTilesOf))
            return false;
        ee_astar_cm_remove_sector_tiles(&theManager.mRecord, theKey);   // `mSectorTiles[key] = tiles`
        for (size_t k = 0; k < theTilesOf.size(); k++)
            ee_astar_cm_add_sector_tile(&theManager.mRecord, theKey, U2DHullPointOf(theTilesOf[k]));
    }
    // the serial table: the manager's own areas now, the herds and towns as their players are repaired
    if (!ioReader.Count(theCount))
        return false;
    std::map<std::pair<long, long>, unsigned long> theTowns, theHerds;
    for (unsigned long i = 0; i < theCount; i++) {
        unsigned long theSerial = 0, theAreaSerial = 0;
        long theOwner = 0, theIndex = -1, thePlayer = -1;
        if (!ioReader.U32(theSerial) || !ioReader.I32(theOwner) || !ioReader.I32(theIndex) || !ioReader.I32(thePlayer)
            || !ioReader.U32(theAreaSerial))
            return false;
        EEAIArea* theArea = NULL;
        size_t theAt = (size_t)theIndex;
        switch (theOwner) {
        case kOwnerContinent: theArea = theAt < theManager.GetContinents().size() ? theManager.GetContinents()[theAt] : NULL; break;
        case kOwnerHill: theArea = theAt < theManager.GetHills().size() ? theManager.GetHills()[theAt] : NULL; break;
        case kOwnerPathArea: theArea = theAt < theManager.GetPathAreas().size() ? theManager.GetPathAreas()[theAt] : NULL; break;
        case kOwnerForest: theArea = theAt < theManager.GetForests().size() ? theManager.GetForests()[theAt] : NULL; break;
        case kOwnerChokePoint: theArea = theAt < theManager.GetChokePoints().size() ? theManager.GetChokePoints()[theAt] : NULL; break;
        case kOwnerRectangle: theArea = theAt < theManager.GetRectangleAreas().size() ? theManager.GetRectangleAreas()[theAt] : NULL; break;
        case kOwnerTown: theTowns[std::make_pair(thePlayer, theIndex)] = theSerial; break;
        case kOwnerHerd: theHerds[std::make_pair(thePlayer, theIndex)] = theSerial; break;
        default:
            ESDebugOut("EEAIContinentSave: serial %lu named an area of no known owner in the saved game\n", theSerial);
            break;
        }
        if (theArea != NULL)
            ee_astar_cm_add_area(&theManager.mRecord, theSerial, (uint64_t)(uintptr_t)theArea);
    }
    // the herds and towns the load made again: their saved serials (by player and id), else the load's
    unsigned long theUnmatched = 0;
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (size_t t = 0; t < thePlayer->mTowns.size(); t++) {
            EEAIArea* theTown = thePlayer->mTowns[t];
            if (theTown == NULL)
                continue;
            std::map<std::pair<long, long>, unsigned long>::iterator theIt =
                theTowns.find(std::make_pair((long)p, thePlayer->mTowns[t]->mTownID));
            if (theIt != theTowns.end()) {
                theTown->SetSerial(theIt->second);
                theTowns.erase(theIt);
            } else
                theUnmatched++;
            theManager.AddArea(theTown);
        }
        for (size_t h = 0; h < thePlayer->mHerds.size(); h++) {
            EEAIArea* theHerd = thePlayer->mHerds[h];
            if (theHerd == NULL)
                continue;
            std::map<std::pair<long, long>, unsigned long>::iterator theIt =
                theHerds.find(std::make_pair((long)p, (long)thePlayer->mHerds[h]->mID));
            if (theIt != theHerds.end()) {
                theHerd->SetSerial(theIt->second);
                theHerds.erase(theIt);
            } else
                theUnmatched++;
            theManager.AddArea(theHerd);
        }
    }
    if (theUnmatched != 0 || !theTowns.empty() || !theHerds.empty())
        ESDebugOut("EEAIContinentSave: %lu herds and towns of the load are not the saved ones (%lu saved towns, %lu saved herds left)\n",
                   theUnmatched, (unsigned long)theTowns.size(), (unsigned long)theHerds.size());
    return ioReader.mAt == ioReader.mBytes.size();
}

}   // namespace

void EEWriteAreaBytes(const EEAIArea& inArea, std::vector<unsigned char>& outBytes)
{
    Writer theWriter;
    WriteArea(theWriter, inArea);
    outBytes.swap(theWriter.mBytes);
}

bool EEReadAreaBytes(const std::vector<unsigned char>& inBytes, EEAIArea& outArea)
{
    Reader theReader(inBytes);
    return ReadArea(theReader, outArea) && theReader.mAt == theReader.mBytes.size();
}

long EESaveContinentRegions(FSFile& outFile)
{
    Writer theWriter;
    WriteRegions(theWriter);
    unsigned long theWritten = 0;
    long theResult = outFile.Write((unsigned long)theWriter.mBytes.size());
    if (theResult == kErrorOK && !theWriter.mBytes.empty())
        theResult = outFile.Write(&theWriter.mBytes[0], (unsigned long)theWriter.mBytes.size(), theWritten);
    return theResult;
}

long EELoadContinentRegions(FSFile& inFile)
{
    unsigned long theSize = 0;
    unsigned long theRead = 0;
    sLoadedRegions.clear();
    long theResult = inFile.Read(theSize);
    if (theResult == kErrorOK && theSize > 0) {
        sLoadedRegions.resize(theSize);
        theResult = inFile.Read(&sLoadedRegions[0], theSize, theRead);
        if (theResult == kErrorOK && theRead != theSize)
            theResult = kErrorBeyondEnd;
    }
    if (theResult != kErrorOK)
        sLoadedRegions.clear();
    return theResult;
}

bool EEContinentRegionsPending()
{
    return !sLoadedRegions.empty();
}

bool EERestoreContinentRegions()
{
    if (sLoadedRegions.empty())
        return false;
    std::vector<unsigned char> theBytes;
    theBytes.swap(sLoadedRegions);
    Reader theReader(theBytes);
    if (!ReadRegions(theReader)) {
        // a damaged block: the regions are built as an older save's load builds them
        ESDebugOut("EEAIContinentSave: the saved regions could not be read: the load builds its own\n");
        gContinentManager.Clear(gWorld.mMap);
        gContinentManager.BuildRegions(gWorld.mMap);
        for (unsigned long p = 0; p < gWorld.mNumPlayers; p++)
            if (gWorld.mPlayers[p] != NULL) {
                gWorld.mPlayers[p]->RegisterHerds();
                gWorld.mPlayers[p]->RegisterTowns();
            }
        return false;
    }
    EEAIContinentManager& theManager = gContinentManager;
    theManager.mRecord.clearing = sSavedClearing;
    // an area of the load past the saved counter (a herd or town the saved game did not have) keeps the counter
    EEAIListView<EeAStarCmArea> theAreas = theManager.GetAreas();
    if (!theAreas.empty() && theAreas[theAreas.size() - 1].serial > sSavedLastSerial) {
        ESDebugOut("EEAIContinentSave: an area of the load has serial %lu past the saved %lu: the counter stays\n",
                   (unsigned long)theAreas[theAreas.size() - 1].serial, sSavedLastSerial);
        theManager.mRecord.last_serial = theAreas[theAreas.size() - 1].serial;
    } else
        theManager.mRecord.last_serial = sSavedLastSerial;
    return true;
}

unsigned long EEContinentRegionsDigest()
{
    Writer theWriter;
    WriteRegions(theWriter);
    unsigned long theHash = 2166136261UL;
    for (size_t i = 0; i < theWriter.mBytes.size(); i++)
        theHash = (theHash ^ theWriter.mBytes[i]) * 16777619UL;
    return theHash;
}
