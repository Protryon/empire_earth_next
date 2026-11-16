// New (port), TEST ONLY: the civilization builder lane's records of the simulation capture (tools/sim_capture.py,
// every profile; ee/EESimCapture.h), recorded from the C++ manager, powers and civilization files before they moved
// to Rust (civilization builder lane, round 2). With EE_SIM_CAPTURE set:
//   CVMG  once, at the first ApplyUpgrade: the civilization manager's and the powers' data as the game reads them
//         (categories, levels, keys, premade civilizations, eras, powers) and every query over it (levels, costs,
//         keys both ways, the power levels, the premade civilizations' files loaded and matched)
//   CVAP  EECivilizationManager::ApplyUpgrade: the upgrade, the player, the result; the player's fields the levels
//         change and every class upgrade of the category's unit set before and after; a power level adds the
//         player's tech tree nodes, mining limit, loaded media count and HasPower of every power after
//   CVLD  EELoadCivilization: the name, the directory, the result, the name read, the upgrades
//   CVAF  EEApplyCivilizationFile: the name, the player, the result, the name after, the player's civilization after
// Off (EE_SIM_CAPTURE unset) each hook is one cached test.
#ifndef EE_EECIVCAPTURE_H
#define EE_EECIVCAPTURE_H

#include <vector>
#include "ee/EECivUpgrade.h"

class EEPlayer;
class UString;
class UWideString;

// CVAP around one ApplyUpgrade: Begin before it (the state before; NULL when off), End after it with its result
struct EECivCaptureApply;
EECivCaptureApply* EECivCaptureApplyBegin(EECivUpgrade inUpgrade, long inPlayer);
void EECivCaptureApplyEnd(EECivCaptureApply* inCapture, bool inResult);
// CVLD after one EELoadCivilization
void EECivCaptureLoad(const UString& inName, const UString& inDirectory, long inResult, const UWideString& inCivName,
                      const std::vector<EECivUpgrade>& inUpgrades);
// CVAF after one EEApplyCivilizationFile
void EECivCaptureApplyFile(const UString& inName, EEPlayer* inPlayer, long inResult);

#endif
