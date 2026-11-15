// C:\EEX\Empire Earth\AI Thread\EECPASGUAircraftCarrier.cpp (file name inferred: no __FILE__; RTTI class EECPASGUAircraftCarrier,
// vtable 0x4391e4). An attack sub group unit kind: 0x6132..0x619b (the previous EECPASGU* unit ends at 0x6132, EECPASGUAirFighter's ctor
// starts at 0x619b). Both ctors set the kind in the body; ??_G follows the first ctor (its first user); the dtor is
// out of line, last.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUAircraftCarrier.h"

// Creates the aircraft carrier entry for a unit in the sub-group
EECPASGUAircraftCarrier::EECPASGUAircraftCarrier(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindAircraftCarrier;
}

// Creates an empty aircraft carrier entry for the sub-group (no unit yet)
EECPASGUAircraftCarrier::EECPASGUAircraftCarrier(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindAircraftCarrier;
}

// Destroys the aircraft carrier entry (base members only)
EECPASGUAircraftCarrier::~EECPASGUAircraftCarrier()
{
}
