// C:\EEX\RTS Engine\RTS Engine\RTSWorld.cpp (__FILE__ 0x4c1304). The engine-side world (RTTI RTSWorld, vtable
// 0x4421bc), the base of EEWorld. LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/world.rs: the
// construction, the teardown, the population split, Clear, Reset, the checksum, the lookups, the placement queue, the
// player slots); the members forward, and the shims below are the C++ calls those bodies make (one statement each).
// The original's addresses and the bodies' history: git history before LLE round 8.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/RTSUnit.h"
#include "rust/game/rts.h"

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


// The shims world.rs calls (WorldCpp)
extern "C" {
EEPlayer** ee_rtswcpp_players(RTSWorld* inWorld) { return inWorld->mPlayers; }
unsigned long* ee_rtswcpp_num_players(RTSWorld* inWorld) { return &inWorld->mNumPlayers; }
long* ee_rtswcpp_population_limit(RTSWorld* inWorld) { return &inWorld->mPopulationLimit; }
long* ee_rtswcpp_player_population_limit(RTSWorld* inWorld) { return &inWorld->mPlayerPopulationLimit; }
EEMap** ee_rtswcpp_map_slot(RTSWorld* inWorld) { return &inWorld->mMap; }
void** ee_rtswcpp_lock_slot(RTSWorld* inWorld) { return reinterpret_cast<void**>(&inWorld->mLock); }
float* ee_rtswcpp_game_speed(RTSWorld* inWorld) { return &inWorld->mGameSpeed; }
float* ee_rtswcpp_camera_yaw(RTSWorld* inWorld) { return &inWorld->mCameraYaw; }
float* ee_rtswcpp_camera_pitch(RTSWorld* inWorld) { return &inWorld->mCameraPitch; }
bool ee_rtswcpp_player_is_active(EEPlayer* inPlayer) { return inPlayer->IsActive() == true; }
void ee_rtswcpp_player_recount(EEPlayer* inPlayer) { inPlayer->RecountPopulation(); }
void ee_rtswcpp_player_delete(EEPlayer* inPlayer) { delete inPlayer; }
void ee_rtswcpp_player_clear(EEPlayer* inPlayer) { inPlayer->Clear(); }
void ee_rtswcpp_player_checksum(EEPlayer* inPlayer, ESChecksum* ioChecksum) { inPlayer->GetChecksum(*ioChecksum); }
RTSUnit* ee_rtswcpp_player_unit(EEPlayer* inPlayer, unsigned long inUnitID) { return inPlayer->GetUnit(inUnitID); }
void ee_rtswcpp_map_delete(EEMap* inMap) { delete inMap; }
// the engine's member is an RTSMap* (EEMap's FSPersistent base makes the name ambiguous): RTSMap's slot 0
void ee_rtswcpp_map_checksum(EEMap* inMap, ESChecksum* ioChecksum) { static_cast<RTSMap*>(inMap)->GetChecksum(*ioChecksum); }
void ee_rtswcpp_checksum_add_world(ESChecksum* ioChecksum, unsigned long inPlayers) { ioChecksum->AddChecksum("RTSWorld", inPlayers); }
void ee_rtswcpp_update_population_limit(RTSWorld* inWorld, bool inRecount) { inWorld->UpdatePlayerPopulationLimit(inRecount); }
void ee_rtswcpp_place_queued_units(RTSWorld* inWorld) { inWorld->PlaceQueuedUnits(); }
size_t ee_rtswcpp_queued_size(RTSWorld* inWorld) { return inWorld->mQueuedPlacements.size(); }
RTSUnit* ee_rtswcpp_queued_unit(RTSWorld* inWorld, size_t inIndex) { return inWorld->mQueuedPlacements[inIndex].mUnit; }
void ee_rtswcpp_queued_clear(RTSWorld* inWorld) { inWorld->mQueuedPlacements.clear(); }
void ee_rtswcpp_queued_push(RTSWorld* inWorld, RTSUnit* inUnit, RTSUnit* inAfter) { inWorld->mQueuedPlacements.push_back(RTSWorldUnitEntry{inUnit, inAfter}); }
void ee_rtswcpp_unit_reference(RTSUnit* inUnit) { inUnit->Reference(); }
void ee_rtswcpp_unit_dereference(RTSUnit* inUnit) { inUnit->Dereference(); }
}
