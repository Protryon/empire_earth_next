// C:\EEX\Empire Earth\Persistent\EEPCalamityHelper.cpp (__FILE__ 0x4c14d4, Save's ESVERIFYs). The saved calamities
// and contagion manager: 0x1497f4..0x149c3c. Unit flags: the exe module set. Data: the vtable
// 0x44220c.
#include <windows.h>
#include <stdio.h>
#include <utility>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/USingleton.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEPersistentTag.h"
#include "rust/game/gamecpp.h"

// Creates the saved-calamities chunk with no calamity list and a fresh contagion manager
EEPCalamityHelper::EEPCalamityHelper()
    : FSPersistent(kPersistentCalamities)
{
    ee_gamecpp_pch_reset(this, false, true);   // (gamecpp persist_more.rs; mCalamities NULL: its initializer)
}

// Destroys the chunk, deleting its calamity list and contagion manager
EEPCalamityHelper::~EEPCalamityHelper()
{
    ee_gamecpp_pch_reset(this, true, false);
}

// Deletes the calamity list
void EEPCalamityHelper::ClearCalamities()
{
    delete std::exchange(this->mCalamities, nullptr);
}

// Deletes the contagion manager
void EEPCalamityHelper::ClearContagionManager()
{
    delete std::exchange(this->mContagionManager, nullptr);
}

// Empties the chunk: no calamity list and a fresh contagion manager
void EEPCalamityHelper::Reset()
{
    ee_gamecpp_pch_reset(this, true, true);
}

// Copies another chunk's calamity list and contagion manager into fresh copies of its own
EEPCalamityHelper& EEPCalamityHelper::operator=(const EEPCalamityHelper& inOther)
{
    return *(EEPCalamityHelper*)ee_gamecpp_pch_assign(this, &inOther);   // (gamecpp persist_more.rs)
}

// Creates an empty saved-calamities chunk (persistent factory)
FSPersistent* EEPCalamityHelper::CreateDefault() const
{
    return new EEPCalamityHelper;
}

// Load, RepairPointers and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); their C++ calls and
// members are EEPCalamityOp's cases below.

// Reads the active calamities and then the contagion manager from a saved game; returns the contagion manager's error
long EEPCalamityHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_pch_load(this, inMajorVersion, inMinorVersion, &inFile);
}

// Reconnects the loaded calamities to their caster and target units and loads their media, deleting those whose units
// are gone; then reconnects the contagions
long EEPCalamityHelper::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return ee_gamecpp_pch_repair(this);
}

// Writes the calamity manager's active calamities (with their current target ids) and the world's contagions to a save
long EEPCalamityHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_pch_save(inMajorVersion, inMinorVersion, &outFile);
}

void EEPCalamityHelper::GetChecksum(class ESChecksum &) const
{
}

void EEPCalamityHelper::Virtual1C(long)
{
}

// ------------------------------------------------------------------------- the calamities' C++ (gamecpp persist_parts.rs)

int64_t EEPChecksumOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the calamity manager's active calamities (GetCalamities' out value)
static std::vector<EECalamity*>* EEActiveCalamities()
{
    std::vector<EECalamity*>* theList;
    USingleton<EECalamityManager>::GetInstance()->GetCalamities(theList);
    return theList;
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define PCH ((EEPCalamityHelper*)p)
#define CL ((EECalamity*)p)
#define CALAMITIES (*(std::vector<EECalamity*>*)p)
#define AFILE (*(FSFile*)a[2])

// The calamities' C++ calls and members (EEPMultiplayerHelper.cpp's EEPMultiplayerOp default): one statement each
int64_t EEPCalamityOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PchClearCalamities: PCH->ClearCalamities();
        break;
    case O::PchCalamities: return P64(PCH->mCalamities);
    case O::PchNewCalamities: PCH->mCalamities = new std::vector<EECalamity*>;
        break;
    case O::PchNewCalamity: return P64(new EECalamity);
    case O::PchCalamityLoad: return CL->Load((unsigned long)a[0], (unsigned long)a[1], AFILE);
    case O::PchCalamitySave: return CL->Save((unsigned long)a[0], (unsigned long)a[1], AFILE);
    case O::PchPush: PCH->mCalamities->push_back((EECalamity*)a[0]);
        break;
    case O::PchContagionLoad: return PCH->mContagionManager->Load((unsigned long)a[0], (unsigned long)a[1], AFILE);
    case O::PchCount: return (int64_t)PCH->mCalamities->size();
    case O::PchAt: return P64((*PCH->mCalamities)[a[0]]);
    case O::PchSetCaster: CL->SetCasterUnit((EEUnit*)a[0]);
        break;
    case O::PchPlaySound: CL->PlaySound();
        break;
    case O::PchDeleteAt: delete std::exchange((*PCH->mCalamities)[a[0]], nullptr);
        break;
    case O::PchRestoreContagions: return PCH->mContagionManager->RestorePointers(*PCH->mCalamities);
    case O::PchActive: return P64(EEActiveCalamities());
    case O::PchVerifyActive: ESVERIFY_AT(a[0] != 0, 334);
        break;
    case O::VecCalamitiesCount: return (int64_t)CALAMITIES.size();
    case O::VecCalamitiesAt: return P64(CALAMITIES[a[0]]);
    case O::PchClassIndex: return USingleton<EECalamityManager>::GetInstance()->GetCalamityClassIndex(CL->mClass, *(long*)a[0]);
    case O::PchVerifyClassIndex: ESVerify(__FILE__, 349, "EECalamityManager::Instance()->GetCalamityClassOffsetToSave(theCurrentCalamityClass, theCurrentClassOffset)");   // the original assert text
        break;
    case O::PchGetTarget: return CL->GetTargetUnit(*(EEUnit**)a[0]);
    case O::PchContagionSave: return gWorld.GetContagionManager()->Save((unsigned long)a[0], (unsigned long)a[1], AFILE);
    default: return EEPChecksumOp(inOp, p, a, f);
    }
    return 0;
}

// ------------------------------------------------------------------------- round 10, lane B: the chunk's own C++

int64_t EECore10OpB9(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETerrainColorTable.cpp

// the chunk's constructor's, destructor's, Reset's and copy's C++ (gamecpp persist_more.rs; p the chunk): one
// statement each
int64_t EECore10OpB8(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PchClearContagion: PCH->ClearContagionManager();
        break;
    case O::PchNewContagion: PCH->mContagionManager = new EEContagionManager;
        break;
    case O::PchContagion: return P64(PCH->mContagionManager);
    case O::PchCopyCalamities: *PCH->mCalamities = *((const EEPCalamityHelper*)a[0])->mCalamities;
        break;
    case O::PchCopyContagion: *PCH->mContagionManager = *((const EEPCalamityHelper*)a[0])->mContagionManager;
        break;
    default: return EECore10OpB9(inOp, p, a, f);
    }
    return 0;
}
