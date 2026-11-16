// C:\EEX\Empire Earth\Scenario Manager\EESMEffect.cpp (file name inferred: no __FILE__): the scenario trigger
// effects, 0x1bb5a5..0x1c1ca9 plus the inline and template COMDATs after them. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EEUnitKind.h"
#include "ee/EESMDialogMessage.h"
#include "lle/FSFile.h"
#include <vector>
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"   // (also defines DeleteVectorContents: ee/EEDeleteVectorContents.h)
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysics.h"
#include "lle/SESoundManager.h"
#include "GEWindow.h"
#include "ee/EECameraControl.h"
#include "ee/EEGraphics.h"
#include "ee/EEUserInterface.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"

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
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EECPCombatManager.h"
#include "ee/EETown.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPEngine.h"

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

#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"

void EEChangeUnit(unsigned long, bool, EEUnit*, long, unsigned long, bool, long);

// Creates units for the scenario object (a random min..max count per chosen owner) or copies of its existing ones,
// placed by the object's rules and finished; a newly created set becomes the object's units (the counts, owners and
// classes: ee_scenario create.rs create_units; each unit EESMCreateUnit below)
void EESMEffect::CreateUnits()
{
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mObject);
    if (theObject == NULL)
        return;
    std::vector<unsigned long> theCreatedUnits;
    GE2DPoint theLocation;   // one for the loop, as the original's (a placement may answer true without writing it)
    EESMHostContext theContext = {NULL, NULL, this, theObject};
    theContext.mCreated = &theCreatedUnits;
    theContext.mLocation = &theLocation;
    EeScenarioHost theHost = EESMHost(&theContext);
    if (ee_scenario_create_units(&theHost, (int32_t)this->mObject, theObject) && theCreatedUnits.size() > 0)
        theObject->SetUnits(theCreatedUnits);
}

// CreateUnits' unit for player inPlayerID (create.rs EE_SCENARIO_WRITE_CREATE_UNIT): of class gUnitClassTable[inUnitType]
// when inCreate, else of the object's unit inIndex's class; placed, finished and kept when created
void EESMCreateUnit(EESMHostContext* inContext, long inPlayerID, long inUnitType, bool inCreate, unsigned long inIndex)
{
    std::vector<EEUnit*> theWallPieces;
    GEVector theDirection;
    EESMObject* theObject = inContext->mObject;
    GE2DPoint& theLocation = *inContext->mLocation;
    EEPlayer* thePlayer = gWorld.GetPlayer(inPlayerID);
    EEMap* theMap = gWorld.mMap;
    EEUnitClass* theClass = inCreate ? gUnitClassTable[inUnitType] : gWorld.GetUnit(theObject->mUnits[inIndex])->GetUnitClass();
    if (!theObject->FindPlacement(inUnitType, theLocation, true))
        return;
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theClass->GetTechnology());
    EEUnit* theUnit;
    if (theNode != NULL && thePlayer->GetTechTree()->Get(theClass->GetTechnology())->mData->mKind == 6) {
        theLocation.mX += 0.5f;
        theLocation.mY += 0.5f;
        EEPlanWall(theLocation, theLocation, theClass, thePlayer, false, 1, theWallPieces);
        theUnit = *(theWallPieces.begin());
        theWallPieces.clear();
    } else {
        theClass->IsLandBuilding();
        theUnit = theClass->CreateUnit((long)theLocation.GetX(),(long)theLocation.GetY(),inPlayerID,false,NULL);
    }
    if (theUnit->mTileNode != NULL)
        theUnit->GetComplexUnit()->SetBuilt(1,0,1);
    if ((theUnit->GetUnitClass()->mDomains & kMovementAir) != 0) {
        theUnit->GetComplexUnit()->SetFlightTime(theUnit->GetStats()->mFlightTime);
        theUnit->GetComplexUnit()->SetAmmunition(theUnit->GetUnitClass()->mAmmunition);
        theDirection.mX = (float)theMap->mSize * 0.5f - theLocation.GetX();
        theDirection.mY = (float)theMap->mSize * 0.5f - theLocation.GetY();   // towards the map centre
        theDirection.mZ = 0.0f;
        theDirection.Normalize();
        float theHeading = UMArcTan2(-theDirection.GetX(),theDirection.GetY());
        GETransformation theTransformation(false);
        theTransformation = theUnit->GetComplexUnit()->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(theHeading,0.0f,0.0f);
        theUnit->GetComplexUnit()->SetTransformation(theTransformation,true);
    }
    if (theUnit->mTileNode != NULL && theUnit->GetComplexUnit()->GetStats()->mPower > 0)
        theUnit->GetComplexUnit()->SetPower(theUnit->GetStats()->mPower);
    thePlayer->AddPendingUnits(false,true);
    gWorld.FlushQueuedPlacements();
    gWorld.mMap->PlaceUnitOnGround(theUnit);
    if (theObject->mHasAttributeCheck != 0)
        EEChangeUnit(theObject->mAttribute,theObject->mIsAttributePercent,theUnit,theObject->mAttributeMin,
                    theObject->mAttributeMax,theObject->mHasAttributeCheck == 2,kSMAttributeModeExactly);
    if (inCreate)
        inContext->mCreated->push_back(theUnit->GetUnitID());
    EESoundObject* theSound = theUnit->GetStats()->mCreationSound;
    if (theSound != NULL && theUnit->GetPlayer()->mPlayerID == gWorld.mCurrentPlayer)
        gSoundManager->Play(theSound->mSampleID,theSound->mVolume,0.0f,1.0f,false,NULL);
}

#include "ee/EEGraphicEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEMiniMap.h"
#include "lle/UIFormManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECalamityClass.h"

extern bool& gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// A plain (non-folded) .rdata float of this unit in the original (0x446090; 0x44608c and 0x446098 are effect.rs's),
// read by one function; the pooled __real copies of the same value are separate (provisional name)
static const float kSMEOneDegree = 0.017453292f;

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

#include "ee/EESMAIFields.h"

namespace {

// EE_SM_STORE begin (the golden driver's stand-in expands this text too: TEST ONLY scratch, PORT_LEARNINGS "R5b.5,
// second round")
// the long and float fields and the building counts by Set AI Variable's field numbers (ee/EESMAIFields.h)
#define EE_SM_MEMBER(inField) &EEComputerPlayer::inField,
long EEComputerPlayer::* const kSMLongFields[] = {EE_SM_AI_LONGS(EE_SM_MEMBER)};
float EEComputerPlayer::* const kSMFloatFields[] = {EE_SM_AI_FLOATS(EE_SM_MEMBER)};
#undef EE_SM_MEMBER
struct EESMBuildingFields {
    long EEComputerPlayer::* mMax;
    long EEComputerPlayer::* mUnitsPer;
    long mUnitType;
};
#define EE_SM_BUILDING(inMax, inUnitsPer, inType) {&EEComputerPlayer::inMax, &EEComputerPlayer::inUnitsPer, inType},
const EESMBuildingFields kSMBuildings[] = {EE_SM_AI_BUILDINGS(EE_SM_BUILDING)};
#undef EE_SM_BUILDING

// one store of Set AI Variable on a computer player
// (the original's 34 inline setters, COMDATs 0x1c1ca9..0x1c24c8)
void EESMStoreAI(EEComputerPlayer* inPlayer, const EeScenarioAiWrite& inWrite)
{
    switch (inWrite.kind) {
    case EE_SCENARIO_AI_KIND_LONG:
        inPlayer->*kSMLongFields[inWrite.field] = inWrite.long_value;
        break;
    case EE_SCENARIO_AI_KIND_FLOAT:
        inPlayer->*kSMFloatFields[inWrite.field] = inWrite.float_value;
        break;
    case EE_SCENARIO_AI_KIND_MAX_COUNT:
    case EE_SCENARIO_AI_KIND_UNITS_PER: {
        const EESMBuildingFields& theBuilding = kSMBuildings[inWrite.field];
        bool theMax = inWrite.kind == EE_SCENARIO_AI_KIND_MAX_COUNT;
        // (the original also mirrored it into the civilization manager's building counts, gone in main's stage 6)
        inPlayer->*(theMax ? theBuilding.mMax : theBuilding.mUnitsPer) = inWrite.long_value;
        break;
    }
    case EE_SCENARIO_AI_KIND_CALL:
        switch (inWrite.field) {
        case EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW: inPlayer->SetNextAttackTime(gPhysics->mGameTime); break;
        case EE_SCENARIO_AI_CALL_ISLAND_EXPANSION_TIMER: inPlayer->SetIslandExpansionTimer(inWrite.long_value); break;
        case EE_SCENARIO_AI_CALL_DM_ATTACK_FREQUENCY_MODIFIER: inPlayer->SetDMAttackFrequencyModifier(inWrite.float_value); break;
        case EE_SCENARIO_AI_CALL_DM_INITIAL_ATTACK_MODIFIER: inPlayer->SetDMInitialAttackModifier(inWrite.float_value); break;
        }
        break;
    }
}
// EE_SM_STORE end

}   // namespace

#include "ee/EEUnitBehavior.h"
#include "ee/EEPPatrolGroup.h"

#include "ee/EEHerd.h"

#include "ee/EELocationCalamityClass.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEActiveGroup.h"             // EEActiveGroup::ClearGoal (inline)

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

// the group's goal and action, then its plan
void SMEGroupGoal(EEActiveGroup* inGroup, EEUGRust* inGoal, long inAction)
{
    inGroup->SetGoal(inGoal, false);
    inGroup->mAction = inAction;
    inGroup->Plan();
}

// a categories Set Favorite Units never takes as the land unit
bool SMENotLand(long inCategory)
{
    switch (inCategory) {
    case kUnitCategoryAmbient: case kUnitCategoryResource: case kUnitCategoryBuilding: case kUnitCategoryAirplane: case kUnitCategoryNaval:
    case kUnitCategoryProjectile: case kUnitCategorySubmergedNaval: case kUnitCategoryBomb: case kUnitCategoryMissile:
    case kUnitCategoryNavalPriest:
    case kUnitCategoryHelicopterTransport: case kUnitCategoryNavalTransport: case kUnitCategoryBuilding19: case kUnitCategoryTorpedo: case kUnitCategoryNavalBuilding: case kUnitCategoryNavalCitizen:
    case kUnitCategoryMiningBuilding: case kUnitCategoryGate:
    case kUnitCategorySpaceship: case kUnitCategorySpaceBuilding: case kUnitCategorySpaceTransport: case kUnitCategoryPlanetSpaceship:
        return true;
    }
    return false;
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

// unit class inClass's tests for Set Favorite Units (EE_SCENARIO_CLASS_*)
int64_t SMEClassValue(int64_t inClass)
{
    EEUnitClass* theClass = inClass >= 0 && (unsigned long)inClass < EEUnitClassCount() ? gUnitClassTable[inClass] : NULL;
    if (theClass == NULL)
        return 0;
    return EE_SCENARIO_CLASS_EXISTS | ((theClass->GetDomains() & kMovementAir) ? EE_SCENARIO_CLASS_AIR : 0)
         | ((theClass->GetDomains() & kMovementSea) ? EE_SCENARIO_CLASS_SEA : 0)
         | (theClass->GetIsProjectile() ? EE_SCENARIO_CLASS_PROJECTILE : 0)
         | (theClass->IsLandBuilding() ? EE_SCENARIO_CLASS_LAND_BUILDING : 0)
         | (theClass->GetAttack() > 0 ? EE_SCENARIO_CLASS_ATTACKS : 0)
         | (theClass->GetMaxVelocity() > 0.0f ? EE_SCENARIO_CLASS_MOVES : 0)
         | (theClass->FliesOver() ? EE_SCENARIO_CLASS_FLIES_OVER : 0)
         | (SMENotLand(theClass->GetCategory()) ? EE_SCENARIO_CLASS_NOT_LAND : 0);
}

// Modify-Tech-Tree's add for player inPlayer: an epoch's node made available (the range raised), any other node
// researched past the epochs, its unit media loaded
void SMETechAdd(EESMEffect* inEffect, EEPlayer* thePlayer)
{
    if (inEffect->mTechnology < EEEpochCount()) {
        if (thePlayer->GetTechTree()->mEndEpoch < inEffect->mTechnology)
            thePlayer->GetTechTree()->ResetEpochRange(thePlayer->GetTechTree()->mStartEpoch, inEffect->mTechnology);
        thePlayer->GetTechTree()->Get(inEffect->mTechnology)->MakeAvailable();
        return;
    }
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(inEffect->mTechnology);
    theNode->MakeAvailable();
    theNode->mEpoch = 0;
    theNode->mObsoleteEpoch = EEEpochCount();
    theNode->SetScenEditorObsoleted(false, false);
    theNode->SetObsolete(false, false);
    if (theNode->NeedsUnitMedia() == true && !theNode->mIsUnitMediaLoaded) {
        thePlayer->SetUnitClassMedia(theNode->mData->mUnitClass, 0x7f);
        theNode->mIsUnitMediaLoaded = true;
    }
    if (theNode->mData->mKind == 4)
        thePlayer->GetTechTree()->mEpochsBeforeWonders = 0;
    thePlayer->GetTechTree()->Research(inEffect->mTechnology, NULL);
}

// Task-Area with an unset area: unit inUnit taken off the map (its 2D position kept, out of its airport, out of the
// selection)
void SMETakeOffMap(EESMEffectRun* inRun, EEUnit* theUnit)
{
    EEComplexUnit* theComplex = theUnit->GetComplexUnit();
    GETransformation& theTransformation = inRun->mTransformation;
    if (!theComplex->GetUnitClass()->IsLandBuilding()) {
        if (theComplex->GetUnitClass()->IsLandBuilding()) {
            for (EEComplexUnit* theLinked = theComplex; theLinked != NULL; theLinked = theLinked->mNextPart)
                theLinked->Set2DPosition(U2DPoint<float>((float)theLinked->GetTileX(), (float)theLinked->GetTileY()));
        } else {
            theTransformation = theComplex->mPhysicalModel.GetLocation();
            theComplex->Set2DPosition(U2DPoint<float>(theTransformation.GetX(), theTransformation.GetY()));
        }
        if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
            theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->RemoveAirport(theComplex);
        if (theComplex->GetPlayer() == gWorld.GetCurrentPlayer() && theComplex->GetPlayer()->mActiveGroup->Contains(theComplex))
            gUserInterface->PostMessage424Once();
        theComplex->TakeOffMap();
        EEWUnitsOffMapInfo* theInfo = theComplex->GetPlayer()->GetOffMapInfo(theComplex);
        if (theInfo != NULL)
            theInfo->mReason = 2;
    } else {
        theTransformation = theComplex->mPhysicalModel.GetLocation();
        theComplex->mLastMapPosition = GE3DPoint(theTransformation);
        theComplex->Set2DPosition(U2DPoint<float>(theTransformation.GetX(), theTransformation.GetY()));
    }
    if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
        theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->RemoveAirport(theComplex);
    if (theComplex->GetPlayer() == gWorld.GetCurrentPlayer() && theComplex->GetPlayer()->mActiveGroup->Contains(theComplex))
        gUserInterface->PostMessage424Once();
}

// Task-Area: off-map unit theUnit placed in the task area (the object's area check pointed at it for FindPlacement),
// on the ground at the location's tile, turned with the terrain, idle or circling
void SMEPlaceInArea(EESMEffectRun* inRun, EEUnit* theUnit)
{
    EESMEffect* theEffect = inRun->mEffect;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(theEffect->mObject);
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(theEffect->mTaskArea);
    EEComplexUnit* theComplex = theUnit->GetComplexUnit();
    GE2DPoint& theLocation = inRun->mPlace;
    GETransformation& theTransformation = inRun->mTransformation;
    unsigned long theSaved68 = theObject->GetInAreaCheck();
    unsigned long theSaved3C = theObject->GetArea();
    unsigned long theSaved6C = theObject->GetNearCheck();
    theObject->mInAreaCheck = 1;
    theObject->mArea = theEffect->mTaskArea;
    theObject->mNearCheck = 0;
    if (theArea->IsOffMap() || theObject->FindPlacement(theUnit->GetUnitClass()->GetUnitType(), theLocation, true)) {
        theTransformation = theComplex->mPhysicalModel.GetLocation();
        if (theArea->IsOffMap()) {
            theTransformation.SetX(theComplex->mU0DC.GetX());
            theTransformation.SetY(theComplex->mU0DC.GetY());
        } else if (theLocation.GetX() != -10000.0f) {
            theTransformation.SetX(theLocation.GetX() + 0.5f);
            theTransformation.SetY(theLocation.GetY() + 0.5f);
            long theX = (long)theLocation.GetX();
            EETile* theTile = gWorld.mMap->GetTile(theX, (long)theLocation.GetY());
            if (theTile != NULL) {
                if (!(theUnit->GetUnitClass()->mDomains & kMovementAir))
                    theTransformation.mMatrix[2][3] = theTile->GetHeight();
                else if (theComplex->mPhysicalModel.GetLocation().GetZ() < theTile->GetHeight())
                    theTransformation.mMatrix[2][3] = theUnit->GetUnitClass()->GetCruiseAltitude();
            }
        } else {
            theTransformation.SetX(-10000.0f);
            theTransformation.SetY(-10000.0f);
        }
        if (theTransformation.GetX() != -10000.0f) {
            GETransformation theGround(false);
            theGround.MakeIdentity();
            theGround.SetPosition(GE3DPoint(theTransformation));
            if (theUnit->GetUnitClass()->mPitchesWithTerrain)
                inRun->mPitch = gWorld.mMap->GetTerrainPitch(theGround);
            if (theUnit->GetUnitClass()->mRollsWithTerrain)
                inRun->mRoll = gWorld.mMap->GetTerrainRoll(theGround);
            theTransformation.SetOrientationYPR(UMArcTan2(theComplex->mPhysicalModel.GetLocation().mMatrix[1][0],
                                                          theComplex->mPhysicalModel.GetLocation().mMatrix[0][0]),
                                                inRun->mPitch, inRun->mRoll);
            if (theComplex->mContainer == NULL)
                theComplex->SetTransformation(theTransformation, true);
            else
                theComplex->mContainer->UnloadCargoByID(theComplex->mUnitID, theTransformation);
            long theSize = theUnit->GetUnitClass()->GetTileSize();
            if (theSize > 1) {
                float theOffset = theSize * 0.5f;
                theTransformation.SetX(theTransformation.GetX() + theOffset);
                theTransformation.SetY(theTransformation.GetY() + theOffset);
                theComplex->mPhysicalModel.SetLocation(theTransformation);
            }
            if (theUnit->GetUnitClass()->mAirTrafficType != 0 && theComplex->GetUnitClass()->IsAirBase())
                theComplex->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1]->AddAirport(theComplex);
            if ((theUnit->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                && (theUnit->GetComplexUnit()->mFlightTime != 0 || theUnit->GetComplexUnit()->mHomeBase != NULL))
                theComplex->StartAction(kActionCircle);
            else
                theComplex->StartAction(kActionIdle);
            theUnit->GetUnitClass()->ActionChanged(theComplex);
        }
    }
    theObject->mInAreaCheck = theSaved68;
    theObject->mArea = theSaved3C;
    theObject->mNearCheck = theSaved6C;
}

// the AI's and the task effects' calls (effect_ai.rs, task.rs)
int64_t SMEUnitsWorld(EESMEffectRun* inRun, uint32_t inOp, const int64_t* a)
{
    EESMEffect* theEffect = inRun->mEffect;
    EEActiveGroup* theGroup = inRun->mGroup;
    switch (inOp) {
    // (the AI military effects' calls went with the C++ attack groups in main's stage 5: ee_scenario effect_ai.rs)
    case EE_SCENARIO_WORLD_TASK_PREPARE: {
        EEUnit* theUnit = SMEUnit(a[0]);
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        theUnit->GetPlayer()->DetachUnitFromAI(theComplex);
        theComplex->SetDisabled(false, 0);
        theComplex->SetIsCommandable(true);
        theComplex->SetIsDeletable(theComplex->GetUnitClass()->mIsDeletable);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_NEW: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        inRun->mGroup = new EEActiveGroup(thePlayer);
        thePlayer->mPendingGroups.push_back(inRun->mGroup);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_ADD_TASKED: {
        EEComplexUnit* theComplex = SMEUnit(a[0])->GetComplexUnit();
        if (theComplex->GetComplexUnit() != NULL && (theComplex->GetUnitClass()->mDomains & kMovementAir)
            && theComplex->GetCommandType() == kActionIdle) {
            theComplex->GetComplexUnit()->SetFlightTime(theComplex->GetStats()->mFlightTime);
            theComplex->GetComplexUnit()->SetAmmunition(theComplex->GetUnitClass()->mAmmunition);
        }
        theGroup->AddMember(theComplex, true);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_ADD: theGroup->AddMember(SMEUnit(a[0])->mComplexUnit, true); return 0;
    case EE_SCENARIO_WORLD_GROUP_COUNT: return (int64_t)theGroup->GetNumUnits();
    case EE_SCENARIO_WORLD_GROUP_UNIT: return SMEID(theGroup->GetUnit((unsigned long)a[0]));
    case EE_SCENARIO_WORLD_GROUP_FIRST: return SMEID(theGroup->GetFirstUnit());
    case EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE: theGroup->KeepMobileUnits(); return 0;
    case EE_SCENARIO_WORLD_GROUP_CAN_GATHER: {
        EEUnit* theTarget = SMEUnit(a[0]);
        EEComplexUnit* theGatherer = theGroup->FindUnitWithFlags(kUnitKindCitizen);
        EEComplexUnit* theCarrier = theGroup->FindUnitWithFlags(kUnitKindNavalCitizen);
        return (theGatherer != NULL && theGatherer->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), true, false))
            || (theCarrier != NULL && theCarrier->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), true, false));
    }
    case EE_SCENARIO_WORLD_GROUP_GATHER: {
        EEUnit* theTarget = SMEUnit(a[0]);
        theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, EEUGRust::NewGather(theTarget, NULL, true), kActionGatherResource);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_ATTACK: {
        EEUnit* theTarget = SMEUnit(a[0]);
        theGroup->SpreadAroundUnit(theTarget, theTarget->GetTileX(), theTarget->GetTileY());
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, EEUGRust::NewAttackUnit(theGroup->GetFirstUnit(), theTarget, true, false),
                     kActionAttackEnemyUnit);
        return 0;
    }
    case EE_SCENARIO_WORLD_CAN_HEAL: return EECanHeal(SMEComplex(a[0]), SMEComplex(a[1]));
    case EE_SCENARIO_WORLD_HEAL: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EEUGRust* theGoal = EEUGRust::NewHealUnit(SMEComplex(a[1]), true);
        theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(theGoal, 0, 0);
        SMEAction(theUnit, kActionTurnToFaceHealee);
        return 0;
    }
    case EE_SCENARIO_WORLD_LOAD_CLEAR: inRun->mLoad.clear(); return 0;
    case EE_SCENARIO_WORLD_LOAD_PUSH: inRun->mLoad.push_back(SMEComplex(a[0])); return 0;
    case EE_SCENARIO_WORLD_LOAD_COUNT: return (int64_t)inRun->mLoad.size();
    case EE_SCENARIO_WORLD_LOAD_AT: return SMEID(inRun->mLoad[(size_t)a[0]]);
    case EE_SCENARIO_WORLD_LOAD_GOAL: {
        EEComplexUnit* theUnit = SMEComplex(a[1]);
        EEUnitGoal* theCurrent = theUnit->GetGoal();
        long theKind = a[0] == 0 ? kGoalLoadGarrison : kGoalLoadTransport;
        if (theCurrent != NULL && theCurrent->mKind == theKind) {
            inRun->mGoal = a[0] == 0 ? EELoadGarrisonGoalCast(theCurrent) : EELoadTransportGoalCast(theCurrent);
            inRun->mGoal->AddLoadUnits(inRun->mLoad);
            return 0;
        }
        inRun->mGoal = EEUGRust::NewLoad(theKind, theUnit, inRun->mLoad, true);
        return 1;
    }
    case EE_SCENARIO_WORLD_GOAL_VALID: return inRun->mGoal->IsLoadGarrisonValid();
    case EE_SCENARIO_WORLD_SET_GOAL:
    case EE_SCENARIO_WORLD_SET_GOAL_CLONE: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        theUnit->ClearGoals(0);
        theUnit->SetGoal(inOp == EE_SCENARIO_WORLD_SET_GOAL ? inRun->mGoal : inRun->mGoal->Clone(), 0, 0);
        SMEAction(theUnit, (long)a[1]);
        return 0;
    }
    case EE_SCENARIO_WORLD_GOAL_DELETE: delete inRun->mGoal; return 0;
    case EE_SCENARIO_WORLD_LOAD_FILTER: {
        EEUnit* theTarget = SMEUnit(a[0]);
        inRun->mLoad.clear();
        for (unsigned long u = 0; u < theGroup->GetNumUnits(); u++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(u);
            if (theUnit != theTarget)
                inRun->mLoad.push_back(theUnit);
        }
        EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theTarget->GetUnitClass())->FilterLoadable(&inRun->mLoad, theTarget->GetComplexUnit(),
                                                                                &inRun->mRejected);
        return 0;
    }
    case EE_SCENARIO_WORLD_GOAL_LOAD_TILE: {
        U2DPoint<long> theTile = inRun->mGoal->GetLoadUnitTile();
        return SMEPack(theTile.GetX(), theTile.GetY());
    }
    case EE_SCENARIO_WORLD_REJECTED_COUNT: return (int64_t)inRun->mRejected.size();
    case EE_SCENARIO_WORLD_REJECTED_MOVE:
        theGroup->Finish();
        for (unsigned long n = 0; n < inRun->mRejected.size(); n++)
            theGroup->AddMember(inRun->mRejected[n], true);
        theGroup->KeepMobileUnits();
        SMEGroupGoal(theGroup, EEUGRust::NewLocation(GE2DPoint((float)(long)a[0], (float)(long)a[1]), true, true, false, 0.0f, false, 0),
                     kActionPrepareToMove);
        return 0;
    case EE_SCENARIO_WORLD_GROUP_SPREAD: theGroup->SpreadAroundTile((long)a[0], (long)a[1]); return 0;
    case EE_SCENARIO_WORLD_REPAIR: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        theUnit->OrderRepair(SMEUnit(a[1]));
        SMEAction(theUnit, kActionIdle);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_DROPOFF: {
        EEUnit* theTarget = SMEUnit(a[0]);
        theGroup->SpreadAroundTile(theTarget->GetTileX(), theTarget->GetTileY());
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, EEUGRust::NewGather(NULL, theTarget->GetComplexUnit(), true), kActionDropoffResource);
        return 0;
    }
    case EE_SCENARIO_WORLD_ACKNOWLEDGE:
        if (theGroup->GetFirstUnit() != NULL)
            EEPlayForLocalPlayer(SMEPlayer(a[0]), theGroup->GetFirstUnit()->GetStats()->GetAcknowledgeSound(-1));
        return 0;
    case EE_SCENARIO_WORLD_GROUP_PATROL_POINT: {
        EEComplexUnit* thePoint = SMEUnit(a[0])->GetComplexUnit();
        theGroup->KeepMobileUnits();
        EEUGRust* theGoal = EEUGRust::NewPatrol(thePoint->mPatrolGroup, thePoint, true);
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, theGoal, kActionMoveToNextPatrolPoint);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_PATROL_TO: {
        EEUGRust* theGoal = EEUGRust::NewPatrol(true);
        EEComplexUnit* theFirst = theGroup->GetFirstUnit();
        if (theFirst != NULL && (a[3] == 0 || (theFirst->GetTileX() != -10000 && theFirst->GetTileY() != -10000))) {
            theGoal->GetPatrolGroup()->AddPoint(GE2DPoint((float)theFirst->GetTileX(), (float)theFirst->GetTileY()), (unsigned long)a[0]);
            theGoal->GetPatrolGroup()->AddPoint(GE2DPoint(SMEFloat(a[1]), SMEFloat(a[2])), (unsigned long)a[0]);
            theFirst->GetPlayer()->AddPatrolGroup(theGoal->GetPatrolGroup());
        }
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, theGoal, kActionMoveToNextPatrolPoint);
        return 0;
    }
    case EE_SCENARIO_WORLD_AIR_QUEUE: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        GE2DPoint thePoint(SMEFloat(a[1]), SMEFloat(a[2]));
        theUnit->ClearGoalQueue(0);
        theUnit->ClearGoalQueue(1);
        theUnit->QueueGoal(EEUGRust::NewLocation(thePoint, true, a[3] != 0, false, 0.0f, false, 0), 0);
        theUnit->QueueGoal(EEUGRust::NewLocation(thePoint, true, a[3] != 0, false, 0.0f, false, 0), 1);
        theUnit->mGoalQueues[1].AddTime(1);
        return 0;
    }
    case EE_SCENARIO_WORLD_LOCATION_GOAL: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EEUGRust* theGoal = EEUGRust::NewLocation(GE2DPoint(SMEFloat(a[1]), SMEFloat(a[2])), true, a[3] != 0, false, 0.0f, false, 0);
        theUnit->ClearGoals(0);
        theUnit->SetGoal(theGoal, 0, 0);
        if (a[4] == 0 || theUnit->mContainer == NULL)
            SMEAction(theUnit, kActionTakeoff);
        return 0;
    }
    case EE_SCENARIO_WORLD_GROUP_MOVE:
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, EEUGRust::NewLocation(GE2DPoint(SMEFloat(a[0]), SMEFloat(a[1])), true, a[2] != 0, false, 0.0f, false, 0),
                     kActionPrepareToMove);
        return 0;
    case EE_SCENARIO_WORLD_CAL_NODES: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        inRun->mCalamities.clear();
        thePlayer->GetTechTree()->GetNodeIDs(inRun->mNodeIDs);
        for (unsigned long n = 0; n < inRun->mNodeIDs.size(); n++) {
            EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(inRun->mNodeIDs[n]);
            if (theNode->mData->mCalamity == (unsigned long)a[1] && theNode->GetIsAvailable())
                inRun->mCalamities.push_back(inRun->mNodeIDs[n]);
        }
        return 0;
    }
    case EE_SCENARIO_WORLD_CAL_TARGET_KIND:
        inRun->mCalamity = EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)a[0]);
        return inRun->mCalamity != NULL ? (int64_t)inRun->mCalamity->GetTargetKind() : -1;
    case EE_SCENARIO_WORLD_CAL_VALID_TARGET: {
        EEUnitCalamityClass* theClass = dynamic_cast<EEUnitCalamityClass*>(inRun->mCalamity);
        return theClass != NULL && theClass->IsValidTarget(SMEComplex(a[0]), SMEUnit(a[1]));
    }
    case EE_SCENARIO_WORLD_CAL_VALID_LOCATION: {
        EELocationCalamityClass* theClass = dynamic_cast<EELocationCalamityClass*>(inRun->mCalamity);
        return theClass != NULL && theClass->IsValidLocation(SMEComplex(a[0]), (long)a[1], (long)a[2]);
    }
    case EE_SCENARIO_WORLD_CAST: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EEPlayer* thePlayer = SMEPlayer(a[3]);
        for (unsigned long n = 0; n < inRun->mCalamities.size(); n++) {
            if (!thePlayer->GetTechTree()->Get(inRun->mCalamities[n])->Contains(theUnit->GetUnitClass()->GetUnitType()))
                continue;
            EEUGRust* theGoal = EEUGRust::NewCastCalamity(theUnit, SMEUnitOrNull(a[1]), (long)a[2], (long)a[4],
                                                          inRun->mCalamities[n], true);
            theUnit->ClearGoals(0);
            theUnit->SetGoal(theGoal, 0, 0);
            SMEAction(theUnit, kActionPrepareToMove);
            break;
        }
        return 0;
    }
    case EE_SCENARIO_WORLD_AREA_TAKE_OFF_MAP: SMETakeOffMap(inRun, SMEUnit(a[0])); return 0;
    case EE_SCENARIO_WORLD_AREA_PLACE: SMEPlaceInArea(inRun, SMEUnit(a[0])); return 0;
    case EE_SCENARIO_WORLD_UNLOAD: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        GE2DPoint theTarget(SMEFloat(a[1]), SMEFloat(a[2]));
        EEUGRust* theGoal = EEUGRust::NewUnloadTransport(theUnit, (long)theTarget.GetX(), (long)theTarget.GetY(), true);
        if (theUnit->GetUnitClass()->IsTransport() && theUnit->GetNumCargoUnits() > 0 && theGoal->CanUnloadAtDestination()) {
            theUnit->ClearGoals(0);
            theUnit->SetGoal(theGoal, 0, 0);
        } else {
            delete theGoal;
            theUnit->ClearGoals(0);
            theUnit->SetGoal(EEUGRust::NewLocation(theTarget.GetX(), theTarget.GetY(), 0.0f, true, true, false, 0.0f, false, 0), 0, 0);
        }
        SMEAction(theUnit, kActionPrepareToMove);
        return 0;
    }
    case EE_SCENARIO_WORLD_UPDATE_ACTIVE: gWorld.UpdateActiveUnits(); return 0;
    case EE_SCENARIO_WORLD_AREA_LOCATION:
        inRun->mPlace.SetX(SMEFloat(a[0]));
        inRun->mPlace.SetY(SMEFloat(a[1]));
        return 0;
    }
    return 0;
}

// a class attribute's upgrade field by attribute from Reload Time+ (armor apart): a long's or a float's
typedef EEUnitClassUpgrade SMEU;
const struct { long SMEU::* mLong; float SMEU::* mFloat; } kSMEClassFields[] = {
    {&SMEU::mReloadTime, NULL}, {NULL, &SMEU::mAttack}, {&SMEU::mWorkRate, NULL}, {&SMEU::mFlightTime, NULL},
    {NULL, &SMEU::mLeadership}, {&SMEU::mMaxObjectCarry, NULL}, {&SMEU::mRepairRate, NULL}, {&SMEU::mMaxHitPoints, NULL},
    {&SMEU::mCarryAmount, NULL}, {&SMEU::mHealingPoints, NULL}, {NULL, &SMEU::mRange}, {NULL, &SMEU::mLineOfSight},
    {NULL, &SMEU::mMaxVelocity}, {&SMEU::mPower, NULL}, {NULL, NULL}, {NULL, &SMEU::mCameraFocus}};

// EEModifyUnitAttribute's calls on player a[0]'s upgrade of unit type a[1] (class_attribute.rs
// EE_SCENARIO_WORLD_CA_*), attribute a[2]: its value, its class's base, its change, the pending effect
int64_t SMEClassAttributeWorld(uint32_t inOp, const int64_t* a)
{
    EEPlayer* thePlayer = a[0] >= 0 ? SMEPlayer(a[0]) : NULL;
    EEUnitClassUpgrade* theUpgrade = thePlayer != NULL && a[1] >= 0 ? thePlayer->GetUpgrade((long)a[1]) : NULL;
    if (inOp == EE_SCENARIO_WORLD_CA_UPGRADE)
        return theUpgrade != NULL;
    if (inOp == EE_SCENARIO_WORLD_CA_CAPTURE) {   // TEST ONLY: SMCU (ee/EEScenarioCapture.h)
        long theArgs[7] = {(long)(uint32_t)a[2], (long)(a[2] >> 32), thePlayer != NULL ? (long)thePlayer->mPlayerID : -1,
                           theUpgrade != NULL ? (long)theUpgrade->mUnitClass->mUnitType : -1, (long)a[3], (long)a[4], 0};
        EESMCaptureUnitChange(1, theArgs, NULL, thePlayer, theUpgrade, (unsigned int)a[5]);
        return 0;
    }
    if (inOp == EE_SCENARIO_WORLD_CA_PENDING) {
        EESMEffect theEffect;
        theEffect.mAttributeUnit = (unsigned long)-1;
        theEffect.mType = kSMEffectObject;
        theEffect.mObjectAction = kSMObjectActionUnitAttribute;
        theEffect.mAttributeUnitType = theUpgrade->mUnitClass->GetUnitType();
        theEffect.mAttributePlayer = thePlayer->GetPlayerID();
        theEffect.mObject = 0;
        theEffect.mAttribute = (unsigned long)a[2];
        theEffect.mUsesPercent = a[3] != 0;
        theEffect.mAttributeAmount = (unsigned long)a[4];
        theEffect.mAttributeMode = (unsigned long)a[5];
        gWorld.mScenarioManager->AddV134Effect(theEffect);
        return 0;
    }
    EEUnitClass* theClass = theUpgrade->mUnitClass;
    long theArmor = (long)a[2] - kSMUnitAttributeShockArmorPlus;   // 0..5 an armor
    bool theIsArmor = theArmor >= 0 && theArmor < 6;
    long SMEU::* theLong = theIsArmor ? NULL : kSMEClassFields[a[2] - kSMUnitAttributeReloadTimePlus].mLong;
    float SMEU::* theFloat = theIsArmor ? NULL : kSMEClassFields[a[2] - kSMUnitAttributeReloadTimePlus].mFloat;
    if (inOp == EE_SCENARIO_WORLD_CA_VALUE)
        return theIsArmor ? SMEBits(theUpgrade->mArmor[theArmor]) : theLong != NULL ? (int64_t)(theUpgrade->*theLong)
             : theFloat != NULL ? SMEBits(theUpgrade->*theFloat) : (int64_t)theUpgrade->mIsInitialized;
    if (inOp == EE_SCENARIO_WORLD_CA_BASE) {
        switch (a[2]) {
        case kSMUnitAttributeReloadTimePlus: return SMEBits((float)theClass->mReloadTime);
        case kSMUnitAttributeAttackPlus: return SMEBits((float)theClass->mAttack);
        case kSMUnitAttributeWorkRatePlus: return SMEBits((float)theClass->mWorkRate);
        case kSMUnitAttributeFlightTimePlus: return SMEBits((float)theClass->mFlightTime);
        case kSMUnitAttributeLeadershipPlus: return SMEBits((float)theClass->mLeadership);
        case kSMUnitAttributeMaxObjectsCarriedPlus: return SMEBits((float)theClass->mMaxObjectCarry);
        case kSMUnitAttributeRepairPlus: return SMEBits((float)theClass->mRepairRate);
        case kSMUnitAttributeHitPointsPlus: return SMEBits((float)theClass->mHitPoints);
        case kSMUnitAttributeCarryAmountPlus: return SMEBits((float)theClass->mCarryAmount);
        case kSMUnitAttributeContainerHealingHPPerSecPlus: return SMEBits((float)theClass->mHealingPoints);
        case kSMUnitAttributeRangePlus: return SMEBits(theClass->GetRange());
        case kSMUnitAttributeLineOfSightPlus: return SMEBits(theClass->GetLineOfSight());
        case kSMUnitAttributeMaxVelocityPlus: return SMEBits(theClass->GetMaxVelocity());
        case kSMUnitAttributePowerPlus: return SMEBits((float)theClass->mPower);
        }
        return SMEBits((float)theClass->mArmor[theArmor]);
    }
    switch (a[2]) {   // EE_SCENARIO_WORLD_CA_SET
    case kSMUnitAttributeRangePlus: theUpgrade->SetRange(SMEFloat(a[3])); break;
    case kSMUnitAttributeMaxVelocityPlus: theUpgrade->SetMaxVelocity(SMEFloat(a[3])); break;
    case kSMUnitAttributeLineOfSightPlus: {
        // the sight leaves the map before the change and comes back after it
        std::vector<EEUnit*> theUnits;
        for (EEUnit* theNode = thePlayer->mFirstUnit; theNode != NULL; theNode = static_cast<EEUnit*>(theNode->mNext)) {
            if (theNode->mComplexUnit != NULL && theNode->mUnitClass == theClass) {
                theNode->mComplexUnit->SightOffHere();
                theUnits.push_back(theNode);
            }
        }
        theUpgrade->mLineOfSight = SMEFloat(a[3]);
        for (unsigned long i = 0; i < theUnits.size(); i++)
            theUnits[i]->SightOnHere();
        break;
    }
    case kSMUnitAttributeAnimalAIActivePlus: {
        std::vector<EEHerd*> theHerds;
        theUpgrade->mIsInitialized = a[3] != 0;
        thePlayer->GetHerdsOfType(theHerds, theClass->mUnitType);
        for (long j = (long)theHerds.size() - 1; j >= 0; --j)
            theHerds[j]->mIsActive = theUpgrade->mIsInitialized;
        break;
    }
    default:
        if (theIsArmor)
            theUpgrade->mArmor[theArmor] = SMEFloat(a[3]);
        else if (theLong != NULL)
            theUpgrade->*theLong = (long)a[3];
        else
            theUpgrade->*theFloat = SMEFloat(a[3]);
        break;
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

}   // namespace

// an effect run's world call (ee_scenario effect.rs EE_SCENARIO_WORLD_*): each the original's statement or
// statement sequence at its place, on the units by id, the players by index
int64_t EESMEffectWorld(void* inContext, uint32_t inOp, const int64_t* a)
{
    if (inOp >= EE_SCENARIO_WORLD_CU_RANDOM && inOp <= EE_SCENARIO_WORLD_CU_SET)   // EEChangeUnit's (no effect run)
        return SMEChangeUnitWorld(static_cast<EESMHostContext*>(inContext), inOp, a);
    if (inOp >= EE_SCENARIO_WORLD_CA_UPGRADE && inOp <= EE_SCENARIO_WORLD_CA_CAPTURE)   // EEModifyUnitAttribute's
        return SMEClassAttributeWorld(inOp, a);
    EESMEffectRun* theRun = static_cast<EESMEffectRun*>(static_cast<EESMHostContext*>(inContext));
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
    case EE_SCENARIO_WORLD_AREA_STATE: {
        EESMArea* theArea = theManager->GetArea((unsigned long)a[0]);
        return theArea == NULL ? -1 : (theArea->IsUnset() ? 1 : 0) | (theArea->IsOffMap() ? 2 : 0);
    }
    case EE_SCENARIO_WORLD_AREA_CENTER: {
        U2DPoint<long> theTile = theManager->GetArea((unsigned long)a[0])->GetCenterTile();
        return SMEPack(theTile.GetX(), theTile.GetY());
    }
    case EE_SCENARIO_WORLD_OBJECT_EVALUATE: {
        EESMObject* theObject = theManager->GetScenarioObject((unsigned long)a[0]);
        return theObject == NULL ? -1 : theObject->Evaluate(false);
    }
    case EE_SCENARIO_WORLD_DIALOG_IDLE: return !theManager->mIsShowingDialog && theManager->GetNumMessages() == 0;
    case EE_SCENARIO_WORLD_CLASS: return SMEClassValue(a[0]);
    case EE_SCENARIO_WORLD_OBJECT_PICK: return theManager->GetScenarioObject((unsigned long)a[0])->PickUnitType();
    case EE_SCENARIO_WORLD_TILE_HEIGHT: {
        EEMapTile* theTile = gWorld.mMap->GetTile((long)a[0], (long)a[1]);
        return theTile != NULL ? SMEBits(theTile->mHeight) : -1;
    }

    case EE_SCENARIO_WORLD_CREATE_UNITS: theEffect->CreateUnits(); return 0;
    case EE_SCENARIO_WORLD_KILL: {
        EEUnit* theUnit = SMEUnit(a[0]);
        theUnit->Kill(a[1] != 0, false, false);
        if (theUnit->mTileNode == NULL)
            theUnit->GetPlayer()->KillUnit(theUnit, false);
        return 0;
    }
    case EE_SCENARIO_WORLD_CLEAR_GOALS: SMEComplex(a[0])->ClearGoals((unsigned long)a[1]); return 0;
    case EE_SCENARIO_WORLD_CLEAR_QUEUE: SMEComplex(a[0])->ClearGoalQueue((unsigned long)a[1]); return 0;
    case EE_SCENARIO_WORLD_ACTION: SMEAction(SMEComplex(a[0]), (long)a[1]); return 0;
    case EE_SCENARIO_WORLD_SET_DELETABLE: SMEComplex(a[0])->SetIsDeletable(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_SET_DISABLED: SMEComplex(a[0])->SetDisabled(a[1] != 0, 0); return 0;
    case EE_SCENARIO_WORLD_SET_COMMANDABLE: SMEComplex(a[0])->SetIsCommandable(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_CONVERT: EEConvertUnit(static_cast<EEComplexUnit*>(SMEUnit(a[0])), (long)a[1], 0); return 0;
    case EE_SCENARIO_WORLD_FACE_TEST: {
        GETransformation theLocation(false);
        theLocation = SMEComplex(a[0])->mPhysicalModel.GetLocation();
        return UMAbs(SMEFloat(a[1]) - UMArcTan2(theLocation.GetElement(1, 0), theLocation.GetElement(0, 0))) >= kSMEOneDegree;
    }
    case EE_SCENARIO_WORLD_FACE_SET: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        GETransformation theLocation(false);
        theLocation = theUnit->mPhysicalModel.GetLocation();
        theLocation.SetOrientationYPR(SMEFloat(a[1]), 0.0f, 0.0f);
        theUnit->SetTransformation(theLocation, true);
        GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        theUnit->PlaceOnGround(thePosition);
        return 0;
    }
    case EE_SCENARIO_WORLD_FACE_GOAL: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        theUnit->ClearGoals(0);
        theUnit->SetGoal(EEUGRust::NewLocation(&GE3DPoint(theUnit->mPhysicalModel.GetLocation()), true, true, true,
                                               SMEFloat(a[1]), false, false), 0, 0);
        SMEAction(theUnit, kActionPrepareToMove);
        return 0;
    }
    case EE_SCENARIO_WORLD_RENAME_UNIT: SMEUnit(a[0])->GetStats()->SetName(theEffect->GetNewName()); return 0;
    case EE_SCENARIO_WORLD_RENAME_CLASS:
        SMEPlayer(a[0])->GetUpgrade(gUnitClassTable[a[1]]->GetUnitType())->SetName(theEffect->GetNewName());
        return 0;
    case EE_SCENARIO_WORLD_REFRESH_PANEL:
        if (gUIManager->GetMainGameForm() != NULL)
            gUIManager->GetMainGameForm()->mRefreshGroup = true;
        return 0;
    case EE_SCENARIO_WORLD_STANCE_EXPLORE: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EEPlayer* thePlayer = theUnit->mPlayer;
        EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
        theGroup->AddMember(theUnit, true);
        thePlayer->mPendingGroups.push_back(theGroup);
        theGroup->KeepMobileUnits();
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, EEUGRust::NewExplore(true), kActionFindUnexploredArea);
        return 0;
    }
    case EE_SCENARIO_WORLD_LOCK: {
        // a single-piece unit (a non-gate, or a gate created by a Create effect) has no second or fourth part
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        bool theLocked = a[1] != 0;
        EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetUnitClass());
        if (theUnit->mIsLocked != theLocked)
            theUnit->mIsLocked = theLocked;
        EEComplexUnit* thePart = theGate->GetSecondPart(theUnit);
        if (thePart != NULL && thePart->mIsLocked != theLocked)
            thePart->mIsLocked = theLocked;
        thePart = theGate->GetFourthPart(theUnit);
        if (thePart != NULL && thePart->mIsLocked != theLocked)
            thePart->mIsLocked = theLocked;
        return 0;
    }
    case EE_SCENARIO_WORLD_STANCE_TOWN_PATROL: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EETown* theTown = theUnit->FindNearestTown();
        EEPPatrolGroup* thePatrolGroup = theTown != NULL ? theTown->mPatrolGroup : NULL;
        EEComplexUnit* theTarget = thePatrolGroup != NULL ? thePatrolGroup->GetNearest(theUnit->GetTileX(), theUnit->GetTileY()) : NULL;
        if (theTarget == NULL)
            return 0;
        EEActiveGroup* theGroup = new EEActiveGroup(theUnit->mPlayer);
        theGroup->AddMember(theUnit, true);
        theUnit->mPlayer->mPendingGroups.push_back(theGroup);
        theGroup->KeepMobileUnits();
        EEUGRust* theGoal = EEUGRust::NewPatrol(thePatrolGroup, theTarget, true);
        theGroup->ClearGoal();
        SMEGroupGoal(theGroup, theGoal, kActionMoveToNextPatrolPoint);
        return 0;
    }
    case EE_SCENARIO_WORLD_BEHAVIOR: {
        EEComplexUnit* theUnit = SMEComplex(a[0]);
        EEUnitBehavior* theBehavior = EEUnitBehavior::sTable[(size_t)a[1]];
        if (theUnit->mBehavior != theBehavior)
            theUnit->mBehavior = theBehavior;
        return 0;
    }
    case EE_SCENARIO_WORLD_CHANGE_UNIT:
        EEChangeUnit((unsigned long)a[1], a[2] != 0, SMEUnitOrNull(a[0]), (long)a[3], 0, false, (long)a[4]);
        return 0;

    case EE_SCENARIO_WORLD_PLAYER_RESULT:
        SMEPlayer(a[0])->SetGameResult(a[1] != 0 ? 1 : 2);
        gWorld.RemovePlayerFromGame(a[1] != 0, (unsigned long)a[0]);
        return 0;
    case EE_SCENARIO_WORLD_COMPUTER_SET_ACTIVE: SMEPlayer(a[0])->mComputerPlayer->SetActive(a[1] != 0); return 0;
    case EE_SCENARIO_WORLD_SET_STANCE: gWorld.SetDiplomaticStance((long)a[0], (long)a[1], (unsigned long)a[2]); return 0;
    case EE_SCENARIO_WORLD_NOTICE_TARGET: SMEPlayer(a[0])->NoticeTargetPlayer(SMEPlayer(a[1]), false); return 0;
    case EE_SCENARIO_WORLD_FADE:
        if (a[0] != 0)
            gGraphics->PostFadeIn(3000);
        else
            gGraphics->PostFadeOut(3000);
        return 0;
    case EE_SCENARIO_WORLD_TECH_ADD: SMETechAdd(theEffect, SMEPlayer(a[0])); return 0;
    case EE_SCENARIO_WORLD_TECH_REMOVE: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        if (thePlayer->GetTechTree()->Get(theEffect->mTechnology)->mData->mMakesUnit) {
            EEUnitClass* theClass = gUnitClassTable[thePlayer->GetTechTree()->Get(theEffect->mTechnology)->mData->mUnitClass];
            if (theClass != NULL)
                EEReplaceQueuedTechnology(thePlayer, theClass, 100000);
        }
        thePlayer->GetTechTree()->MakeObsolete(theEffect->mTechnology);
        return 0;
    }
    case EE_SCENARIO_WORLD_ADD_PENDING: theManager->AddV134Effect(*theEffect); return 0;
    case EE_SCENARIO_WORLD_SEES_ALL: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        thePlayer->mSeesAll = a[1] != 0;
        if (thePlayer == gWorld.GetPlayer(gWorld.mCurrentPlayer))
            gMiniMap->Redraw();
        return 0;
    }
    case EE_SCENARIO_WORLD_CIV_POINTS:
        SMEPlayer(a[0])->mCivilization.AddPoints((long)a[1]);
        EESetCivBuilderButtonBlinking(true);
        SMEPlayTrumpets();
        return 0;
    case EE_SCENARIO_WORLD_FLARE_UNIT_TILE: {
        EEUnit* theUnit = SMEUnit(a[0]);
        if (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mContainer == NULL)
            return theUnit->mTile != NULL ? SMEPack(theUnit->mTileX, theUnit->mTileY) : -1;
        EEComplexUnit* theContainer = theUnit->mComplexUnit->mContainer;
        return theContainer->mTile != NULL ? SMEPack(theContainer->mTileX, theContainer->mTileY) : -1;
    }
    case EE_SCENARIO_WORLD_FLARE: {
        EEPlayer* thePlayer = SMEPlayer(a[0]);
        thePlayer->mEvents.AddEvent(U2DPoint<long>((long)a[1], (long)a[2]), 3, thePlayer, -1, true);
        return 0;
    }
    case EE_SCENARIO_WORLD_TRIGGER_SET: {
        EESMTrigger* theTrigger = theManager->GetTrigger((unsigned long)a[0]);
        if (a[1] == 0)
            theTrigger->mConditionStartTime = (unsigned long)a[2];
        else if (a[1] == 1)
            theTrigger->mIsOn = a[2] != 0;
        else
            theTrigger->mHasFired = a[2] != 0;
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
    case EE_SCENARIO_WORLD_CAL_TILE_UNITS: {
        EEMapTile* theMapTile = gWorld.mMap->GetTile((long)a[1], (long)a[2]);
        if (theMapTile == NULL)
            return 0;
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit == NULL || std::find(theRun->mSeen.begin(), theRun->mSeen.end(), theUnit) != theRun->mSeen.end()
                || theRun->mCalamity->CanCastAt(NULL, theRun->mPoint, theUnit->mTileX, theUnit->mTileY, true, theUnit) != 0)
                continue;
            USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity((unsigned long)a[0], -1, theUnit->GetUnitID(), 0);
            theRun->mSeen.push_back(theUnit);
        }
        return 0;
    }
    case EE_SCENARIO_WORLD_GFX_BEGIN: {
        theRun->mGraphic = EEGraphicEffect::Get((unsigned long)a[0]);
        if (theRun->mGraphic == NULL)
            return -1;
        theRun->mGraphic->Load(false);
        long theKind = theRun->mGraphic->mKind;
        return (theKind == kEffect3DRain || theKind == kEffect3DSnow ? EE_SCENARIO_GFX_WEATHER : 0)
             | (theKind == kEffect3DTracers || theKind == kEffect3DShells || theKind == kEffect2DWinterBreath
                || theKind == kEffect2DSmokeMuzzleBlast ? EE_SCENARIO_GFX_PAIR : 0)
             | (theKind == kEffect3DMarchingLine ? EE_SCENARIO_GFX_MARCHING : 0);
    }
    case EE_SCENARIO_WORLD_GFX_WEATHER: {
        const int32_t* theXY = (const int32_t*)(intptr_t)a[0];
        std::vector<U2DPoint<long> > theTiles;
        for (int64_t i = 0; i < a[1]; i++)
            theTiles.push_back(U2DPoint<long>(theXY[2 * i], theXY[2 * i + 1]));
        gEffectManager->CreateWeatherEffect(theTiles, theRun->mGraphic, NULL);
        return 0;
    }
    case EE_SCENARIO_WORLD_GFX_PLACED: {
        EEMapTile* theTile = gWorld.mMap->GetTile((long)a[0], (long)a[1]);
        GE3DPoint& thePoint = theRun->mPoint;
        thePoint.mX = (float)(long)a[0] + 0.5f;
        thePoint.mY = (float)(long)a[1] + 0.5f;
        thePoint.mZ = theTile->GetHeight();
        if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
            thePoint.mZ = 0.0f;
        gEffectManager->CreatePlacedEffect(thePoint, theRun->mGraphic, NULL);
        return 0;
    }
    case EE_SCENARIO_WORLD_GFX_BETWEEN: gEffectManager->CreateEffectBetweenUnits(SMEUnit(a[0]), SMEUnit(a[1]), theRun->mGraphic, NULL); return 0;
    case EE_SCENARIO_WORLD_GFX_UNIT: gEffectManager->CreateUnitEffect(SMEUnit(a[0]), theRun->mGraphic, NULL, false, 0, 0); return 0;
    case EE_SCENARIO_WORLD_GFX_END: theRun->mGraphic->Unload(); return 0;
    case EE_SCENARIO_WORLD_START_CINEMATIC: theManager->StartCinematic(); return 0;
    case EE_SCENARIO_WORLD_SET_GAME_SPEED: gWorld.SetGameSpeed(SMEFloat(a[0])); return 0;
    case EE_SCENARIO_WORLD_END_CINEMATIC: theManager->EndCinematic(); return 0;
    case EE_SCENARIO_WORLD_TIMED_NOT_RUN: theManager->SetTimedEffectHasRun(theEffect, false); return 0;
    case EE_SCENARIO_WORLD_WEATHER:
        if (gWorld.mEnvironmentManager != NULL)
            gWorld.mEnvironmentManager->SetEnabled(a[0] != 0);
        return 0;

    case EE_SCENARIO_WORLD_AI_TOGGLE: {
        typedef void (EEComputerPlayer::*EESMToggle)(bool);
        static const EESMToggle kToggles[] = {&EEComputerPlayer::SetActive, &EEComputerPlayer::SetIsCheating,
            &EEComputerPlayer::SetCanAttack, &EEComputerPlayer::SetCanDefend, &EEComputerPlayer::SetCanBuild,
            &EEComputerPlayer::SetCanExplore, &EEComputerPlayer::SetIgnoreOthers, &EEComputerPlayer::SetCanGather};
        (static_cast<EEComputerPlayer*>(SMEPlayer(a[0]))->*kToggles[a[1]])(a[2] != 0);
        return 0;
    }
    case EE_SCENARIO_WORLD_AI_FAVORITE: {
        EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(SMEPlayer(a[0]));
        if (a[1] == 0)
            thePlayer->SetFavoriteAirType((unsigned long)a[2]);
        else if (a[1] == 1)
            thePlayer->SetFavoriteLandType((unsigned long)a[2]);
        else
            thePlayer->SetFavoriteSeaType((unsigned long)a[2]);
        return 0;
    }
    case EE_SCENARIO_WORLD_AI_STORE:
        EESMStoreAI(static_cast<EEComputerPlayer*>(SMEPlayer(a[0])), *(const EeScenarioAiWrite*)(intptr_t)a[1]);
        return 0;
    case EE_SCENARIO_WORLD_ADD_PENDING_PLAYER: {
        EESMEffect theCopy;
        theCopy.Copy(*theEffect);
        theCopy.mPlayer = (unsigned long)a[0];
        theManager->AddV134Effect(theCopy);
        return 0;
    }

    case EE_SCENARIO_WORLD_CAMERA_RESET: theCamera.ResetPath(); return 0;
    case EE_SCENARIO_WORLD_CAMERA_TURN_RESET: theCamera.mTurnYaw = 0.0f; return 0;
    case EE_SCENARIO_WORLD_CAMERA_ZOOM:
        theCamera.SetMode(3);
        theCamera.SetZoom(SMEFloat(a[0]));
        return 0;
    case EE_SCENARIO_WORLD_CAMERA_KEY: {
        // These camera APIs consume the original base-unit pointer, including its optional complex-unit link.
        long theStepDuration = (long)a[1];
        EEUnit* theUnit = SMEUnit(a[0]);
        EECameraKey theKey;
        theKey.mOffset = theCamera.mEndOffset;
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
