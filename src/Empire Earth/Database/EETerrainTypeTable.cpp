// C:\EEX\Empire Earth\Database\EETerrainTypeTable.cpp (name from RTTI EETerrainTypeTable; no __FILE__). 0xdbcc8..
// 0xdc682: the damage lookup 0xdbe44 and the damage table's readers. The table's load, the terrain types' list and the
// damage tables are header-only over ee_data and ee_combat since the game's leaves round 2 (ee/EEDatabaseTables.h).

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"

#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"

// the lookup's classes (included after the table's init chain, as EETerrainTable.cpp does)
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EERulesWorld.h"
#include "ee/EECombatFormulas.h"

// Returns the damage factor of an attack kind against a unit class (its family row), 0 when it cannot be attacked or a
// player is missing; the base table unless exactly one player (player 0 aside) is human, then a twin by difficulty
float EEToHitTable::GetAttackFactor(long inKind, EEUnitClass* inClass, unsigned long inPlayer, unsigned long inTargetPlayer)
{
    EeCombatAttackFactorIn theIn = {};
    theIn.attackable = inClass->CanBeAttacked();
    if (theIn.attackable) {
        EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
        EEPlayer* theTarget = gWorld.mPlayers[inTargetPlayer];
        theIn.players_present = thePlayer != NULL && theTarget != NULL;
        if (theIn.players_present) {
            // the scale by difficulty (gameoptions.yaml difficulties: dbworld difficulty_damage_scale F / F2 on easy
            // and hard, none on medium) when exactly one side is "human" (IsHuman, not player 0; provisional name)
            theIn.attacker_human = thePlayer->IsHuman() && inPlayer != 0;
            theIn.target_human = theTarget->IsHuman() && inTargetPlayer != 0;
            theIn.factor = EEDamageFactor(inClass->mFamily, inKind);
            const EERuleList<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
            unsigned long theDifficulty = gWorld.mGameOptions.mDifficulty;
            theIn.has_rule = theDifficulty < theList.size();
            if (theIn.has_rule) {
                theIn.scale_human_target = EERuleFloat(theList[theDifficulty].mScaleHumanTarget);
                theIn.scale_human_attacker = EERuleFloat(theList[theDifficulty].mScaleHumanAttacker);
            }
        }
    }
    return EECombatAttackFactor(theIn);
}


// gDamageTable[inRow][inColumn] (ee_combat's checked accessor: 0 off the table)
float EEDamageFactor(long inRow, long inColumn)
{
    return ee_combat_damage_factor((int32_t)inRow, (int32_t)inColumn);
}
