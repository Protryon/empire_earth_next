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

#include "ee/EECalamityManager.h"
#include "ee/EEPCalamityHelper.h"

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


// the calamity manager's active calamities (GetCalamities' out value)
std::vector<EECalamity*>* EEActiveCalamities()
{
    std::vector<EECalamity*>* theList;
    USingleton<EECalamityManager>::GetInstance()->GetCalamities(theList);
    return theList;
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define PCH ((EEPCalamityHelper*)p)
#define CL ((EECalamity*)p)
#define CALAMITIES (*(std::vector<EECalamity*>*)p)
#define CAFILE (*(FSFile*)a[2])

// ------------------------------------------------------------------------- round 10, lane B: the chunk's own C++


