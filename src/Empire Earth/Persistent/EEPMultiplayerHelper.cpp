// C:\EEX\Empire Earth\Persistent\EEPMultiplayerHelper.cpp (file name inferred: no __FILE__; the class family and its
// keys "EEPMultiplayerHelper-..."). The saved multiplayer state: 0x14ac35..0x14b22d. Unit flags: the exe's module set.
// Data: the vtable 0x44225c; the eight class static keys share the guard byte 0x531ab8.
// Load/Save keep the synchronous communications' tick counters next to the physics state; the counters come from the
// server or the client (dynamic_cast of gCommunications, slot +0x10 tells which).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

#include "ee/EECommunications.h"
#include "ee/EEPhysics.h"
#include "ee/EEPMultiplayerHelper.h"
#include "ee/EEPersistentTag.h"
#include "rust/game/gamecpp.h"


// Creates the saved multiplayer-state chunk
EEPMultiplayerHelper::EEPMultiplayerHelper()
    : FSPersistent(kPersistentMultiplayer)
{
}

// Destroys the saved multiplayer-state chunk
EEPMultiplayerHelper::~EEPMultiplayerHelper()
{
}

// Creates an empty saved multiplayer-state chunk (persistent factory)
FSPersistent* EEPMultiplayerHelper::CreateDefault() const
{
    return new EEPMultiplayerHelper;
}

// Load and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); their C++ calls are EEPMultiplayerOp's
// cases below.

// Reads the physics state and the eight network tick counters; restores only the server's or client's physics tick
// from them (the other counters are read and dropped); returns the first read error
long EEPMultiplayerHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_pmh_load(inMajorVersion, inMinorVersion, &inFile);
}

// Writes the physics state and the server's or client's network tick counters to a save (nothing for versions up to
// 0.12); returns the first write error
long EEPMultiplayerHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_pmh_save(inMajorVersion, inMinorVersion, &outFile);
}

// ------------------------------------------------------------------------- the multiplayer state's C++ (gamecpp persist_parts.rs)

int64_t EEPCalamityOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define SERVER dynamic_cast<NESynchronousServer*>(gCommunications)
#define CLIENT dynamic_cast<NESynchronousClient*>(gCommunications)
#define COMMS dynamic_cast<NESynchronousCommunications*>(gCommunications)

// The multiplayer state's C++ calls (EEPSaveGameHeader.cpp's EEPHeaderOp default): one statement each
int64_t EEPMultiplayerOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PmPhysicsLoad: return gPhysics->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)p);
    case O::PmPhysicsSave: return gPhysics->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)p);
    case O::PmServerSetPhysicsTick: SERVER->SetCurrentPhysicsTick(*(unsigned long*)a[0]);
        break;
    case O::PmClientSetPhysicsTick: CLIENT->SetCurrentPhysicsTick(*(unsigned long*)a[0]);
        break;
    case O::PmCommCtsProcessed: return COMMS->GetNumberOfCTsProcessedInPhysics();
    case O::PmCommLastTick: return COMMS->GetLastTickReceived();
    case O::PmServerLastCt: return SERVER->GetLastCTMessageID();
    case O::PmServerCt: return SERVER->GetCurrentCommunicationsTick();
    case O::PmServerPt: return SERVER->GetCurrentPhysicsTick();
    case O::PmServerMessageNumber: return SERVER->GetMessageManagerCurrentMessageNumber();
    case O::PmClientCt: return CLIENT->GetCurrentCommunicationsTick();
    case O::PmClientPt: return CLIENT->GetCurrentPhysicsTick();
    default: return EEPCalamityOp(inOp, p, a, f);
    }
    return 0;
}
