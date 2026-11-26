// C:\EEX\Empire Earth\Calamities\EECalamityManager.cpp (__FILE__ 0x4b9850). The calamity classes and the active
// calamities: 0xadf97..0xaebb7. Unit flags: the exe's module set.
// The unit closes with its template COMDATs: vector<EECalamity*>::push_back (0xaea04), USingleton's ??_G (0xaea30),
// DeleteVectorContents<EECalamity> (0xaea54), GetCalamityClass<EEGlobalCalamityClass> (0xaea85),
// <EELocationCalamityClass> (0xaeaec), DeleteVectorContents<EECalamityClass> (0xaeb53). The three bases' units
// (EEGlobalCalamityClass.cpp, EELocationCalamityClass.cpp 0xb6f21, EEUnitCalamityClass.cpp 0xb70cc) are folded into
// EECalamityClass and the creations below (calamities lane, round 4).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"



// Creates a unit of the class at the transformation for the player (-1: the calamity's player); NULL if the class
// does not exist
// cdecl; the hurricane and volcano calamities call it (provisional name). It sits after the unit's template COMDATs
// in the original.
EEUnit* EECreateCalamityUnit(EECalamity* inCalamity, long inUnitClass, GETransformation& inTransformation,
                             long inPlayerNumber)
{
    return (EEUnit*)ee_gamecpp_calamity_create_unit(inCalamity, inUnitClass, &inTransformation, inPlayerNumber);
}
