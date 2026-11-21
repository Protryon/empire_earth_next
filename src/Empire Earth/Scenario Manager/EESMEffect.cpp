// C:\EEX\Empire Earth\Scenario Manager\EESMEffect.cpp (file name inferred: no __FILE__): the scenario trigger
// effects, 0x1bb5a5..0x1c1ca9 plus the inline and template COMDATs after them. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include <vector>
#include "ee/EEScenarioManager.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"   // (also defines DeleteVectorContents: ee/EEDeleteVectorContents.h)
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysics.h"
#include "lle/SESoundManager.h"
#include "ee/EEGraphics.h"
#include "ee/EEUserInterface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"
#include "ee/EEActiveGroup.h"             // EEActiveGroup::ClearGoal (inline)

// Creates a scenario effect with the default settings (see Reset)
EESMEffect::EESMEffect()
{
    this->Reset();
}

// Destroys the effect (members only)
EESMEffect::~EESMEffect()
{
}

// Resets every effect setting to its default: an object Create effect, 20 s duration, empty texts and object list
// (the numbers: ee_scenario fields.rs effect_defaults)
void EESMEffect::Reset()
{
    this->mName = "";
    this->mText = "";
    this->mMessage = "";
    this->mS64 = "";
    this->mS78 = "";
    this->mNewName = "";
    this->mFileName = "";
    this->mDescription = "";
    *static_cast<EeScenarioEffectFields*>(this) = ee_scenario_effect_defaults();
    this->mObjects.clear();
}

#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"

// Copies another effect's settings, texts and object list
void EESMEffect::Copy(const EESMEffect& inEffect)
{
    this->mName = inEffect.mName;
    this->mText = inEffect.mText;
    this->mMessage = inEffect.mMessage;
    this->mS64 = inEffect.mS64;
    this->mS78 = inEffect.mS78;
    this->mNewName = inEffect.mNewName;
    this->mFileName = inEffect.mFileName;
    this->mDescription = inEffect.mDescription;
    *static_cast<EeScenarioEffectFields*>(this) = inEffect;   // every number
    this->mObjects = inEffect.mObjects;
}

// Sets, raises or lowers a player value: a resource, the population cap (also queued as a pending effect), difficulty,
// a scenario variable, a tribute or other statistic or the mining limit; any/all resources recurse per resource
void EESMEffect::SetPlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount)
{
    if (!EESimCaptureRecord::Active()) {
        this->SetPlayerAttributeUncaptured(inPlayer, inAttribute, inMode, inAmount);
        return;
    }
    unsigned int theSeed = EESMCaptureSeed();
    this->SetPlayerAttributeUncaptured(inPlayer, inAttribute, inMode, inAmount);
    EESMCapturePlayerAttribute(inPlayer, inAttribute, inMode, inAmount, theSeed);
}

// SetPlayerAttribute's body (SetPlayerAttribute adds the simulation capture's SMPA record: ee/EEScenarioCapture.h)
void EESMEffect::SetPlayerAttributeUncaptured(long inPlayer, unsigned long inAttribute, long inMode, long inAmount)
{
    // ee_scenario player.rs: the plan of writes (EEScenarioHost.cpp's Write; the any/all resources come back here
    // through SetPlayerAttribute)
    EESMHostContext theContext = {NULL, NULL, this};
    EeScenarioHost theHost = EESMHost(&theContext);
    ee_scenario_set_player_attribute(&theHost, inPlayer, inAttribute, inMode, inAmount);
}

#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"

void EEChangeUnit(unsigned long, bool, EEUnit*, long, unsigned long, bool, long);

// Creates units for the scenario object (a random min..max count per chosen owner) or copies of its existing ones,
// placed by the object's rules and finished; a newly created set becomes the object's units (the counts, owners and
// classes: ee_scenario create.rs create_units; each unit world_compose.rs create_unit over SMECreateWorld below)
void EESMEffect::CreateUnits()
{
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    if (theObject == NULL)
        return;
    std::vector<unsigned long> theCreatedUnits;
    GE2DPoint theLocation;   // one for the loop, as the original's (a placement may answer true without writing it)
    EESMHostContext theContext = {NULL, NULL, this, theObject};
    std::vector<EEUnit*> theWallPieces;
    GETransformation theTransformation(false);
    theContext.mCreated = &theCreatedUnits;
    theContext.mLocation = &theLocation;
    theContext.mWallPieces = &theWallPieces;
    theContext.mTransform = &theTransformation;
    EeScenarioHost theHost = EESMHost(&theContext);
    if (ee_scenario_create_units(&theHost, (int32_t)this->mObject, theObject) && theCreatedUnits.size() > 0)
        theObject->SetUnits(theCreatedUnits);
}

#include "ee/EEGraphicEffect.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEMiniMap.h"
#include "lle/UIFormManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECalamityClass.h"

extern bool& gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")


// Runs the effect: an object action, a player action (victory, diplomacy, tech tree, camera, flare...), trigger state,
// media (sound, text, dialogue, chat), a game action (time of day, calamity, graphic effect, cinematic) or an AI action
void EESMEffect::Execute(bool inIsCinematic)
{
    if (!EESimCaptureRecord::Active()) {
        this->ExecuteUncaptured(inIsCinematic);
        return;
    }
    unsigned int theSeed = EESMCaptureSeed();
    this->ExecuteUncaptured(inIsCinematic);
    EESMCaptureEffect(this, inIsCinematic, theSeed);
}

// Copies a message text, replacing each /#OA, /#PA or /#OC tag and its 4-digit id with an object's attribute value,
// a player condition's current value or an object's unit count
void EESMEffect::ExpandTokens(const UWideString& inText, UWideString& outText)
{
    bool theCapture = EESimCaptureRecord::Active();
    unsigned int theSeed = theCapture ? EESMCaptureSeed() : 0;
    UWideString theText(inText);
    EESMHostContext theContext = {NULL, NULL, this, NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);   // ee_scenario tokens.rs expand_tokens
    char* theResult = ee_scenario_expand_tokens(&theHost, (const char*)inText);
    outText = theResult;
    ee_scenario_text_free(theResult);
    if (theCapture)
        EESMCaptureTokens(theText, outText, theSeed);   // TEST ONLY: SMTK
}

// Reads the effect from a scenario file, renumbering player actions and unit attributes of older files; returns the
// first read error (ee_scenario save.rs effect_load)
long EESMEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText, &this->mMessage, &this->mS64, &this->mS78,
                                 &this->mNewName, &this->mFileName, &this->mDescription};
    EESMStream theStream = {&inFile, theStrings, &this->mObjects};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_effect_load(&theTable, this, inMajorVersion, inMinorVersion);
}

// Writes the effect to a scenario file (civ points from minor version 0x2a, mB193 from 0x14); returns the first write
// error (ee_scenario save.rs effect_save)
long EESMEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText, &this->mMessage, &this->mS64, &this->mS78,
                                 &this->mNewName, &this->mFileName, &this->mDescription};
    EESMStream theStream = {&outFile, theStrings, &this->mObjects};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_effect_save(&theTable, this, inMajorVersion, inMinorVersion);
}


#include "ee/EEUnitBehavior.h"
#include "ee/EEPPatrolGroup.h"

#include "ee/EEHerd.h"

#include "ee/EEAirTrafficControl.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEUnitVectorItem.h"
#include "rust/sim/units.h"

// what one effect's run keeps between its world calls (ee_scenario effect.rs, effect_ai.rs, task.rs): the
// calamity point and class, the units a calamity found in an area, the graphic effect, the expanded message, the
// camera move's points, the AI's location, task, strategy, requirement and unit ids, the task effects' group, goal,
// load and rejected lists, calamity technologies, and the placement's location, transformation, roll and pitch
// (each as the original's local, so what one player's or unit's pass leaves the next starts from)
struct EESMEffectRun : EESMHostContext {
    explicit EESMEffectRun(EESMEffect* inEffect) : EESMHostContext(), mCalamity(NULL), mGraphic(NULL), mGroup(NULL),
        mGoal(NULL), mRoll(0.0f), mPitch(0.0f) { this->mEffect = inEffect; }
    GE3DPoint mPoint;
    EECalamityClass* mCalamity;
    std::vector<EEUnit*> mSeen;
    EEGraphicEffect* mGraphic;
    UWideString mText;
    GE3DPoint mEnd;
    GE3DPoint mStart;
    EEActiveGroup* mGroup;
    EEUGRust* mGoal;
    std::vector<EEComplexUnit*> mLoad;
    std::vector<EEComplexUnit*> mRejected;
    std::vector<long> mNodeIDs;
    std::vector<long> mCalamities;
    GE2DPoint mPlace;
    GETransformation mTransformation;
    float mRoll;
    float mPitch;
    // the single statements' objects (world_compose.rs): the group's searches, the group's and a unit's new goal,
    // the unit a kill holds, the facing's transformation and point, a gate's part, the town patrol's town, group and
    // target, the calamity's tile walk, the weather's tiles
    EEComplexUnit* mFound[2] = {NULL, NULL};
    EEUGRust* mGroupGoal = NULL;
    EEUGRust* mUnitGoal = NULL;
    EEUnit* mHeld = NULL;
    GETransformation mFace;
    GE3DPoint mFacePoint;
    EEComplexUnit* mPart = NULL;
    EETown* mTown = NULL;
    EEPPatrolGroup* mPatrol = NULL;
    EEComplexUnit* mPatrolTarget = NULL;
    EEMapTile* mMapTile = NULL;
    RTSTileUnitNode* mNode = NULL;
    std::vector<U2DPoint<long> > mTiles;
    GETransformation mGround;
};

// Runs the effect: an object action, a player action (victory, diplomacy, tech tree, camera, flare...), trigger state,
// media (sound, text, dialogue, chat), a game action (time of day, calamity, graphic effect, cinematic) or an AI action
// (the decisions: ee_scenario effect.rs; each world call EESMEffectWorld below)
void EESMEffect::ExecuteUncaptured(bool inIsCinematic)
{
    EESMEffectRun theRun(this);
    EeScenarioHost theHost = EESMHost(&theRun);
    ee_scenario_effect_execute(&theHost, this, inIsCinematic);
}

namespace {

EEUnit* SMEUnit(int64_t inID) { return gWorld.GetUnit((unsigned long)inID); }
EEUnit* SMEUnitOrNull(int64_t inID) { return inID < 0 ? NULL : gWorld.GetUnit((unsigned long)inID); }
EEComplexUnit* SMEComplex(int64_t inID) { return gWorld.GetUnit((unsigned long)inID)->mComplexUnit; }
EEPlayer* SMEPlayer(int64_t inIndex) { return gWorld.GetPlayer((unsigned long)inIndex); }
EEComputerPlayer* SMEComputer(int64_t inIndex) { return dynamic_cast<EEComputerPlayer*>(SMEPlayer(inIndex)); }
float SMEFloat(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}
int64_t SMEBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}
int64_t SMEPack(long inX, long inY) { return (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32); }
int64_t SMEID(EEUnit* inUnit) { return inUnit != NULL ? (int64_t)(uint32_t)inUnit->mUnitID : -1; }
int64_t SMEPtr(const void* inObject) { return (int64_t)(intptr_t)inObject; }

// the campaign's sound folder ("" outside a campaign)
UString SMESoundPath()
{
    UString thePath;
    if (gWorld.mGameOptions.mCampaignName.GetLength() > 0 && gWorld.mGameOptions.mCampaignChecksum > 0)
        thePath = gU52A280;
    else
        thePath = gEmptyString;
    return thePath;
}

void SMEPlayTrumpets()
{
    EESoundObject* theSound = EESoundObject::Get(kSoundTrumpets);
    if (theSound != NULL)
        theSound->Play();
}

// unit inUnit: StartAction(inAction) and its class's ActionChanged (every order of the effects ends so)
void SMEAction(EEComplexUnit* inUnit, long inAction)
{
    inUnit->StartAction(inAction);
    inUnit->GetUnitClass()->ActionChanged(inUnit);
}

// a unit's value for the effects (EE_SCENARIO_UV_*)
int64_t SMEUnitValue(int64_t inID, int64_t inField)
{
    EEUnit* theUnit = SMEUnit(inID);
    if (inField == EE_SCENARIO_UV_EXISTS)
        return theUnit != NULL;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    EEUnitClass* theClass = theUnit->GetUnitClass();
    switch (inField) {
    case EE_SCENARIO_UV_COMPLEX: return theComplex != NULL;
    case EE_SCENARIO_UV_HIT_POINTS: return theUnit->GetHitPoints();
    case EE_SCENARIO_UV_DAMAGED: return theUnit->GetHitPoints() < theUnit->GetStats()->mMaxHitPoints;
    case EE_SCENARIO_UV_TILE_X: return theUnit->GetTileX();
    case EE_SCENARIO_UV_TILE_Y: return theUnit->GetTileY();
    case EE_SCENARIO_UV_ON_TILE: return theUnit->mTile != NULL;
    case EE_SCENARIO_UV_PLAYER: return theUnit->mPlayer != NULL ? (int64_t)theUnit->mPlayer->mPlayerID : -1;
    case EE_SCENARIO_UV_PLAYER_INDEX: return gWorld.GetPlayerIndex(theUnit->GetPlayer());
    case EE_SCENARIO_UV_TYPE: return theClass->GetUnitType();
    case EE_SCENARIO_UV_CATEGORY: return theClass->GetCategory();
    case EE_SCENARIO_UV_COMMAND: return theComplex->GetCommandType();
    case EE_SCENARIO_UV_DISABLED: return theComplex->mIsDisabled;
    case EE_SCENARIO_UV_COMMANDABLE: return theComplex->mIsCommandable;
    case EE_SCENARIO_UV_CLASS_DELETABLE: return theClass->mIsDeletable;
    case EE_SCENARIO_UV_WALL: return theClass->mIsWall;
    case EE_SCENARIO_UV_HAS_GOAL: return theComplex->GetGoal() != NULL;
    case EE_SCENARIO_UV_TURNS: return theComplex->GetMaxAngularVelocity() > 0.0f;
    case EE_SCENARIO_UV_TREE: return theClass->GetIsTree();
    case EE_SCENARIO_UV_ID: return (int64_t)(uint32_t)theUnit->GetUnitID();
    case EE_SCENARIO_UV_CONTAINER: return SMEID(theComplex->mContainer);
    case EE_SCENARIO_UV_CONTAINER_TYPE: return theComplex->mContainer->GetUnitClass()->GetUnitType();
    case EE_SCENARIO_UV_AIR: return (theClass->mDomains & kMovementAir) != 0;
    case EE_SCENARIO_UV_LAND_BUILDING: return theClass->IsLandBuilding();
    case EE_SCENARIO_UV_TRANSPORT: return theClass->IsTransport();
    case EE_SCENARIO_UV_DROP_SITE: return theClass->IsDropSite();
    case EE_SCENARIO_UV_GARRISON: return theClass->mGarrisonCapacity;
    case EE_SCENARIO_UV_DETACHED: return theComplex->mIsDetachedFromAI;
    case EE_SCENARIO_UV_CARGO: return theComplex->GetNumCargoUnits();
    }
    return 0;
}

// the AI's and the task effects' calls (effect_ai.rs, task.rs)
int64_t SMEUnitsWorld(EESMEffectRun* inRun, uint32_t inOp, const int64_t* a)
{
    EEActiveGroup* theGroup = inRun->mGroup;
    switch (inOp) {
    // (the AI military effects' calls went with the C++ attack groups in main's stage 5: ee_scenario effect_ai.rs)
    case EE_SCENARIO_WORLD_GROUP_ADD: theGroup->AddMember(SMEUnit(a[0])->mComplexUnit, true); return 0;
    case EE_SCENARIO_WORLD_GROUP_COUNT: return (int64_t)theGroup->GetNumUnits();
    case EE_SCENARIO_WORLD_GROUP_UNIT: return SMEID(theGroup->GetUnit((unsigned long)a[0]));
    case EE_SCENARIO_WORLD_GROUP_FIRST: return SMEID(theGroup->GetFirstUnit());
    case EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE: theGroup->KeepMobileUnits(); return 0;
    case EE_SCENARIO_WORLD_CAN_HEAL: return EECanHeal(SMEComplex(a[0]), SMEComplex(a[1]));
    case EE_SCENARIO_WORLD_LOAD_CLEAR: inRun->mLoad.clear(); return 0;
    case EE_SCENARIO_WORLD_LOAD_PUSH: inRun->mLoad.push_back(SMEComplex(a[0])); return 0;
    case EE_SCENARIO_WORLD_LOAD_COUNT: return (int64_t)inRun->mLoad.size();
    case EE_SCENARIO_WORLD_LOAD_AT: return SMEID(inRun->mLoad[(size_t)a[0]]);
    case EE_SCENARIO_WORLD_GOAL_VALID: return inRun->mGoal->IsLoadGarrisonValid();
    case EE_SCENARIO_WORLD_GOAL_DELETE: delete inRun->mGoal; return 0;
    case EE_SCENARIO_WORLD_GOAL_LOAD_TILE: {
        U2DPoint<long> theTile = inRun->mGoal->GetLoadUnitTile();
        return SMEPack(theTile.GetX(), theTile.GetY());
    }
    case EE_SCENARIO_WORLD_REJECTED_COUNT: return (int64_t)inRun->mRejected.size();
    case EE_SCENARIO_WORLD_GROUP_SPREAD: theGroup->SpreadAroundTile((long)a[0], (long)a[1]); return 0;
    case EE_SCENARIO_WORLD_CAL_TARGET_KIND:
        inRun->mCalamity = EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)a[0]);
        return inRun->mCalamity != NULL ? (int64_t)inRun->mCalamity->GetTargetKind() : -1;
    case EE_SCENARIO_WORLD_UPDATE_ACTIVE: gWorld.UpdateActiveUnits(); return 0;
    }
    return 0;
}

// a class attribute's and Set AI Variable's world calls (ca_compose.rs, ai_store.rs EE_SCENARIO_WK_*: the Rust
// composes EE_SCENARIO_WORLD_CA_* and _AI_STORE and reads and writes the upgrade's, class's and computer player's
// records itself): one statement each, on handles
static_assert(kSMEffectObject == 1 && kSMObjectActionUnitAttribute == 8 && kSMUnitAttributeShockArmorPlus == 20 &&
                  kSMUnitAttributeReloadTimePlus == 26 && kSMUnitAttributeRangePlus == 36 &&
                  kSMUnitAttributeLineOfSightPlus == 37 && kSMUnitAttributeMaxVelocityPlus == 38 &&
                  kSMUnitAttributeAnimalAIActivePlus == 40 && kSMUnitAttributeCameraFocusPlus == 41,
              "ca_compose.rs's numbers");
std::vector<EEHerd*> sSMEHerds;   // GetHerdsOfType's list (the original's local)
EEPlayer* SMEPlayerOf(int64_t inHandle) { return (EEPlayer*)(intptr_t)inHandle; }
EEUnitClassUpgrade* SMEUpgradeOf(int64_t inHandle) { return (EEUnitClassUpgrade*)(intptr_t)inHandle; }
EEUnitClass* SMEClassOf(int64_t inHandle);
EEUnit* SMEUnitOf(int64_t inHandle);
int64_t SMEClassWorld(uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_WK_PLAYER: return SMEPtr(SMEPlayer(a[0]));
    case EE_SCENARIO_WK_UPGRADE: return SMEPtr(SMEPlayerOf(a[0])->GetUpgrade((long)a[1]));
    case EE_SCENARIO_WK_UPGRADE_FIELDS: return SMEPtr(static_cast<EeUnitsUnitClassUpgradeFields*>(SMEUpgradeOf(a[0])));
    case EE_SCENARIO_WK_CLASS: return SMEPtr(SMEUpgradeOf(a[0])->mUnitClass);
    case EE_SCENARIO_WK_CLASS_FIELDS: return SMEPtr(static_cast<EeUnitsUnitClassFields*>(SMEClassOf(a[0])));
    case EE_SCENARIO_WK_PLAYER_ID: return SMEPlayerOf(a[0])->mPlayerID;
    case EE_SCENARIO_WK_CAPTURE:   // TEST ONLY: SMCU (ee/EEScenarioCapture.h)
        EESMCaptureUnitChange(1, (const long*)(intptr_t)a[0], NULL, SMEPlayerOf(a[1]), SMEUpgradeOf(a[2]), (unsigned int)a[3]);
        return 0;
    case EE_SCENARIO_WK_EFFECT_NEW: return SMEPtr(new EESMEffect);
    case EE_SCENARIO_WK_EFFECT_FIELDS: return SMEPtr(static_cast<EeScenarioEffectFields*>((EESMEffect*)(intptr_t)a[0]));
    case EE_SCENARIO_WK_EFFECT_ADD:
        gWorld.mScenarioManager->AddV134Effect(*(EESMEffect*)(intptr_t)a[0]);
        return 0;
    case EE_SCENARIO_WK_EFFECT_FREE:
        delete (EESMEffect*)(intptr_t)a[0];
        return 0;
    case EE_SCENARIO_WK_SET_RANGE:
        SMEUpgradeOf(a[0])->SetRange(SMEFloat(a[1]));
        return 0;
    case EE_SCENARIO_WK_SET_VELOCITY:
        SMEUpgradeOf(a[0])->SetMaxVelocity(SMEFloat(a[1]));
        return 0;
    case EE_SCENARIO_WK_FIRST_UNIT: return SMEPtr(SMEPlayerOf(a[0])->mFirstUnit);
    case EE_SCENARIO_WK_NEXT_UNIT: return SMEPtr(SMEUnitOf(a[0])->mNext);
    case EE_SCENARIO_WK_UNIT_COMPLEX: return SMEUnitOf(a[0])->mComplexUnit != NULL;
    case EE_SCENARIO_WK_UNIT_CLASS: return SMEPtr(SMEUnitOf(a[0])->mUnitClass);
    case EE_SCENARIO_WK_SIGHT_OFF:
        SMEUnitOf(a[0])->mComplexUnit->SightOffHere();
        return 0;
    case EE_SCENARIO_WK_SIGHT_ON:
        SMEUnitOf(a[0])->SightOnHere();
        return 0;
    case EE_SCENARIO_WK_HERDS_CLEAR:
        sSMEHerds.clear();
        return 0;
    case EE_SCENARIO_WK_HERDS:
        SMEPlayerOf(a[0])->GetHerdsOfType(sSMEHerds, (long)a[1]);
        return 0;
    case EE_SCENARIO_WK_HERD_COUNT: return (int64_t)sSMEHerds.size();
    case EE_SCENARIO_WK_HERD_AT: return SMEPtr(sSMEHerds[(size_t)a[0]]);
    case EE_SCENARIO_WK_HERD_ACTIVE: return ((EEHerd*)(intptr_t)a[0])->mIsActive = a[1] != 0;
    case EE_SCENARIO_WK_AI_FIELDS: return SMEPtr(static_cast<EeCpFields*>(static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))));
    case EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW:
        static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetNextAttackTime(gPhysics->mGameTime);
        return 0;
    }
    return 0;
}

// EEChangeUnit's calls on the context's unit (ee_scenario unit_change.rs EE_SCENARIO_WORLD_CU_*): its values (the
// statements' reads), the draws and each change's statement
int64_t SMEChangeUnitWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    EEUnit* theUnit = inContext->mUnit;
    if (inOp == EE_SCENARIO_WORLD_CU_RANDOM)
        return gPhysics->GetRandom((long)a[0], (long)a[1]);
    EEComplexUnit* theComplex = theUnit != NULL ? theUnit->GetComplexUnit() : NULL;
    long* theVariable = a[0] >= EE_SCENARIO_CU_VARIABLE && a[0] < EE_SCENARIO_CU_VARIABLE + 3
                            ? &theComplex->mUnitVariables[(size_t)(a[0] - EE_SCENARIO_CU_VARIABLE)] : NULL;
    if (inOp == EE_SCENARIO_WORLD_CU_VALUE) {
        switch (a[0]) {
        case EE_SCENARIO_CU_EXISTS: return theUnit != NULL;
        case EE_SCENARIO_CU_COMPLEX: return theComplex != NULL;
        case EE_SCENARIO_CU_HIT_POINTS: return theUnit->GetHitPoints();
        case EE_SCENARIO_CU_MAX_HIT_POINTS: return theUnit->GetStats()->mMaxHitPoints;
        case EE_SCENARIO_CU_CATEGORY: return theUnit->GetUnitClass()->GetCategory();
        case EE_SCENARIO_CU_CARRY: return theUnit->GetStats()->mCarryAmount;
        case EE_SCENARIO_CU_RESOURCE_AMOUNT: return theUnit->mResourceAmount;
        case EE_SCENARIO_CU_GATHERING: return theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionGatherResource;
        case EE_SCENARIO_CU_RESOURCE_TYPE: return theComplex->mResourceType;
        case EE_SCENARIO_CU_STATS_FLIGHT_TIME: return theUnit->GetStats()->mFlightTime;
        case EE_SCENARIO_CU_FLIGHT_TIME: return theComplex->mFlightTime;
        case EE_SCENARIO_CU_GARRISON_CAPACITY: return theUnit->GetUnitClass()->mGarrisonCapacity;
        case EE_SCENARIO_CU_GARRISON: return theComplex->mGarrisonCount;
        case EE_SCENARIO_CU_CLASS_AMMUNITION: return theUnit->GetUnitClass()->mAmmunition;
        case EE_SCENARIO_CU_AMMUNITION: return (int64_t)theComplex->mAmmunition;
        case EE_SCENARIO_CU_CLASS_SCALE: return SMEBits(theUnit->GetUnitClass()->GetScale());
        case EE_SCENARIO_CU_SCALE: return SMEBits(theComplex->mPhysicalModel.GetScale());
        case EE_SCENARIO_CU_STATS_RELOAD: return theUnit->GetStats()->mReloadTime;
        case EE_SCENARIO_CU_RELOAD_TIMER: return theComplex->mReloadTimer;
        case EE_SCENARIO_CU_STATS_POWER: return theUnit->GetStats()->mPower;
        case EE_SCENARIO_CU_POWER: return theComplex->mPower;
        case EE_SCENARIO_CU_CLASS_ROTTING: return theUnit->GetUnitClass()->mRottingTime;
        case EE_SCENARIO_CU_ROTTING: return theComplex->mRottingAwayTime;
        }
        return *theVariable;
    }
    switch (a[0]) {
    case EE_SCENARIO_CU_HIT_POINTS: theUnit->SetHitPoints((long)a[1]); break;
    case EE_SCENARIO_CU_RESOURCE_AMOUNT: theUnit->SetResourceAmount((long)a[1]); break;
    case EE_SCENARIO_CU_RESOURCE_TYPE: theUnit->SetResourceType((unsigned long)a[1]); break;
    case EE_SCENARIO_CU_FLIGHT_TIME: theComplex->SetFlightTime((long)a[1]); break;
    case EE_SCENARIO_CU_GARRISON: theComplex->AddGarrison((long)a[1], theComplex->mGarrisonPopulation); break;
    case EE_SCENARIO_CU_AMMUNITION: theComplex->mAmmunition = (unsigned long)a[1]; break;
    case EE_SCENARIO_CU_SCALE: theComplex->mPhysicalModel.SetScale(SMEFloat(a[1])); break;
    case EE_SCENARIO_CU_RELOAD_TIMER: theComplex->mReloadTimer = (long)a[1]; break;
    case EE_SCENARIO_CU_POWER: theComplex->mPower = (long)a[1]; break;
    case EE_SCENARIO_CU_ROTTING: theComplex->mRottingAwayTime = (long)a[1]; break;
    case EE_SCENARIO_CU_PAUSED:
        theComplex->SetAnimation(theComplex->mAnimation, theComplex->mPhysicalModel.GetFrame(), a[1] != 0);
        break;
    case EE_SCENARIO_CU_BUILT: theComplex->SetBuilt(a[1] != 0, 1, 1); break;
    case EE_SCENARIO_CU_GARRISON_WAIT: {
        std::vector<EEComplexUnit*> theUnits;
        theComplex->QueueGoal(EEUGRust::NewLoad(kGoalLoadGarrison, theComplex, theUnits, false), 0);
        SMEAction(theComplex, kActionWaitForGarrisonCost);
        break;
    }
    case EE_SCENARIO_CU_KILL:
        if (a[1] == 0) {
            theUnit->Kill(0, 0, 0);
            break;
        }
        theUnit->SetResourceAmount(0);
        theUnit->GetPlayer()->UnitTaken(theUnit, false);
        theUnit->GetPlayer()->KillUnit(theUnit, false);
        break;
    default: *theVariable = (long)a[1]; break;
    }
    return 0;
}

// Create Units' unit's single statements (world_compose.rs create_unit, EE_SCENARIO_WC_*): the context's object,
// location, created list, wall pieces and transformation; classes and units are handles the Rust hands back
EEUnitClass* SMEClassOf(int64_t inHandle) { return (EEUnitClass*)(intptr_t)inHandle; }
EEUnit* SMEUnitOf(int64_t inHandle) { return (EEUnit*)(intptr_t)inHandle; }
int64_t SMECreateWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    GE2DPoint& theLocation = *inContext->mLocation;
    EESMObject* theObject = inContext->mObject;
    switch (inOp) {
    case EE_SCENARIO_WC_CLASS: return SMEPtr(gUnitClassTable[a[0]]);
    case EE_SCENARIO_WC_OBJECT_UNIT_CLASS: return SMEPtr(gWorld.GetUnit(theObject->mUnits[(unsigned long)a[0]])->GetUnitClass());
    case EE_SCENARIO_WC_FIND_PLACEMENT: return theObject->FindPlacement((unsigned long)a[0], theLocation, true);
    case EE_SCENARIO_WC_TECH_NODE: return SMEPtr(gWorld.GetPlayer((unsigned long)a[0])->GetTechTree()->Get(SMEClassOf(a[1])->GetTechnology()));
    case EE_SCENARIO_WC_TECH_KIND:
        return gWorld.GetPlayer((unsigned long)a[0])->GetTechTree()->Get(SMEClassOf(a[1])->GetTechnology())->mData->mKind;
    case EE_SCENARIO_WC_LOCATION_X: return SMEBits(theLocation.mX);
    case EE_SCENARIO_WC_LOCATION_Y: return SMEBits(theLocation.mY);
    case EE_SCENARIO_WC_LOCATION_SET: theLocation = GE2DPoint(SMEFloat(a[0]), SMEFloat(a[1]));
        return 0;
    case EE_SCENARIO_WC_PLAN_WALL:
        ee_units_uu_plan_wall(theLocation.mX, theLocation.mY, theLocation.mX, theLocation.mY, (uint64_t)a[0], (uint64_t)(uintptr_t)gWorld.GetPlayer((unsigned long)a[1]), false, 1, inContext->mWallPieces);
        return 0;
    case EE_SCENARIO_WC_PIECES_FIRST: return SMEPtr(*inContext->mWallPieces->begin());
    case EE_SCENARIO_WC_PIECES_CLEAR: inContext->mWallPieces->clear();
        return 0;
    case EE_SCENARIO_WC_IS_LAND_BUILDING: return SMEClassOf(a[0])->IsLandBuilding();
    case EE_SCENARIO_WC_CREATE: return SMEPtr(SMEClassOf(a[0])->CreateUnit((long)theLocation.GetX(), (long)theLocation.GetY(), (long)a[1], false, NULL));
    case EE_SCENARIO_WC_ON_NODE: return SMEUnitOf(a[0])->mTileNode != NULL;
    case EE_SCENARIO_WC_SET_BUILT: SMEUnitOf(a[0])->GetComplexUnit()->SetBuilt(1, 0, 1);
        return 0;
    case EE_SCENARIO_WC_AIR: return (SMEUnitOf(a[0])->GetUnitClass()->mDomains & kMovementAir) != 0;
    case EE_SCENARIO_WC_REFUEL: SMEUnitOf(a[0])->GetComplexUnit()->SetFlightTime(SMEUnitOf(a[0])->GetStats()->mFlightTime);
        return 0;
    case EE_SCENARIO_WC_REARM: SMEUnitOf(a[0])->GetComplexUnit()->SetAmmunition(SMEUnitOf(a[0])->GetUnitClass()->mAmmunition);
        return 0;
    case EE_SCENARIO_WC_MAP_SIZE: return (int64_t)gWorld.mMap->mSize;
    case EE_SCENARIO_WC_T_FROM: *inContext->mTransform = SMEUnitOf(a[0])->GetComplexUnit()->mPhysicalModel.GetLocation();
        return 0;
    case EE_SCENARIO_WC_T_ORIENT: inContext->mTransform->SetOrientationYPR(SMEFloat(a[0]), 0.0f, 0.0f);
        return 0;
    case EE_SCENARIO_WC_T_APPLY: SMEUnitOf(a[0])->GetComplexUnit()->SetTransformation(*inContext->mTransform, true);
        return 0;
    case EE_SCENARIO_WC_COMPLEX_POWER: return SMEUnitOf(a[0])->GetComplexUnit()->GetStats()->mPower;
    case EE_SCENARIO_WC_SET_POWER: SMEUnitOf(a[0])->GetComplexUnit()->SetPower(SMEUnitOf(a[0])->GetStats()->mPower);
        return 0;
    case EE_SCENARIO_WC_PENDING: gWorld.GetPlayer((unsigned long)a[0])->AddPendingUnits(false, true);
        return 0;
    case EE_SCENARIO_WC_FLUSH: gWorld.FlushQueuedPlacements();
        return 0;
    case EE_SCENARIO_WC_PLACE_ON_GROUND: gWorld.mMap->PlaceUnitOnGround(SMEUnitOf(a[0]));
        return 0;
    case EE_SCENARIO_WC_ATTRIBUTE_CHECK: return (int64_t)theObject->mHasAttributeCheck;
    case EE_SCENARIO_WC_CHANGE_UNIT:
        EEChangeUnit(theObject->mAttribute, theObject->mIsAttributePercent, SMEUnitOf(a[0]), theObject->mAttributeMin, theObject->mAttributeMax, theObject->mHasAttributeCheck == 2, kSMAttributeModeExactly);
        return 0;
    case EE_SCENARIO_WC_PUSH_CREATED: inContext->mCreated->push_back(SMEUnitOf(a[0])->GetUnitID());
        return 0;
    case EE_SCENARIO_WC_SOUND: return SMEUnitOf(a[0])->GetStats()->mCreationSound != NULL;
    case EE_SCENARIO_WC_SOUND_LOCAL: return SMEUnitOf(a[0])->GetPlayer()->mPlayerID == gWorld.mCurrentPlayer;
    case EE_SCENARIO_WC_SOUND_PLAY:
        gSoundManager->Play(SMEUnitOf(a[0])->GetStats()->mCreationSound->mSampleID, SMEUnitOf(a[0])->GetStats()->mCreationSound->mVolume, 0.0f, 1.0f, false, NULL);
        return 0;
    }
    return 0;
}

// the effect run's single statements (ee_scenario world_compose.rs EE_SCENARIO_WX_*: the Rust composes the
// sequences the effects make of them)
int64_t SMEAtomWorld(EESMEffectRun* inRun, uint32_t inOp, const int64_t* a)
{
    EESMEffect* theEffect = inRun->mEffect;
    EEActiveGroup* theGroup = inRun->mGroup;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    switch (inOp) {
    case EE_SCENARIO_WX_DETACH: SMEUnit(a[0])->GetPlayer()->DetachUnitFromAI(SMEUnit(a[0])->GetComplexUnit());
        return 0;
    case EE_SCENARIO_WX_GROUP_CREATE: return SMEPtr(inRun->mGroup = new EEActiveGroup(SMEPlayer(a[0])));
    case EE_SCENARIO_WX_GROUP_PEND: SMEPlayer(a[0])->mPendingGroups.push_back(inRun->mGroup);
        return 0;
    case EE_SCENARIO_WX_HAS_COMPLEX: return SMEComplex(a[0])->GetComplexUnit() != NULL;
    case EE_SCENARIO_WX_REFUEL: SMEComplex(a[0])->GetComplexUnit()->SetFlightTime(SMEComplex(a[0])->GetStats()->mFlightTime);
        return 0;
    case EE_SCENARIO_WX_REARM: SMEComplex(a[0])->GetComplexUnit()->SetAmmunition(SMEComplex(a[0])->GetUnitClass()->mAmmunition);
        return 0;
    case EE_SCENARIO_WX_GROUP_FIND: return SMEPtr(inRun->mFound[0] = theGroup->FindUnitWithFlags(kUnitKindCitizen));
    case EE_SCENARIO_WX_GROUP_FIND_NAVAL: return SMEPtr(inRun->mFound[1] = theGroup->FindUnitWithFlags(kUnitKindNavalCitizen));
    case EE_SCENARIO_WX_FOUND: return inRun->mFound[a[0]] != NULL;
    case EE_SCENARIO_WX_FOUND_CAN_GATHER:
        return inRun->mFound[a[0]]->CanGather(SMEUnit(a[1]), SMEUnit(a[1])->GetTileX(), SMEUnit(a[1])->GetTileY(), true, false);
    case EE_SCENARIO_WX_GROUP_SPREAD_AT_UNIT: theGroup->SpreadAroundTile(SMEUnit(a[0])->GetTileX(), SMEUnit(a[0])->GetTileY());
        return 0;
    case EE_SCENARIO_WX_GROUP_SPREAD_AROUND:
        theGroup->SpreadAroundUnit(SMEUnit(a[0]), SMEUnit(a[0])->GetTileX(), SMEUnit(a[0])->GetTileY());
        return 0;
    case EE_SCENARIO_WX_GROUP_CLEAR_GOAL: theGroup->ClearGoal();
        return 0;
    case EE_SCENARIO_WX_GOAL_GATHER: return SMEPtr(inRun->mGroupGoal = EEUGRust::NewGather(SMEUnit(a[0]), NULL, true));
    case EE_SCENARIO_WX_GOAL_ATTACK:
        return SMEPtr(inRun->mGroupGoal = EEUGRust::NewAttackUnit(theGroup->GetFirstUnit(), SMEUnit(a[0]), true, false));
    case EE_SCENARIO_WX_GOAL_DROPOFF:
        return SMEPtr(inRun->mGroupGoal = EEUGRust::NewGather(NULL, SMEUnit(a[0])->GetComplexUnit(), true));
    case EE_SCENARIO_WX_GOAL_TILE:
        return SMEPtr(inRun->mGroupGoal = EEUGRust::NewLocation(GE2DPoint((float)(long)a[0], (float)(long)a[1]), true, true, false, 0.0f, false, 0));
    case EE_SCENARIO_WX_GOAL_PATROL_POINT:
        return SMEPtr(inRun->mGroupGoal = EEUGRust::NewPatrol(SMEUnit(a[0])->GetComplexUnit()->mPatrolGroup, SMEUnit(a[0])->GetComplexUnit(), true));
    case EE_SCENARIO_WX_GOAL_PATROL_NEW: return SMEPtr(inRun->mGroupGoal = EEUGRust::NewPatrol(true));
    case EE_SCENARIO_WX_GOAL_MOVE:
        return SMEPtr(inRun->mGroupGoal = EEUGRust::NewLocation(GE2DPoint(SMEFloat(a[0]), SMEFloat(a[1])), true, a[2] != 0, false, 0.0f, false, 0));
    case EE_SCENARIO_WX_GOAL_EXPLORE: return SMEPtr(inRun->mGroupGoal = EEUGRust::NewExplore(true));
    case EE_SCENARIO_WX_GOAL_TOWN_PATROL: return SMEPtr(inRun->mGroupGoal = EEUGRust::NewPatrol(inRun->mPatrol, inRun->mPatrolTarget, true));
    case EE_SCENARIO_WX_GROUP_SET_GOAL: theGroup->SetGoal(inRun->mGroupGoal, false);
        return 0;
    case EE_SCENARIO_WX_GROUP_ACTION: return theGroup->mAction = (long)a[0];
    case EE_SCENARIO_WX_GROUP_PLAN: theGroup->Plan();
        return 0;
    case EE_SCENARIO_WX_UGOAL_HEAL: return SMEPtr(inRun->mUnitGoal = EEUGRust::NewHealUnit(SMEComplex(a[0]), true));
    case EE_SCENARIO_WX_UGOAL_LOCATION:
        return SMEPtr(inRun->mUnitGoal = EEUGRust::NewLocation(GE2DPoint(SMEFloat(a[0]), SMEFloat(a[1])), true, a[2] != 0, false, 0.0f, false, 0));
    case EE_SCENARIO_WX_UGOAL_CAST:
        return SMEPtr(inRun->mUnitGoal = EEUGRust::NewCastCalamity(SMEComplex(a[0]), SMEUnitOrNull(a[1]), (long)a[2], (long)a[3], inRun->mCalamities[(size_t)a[4]], true));
    case EE_SCENARIO_WX_UGOAL_UNLOAD:
        return SMEPtr(inRun->mUnitGoal = EEUGRust::NewUnloadTransport(SMEComplex(a[0]), (long)SMEFloat(a[1]), (long)SMEFloat(a[2]), true));
    case EE_SCENARIO_WX_UGOAL_POINT:
        return SMEPtr(inRun->mUnitGoal = EEUGRust::NewLocation(SMEFloat(a[0]), SMEFloat(a[1]), 0.0f, true, true, false, 0.0f, false, 0));
    case EE_SCENARIO_WX_UGOAL_FACE:
        return SMEPtr(inRun->mUnitGoal = EEUGRust::NewLocation(&GE3DPoint(SMEComplex(a[0])->mPhysicalModel.GetLocation()), true, true, true, SMEFloat(a[1]), false, false));
    case EE_SCENARIO_WX_UNIT_SET_GOAL: SMEComplex(a[0])->SetGoal(inRun->mUnitGoal, 0, 0);
        return 0;
    case EE_SCENARIO_WX_UGOAL_CAN_UNLOAD: return inRun->mUnitGoal->CanUnloadAtDestination();
    case EE_SCENARIO_WX_UGOAL_DELETE: delete inRun->mUnitGoal;
        return 0;
    case EE_SCENARIO_WX_GOAL_KIND: return SMEComplex(a[0])->GetGoal()->mKind;
    case EE_SCENARIO_WX_GOAL_CAST_GARRISON: return SMEPtr(inRun->mGoal = EELoadGarrisonGoalCast(SMEComplex(a[0])->GetGoal()));
    case EE_SCENARIO_WX_GOAL_CAST_TRANSPORT: return SMEPtr(inRun->mGoal = EELoadTransportGoalCast(SMEComplex(a[0])->GetGoal()));
    case EE_SCENARIO_WX_GOAL_ADD_LOAD: inRun->mGoal->AddLoadUnits(inRun->mLoad);
        return 0;
    case EE_SCENARIO_WX_GOAL_NEW_LOAD: return SMEPtr(inRun->mGoal = EEUGRust::NewLoad((long)a[0], SMEComplex(a[1]), inRun->mLoad, true));
    case EE_SCENARIO_WX_SET_RUN_GOAL: SMEComplex(a[0])->SetGoal(inRun->mGoal, 0, 0);
        return 0;
    case EE_SCENARIO_WX_SET_RUN_GOAL_CLONE: SMEComplex(a[0])->SetGoal(inRun->mGoal->Clone(), 0, 0);
        return 0;
    case EE_SCENARIO_WX_GROUP_UNIT_IS_NOT: return theGroup->GetUnit((unsigned long)a[0]) != SMEUnit(a[1]);
    case EE_SCENARIO_WX_LOAD_PUSH_GROUP_UNIT: inRun->mLoad.push_back(theGroup->GetUnit((unsigned long)a[0]));
        return 0;
    case EE_SCENARIO_WX_FILTER_LOADABLE:
        EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(SMEUnit(a[0])->GetUnitClass())->FilterLoadable(&inRun->mLoad, SMEUnit(a[0])->GetComplexUnit(), &inRun->mRejected);
        return 0;
    case EE_SCENARIO_WX_GROUP_FINISH: theGroup->Finish();
        return 0;
    case EE_SCENARIO_WX_GROUP_ADD_REJECTED: theGroup->AddMember(inRun->mRejected[(size_t)a[0]], true);
        return 0;
    case EE_SCENARIO_WX_ORDER_REPAIR: SMEComplex(a[0])->OrderRepair(SMEUnit(a[1]));
        return 0;
    case EE_SCENARIO_WX_ACKNOWLEDGE:
        EEPlayForLocalPlayer(SMEPlayer(a[0]), theGroup->GetFirstUnit()->GetStats()->GetAcknowledgeSound(-1));
        return 0;
    case EE_SCENARIO_WX_FIRST_TILE_X: return theGroup->GetFirstUnit()->GetTileX();
    case EE_SCENARIO_WX_FIRST_TILE_Y: return theGroup->GetFirstUnit()->GetTileY();
    case EE_SCENARIO_WX_PATROL_ADD_FIRST:
        ee_units_pg_add_point(inRun->mGroupGoal->GetPatrolGroup(), (float)theGroup->GetFirstUnit()->GetTileX(), (float)theGroup->GetFirstUnit()->GetTileY(), (uint32_t)a[0]);
        return 0;
    case EE_SCENARIO_WX_PATROL_ADD: ee_units_pg_add_point(inRun->mGroupGoal->GetPatrolGroup(), SMEFloat(a[1]), SMEFloat(a[2]), (uint32_t)a[0]);
        return 0;
    case EE_SCENARIO_WX_FIRST_ADD_PATROL: theGroup->GetFirstUnit()->GetPlayer()->AddPatrolGroup(inRun->mGroupGoal->GetPatrolGroup());
        return 0;
    case EE_SCENARIO_WX_QUEUE_LOCATION:
        SMEComplex(a[0])->QueueGoal(EEUGRust::NewLocation(GE2DPoint(SMEFloat(a[1]), SMEFloat(a[2])), true, a[3] != 0, false, 0.0f, false, 0), (unsigned long)a[4]);
        return 0;
    case EE_SCENARIO_WX_QUEUE_ADD_TIME: SMEComplex(a[0])->mGoalQueues[(size_t)a[1]].AddTime((long)a[2]);
        return 0;
    case EE_SCENARIO_WX_CAL_CLEAR: inRun->mCalamities.clear();
        return 0;
    case EE_SCENARIO_WX_NODE_IDS: SMEPlayer(a[0])->GetTechTree()->GetNodeIDs(inRun->mNodeIDs);
        return 0;
    case EE_SCENARIO_WX_NODE_COUNT: return (int64_t)inRun->mNodeIDs.size();
    case EE_SCENARIO_WX_NODE_CALAMITY: return (int64_t)SMEPlayer(a[0])->GetTechTree()->Get(inRun->mNodeIDs[(size_t)a[1]])->mData->mCalamity;
    case EE_SCENARIO_WX_NODE_AVAILABLE: return SMEPlayer(a[0])->GetTechTree()->Get(inRun->mNodeIDs[(size_t)a[1]])->GetIsAvailable();
    case EE_SCENARIO_WX_CAL_PUSH: inRun->mCalamities.push_back(inRun->mNodeIDs[(size_t)a[0]]);
        return 0;
    case EE_SCENARIO_WX_CAL_COUNT: return (int64_t)inRun->mCalamities.size();
    case EE_SCENARIO_WX_CAL_CONTAINS:
        return SMEPlayer(a[1])->GetTechTree()->Get(inRun->mCalamities[(size_t)a[2]])->Contains(SMEComplex(a[0])->GetUnitClass()->GetUnitType());
    case EE_SCENARIO_WX_CAL_HAS: return inRun->mCalamity != NULL;
    case EE_SCENARIO_WX_CAL_KIND: return (int64_t)inRun->mCalamity->GetTargetKind();
    case EE_SCENARIO_WX_CAL_IS_VALID_TARGET: return inRun->mCalamity->IsValidTarget(SMEComplex(a[0]), SMEUnit(a[1]));
    case EE_SCENARIO_WX_CAL_IS_VALID_LOCATION: return inRun->mCalamity->IsValidLocation(SMEComplex(a[0]), (long)a[1], (long)a[2]);
    case EE_SCENARIO_WX_PLACE_SET: inRun->mPlace = GE2DPoint(SMEFloat(a[0]), SMEFloat(a[1]));
        return 0;
    case EE_SCENARIO_WX_AREA_EXISTS: return theManager->GetArea((unsigned long)a[0]) != NULL;
    case EE_SCENARIO_WX_AREA_UNSET: return theManager->GetArea((unsigned long)a[0])->IsUnset();
    case EE_SCENARIO_WX_AREA_OFF_MAP: return theManager->GetArea((unsigned long)a[0])->IsOffMap();
    case EE_SCENARIO_WX_OBJECT_EXISTS: return theManager->GetScenarioObject((unsigned long)a[0]) != NULL;
    case EE_SCENARIO_WX_OBJECT_EVALUATE: return theManager->GetScenarioObject((unsigned long)a[0])->Evaluate(false);
    case EE_SCENARIO_WX_TILE_EXISTS: return gWorld.mMap->GetTile((long)a[0], (long)a[1]) != NULL;
    case EE_SCENARIO_WX_TILE_HEIGHT: return SMEBits(gWorld.mMap->GetTile((long)a[0], (long)a[1])->mHeight);
    case EE_SCENARIO_WX_HOLD: return SMEPtr(inRun->mHeld = SMEUnit(a[0]));
    case EE_SCENARIO_WX_HELD_KILL: inRun->mHeld->Kill(a[0] != 0, false, false);
        return 0;
    case EE_SCENARIO_WX_HELD_ON_NODE: return inRun->mHeld->mTileNode != NULL;
    case EE_SCENARIO_WX_HELD_PLAYER_KILL: inRun->mHeld->GetPlayer()->KillUnit(inRun->mHeld, false);
        return 0;
    case EE_SCENARIO_WX_LOCATION_AT: return SMEBits(SMEComplex(a[0])->mPhysicalModel.GetLocation().GetElement((long)a[1], (long)a[2]));
    case EE_SCENARIO_WX_FACE_FROM_UNIT: inRun->mFace = SMEComplex(a[0])->mPhysicalModel.GetLocation();
        return 0;
    case EE_SCENARIO_WX_FACE_ORIENT: inRun->mFace.SetOrientationYPR(SMEFloat(a[0]), SMEFloat(a[1]), SMEFloat(a[2]));
        return 0;
    case EE_SCENARIO_WX_FACE_APPLY: SMEComplex(a[0])->SetTransformation(inRun->mFace, true);
        return 0;
    case EE_SCENARIO_WX_FACE_POINT: inRun->mFacePoint = GE3DPoint(inRun->mFace.GetX(), inRun->mFace.GetY(), inRun->mFace.GetZ());
        return 0;
    case EE_SCENARIO_WX_PLACE_ON_GROUND: SMEComplex(a[0])->PlaceOnGround(inRun->mFacePoint);
        return 0;
    case EE_SCENARIO_WX_MAIN_FORM: return gUIManager->GetMainGameForm() != NULL;
    case EE_SCENARIO_WX_REFRESH_GROUP: return gUIManager->GetMainGameForm()->mRefreshGroup = true;
    case EE_SCENARIO_WX_UNIT_GROUP_CREATE: return SMEPtr(inRun->mGroup = new EEActiveGroup(SMEComplex(a[0])->mPlayer));
    case EE_SCENARIO_WX_UNIT_GROUP_PEND: SMEComplex(a[0])->mPlayer->mPendingGroups.push_back(inRun->mGroup);
        return 0;
    case EE_SCENARIO_WX_GROUP_ADD_COMPLEX: theGroup->AddMember(SMEComplex(a[0]), true);
        return 0;
    case EE_SCENARIO_WX_LOCKED: return SMEComplex(a[0])->mIsLocked;
    case EE_SCENARIO_WX_SET_LOCKED: return SMEComplex(a[0])->mIsLocked = a[1] != 0;
    case EE_SCENARIO_WX_GATE_SECOND:
        return SMEPtr(inRun->mPart = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(SMEComplex(a[0])->GetUnitClass())->GetSecondPart(SMEComplex(a[0])));
    case EE_SCENARIO_WX_GATE_FOURTH:
        return SMEPtr(inRun->mPart = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(SMEComplex(a[0])->GetUnitClass())->GetFourthPart(SMEComplex(a[0])));
    case EE_SCENARIO_WX_PART_LOCKED: return inRun->mPart->mIsLocked;
    case EE_SCENARIO_WX_PART_SET_LOCKED: return inRun->mPart->mIsLocked = a[0] != 0;
    case EE_SCENARIO_WX_TOWN_FIND: return SMEPtr(inRun->mTown = SMEComplex(a[0])->FindNearestTown());
    case EE_SCENARIO_WX_TOWN_PATROL: return SMEPtr(inRun->mPatrol = inRun->mTown->mPatrolGroup);
    case EE_SCENARIO_WX_PATROL_NEAREST:
        return SMEPtr(inRun->mPatrolTarget = static_cast<EEComplexUnit*>(ee_units_pg_nearest(inRun->mPatrol, SMEComplex(a[0])->GetTileX(), SMEComplex(a[0])->GetTileY())));
    case EE_SCENARIO_WX_BEHAVIOR_IS: return SMEComplex(a[0])->mBehavior == EEUnitBehavior::sTable[(size_t)a[1]];
    case EE_SCENARIO_WX_SET_BEHAVIOR: return SMEPtr(SMEComplex(a[0])->mBehavior = EEUnitBehavior::sTable[(size_t)a[1]]);
    case EE_SCENARIO_WX_SET_GAME_RESULT: SMEPlayer(a[0])->SetGameResult((unsigned long)a[1]);
        return 0;
    case EE_SCENARIO_WX_REMOVE_PLAYER: gWorld.RemovePlayerFromGame(a[0] != 0, (unsigned long)a[1]);
        return 0;
    case EE_SCENARIO_WX_FADE_IN: gGraphics->PostFadeIn(3000);
        return 0;
    case EE_SCENARIO_WX_FADE_OUT: gGraphics->PostFadeOut(3000);
        return 0;
    case EE_SCENARIO_WX_TECH_MAKES_UNIT: return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mData->mMakesUnit;
    case EE_SCENARIO_WX_TECH_CLASS_EXISTS:
        return gUnitClassTable[SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mData->mUnitClass] != NULL;
    case EE_SCENARIO_WX_TECH_REPLACE_QUEUED:
        EEReplaceQueuedTechnology(SMEPlayer(a[0]), gUnitClassTable[SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mData->mUnitClass], 100000);
        return 0;
    case EE_SCENARIO_WX_TECH_OBSOLETE: SMEPlayer(a[0])->GetTechTree()->MakeObsolete(theEffect->mTechnology);
        return 0;
    case EE_SCENARIO_WX_SET_SEES_ALL: return SMEPlayer(a[0])->mSeesAll = a[1] != 0;
    case EE_SCENARIO_WX_PLAYER_IS_CURRENT: return SMEPlayer(a[0]) == gWorld.GetPlayer(gWorld.mCurrentPlayer);
    case EE_SCENARIO_WX_MINIMAP_REDRAW: gMiniMap->Redraw();
        return 0;
    case EE_SCENARIO_WX_CIV_ADD: SMEPlayer(a[0])->mCivilization.AddPoints((long)a[1]);
        return 0;
    case EE_SCENARIO_WX_CIV_BLINK: EESetCivBuilderButtonBlinking(true);
        return 0;
    case EE_SCENARIO_WX_TRUMPETS: return EESoundObject::Get(kSoundTrumpets) != NULL;
    case EE_SCENARIO_WX_TRUMPETS_PLAY: EESoundObject::Get(kSoundTrumpets)->Play();
        return 0;
    case EE_SCENARIO_WX_UNIT_ON_TILE: return SMEUnit(a[0])->mTile != NULL;
    case EE_SCENARIO_WX_UNIT_TILE_X: return SMEUnit(a[0])->mTileX;
    case EE_SCENARIO_WX_UNIT_TILE_Y: return SMEUnit(a[0])->mTileY;
    case EE_SCENARIO_WX_UNIT_HAS_COMPLEX: return SMEUnit(a[0])->mComplexUnit != NULL;
    case EE_SCENARIO_WX_UNIT_IN_CONTAINER: return SMEUnit(a[0])->mComplexUnit->mContainer != NULL;
    case EE_SCENARIO_WX_CONTAINER_ON_TILE: return SMEUnit(a[0])->mComplexUnit->mContainer->mTile != NULL;
    case EE_SCENARIO_WX_CONTAINER_TILE_X: return SMEUnit(a[0])->mComplexUnit->mContainer->mTileX;
    case EE_SCENARIO_WX_CONTAINER_TILE_Y: return SMEUnit(a[0])->mComplexUnit->mContainer->mTileY;
    case EE_SCENARIO_WX_TRIGGER_START: return theManager->GetTrigger((unsigned long)a[0])->mConditionStartTime = (unsigned long)a[1];
    case EE_SCENARIO_WX_TRIGGER_ON: return theManager->GetTrigger((unsigned long)a[0])->mIsOn = a[1] != 0;
    case EE_SCENARIO_WX_TRIGGER_FIRED: return theManager->GetTrigger((unsigned long)a[0])->mHasFired = a[1] != 0;
    case EE_SCENARIO_WX_MAP_TILE: return SMEPtr(inRun->mMapTile = gWorld.mMap->GetTile((long)a[0], (long)a[1]));
    case EE_SCENARIO_WX_NODE_FIRST: return SMEPtr(inRun->mNode = inRun->mMapTile->mUnits);
    case EE_SCENARIO_WX_NODE_NEXT: return SMEPtr(inRun->mNode = inRun->mNode->mNext);
    case EE_SCENARIO_WX_NODE_NO_UNIT: return inRun->mNode->mUnit == NULL;
    case EE_SCENARIO_WX_NODE_SEEN: return std::find(inRun->mSeen.begin(), inRun->mSeen.end(), inRun->mNode->mUnit) != inRun->mSeen.end();
    case EE_SCENARIO_WX_NODE_REFUSED:
        return inRun->mCalamity->CanCastAt(NULL, inRun->mPoint, inRun->mNode->mUnit->mTileX, inRun->mNode->mUnit->mTileY, true, inRun->mNode->mUnit) != 0;
    case EE_SCENARIO_WX_NODE_CREATE:
        USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity((unsigned long)a[0], -1, inRun->mNode->mUnit->GetUnitID(), 0);
        return 0;
    case EE_SCENARIO_WX_NODE_MARK: inRun->mSeen.push_back(inRun->mNode->mUnit);
        return 0;
    case EE_SCENARIO_WX_GFX_GET: return SMEPtr(inRun->mGraphic = EEGraphicEffect::Get((unsigned long)a[0]));
    case EE_SCENARIO_WX_GFX_LOAD: inRun->mGraphic->Load(false);
        return 0;
    case EE_SCENARIO_WX_GFX_KIND: return inRun->mGraphic->mKind;
    case EE_SCENARIO_WX_TILES_CLEAR: inRun->mTiles.clear();
        return 0;
    case EE_SCENARIO_WX_TILES_PUSH: inRun->mTiles.push_back(U2DPoint<long>((long)a[0], (long)a[1]));
        return 0;
    case EE_SCENARIO_WX_WEATHER_CREATE: gEffectManager->CreateWeatherEffect(inRun->mTiles, inRun->mGraphic, NULL);
        return 0;
    case EE_SCENARIO_WX_TILE_GET_HEIGHT: return SMEBits(gWorld.mMap->GetTile((long)a[0], (long)a[1])->GetHeight());
    case EE_SCENARIO_WX_TILE_WATER:
        return gWorld.mMap->GetTile((long)a[0], (long)a[1])->mTerrainType == kTerrainDeepWater || gWorld.mMap->GetTile((long)a[0], (long)a[1])->mTerrainType == kTerrainShallowWater;
    case EE_SCENARIO_WX_POINT_SET: inRun->mPoint = GE3DPoint(SMEFloat(a[0]), SMEFloat(a[1]), SMEFloat(a[2]));
        return 0;
    case EE_SCENARIO_WX_PLACED_CREATE: gEffectManager->CreatePlacedEffect(inRun->mPoint, inRun->mGraphic, NULL);
        return 0;
    case EE_SCENARIO_WX_ENV_EXISTS: return gWorld.mEnvironmentManager != NULL;
    case EE_SCENARIO_WX_ENV_SET: gWorld.mEnvironmentManager->SetEnabled(a[0] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 0: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetActive(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 1: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetIsCheating(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 2: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetCanAttack(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 3: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetCanDefend(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 4: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetCanBuild(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 5: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetCanExplore(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 6: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetIgnoreOthers(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_TOGGLE + 7: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetCanGather(a[1] != 0);
        return 0;
    case EE_SCENARIO_WX_AI_FAVORITE_AIR: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetFavoriteAirType((unsigned long)a[1]);
        return 0;
    case EE_SCENARIO_WX_AI_FAVORITE_LAND: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetFavoriteLandType((unsigned long)a[1]);
        return 0;
    case EE_SCENARIO_WX_AI_FAVORITE_SEA: static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->SetFavoriteSeaType((unsigned long)a[1]);
        return 0;
    case EE_SCENARIO_WX_CAMERA_SET_ZOOM: gWorld.mCameraControl.SetZoom(SMEFloat(a[0]));
        return 0;
    case EE_SCENARIO_WX_START_ACTION: SMEComplex(a[0])->StartAction((long)a[1]);
        return 0;
    case EE_SCENARIO_WX_ACTION_CHANGED: SMEComplex(a[0])->GetUnitClass()->ActionChanged(SMEComplex(a[0]));
        return 0;
    case EE_SCENARIO_WX_LAND_BUILDING: return SMEUnit(a[0])->GetComplexUnit()->GetUnitClass()->IsLandBuilding();
    case EE_SCENARIO_WX_T_FROM_UNIT: inRun->mTransformation = SMEUnit(a[0])->GetComplexUnit()->mPhysicalModel.GetLocation();
        return 0;
    case EE_SCENARIO_WX_SET_2D_FROM_T:
        SMEUnit(a[0])->GetComplexUnit()->Set2DPosition(U2DPoint<float>(inRun->mTransformation.GetX(), inRun->mTransformation.GetY()));
        return 0;
    case EE_SCENARIO_WX_AIRPORT:
        return SMEUnit(a[0])->GetUnitClass()->mAirTrafficType != 0 && SMEUnit(a[0])->GetComplexUnit()->GetUnitClass()->IsAirBase();
    case EE_SCENARIO_WX_AIRPORT_REMOVE:
        ee_units_atc_remove_airport(SMEUnit(a[0])->GetComplexUnit()->GetPlayer()->mAirTraffic[SMEUnit(a[0])->GetUnitClass()->mAirTrafficType - 1], SMEUnit(a[0])->GetComplexUnit());
        return 0;
    case EE_SCENARIO_WX_AIRPORT_ADD:
        ee_units_atc_add_airport(SMEUnit(a[0])->GetComplexUnit()->GetPlayer()->mAirTraffic[SMEUnit(a[0])->GetUnitClass()->mAirTrafficType - 1], SMEUnit(a[0])->GetComplexUnit());
        return 0;
    case EE_SCENARIO_WX_SELECTED_LOCALLY:
        return SMEUnit(a[0])->GetComplexUnit()->GetPlayer() == gWorld.GetCurrentPlayer() && SMEUnit(a[0])->GetComplexUnit()->GetPlayer()->mActiveGroup->Contains(SMEUnit(a[0])->GetComplexUnit());
    case EE_SCENARIO_WX_POST_424: gUserInterface->PostMessage424Once();
        return 0;
    case EE_SCENARIO_WX_TAKE_OFF_MAP: SMEUnit(a[0])->GetComplexUnit()->TakeOffMap();
        return 0;
    case EE_SCENARIO_WX_OFF_MAP_INFO: return SMEPtr(SMEUnit(a[0])->GetComplexUnit()->GetPlayer()->GetOffMapInfo(SMEUnit(a[0])->GetComplexUnit()));
    case EE_SCENARIO_WX_OFF_MAP_REASON: return SMEUnit(a[0])->GetComplexUnit()->GetPlayer()->GetOffMapInfo(SMEUnit(a[0])->GetComplexUnit())->mReason = 2;
    case EE_SCENARIO_WX_LAST_MAP_POSITION: SMEUnit(a[0])->GetComplexUnit()->mLastMapPosition = GE3DPoint(inRun->mTransformation);
        return 0;
    case EE_SCENARIO_WX_OBJ_IN_AREA_CHECK: return (int64_t)theManager->GetScenarioObject(theEffect->mObject)->GetInAreaCheck();
    case EE_SCENARIO_WX_OBJ_AREA: return (int64_t)theManager->GetScenarioObject(theEffect->mObject)->GetArea();
    case EE_SCENARIO_WX_OBJ_NEAR_CHECK: return (int64_t)theManager->GetScenarioObject(theEffect->mObject)->GetNearCheck();
    case EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK: return theManager->GetScenarioObject(theEffect->mObject)->mInAreaCheck = (unsigned long)a[0];
    case EE_SCENARIO_WX_OBJ_SET_AREA: return theManager->GetScenarioObject(theEffect->mObject)->mArea = (unsigned long)a[0];
    case EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK: return theManager->GetScenarioObject(theEffect->mObject)->mNearCheck = (unsigned long)a[0];
    case EE_SCENARIO_WX_EFFECT_TASK_AREA: return (int64_t)theEffect->mTaskArea;
    case EE_SCENARIO_WX_TASK_AREA_OFF_MAP: return theManager->GetArea(theEffect->mTaskArea)->IsOffMap();
    case EE_SCENARIO_WX_TASK_FIND_PLACEMENT:
        return theManager->GetScenarioObject(theEffect->mObject)->FindPlacement(SMEUnit(a[0])->GetUnitClass()->GetUnitType(), inRun->mPlace, true);
    case EE_SCENARIO_WX_T_SET_X: inRun->mTransformation.SetX(SMEFloat(a[0]));
        return 0;
    case EE_SCENARIO_WX_T_SET_Y: inRun->mTransformation.SetY(SMEFloat(a[0]));
        return 0;
    case EE_SCENARIO_WX_T_SET_Z: return SMEBits(inRun->mTransformation.mMatrix[2][3] = SMEFloat(a[0]));
    case EE_SCENARIO_WX_T_X: return SMEBits(inRun->mTransformation.GetX());
    case EE_SCENARIO_WX_T_Y: return SMEBits(inRun->mTransformation.GetY());
    case EE_SCENARIO_WX_U0DC_X: return SMEBits(SMEUnit(a[0])->GetComplexUnit()->mU0DC.GetX());
    case EE_SCENARIO_WX_U0DC_Y: return SMEBits(SMEUnit(a[0])->GetComplexUnit()->mU0DC.GetY());
    case EE_SCENARIO_WX_PLACE_X: return SMEBits(inRun->mPlace.GetX());
    case EE_SCENARIO_WX_PLACE_Y: return SMEBits(inRun->mPlace.GetY());
    case EE_SCENARIO_WX_PLACE_TILE: return SMEPtr(inRun->mMapTile = gWorld.mMap->GetTile((long)inRun->mPlace.GetX(), (long)inRun->mPlace.GetY()));
    case EE_SCENARIO_WX_PLACE_TILE_HEIGHT: return SMEBits(inRun->mMapTile->GetHeight());
    case EE_SCENARIO_WX_UNIT_AIR: return (SMEUnit(a[0])->GetUnitClass()->mDomains & kMovementAir) != 0;
    case EE_SCENARIO_WX_UNIT_Z: return SMEBits(SMEUnit(a[0])->GetComplexUnit()->mPhysicalModel.GetLocation().GetZ());
    case EE_SCENARIO_WX_CRUISE: return SMEBits(SMEUnit(a[0])->GetUnitClass()->GetCruiseAltitude());
    case EE_SCENARIO_WX_GROUND_IDENTITY: inRun->mGround.MakeIdentity();
        return 0;
    case EE_SCENARIO_WX_GROUND_POSITION: inRun->mGround.SetPosition(GE3DPoint(inRun->mTransformation));
        return 0;
    case EE_SCENARIO_WX_PITCHES: return SMEUnit(a[0])->GetUnitClass()->mPitchesWithTerrain;
    case EE_SCENARIO_WX_ROLLS: return SMEUnit(a[0])->GetUnitClass()->mRollsWithTerrain;
    case EE_SCENARIO_WX_TERRAIN_PITCH: return SMEBits(inRun->mPitch = gWorld.mMap->GetTerrainPitch(inRun->mGround));
    case EE_SCENARIO_WX_TERRAIN_ROLL: return SMEBits(inRun->mRoll = gWorld.mMap->GetTerrainRoll(inRun->mGround));
    case EE_SCENARIO_WX_T_ORIENT_HEADING:
        inRun->mTransformation.SetOrientationYPR(UMArcTan2(SMEUnit(a[0])->GetComplexUnit()->mPhysicalModel.GetLocation().mMatrix[1][0], SMEUnit(a[0])->GetComplexUnit()->mPhysicalModel.GetLocation().mMatrix[0][0]), inRun->mPitch, inRun->mRoll);
        return 0;
    case EE_SCENARIO_WX_CONTAINER_NULL: return SMEUnit(a[0])->GetComplexUnit()->mContainer == NULL;
    case EE_SCENARIO_WX_T_APPLY: SMEUnit(a[0])->GetComplexUnit()->SetTransformation(inRun->mTransformation, true);
        return 0;
    case EE_SCENARIO_WX_UNLOAD_CARGO:
        SMEUnit(a[0])->GetComplexUnit()->mContainer->UnloadCargoByID(SMEUnit(a[0])->GetComplexUnit()->mUnitID, inRun->mTransformation);
        return 0;
    case EE_SCENARIO_WX_TILE_SIZE: return SMEUnit(a[0])->GetUnitClass()->GetTileSize();
    case EE_SCENARIO_WX_T_SET_LOCATION: SMEUnit(a[0])->GetComplexUnit()->mPhysicalModel.SetLocation(inRun->mTransformation);
        return 0;
    case EE_SCENARIO_WX_CATEGORY: return SMEUnit(a[0])->GetUnitClass()->GetCategory();
    case EE_SCENARIO_WX_FLIGHT_TIME: return SMEUnit(a[0])->GetComplexUnit()->mFlightTime;
    case EE_SCENARIO_WX_HOME_BASE: return SMEUnit(a[0])->GetComplexUnit()->mHomeBase != NULL;
    case EE_SCENARIO_WX_EFFECT_TECH: return theEffect->mTechnology;
    case EE_SCENARIO_WX_EPOCH_COUNT: return EEEpochCount();
    case EE_SCENARIO_WX_TREE_END_EPOCH: return SMEPlayer(a[0])->GetTechTree()->mEndEpoch;
    case EE_SCENARIO_WX_TREE_RESET_RANGE:
        SMEPlayer(a[0])->GetTechTree()->ResetEpochRange(SMEPlayer(a[0])->GetTechTree()->mStartEpoch, theEffect->mTechnology);
        return 0;
    case EE_SCENARIO_WX_TECH_MAKE_AVAILABLE: SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->MakeAvailable();
        return 0;
    case EE_SCENARIO_WX_TECH_EPOCH_ZERO: return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mEpoch = 0;
    case EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH: return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mObsoleteEpoch = EEEpochCount();
    case EE_SCENARIO_WX_TECH_SCEN_OBSOLETED: SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->SetScenEditorObsoleted(false, false);
        return 0;
    case EE_SCENARIO_WX_TECH_SET_OBSOLETE: SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->SetObsolete(false, false);
        return 0;
    case EE_SCENARIO_WX_TECH_NEEDS_MEDIA:
        return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->NeedsUnitMedia() == true && !SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mIsUnitMediaLoaded;
    case EE_SCENARIO_WX_TECH_LOAD_MEDIA:
        SMEPlayer(a[0])->SetUnitClassMedia(SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mData->mUnitClass, 0x7f);
        return 0;
    case EE_SCENARIO_WX_TECH_MEDIA_LOADED: return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mIsUnitMediaLoaded = true;
    case EE_SCENARIO_WX_TECH_KIND: return SMEPlayer(a[0])->GetTechTree()->Get(theEffect->mTechnology)->mData->mKind;
    case EE_SCENARIO_WX_TREE_WONDERS_ZERO: return SMEPlayer(a[0])->GetTechTree()->mEpochsBeforeWonders = 0;
    case EE_SCENARIO_WX_TECH_RESEARCH: SMEPlayer(a[0])->GetTechTree()->Research(theEffect->mTechnology, NULL);
        return 0;
    case EE_SCENARIO_WX_CLASS_COUNT: return (int64_t)EEUnitClassCount();
    case EE_SCENARIO_WX_CLASS_EXISTS: return gUnitClassTable[a[0]] != NULL;
    case EE_SCENARIO_WX_CLASS_AIR: return (gUnitClassTable[a[0]]->GetDomains() & kMovementAir) != 0;
    case EE_SCENARIO_WX_CLASS_SEA: return (gUnitClassTable[a[0]]->GetDomains() & kMovementSea) != 0;
    case EE_SCENARIO_WX_CLASS_PROJECTILE: return gUnitClassTable[a[0]]->GetIsProjectile();
    case EE_SCENARIO_WX_CLASS_LAND_BUILDING: return gUnitClassTable[a[0]]->IsLandBuilding();
    case EE_SCENARIO_WX_CLASS_ATTACKS: return gUnitClassTable[a[0]]->GetAttack() > 0;
    case EE_SCENARIO_WX_CLASS_MOVES: return gUnitClassTable[a[0]]->GetMaxVelocity() > 0.0f;
    case EE_SCENARIO_WX_CLASS_FLIES_OVER: return gUnitClassTable[a[0]]->FliesOver();
    case EE_SCENARIO_WX_CLASS_CATEGORY: return gUnitClassTable[a[0]]->GetCategory();
    }
    return 0;
}

}   // namespace

// an effect run's world call (ee_scenario effect.rs EE_SCENARIO_WORLD_*): each the original's statement or
// statement sequence at its place, on the units by id, the players by index
int64_t SMEReadWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a);   // EEScenarioHost.cpp
int64_t SMEObjectWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a);   // EESMObject.cpp
int64_t EESMEffectWorld(void* inContext, uint32_t inOp, const int64_t* a)
{
    if (inOp >= EE_SCENARIO_WORLD_CU_RANDOM && inOp <= EE_SCENARIO_WORLD_CU_SET)   // EEChangeUnit's (no effect run)
        return SMEChangeUnitWorld(static_cast<EESMHostContext*>(inContext), inOp, a);
    if (inOp >= EE_SCENARIO_WK_PLAYER && inOp <= EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW)   // ca_compose.rs, ai_store.rs
        return SMEClassWorld(inOp, a);
    if (inOp >= EE_SCENARIO_RX_TECH_NODE && inOp <= EE_SCENARIO_RX_EFFECT_COPY)   // the host's reads (read_compose.rs)
        return SMEReadWorld(static_cast<EESMHostContext*>(inContext), inOp, a);
    if (inOp >= EE_SCENARIO_WZ_UNIT && inOp <= EE_SCENARIO_WZ_TILE_Y)   // the object reads (object_reads.rs)
        return SMEObjectWorld(static_cast<EESMHostContext*>(inContext), inOp, a);
    if (inOp >= EE_SCENARIO_WC_CLASS && inOp <= EE_SCENARIO_WC_SOUND_PLAY)   // Create Units' (no effect run)
        return SMECreateWorld(static_cast<EESMHostContext*>(inContext), inOp, a);
    EESMEffectRun* theRun = static_cast<EESMEffectRun*>(static_cast<EESMHostContext*>(inContext));
    if (inOp >= EE_SCENARIO_WX_DETACH)   // the single statements (world_compose.rs)
        return SMEAtomWorld(theRun, inOp, a);
    EESMEffect* theEffect = theRun->mEffect;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EECameraControl& theCamera = gWorld.mCameraControl;
    switch (inOp) {
    case EE_SCENARIO_WORLD_UNIT: return SMEUnitValue(a[0], a[1]);
    case EE_SCENARIO_WORLD_CURRENT_PLAYER: return gWorld.mCurrentPlayer;
    case EE_SCENARIO_WORLD_IN_CINEMATIC: return gInCinematic;
    case EE_SCENARIO_WORLD_PLAYER_OBSERVER: return SMEPlayer(a[0])->mObserverMode;
    case EE_SCENARIO_WORLD_PLAYER_IS_HUMAN: return SMEPlayer(a[0])->IsHuman();
    case EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER: return SMEComputer(a[0]) != NULL;
    case EE_SCENARIO_WORLD_AREA_CENTER: {
        U2DPoint<long> theTile = theManager->GetArea((unsigned long)a[0])->GetCenterTile();
        return SMEPack(theTile.GetX(), theTile.GetY());
    }
    case EE_SCENARIO_WORLD_DIALOG_IDLE: return !theManager->mIsShowingDialog && theManager->GetNumMessages() == 0;
    case EE_SCENARIO_WORLD_OBJECT_PICK: return theManager->GetScenarioObject((unsigned long)a[0])->PickUnitType();

    case EE_SCENARIO_WORLD_CREATE_UNITS: theEffect->CreateUnits(); return 0;
    case EE_SCENARIO_WORLD_CLEAR_GOALS: SMEComplex(a[0])->ClearGoals((unsigned long)a[1]); return 0;
    case EE_SCENARIO_WORLD_CLEAR_QUEUE: SMEComplex(a[0])->ClearGoalQueue((unsigned long)a[1]); return 0;
    case EE_SCENARIO_WORLD_SET_DELETABLE: SMEComplex(a[0])->SetIsDeletable(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_SET_DISABLED: SMEComplex(a[0])->SetDisabled(a[1] != 0, 0); return 0;
    case EE_SCENARIO_WORLD_SET_COMMANDABLE: SMEComplex(a[0])->SetIsCommandable(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_CONVERT: EEConvertUnit(static_cast<EEComplexUnit*>(SMEUnit(a[0])), (long)a[1], 0); return 0;
    case EE_SCENARIO_WORLD_RENAME_UNIT: SMEUnit(a[0])->GetStats()->SetName(theEffect->GetNewName()); return 0;
    case EE_SCENARIO_WORLD_RENAME_CLASS:
        SMEPlayer(a[0])->GetUpgrade(gUnitClassTable[a[1]]->GetUnitType())->SetName(theEffect->GetNewName());
        return 0;
    case EE_SCENARIO_WORLD_CHANGE_UNIT:
        EEChangeUnit((unsigned long)a[1], a[2] != 0, SMEUnitOrNull(a[0]), (long)a[3], 0, false, (long)a[4]);
        return 0;

    case EE_SCENARIO_WORLD_COMPUTER_SET_ACTIVE: SMEPlayer(a[0])->mComputerPlayer->SetActive(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_SET_STANCE: gWorld.SetDiplomaticStance((long)a[0], (long)a[1], (unsigned long)a[2]); return 0;
    case EE_SCENARIO_WORLD_NOTICE_TARGET: SMEPlayer(a[0])->NoticeTargetPlayer(SMEPlayer(a[1]), false); return 0;
    case EE_SCENARIO_WORLD_ADD_PENDING: theManager->AddV134Effect(*theEffect); return 0;
    case EE_SCENARIO_WORLD_FLARE: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        thePlayer->mEvents.AddEvent(U2DPoint<long>((long)a[1], (long)a[2]), 3, thePlayer, -1, true);
        return 0;
    }

    case EE_SCENARIO_WORLD_PLAY_SOUND: {
        unsigned long theSample;
        unsigned long theInstance;
        EELoadSound("Sounds\\" + SMESoundPath() + UString(theEffect->mFileName), 0x31, theSample, theInstance, 0);
        if (!UIsNone32(theInstance))
            gSoundManager->RegisterOneShotSound(theSample, theInstance);
        return 0;
    }
    case EE_SCENARIO_WORLD_CHANGE_TEXT: {
        static const unsigned long kSlots[] = {0, 3, 2};   // hints, instructions, history (GetSection's slots)
        EEUISCIFormManager* theForm = EEUISCIFormManager::Get();
        UWideString theMediaText = "\n";
        theMediaText += theEffect->mDescription;
        if (a[0] >= 0 && a[0] < 3) {
            UWideString& theSection = theManager->mMediaStrings.GetSection(kSlots[a[0]]);
            if (a[1] != 0)
                theSection = UWideString(gEmptyString);
            else
                theSection = theSection + theMediaText;
        }
        SMEPlayTrumpets();
        theForm->SetBlinking(true);
        return 0;
    }
    case EE_SCENARIO_WORLD_EXPAND_MESSAGE: theEffect->ExpandTokens(theEffect->mMessage, theRun->mText); return 0;
    case EE_SCENARIO_WORLD_DIALOGUE: {
        EESMDialogMessage theMessage;
        UWideString theName;
        theMessage.mText = theName;
        theMessage.mText += theRun->mText;
        theMessage.mDuration = theEffect->mDuration * 1000;
        theMessage.mIsCinematic = a[0] != 0;
        EESMObject* theObject = theManager->GetScenarioObject(theEffect->mObject);
        theObject->Evaluate(false);
        if (theObject->mUnits.size() > 0 && gWorld.GetUnit(theObject->mUnits[0]) != NULL) {
            EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[0]);
            theMessage.mUnitType = theUnit->mUnitClass->mUnitType;
            theMessage.mScrollSpeed = theEffect->mIntValue1;
            theMessage.mUnitID = theUnit->mUnitID;
        } else
            theMessage.mUnitType = 0;
        theMessage.mSound = (const char*)(SMESoundPath() + (const char*)theEffect->mFileName);
        theManager->AddMessage(theMessage);
        return 0;
    }
    case EE_SCENARIO_WORLD_DISPLAY_EMPTY: gUIManager->DisplayMessage(UWideString(), 1, NULL, NULL, true); return 0;
    case EE_SCENARIO_WORLD_CHAT_LOG: {
        GEColor theColor;
        UWideString theName;
        gUserInterface->AddChatLogEntry(theColor, theName, theRun->mText, theEffect->mDuration * 1000, 1);
        return 0;
    }
    case EE_SCENARIO_WORLD_CHAT_BOX: {
        // A chat message without a duration is shown in a message box: with its text (the box showed the media
        // text, which only the Change Text action fills, so it was empty).
        UWideString theChat;
        theChat += theRun->mText;
        gUIManager->DisplayMessage(theChat, 1, NULL, NULL, true);
        return 0;
    }

    case EE_SCENARIO_WORLD_SET_TIME_OF_DAY: gWorld.SetTimeOfDay(SMEFloat(a[0])); return 0;
    case EE_SCENARIO_WORLD_TIME_OF_DAY_RUNS: gPhysics->mTimeOfDayRuns = a[0] != 0; return 0;
    case EE_SCENARIO_WORLD_CAL_CLASS:
        theRun->mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)a[0]);
        return theRun->mCalamity != NULL;
    case EE_SCENARIO_WORLD_CAL_TYPE: {
        long theType = 0;
        return USingleton<EECalamityManager>::GetInstance()->GetCalamityType((EECalamityEnum)a[0], theType) ? theType : -1;
    }
    case EE_SCENARIO_WORLD_CAL_CAN_CAST:
        return theRun->mCalamity->CanCastAt(NULL, theRun->mPoint, (long)a[0], (long)a[1], true, SMEUnitOrNull(a[2])) == 0;
    case EE_SCENARIO_WORLD_CAL_GLOBAL: USingleton<EECalamityManager>::GetInstance()->CreateGlobalCalamity((unsigned long)a[0], 0, 0); return 0;
    case EE_SCENARIO_WORLD_CAL_LOCATION:
        USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity((unsigned long)a[0], -1, (long)a[1], (long)a[2], 0);
        return 0;
    case EE_SCENARIO_WORLD_CAL_UNIT:
        USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity((unsigned long)a[0], -1, (unsigned long)a[1], 0);
        return 0;
    case EE_SCENARIO_WORLD_GFX_BETWEEN: gEffectManager->CreateEffectBetweenUnits(SMEUnit(a[0]), SMEUnit(a[1]), theRun->mGraphic, NULL); return 0;
    case EE_SCENARIO_WORLD_GFX_UNIT: gEffectManager->CreateUnitEffect(SMEUnit(a[0]), theRun->mGraphic, NULL, false, 0, 0); return 0;
    case EE_SCENARIO_WORLD_GFX_END: theRun->mGraphic->Unload(); return 0;
    case EE_SCENARIO_WORLD_START_CINEMATIC: theManager->StartCinematic(); return 0;
    case EE_SCENARIO_WORLD_SET_GAME_SPEED: gWorld.SetGameSpeed(SMEFloat(a[0])); return 0;
    case EE_SCENARIO_WORLD_END_CINEMATIC: theManager->EndCinematic(); return 0;
    case EE_SCENARIO_WORLD_TIMED_NOT_RUN: theManager->SetTimedEffectHasRun(theEffect, false); return 0;

    case EE_SCENARIO_WORLD_ADD_PENDING_PLAYER: {
        EESMEffect theCopy;
        theCopy.Copy(*theEffect);
        theCopy.mPlayer = (unsigned long)a[0];
        theManager->AddV134Effect(theCopy);
        return 0;
    }

    case EE_SCENARIO_WORLD_CAMERA_RESET: theCamera.ResetPath(); return 0;
    case EE_SCENARIO_WORLD_CAMERA_TURN_RESET: theCamera.mTurnYaw = 0.0f; return 0;
    case EE_SCENARIO_WORLD_CAMERA_KEY: {
        // These camera APIs consume the original base-unit pointer, including its optional complex-unit link.
        long theStepDuration = (long)a[1];
        EEUnit* theUnit = SMEUnit(a[0]);
        EECameraKey theKey;
        theKey.mOffset = theCamera.GetEndOffset();
        theCamera.GetUnitPoint(theUnit, theKey.mFocus);
        theCamera.GetUnitTurn(theUnit, theKey.mT40);
        theKey.mWait = 0.0f;
        theKey.mChanged = theStepDuration == 0;
        theKey.mRate = theStepDuration == 0 ? 0.0f : 1.0f / (float)theStepDuration;
        theCamera.AddKey(theKey);
        return 0;
    }
    case EE_SCENARIO_WORLD_CAMERA_START: theCamera.StartPath(); return 0;
    case EE_SCENARIO_WORLD_CAMERA_POINT:
        (a[0] != 0 ? theRun->mStart : theRun->mEnd) = GE3DPoint(SMEFloat(a[1]), SMEFloat(a[2]), SMEFloat(a[3]));
        return 0;
    case EE_SCENARIO_WORLD_CAMERA_MODE: theCamera.SetMode(3); return 0;
    case EE_SCENARIO_WORLD_CAMERA_MOVE:
        theCamera.MoveTo(SMEUnitOrNull(a[0]), theRun->mEnd, (a[2] & 1) != 0, (float)(long)a[3], SMEUnitOrNull(a[1]),
                         theRun->mStart, (a[2] & 2) != 0, SMEFloat(a[4]), 0.0f, 0.0f);
        return 0;
    }
    return SMEUnitsWorld(theRun, inOp, a);
}
