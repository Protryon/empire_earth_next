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

// this unit's globals (ee/EEEvent.h): the effect table (.bss 0x531ff8, 0x532018.. are this unit's) and the switch
// (.data 0x4c5a84 = 1, between EESMObject's and EETechTree's data)
std::vector<EEEventEffect> gEventEffects;
bool gEventsEnabled = true;

// chain at 0x5ca4ee (wrapper)
//   object 0x95cf80  ctor ??0EEEvent@@QAE@XZ  dtor ??1EEEvent@@UAE@XZ
//   guarded dtor (byte ptr [0x95cf78], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sEvents@EEEvent@@2PAV1@A

// Creates an event with no effects
EEEvent::EEEvent() : mEffects(NULL), mNumEffects(0), mID(0)
{
}

// Destroys the event, freeing its effect index list
EEEvent::~EEEvent()
{
    if (this->mEffects != NULL) {
        delete[] this->mEffects;
    }
}

std::vector<std::unique_ptr<EEEvent>> EEEvent::sEvents;

// Loads every event effect and every event with its effect list (as many as the files hold: the original read
// exactly 2000 effects and 1500 events); returns the last read's result (ee_sim scenario event.rs)
long EEEvent::LoadEvents(FSFileSpec& inEvents, FSFileSpec& inEffects)
{
    return ee_scenario_ev_load(&inEvents, &inEffects);
}

// (EEUnloadBuiltInGraphicObjects 0x1ca6d9 unloaded four graphic objects outside the media table: nothing ever set
// them, so it is gone with them; data lane round 3)

// Runs every effect of an event for a player (a given player's own index wins); returns the last effect's result.
// RunEffect 0x1ca7cf and its handlers (the stats, button, model, sounds, research, unit conversion, with the EVRN
// and EVAT records) are ee_sim scenario event.rs, over the statements below
long EEEvent::Trigger(long inEvent, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit)
{
    return ee_scenario_ev_trigger(inEvent, inPlayerIndex, inID, inPlayer, inUnit);
}


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
    case EE_SCENARIO_EV_EFFECTS_DATA: return R(gEventEffects.data());
    case EE_SCENARIO_EV_EFFECTS_COUNT: return (int64_t)gEventEffects.size();
    case EE_SCENARIO_EV_LOADING: return gLoadingSavedGame;
    case EE_SCENARIO_EV_EVENTS_ENABLED: return gEventsEnabled;
    case EE_SCENARIO_EV_PLAYER: return R(gWorld.mPlayers[a[0]]);
    case EE_SCENARIO_EV_CURRENT_PLAYER: return R(gWorld.GetCurrentPlayer());
    case EE_SCENARIO_EV_PLAYER_ID: return (int64_t)P(EEPlayer, 0)->mPlayerID;
    case EE_SCENARIO_EV_PLAYER_FIELDS: return R(static_cast<EeUnitsPlayerFields*>(P(EEPlayer, 0)));
    case EE_SCENARIO_EV_CLASS: return R(gUnitClassTable[a[0]]);
    case EE_SCENARIO_EV_CLASS_FIELDS: return R(static_cast<EeUnitsUnitClassFields*>(P(EEUnitClass, 0)));
    case EE_SCENARIO_EV_CLASS_COUNT: return (int64_t)EEUnitClassCount();
    case EE_SCENARIO_EV_UPGRADE: return R(P(EEPlayer, 0)->mUnitClassUpgrades[a[1]]);
    case EE_SCENARIO_EV_GET_UPGRADE: return R(P(EEPlayer, 0)->GetUpgrade(a[1]));
    case EE_SCENARIO_EV_UPGRADE_FIELDS: return R(static_cast<EeUnitsUnitClassUpgradeFields*>(P(EEUnitClassUpgrade, 0)));
    case EE_SCENARIO_EV_UPGRADE_CLASS: return R(P(EEUnitClassUpgrade, 0)->mUnitClass);
    case EE_SCENARIO_EV_FIRST_UNIT: return R(P(EEPlayer, 0)->mFirstUnit);
    case EE_SCENARIO_EV_NEXT_UNIT: return R(static_cast<EEUnit*>(P(EEUnit, 0)->mNext));
    case EE_SCENARIO_EV_PENDING_COUNT: return (int64_t)P(EEPlayer, 0)->GetNumPendingUnits();
    case EE_SCENARIO_EV_PENDING_UNIT: return R(static_cast<EEUnit*>(P(EEPlayer, 0)->GetPendingUnit((unsigned long)a[1])));
    case EE_SCENARIO_EV_UNIT_TYPE: return (int64_t)P(EEUnit, 0)->mUnitClass->mUnitType;
    case EE_SCENARIO_EV_UNIT_HIT_POINTS: return (int64_t)P(EEUnit, 0)->GetHitPoints();
    case EE_SCENARIO_EV_UNIT_COMPLEX: return R(P(EEUnit, 0)->mComplexUnit);
    case EE_SCENARIO_EV_COMPLEX_FLIGHT: return (int64_t)P(EEComplexUnit, 0)->mFlightTime;
    case EE_SCENARIO_EV_COMPLEX_BUILT: return P(EEComplexUnit, 0)->mIsBuilt == true;
    case EE_SCENARIO_EV_UNIT_ON_TILE: return P(EEUnit, 0)->mTileNode != NULL;
    case EE_SCENARIO_EV_COMPLEX_CARRIED: return P(EEComplexUnit, 0)->mPreviousPart != NULL;
    case EE_SCENARIO_EV_UNIT_ID: return (int64_t)P(EEUnit, 0)->mUnitID;
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
    case EE_SCENARIO_EV_GET_COMPLEX: return R(P(EEUnit, 0)->GetComplexUnit());
    case EE_SCENARIO_EV_COMPLEX_CLASS: return R(P(EEComplexUnit, 0)->GetUnitClass());
    case EE_SCENARIO_EV_COMPLEX_CLASS_TYPE: return (int64_t)P(EEComplexUnit, 0)->GetClass()->mUnitType;
    case EE_SCENARIO_EV_COMPLEX_AS_UNIT: return R(static_cast<EEUnit*>(P(EEComplexUnit, 0)));
    case EE_SCENARIO_EV_UNIT_CLASS: return R(P(EEUnit, 0)->mUnitClass);
    case EE_SCENARIO_EV_TREE_NODE: return R(P(EEPlayer, 0)->GetTechTree()->Get(a[1]));
    case EE_SCENARIO_EV_NODE_SET_BUTTON:
        P(EETechTreeNode, 0)->SetButton(a[1]);
        return 0;
    case EE_SCENARIO_EV_NODE_UNIT_CLASS: return (int64_t)P(EETechTreeNode, 0)->mData->mUnitClass;
    case EE_SCENARIO_EV_NODE_KIND: return (int64_t)P(EETechTreeNode, 0)->mData->mKind;
    case EE_SCENARIO_EV_NODE_DELAY: return (int64_t)P(EETechTreeNode, 0)->mResearchDelay++;
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
    case EE_SCENARIO_EV_TREE_SET_MEDIA: return (int64_t)(P(EEPlayer, 0)->GetTechTree()->mMediaID = a[1]);
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
    case EE_SCENARIO_EV_EVENT_COUNT: return (int64_t)EEEvent::sEvents.size();
    case EE_SCENARIO_EV_EVENT_AT: return R(EEEvent::sEvents[(size_t)a[0]].get());
    case EE_SCENARIO_EV_EVENT_EFFECTS: return R(P(EEEvent, 0)->mEffects);
    case EE_SCENARIO_EV_EVENT_NUM: return (int64_t)P(EEEvent, 0)->mNumEffects;
    case EE_SCENARIO_EV_FILE_OPEN: return P(FSFileSpec, 0)->Open(kFileAccessRead);
    case EE_SCENARIO_EV_FILE_LENGTH_INTO: return P(FSFileSpec, 0)->GetLength(*P(unsigned long, 1));
    case EE_SCENARIO_EV_EFFECTS_ASSIGN:
        gEventEffects.assign((size_t)a[0], EEEventEffect());
        return 0;
    case EE_SCENARIO_EV_EFFECTS_READ:
        return P(FSFileSpec, 0)->Read((char*)&gEventEffects[0], (unsigned long)(gEventEffects.size() * sizeof(EEEventEffect)), sEvRead);
    case EE_SCENARIO_EV_EVENTS_CLEAR:
        EEEvent::sEvents.clear();
        return 0;
    case EE_SCENARIO_EV_EVENT_NEW: return R(EEEvent::sEvents.emplace_back(new EEEvent).get());
    case EE_SCENARIO_EV_READ_NAME: return P(FSFileSpec, 0)->Read(sEvName, 100, sEvRead);
    case EE_SCENARIO_EV_READ_ID: return P(FSFileSpec, 0)->Read(P(EEEvent, 1)->mID);
    case EE_SCENARIO_EV_READ_COUNT: return P(FSFileSpec, 0)->Read(P(EEEvent, 1)->mNumEffects);
    case EE_SCENARIO_EV_EVENT_ALLOC: return R(P(EEEvent, 0)->mEffects = new int32_t[(size_t)a[1]]);
    case EE_SCENARIO_EV_READ_EFFECTS:
        return P(FSFileSpec, 0)->Read(reinterpret_cast<char*>(P(EEEvent, 1)->mEffects), P(EEEvent, 1)->mNumEffects * 4, sEvRead);
    case EE_SCENARIO_EV_EVENT_NO_EFFECTS: return R(P(EEEvent, 0)->mEffects = NULL);
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
