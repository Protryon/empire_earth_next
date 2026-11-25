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

// Copies another chunk's calamity list and contagion manager into fresh copies of its own
EEPCalamityHelper& EEPCalamityHelper::operator=(const EEPCalamityHelper& inOther)
{
    return *(EEPCalamityHelper*)ee_gamecpp_pch_assign(this, &inOther);   // (gamecpp persist_more.rs)
}

// Load, RepairPointers and Save are ee_game gamecpp persist_parts.rs (game core lane, round 9); their C++ calls and
// members are EEPCalamityOp's cases below.

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


