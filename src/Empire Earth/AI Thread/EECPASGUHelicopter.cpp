// C:\EEX\Empire Earth\AI Thread\EECPASGUHelicopter.cpp (file name inferred: no __FILE__; RTTI class EECPASGUHelicopter,
// vtable 0x4392e8). An attack sub group unit kind: 0x6328..0x6391 (the previous EECPASGU* unit ends at 0x6328, EECPASGUHero's ctor
// starts at 0x6391). Both ctors set the kind in the body; ??_G follows the first ctor (its first user); the dtor is
// out of line, last.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUHelicopter.h"

// Creates the helicopter entry for a unit in the sub-group
EECPASGUHelicopter::EECPASGUHelicopter(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindHelicopter;
}

// Creates an empty helicopter entry for the sub-group (no unit yet)
EECPASGUHelicopter::EECPASGUHelicopter(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindHelicopter;
}

// Destroys the helicopter entry (base members only)
EECPASGUHelicopter::~EECPASGUHelicopter()
{
}
