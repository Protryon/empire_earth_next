// C:\EEX\RTS Engine\RTS Engine\RTSWorld.cpp (__FILE__ 0x4c1304). The engine-side world (RTTI RTSWorld, vtable
// 0x4421bc), the base of EEWorld: 0x147dfd..0x148291 (the ctor to RTSDummyHeader<RTSUnit>'s ??_G; the exe's own
// naked `_ftol` 0x148291..0x1482e4 follows, a separate object). Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/RTSUnit.h"

// FUNCTION: EXE 0x547dfd
// Creates an empty world: no players or map, normal game speed, a population limit of 800
RTSWorld::RTSWorld()
{
    ESVERIFY_AT(TSScheduler::CreateReadWriteLock(this->mLock) == keseOK, 49);
    for (long i = 0; i < 17; i++) {
        this->mPlayers[i] = NULL;
    }
    this->mGameSpeed = 1.0f;
    this->mCameraYaw = 0.0f;
    this->mMap = NULL;
    this->mCameraPitch = 0.0f;
    this->mNumPlayers = 0;
    this->mPopulationLimit = 800;
}

// SYNTHETIC: EXE 0x547e9a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x547eb6
// Destroys the world's lock and its map
RTSWorld::~RTSWorld()
{
    TSScheduler::DestroyReadWriteLock(this->mLock);
    if (this->mMap != NULL) {
        delete this->mMap;
    }
}

// FUNCTION: EXE 0x547eee
// Splits the population limit among the active players (player 0 excluded), optionally recounting each
// player's population
void RTSWorld::UpdatePlayerPopulationLimit(bool inRecount)
{
    unsigned long theCount = 0;
    unsigned long i;
    for (i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL && this->mPlayers[i]->IsActive() == true) {
            theCount++;
        }
    }
    if (theCount > 0) {
        this->mPlayerPopulationLimit = this->mPopulationLimit / theCount;
    } else {
        this->mPlayerPopulationLimit = 0;
    }
    if (inRecount != true)
        return;
    for (i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->RecountPopulation();
        }
    }
}

// FUNCTION: EXE 0x547f71
// Deletes every player and the map
void RTSWorld::Clear()
{
    for (long i = 0; i < 17; i++) {
        if (this->mPlayers[i] != NULL) {
            delete this->mPlayers[i];
        }
        this->mPlayers[i] = NULL;
    }
    if (this->mMap != NULL) {
        delete this->mMap;
    }
    this->mMap = NULL;
}

// FUNCTION: EXE 0x547faf
// Adds every player, the map and the player count to the sync checksum
void RTSWorld::GetChecksum(ESChecksum& ioChecksum)
{
    unsigned long theNumPlayers;
    for (unsigned long i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->GetChecksum(ioChecksum);
        }
    }
    if (this->mMap != NULL) {
        // the engine's member is an RTSMap* (ee/RTSWorld.h declares the game's EEMap* view, whose FSPersistent base
        // makes the name ambiguous): the call is RTSMap's slot 0
        static_cast<RTSMap*>(this->mMap)->GetChecksum(ioChecksum);
    }
    theNumPlayers = this->mNumPlayers;
    ioChecksum.AddChecksum("RTSWorld", theNumPlayers);
}

// FUNCTION: EXE 0x548024
// Returns the player's index in the world, -1 if absent
long RTSWorld::GetPlayerIndex(EEPlayer* inPlayer)
{
    long theResult = -1;
    for (unsigned long i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] == inPlayer) {
            theResult = i;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x54804f
// Finds a unit by id through the player in the id's top byte; NULL for an out-of-range player
RTSUnit* RTSWorld::GetUnit(unsigned long inUnitID)
{
    unsigned long thePlayerIndex = inUnitID >> 24;
    if (thePlayerIndex < this->mNumPlayers) {
        return this->mPlayers[thePlayerIndex]->GetUnit(inUnitID);
    }
    return NULL;
}

// FUNCTION: EXE 0x548072
// Places the queued units, then releases and empties the placement queue
void RTSWorld::FlushQueuedPlacements()
{
    this->PlaceQueuedUnits();
    for (unsigned long i = 0; i < this->mQueuedPlacements.size(); i++) {
        this->mQueuedPlacements[i].mUnit->Dereference();
    }
    this->mQueuedPlacements.clear();
}

// FUNCTION: EXE 0x5480ad
// Queues a referenced unit for placement after another unit
void RTSWorld::AddUnitEntry(RTSUnit* inUnit, RTSUnit* inInsertAfter)
{
    RTSWorldUnitEntry theEntry;
    inUnit->Reference();
    theEntry.mUnit = inUnit;
    theEntry.mInsertAfter = inInsertAfter;
    this->mQueuedPlacements.push_back(theEntry);
}

// FUNCTION: EXE 0x5480d1
// Clears every player and deletes the map
void RTSWorld::Reset()
{
    for (long i = 0; i < 17; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->Clear();
        }
    }
    if (this->mMap != NULL) {
        delete this->mMap;
    }
    this->mMap = NULL;
}

// FUNCTION: EXE 0x54810b
// Sets the camera, deriving its view line, yaw and pitch, and reports the change
void RTSWorld::SetCameraTransformation(const GETransformation& inTransformation)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    this->mTransformation = inTransformation;
    GE3DPoint thePosition(inTransformation.GetX(), inTransformation.GetY(), inTransformation.GetZ());
    inTransformation.Apply(theForward, theDirection);
    this->mLine.SetStart(thePosition);
    this->mLine.SetDirection(theDirection);
    this->mCameraYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    this->mCameraPitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    this->CameraChanged(inTransformation);
}

// FUNCTION: EXE 0x5481c9
// Puts a player in a slot, deleting the slot's previous player
void RTSWorld::SetPlayer(long inIndex, EEPlayer* inPlayer)
{
    if (inPlayer == this->mPlayers[inIndex])
        return;
    if (this->mPlayers[inIndex] != NULL) {
        delete this->mPlayers[inIndex];
    }
    this->mPlayers[inIndex] = inPlayer;
}

// FUNCTION: EXE 0x5481ee
// Sets the player count, then re-splits the population limit and recounts every player's population
void RTSWorld::SetNumPlayers(unsigned long inNumPlayers)
{
    this->mNumPlayers = inNumPlayers;
    this->UpdatePlayerPopulationLimit(true);
}

// FUNCTION: EXE 0x548202
// Sets the world's total population limit
void RTSWorld::SetPopulationLimit(unsigned long inLimit)
{
    this->mPopulationLimit = inLimit;
}

// TEMPLATE: EXE 0x54820f
// std::vector<RTSWorldUnitEntry>::push_back (first user 0x1480ad)

// TEMPLATE: EXE 0x54823b
// std::vector<RTSWorldUnitEntry>::_Buy (the ctor's vector(); ICF-folded name vector<EETileCoordinate>::_Buy)

// SYNTHETIC: EXE 0x548274
// RTSDummyHeader<RTSUnit>'s scalar deleting destructor (shared with RTSDummyUnit)
