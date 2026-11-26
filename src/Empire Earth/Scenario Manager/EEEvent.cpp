// C:\EEX\Empire Earth\Scenario Manager\EEEvent.cpp (file name inferred: no __FILE__): 0x1ca4ee..0x1cba2f,
// the data-driven events and event effects. Unit flags: the exe module set.
#include <windows.h>
#include <vector>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEEvent.h"
#include "ee/EEGameVariant.h"
#include "lle/UResourceFile.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEScenarioHost.h"
#include "ee/EEUnitUtilities.h"   // EEReplaceQueuedTechnology

// The pathfinding functor of attribute 42 (its step: ee_sim scenario event.rs)
class EEUnitSetFunctorModifyPathfindingBehavior : public EEUnitSetFunctor {
public:
    EEUnitSetFunctorModifyPathfindingBehavior(EEPlayer* inPlayer, long inType, bool inEnabled)
        : mPlayer(inPlayer), mType(inType), mEnabled(inEnabled) {}
    virtual bool Execute(long inUnitType) { return ee_scenario_ev_pathfinding(this->mPlayer, this->mType, this->mEnabled, inUnitType); }
    void ForEachIn(EEUnitSet* inSet) { inSet->ForEach(this); }
    EEPlayer* mPlayer;
    long mType;
    bool mEnabled;
};

// this unit's global (ee/EEEvent.h): the switch (.data 0x4c5a84 = 1, between EESMObject's and EETechTree's data); the
// effect and event tables are ee_sim scenario event.rs'
bool gEventsEnabled = true;

// chain at 0x5ca4ee (wrapper)
//   object 0x95cf80  ctor ??0EEEvent@@QAE@XZ  dtor ??1EEEvent@@UAE@XZ
//   guarded dtor (byte ptr [0x95cf78], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sEvents@EEEvent@@2PAV1@A

// (EEUnloadBuiltInGraphicObjects 0x1ca6d9 unloaded four graphic objects outside the media table: nothing ever set
// them, so it is gone with them; data lane round 3)


namespace {

unsigned long sEvRead = 0;   // a read's byte count (LoadEvents' theRead)
char sEvName[100];           // an event's name (read and dropped)

UWideString sEvText;         // an upgrade's new name (EE_SCENARIO_EV_NAME_TEXT)

}   // namespace

#define P(T, k) reinterpret_cast<T*>((uintptr_t)a[k])
#define R(x) (int64_t)(uintptr_t)(x)

// the events' single statements (ee_sim scenario event.rs EE_SCENARIO_EV_*: the Rust composes the effects)
extern "C" int64_t ee_scenariocpp_event(uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_EV_EVENTS_ENABLED: return gEventsEnabled;
    case EE_SCENARIO_EV_UPGRADE: return R(P(EEPlayer, 0)->mUnitClassUpgrades[a[1]]);
    case EE_SCENARIO_EV_GET_UPGRADE: return R(P(EEPlayer, 0)->GetUpgrade(a[1]));
    case EE_SCENARIO_EV_MAKE_AREA_EFFECTS:
        P(EEUnit, 0)->mComplexUnit->MakeAreaEffects();
        return 0;
    case EE_SCENARIO_EV_SIGHT_OFF:
        P(EEUnit, 0)->SightOffHere();
        return 0;
    case EE_SCENARIO_EV_SIGHT_ON:
        P(EEUnit, 0)->SightOnHere();
        return 0;
    case EE_SCENARIO_EV_SET_FLIGHT:
        P(EEUnit, 0)->mComplexUnit->SetFlightTime((long)a[1]);
        return 0;
    case EE_SCENARIO_EV_SET_HIT_POINTS:
        P(EEUnit, 0)->SetHitPoints((long)a[1]);
        return 0;
    case EE_SCENARIO_EV_NODE_SET_BUTTON:
        P(EETechTreeNode, 0)->SetButton(a[1]);
        return 0;
    case EE_SCENARIO_EV_RESEARCH_NODE: return (int64_t)P(EETechnology, 0)->ResearchNode(NULL);
    case EE_SCENARIO_EV_BUTTON: return R(EEButtonObject::Get((unsigned long)a[0]));
    case EE_SCENARIO_EV_SET_CLASS_BUTTON:
        P(EEPlayer, 0)->SetClassButton((unsigned long)a[1], P(EEButtonObject, 2));
        return 0;
    case EE_SCENARIO_EV_GROUP_FIRST: return R(P(EEPlayer, 0)->mGroups[a[1]]->GetFirstUnit());
    case EE_SCENARIO_EV_UI_EXISTS: return gUserInterface != NULL;
    case EE_SCENARIO_EV_POST_REFRESH_GROUP:
        gUserInterface->PostRefreshSavedGroup(a[0]);
        return 0;
    case EE_SCENARIO_EV_POST_BACKGROUND:
        gUserInterface->PostUpdateBackgroundMedia();
        return 0;
    case EE_SCENARIO_EV_IS_LATEST: return EETechTree::IsLatestEffect(P(EEPlayer, 0)->GetTechTree(), a[1], a[2], a[3]);
    case EE_SCENARIO_EV_MEDIA: return R(static_cast<EEGraphicObject*>(EEMedia::Get(a[0])));
    case EE_SCENARIO_EV_UPGRADE_MODEL: return R(P(EEUnitClassUpgrade, 0)->mModel);
    case EE_SCENARIO_EV_SET_CLASS_MODEL:
        P(EEPlayer, 0)->SetClassModel((unsigned long)a[1], P(EEGraphicObject, 2));
        return 0;
    case EE_SCENARIO_EV_SWAP_GRAPHIC:
        P(EEComplexUnit, 0)->SwapGraphic(P(EEGraphicObject, 1), P(EEGraphicObject, 2));
        return 0;
    case EE_SCENARIO_EV_SOUND: return R(EESoundObject::Get(a[0]));
    case EE_SCENARIO_EV_SET_ACKNOWLEDGE:
        P(EEPlayer, 0)->SetClassAcknowledgeSound((unsigned long)a[1], P(EESoundObject, 2), (long)a[3]);
        return 0;
    case EE_SCENARIO_EV_SET_CREATION:
        P(EEPlayer, 0)->SetClassCreationSound((unsigned long)a[1], P(EESoundObject, 2));
        return 0;
    case EE_SCENARIO_EV_SET_DEATH:
        P(EEPlayer, 0)->SetClassDeathSound((unsigned long)a[1], P(EESoundObject, 2));
        return 0;
    case EE_SCENARIO_EV_SET_SELECTION:
        P(EEPlayer, 0)->SetClassSelectionSound((unsigned long)a[1], P(EESoundObject, 2), (long)a[3]);
        return 0;
    case EE_SCENARIO_EV_PLAY:
        P(EESoundObject, 0)->Play();
        return 0;
    case EE_SCENARIO_EV_MAKE_OBSOLETE:
        P(EEPlayer, 0)->mTechnologies->MakeObsolete(a[1]);
        return 0;
    case EE_SCENARIO_EV_SET_PREREQUISITE:
        P(EEPlayer, 0)->mTechnologies->SetPrerequisite(a[1], a[2]);
        return 0;
    case EE_SCENARIO_EV_SET_CONTAINS: return gUnitSetManager.GetSet(a[0])->Contains(a[1]);
    case EE_SCENARIO_EV_COPY_LEVELS:
        P(EEUnitClassUpgrade, 0)->CopyUpgradeLevels(P(EEUnitClassUpgrade, 1));
        return 0;
    case EE_SCENARIO_EV_REPLACE_QUEUED:
        EEReplaceQueuedTechnology(P(EEPlayer, 0), P(EEUnitClass, 1), (long)a[2]);
        return 0;
    case EE_SCENARIO_EV_MOVE_QUEUED:
        P(EEPlayer, 0)->MoveQueuedImprovements((unsigned long)a[1], (unsigned long)a[2]);
        return 0;
    case EE_SCENARIO_EV_REPLACE_UNIT:
        return R(EEReplaceUnit(static_cast<EEComplexUnit*>(P(EEUnit, 0)), (unsigned long)a[1], P(EEUnitClass, 2)));
    case EE_SCENARIO_EV_SET_BEHAVIOR:
        P(EEComplexUnit, 0)->SetBehavior(P(EEComplexUnit, 0)->GetClass()->mBehavior);
        return 0;
    case EE_SCENARIO_EV_VARIANT_FACTOR: return EESMBits(gGameVariants[gWorld.mGameOptions.GetVariant()]->GetFactor(a[0]));
    case EE_SCENARIO_EV_CLASSES_NEW: return R(new std::vector<EEUnitClass*>);
    case EE_SCENARIO_EV_CLASSES_IN_SET:
        EEUnitClass::GetClassesInSet((unsigned long)a[1], *P(std::vector<EEUnitClass*>, 0));
        return 0;
    case EE_SCENARIO_EV_CLASSES_SIZE: return (int64_t)P(std::vector<EEUnitClass*>, 0)->size();
    case EE_SCENARIO_EV_CLASSES_AT: return R((*P(std::vector<EEUnitClass*>, 0))[(size_t)a[1]]);
    case EE_SCENARIO_EV_CLASSES_FREE:
        delete P(std::vector<EEUnitClass*>, 0);
        return 0;
    case EE_SCENARIO_EV_SET_RANGE:
        P(EEUnitClassUpgrade, 0)->SetRange(EESMFloat(a[1]));
        return 0;
    case EE_SCENARIO_EV_SET_MAX_VELOCITY:
        P(EEUnitClassUpgrade, 0)->SetMaxVelocity(EESMFloat(a[1]));
        return 0;
    case EE_SCENARIO_EV_UNIT_SET: return R(gUnitSetManager.GetSet(a[0]));
    case EE_SCENARIO_EV_PATHFINDING:
        EEUnitSetFunctorModifyPathfindingBehavior(P(EEPlayer, 1), (long)a[2], a[3] != 0).ForEachIn(P(EEUnitSet, 0));
        return 0;
    case EE_SCENARIO_EV_NAME_TEXT:
        gResource->GetString(UMFloatToLong(EESMFloat(a[0])), sEvText);
        return 0;
    case EE_SCENARIO_EV_SET_NAME_TEXT:
        P(EEUnitClassUpgrade, 0)->SetName(sEvText);
        return 0;
    case EE_SCENARIO_EV_FILE_OPEN: return P(FSFileSpec, 0)->Open(kFileAccessRead);
    case EE_SCENARIO_EV_FILE_LENGTH_INTO: return P(FSFileSpec, 0)->GetLength(*P(unsigned long, 1));
    case EE_SCENARIO_EV_EFFECTS_READ:
        return P(FSFileSpec, 0)->Read(P(char, 1), (unsigned long)a[2], sEvRead);   // (the Rust table's bytes: event.rs)
    case EE_SCENARIO_EV_READ_NAME: return P(FSFileSpec, 0)->Read(sEvName, 100, sEvRead);
    case EE_SCENARIO_EV_READ_EFFECTS:
        return P(FSFileSpec, 0)->Read(reinterpret_cast<char*>(P(EEEvent, 1)->mEffects), P(EEEvent, 1)->mNumEffects * 4, sEvRead);
    }
    return 0;
}

#undef P
#undef R

// Returns the button object with the given index, or NULL when out of range
EEButtonObject* EEButtonObject::Get(unsigned long inIndex)
{
    if (inIndex < EEButtonObject::sTable.size())
        return EEButtonObject::sTable[inIndex];
    return NULL;
}
