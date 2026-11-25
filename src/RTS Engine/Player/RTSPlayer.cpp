// C:\EEX\RTS Engine\Player\RTSPlayer.cpp (file name inferred: no __FILE__; the six "RTSPlayer-*" key chains). The
// engine-side player (RTTI RTSPlayer, vtable 0x442b5c; class in ee/EEPlayer.h). LLE round 8: the unit list's bodies
// are Rust (rust/crates/ee_game/src/rts/units.rs: the pending and killed units, the id buckets, the checksum, the
// units' save order, Reset); the members forward, and the shims below are the C++ calls those bodies make (one
// statement each). The bodies' history: git history before LLE round 8.

#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"

static_assert(sizeof(RTSUnitBucket) == sizeof(EeRtsUnitBucket), "a bucket");

// Creates an empty player with no units
RTSPlayer::RTSPlayer()
{
    this->Reset();
}

// Creates a named player with the given id; every player but player 0 keeps a visibility map
RTSPlayer::RTSPlayer(const UWideString& inName, unsigned long inPlayerID)
{
    this->Reset();
    this->mName = inName;
    this->mPlayerID = inPlayerID;
    this->mSaveVisMap = inPlayerID != 0;
}

// Destroys the player
RTSPlayer::~RTSPlayer()
{
    EEUnitStoreClearPlayer(this);   // New (R5b.4): its list's records leave the unit store (ee/EEUnitStore.h)
}

void RTSPlayer::AddPendingUnits(bool inKeepID, bool inRelease) { ee_rts_player_add_pending_units(this, inKeepID, inRelease); }
void RTSPlayer::AddUnit(RTSUnit* inUnit, bool inKeepID) { ee_rts_player_add_unit(this, inUnit, inKeepID); }
void RTSPlayer::RemoveKilledUnits() { ee_rts_player_remove_killed_units(this); }
void RTSPlayer::ReleasePendingUnits() { ee_rts_player_release_pending_units(this); }
void RTSPlayer::GetChecksum(ESChecksum& ioChecksum) { ee_rts_player_checksum(this, &ioChecksum); }
RTSUnit* RTSPlayer::GetPendingUnit(unsigned long inIndex) { return static_cast<RTSUnit*>(ee_rts_player_pending_unit(this, inIndex)); }
RTSUnit* RTSPlayer::GetUnit(unsigned long inUnitID) { return static_cast<RTSUnit*>(ee_rts_player_unit(this, inUnitID)); }
void RTSPlayer::Reset() { ee_rts_player_reset(this); }
void RTSPlayer::InsertUnit(RTSUnit* inUnit) { ee_rts_player_insert_unit(this, inUnit); }

// Reads the player's vision flags, ids, color (deriving the shade color), name, units and observer mode;
// returns the first read error.
// port (R5b.4, round 29): the body is Rust (ee_units rts_player_save.rs `load`) over the record this class derives
// from (EeUnitsRtsPlayerFields) and RTSPlayerSaveOperation's parts
struct RTSPlayerSaveContext {
    RTSPlayer* mPlayer;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
};

static int32_t RTSPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t, EeUnitsPlayerSaveIo* io)
{
    RTSPlayerSaveContext& theContext = *static_cast<RTSPlayerSaveContext*>(inContext);
    RTSPlayer* thePlayer = theContext.mPlayer;
    FSFile& theFile = *theContext.mFile;
    switch (inOp) {
    case EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD: return thePlayer->mColor.Load(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_SHADE:
        thePlayer->mShadeColor.SetRed(thePlayer->mColor.GetRed() * 0.6f);
        thePlayer->mShadeColor.SetGreen(thePlayer->mColor.GetGreen() * 0.6f);
        thePlayer->mShadeColor.SetBlue(thePlayer->mColor.GetBlue() * 0.6f);
        return 0;
    case EE_UNITS_RTS_PLAYER_SAVE_NAME_LOAD: return thePlayer->mName.Load(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_LOAD_UNIT:
        return thePlayer->LoadUnit(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING: thePlayer->AddPendingUnits(true, false); return 0;
    case EE_UNITS_RTS_PLAYER_SAVE_COLOR_SAVE: return thePlayer->mColor.Save(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE: return thePlayer->mName.Save(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS:
        return thePlayer->SaveUnits(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    }
    return EESavePrimitive(theFile, inOp, io);
}

long RTSPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    RTSPlayerSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion};
    return ee_units_rts_player_load(&theContext, &RTSPlayerSaveOperation, this, (uint32_t)inMajorVersion,
                                    (uint32_t)inMinorVersion);
}

void RTSPlayer::KillUnit(RTSUnit* inUnit, bool inForce) { ee_rts_player_kill_unit(this, inUnit, inForce); }
long RTSPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond) { return ee_rts_player_repair_pointers(this, inFirst, inSecond); }

long RTSPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    RTSPlayerSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion};
    return ee_units_rts_player_save(&theContext, &RTSPlayerSaveOperation, this);
}

long RTSPlayer::SaveUnits(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_rts_player_save_units(this, inMajorVersion, inMinorVersion, &outFile); }

void RTSPlayer::Clear()
{
}


// The shims units.rs calls (UnitsCpp: the player's side)
extern "C" {
EeUnitsRtsPlayerFields* ee_rtspcpp_player_fields(RTSPlayer* inPlayer) { return inPlayer; }
RTSUnitBucket* ee_rtspcpp_buckets(RTSPlayer* inPlayer) { return inPlayer->mUnitBuckets; }
EEUnit** ee_rtspcpp_first_slot(RTSPlayer* inPlayer) { return &inPlayer->mFirstUnit; }
EEUnit** ee_rtspcpp_last_slot(RTSPlayer* inPlayer) { return &inPlayer->mLastUnit; }
size_t ee_rtspcpp_pending_size(RTSPlayer* inPlayer) { return inPlayer->mPendingUnits.size(); }
RTSUnit* ee_rtspcpp_pending_at(RTSPlayer* inPlayer, size_t inIndex) { return inPlayer->mPendingUnits[inIndex]; }
void ee_rtspcpp_pending_clear(RTSPlayer* inPlayer) { inPlayer->mPendingUnits.clear(); }
size_t ee_rtspcpp_killed_size(RTSPlayer* inPlayer) { return inPlayer->mKilledUnits.size(); }
RTSUnit* ee_rtspcpp_killed_at(RTSPlayer* inPlayer, size_t inIndex) { return inPlayer->mKilledUnits[inIndex]; }
void ee_rtspcpp_killed_clear(RTSPlayer* inPlayer) { inPlayer->mKilledUnits.clear(); }
void ee_rtspcpp_killed_push(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->mKilledUnits.push_back(inUnit); }
void ee_rtspcpp_add_unit(RTSPlayer* inPlayer, RTSUnit* inUnit, bool inKeepID) { inPlayer->AddUnit(inUnit, inKeepID); }
void ee_rtspcpp_release_pending(RTSPlayer* inPlayer) { inPlayer->ReleasePendingUnits(); }
void ee_rtspcpp_removing_unit(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->RemovingUnit(inUnit); }
void ee_rtspcpp_unit_removed(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->UnitRemoved(inUnit); }
void ee_rtspcpp_removals_done(RTSPlayer* inPlayer) { inPlayer->RemovalsDone(); }
long ee_rtspcpp_save_unit(RTSPlayer* inPlayer, RTSUnit* inUnit, unsigned long inMajor, unsigned long inMinor, FSFile* inFile) { return inPlayer->SaveUnit(inUnit, inMajor, inMinor, *inFile); }
void ee_rtspcpp_name_checksum(RTSPlayer* inPlayer, ESChecksum* ioChecksum) { inPlayer->mName.GetChecksum(*ioChecksum); }
void ee_rtspcpp_checksum_add(ESChecksum* ioChecksum, const char* inName, unsigned long inValue) { ioChecksum->AddChecksum(inName, inValue); }
void ee_rtsucpp_store_insert(RTSUnit* inUnit, unsigned long inOwner, RTSPlayer* inPlayer) { EEUnitStoreInsert(inUnit, inOwner, inPlayer); }
}
