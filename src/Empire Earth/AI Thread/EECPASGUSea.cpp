// C:\EEX\Empire Earth\AI Thread\EECPASGUSea.cpp (file name inferred: no __FILE__; RTTI class EECPASGUSea,
// vtable 0x439948). An attack sub group unit kind: 0xd1b6..0xd22b (EECPASGUProphetCalamityAnalyzer*'s code ends at 0xd1b6,
// EECPASGUSpace's ctor starts at 0xd22b). Both ctors set the kind in the body; ??_G follows the first ctor (its first user);
// the dtor and Virtual6C are out of line.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUSea.h"

// Creates a sea sub group unit for a unit
EECPASGUSea::EECPASGUSea(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindSea;
}

// Creates an empty sea sub group unit (no unit yet; used when loading)
EECPASGUSea::EECPASGUSea(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindSea;
}

// Destroys the sea sub group unit (base members only)
EECPASGUSea::~EECPASGUSea()
{
}

// Updates the unit like any sub group unit
void EECPASGUSea::Update(unsigned long inTime)
{
    EECPAttackSubGroupUnit::Update(inTime);
}
