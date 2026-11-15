// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.cpp (__FILE__ 0x4cde4c, ESVERIFY line 195). The
// main game form's command buttons (RTTI EEUIFMGStatusButton > UIButton; vtable 0x44ad98): 0x26de97..0x271319 (ctor,
// ??_G, dtor, the overrides and members, the EEM* inline ctors after their first users, EEUIManager's two main-group
// setters, EEUIEndCinematicModeEvent::ProcessEvent, the main game form's button tests 0x26fc22..0x27115c and its
// UpdateKeyboard/UpdateMouse). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIControlsTable.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEABuild.h"
#include "ee/EEACalamity.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitSetManager.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEMouse.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"   // ee_screens_button_*, ee_screens_command_*: the command panel (ee_screens command_panel.rs)
extern float& gSavedGameSpeed;               // 0x5318b0 (the cinematic's end time) (provisional name)
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (= 100, a word: EEUCFortress.cpp defines it)
extern long& gGameResources;                 // 0x53123c (4: a game kind that delays JIT manufacturing) (provisional name)
// 0x20d571 / 0x20da86: EEGetConversionCost / EECanConvert (ee/EEAreaUtilities.h; EEUCBasicPriest.cpp)

long EEModeAirport(void* inLocation, void* inForm, long inCommand, long inKind);
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)


static bool sB5652F0;                       // 0x5652f0 (the airport commands' shift state)
static bool sB5652F1;                       // 0x5652f1 (the rally point's shift state)

// FUNCTION: EXE 0x66de97
// Creates a main game command button with its icon, dither and queue count text
EEUIFMGStatusButton::EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable)
    : UIButton(inTable.GetID(), NULL, NULL, NULL, inRectangle, inManager->GetFormByID(inTable.GetFormID()),
               inTable.GetFormIndex(), 0)
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GETextureManager* theManager = inManager->GetRasterizer()->GetTextureManager(inManager->GetScreen());
    this->mDrawIcon = false;
    this->mIsCommand = false;
    this->mShowCount = false;
    this->mUnitID = 0;
    this->mTechnology = 0;
    this->mCount = 0;
    this->mButton = 0;
    this->mButtonObject = NULL;
    this->mLastErrorTime = 0;
    GEBitmap* theNormal = inTable.CreateNormalBitmap(inManager, this->GetRectangle());
    GEBitmap* thePressed = inTable.CreatePressedBitmap(inManager, this->GetRectangle());
    this->SetPressedBitmap(thePressed);
    this->SetNormalBitmap(theNormal);
    theNormal->Dereference();
    thePressed->Dereference();
    this->mF138 = this->mF134 = 40.0f;
    this->mIcon = new GEBitmap;
    this->mIcon->SetLocation(this->GetRectangle().GetTopLeft());
    GE2DPoint theLocation;
    theLocation = this->GetRectangle().GetTopLeft();
    theLocation.SetX(theLocation.GetX() + 3.0f);
    theLocation.SetY(theLocation.GetY() + 1.0f);
    this->mBuildQueue = new GEText(inTable.GetFont());
    ESVERIFY_AT(this->mBuildQueue, 195);
    this->mBuildQueue->SetLocation(theLocation);
    inTable.SetupGEText(this->mBuildQueue);
    theDescription.mHasAlpha = true;
    theDescription.mPixelDepth = inManager->GetUITextureBitDepth();
    GETexture* theTexture = theManager->LoadTexture("textures\\Dither", theDescription);
    if (theTexture) {
        theTexture->SetStageAlphaMode(kStageOpModulate, -1);
        theTexture->SetStageColorMode(kStageOpModulate, -1);
        theTexture->SetStageMinFilterMode(1, -1);
        theTexture->SetStageMagFilterMode(1, -1);
        theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        theTexture->SetStageMipMapBias(-0.75f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
        this->mDither = new GEBitmap;
        this->mDither->SetTexture(theTexture, inManager->GetScreen());
        unsigned long theScreen = inManager->GetScreen();
        unsigned long theHeight = theTexture->GetSurface()->GetHeight();
        unsigned long theWidth = theTexture->GetSurface()->GetWidth();
        GERectangle theTextureRectangle(0.0f, 0.0f, (float)theWidth, (float)theHeight);
        this->mDither->ComputeStretchedCoordsAndScale(inRectangle, inRectangle, theTextureRectangle, theScreen);
    }
}

// SYNTHETIC: EXE 0x66e209
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFMGStatusButton+0x0)

// FUNCTION: EXE 0x66e225
// Destroys the command button and its parts
EEUIFMGStatusButton::~EEUIFMGStatusButton()
{
    this->mIcon->Dereference();
    delete this->mBuildQueue;
    this->mDither->Dereference();
    this->ReleaseButton();
}

// One press of a command button (ee_screens status_button.rs asks its reads and calls through
// ee_screenscpp_status_button): the button, the first selected unit (the unload's transport), CanProduce's reason
struct EEUIFMGStatusButtonPress {
    EEUIFMGStatusButton* mButton;
    EEComplexUnit* mTransport;
    UWideString mReason;
    UWideString mText;          // GetHelpString's held text
    UWideString* mOut;          // GetHelpString's result
};
static_assert(EE_SCREENS_SB_TYPE_TELEPORTER == kUnitTypeTeleporter && EE_SCREENS_SB_CATEGORY_FORTRESS == kUnitCategoryFortress &&
                  EE_SCREENS_SB_SOUND_BUTTON1 == kSoundButton1 && EE_SCREENS_SB_SOUND_BUTTON2 == kSoundButton2 &&
                  EE_SCREENS_SB_WORLD_JIT_DELAY == kWorldDeathmatchJitManufacturingDelay &&
                  EE_SCREENS_SB_WORLD_REFUSAL_INTERVAL == kWorldRefusalMessageInterval &&
                  EE_SCREENS_SB_IDS_JIT_NOT_YET == IDS_JUST_IN_TIME_MANUFACTURING_IS_NOT &&
                  EE_SCREENS_SB_IDS_INSUFFICIENT_POWER == IDS_INSUFFICIENT_POWER &&
                  EE_SCREENS_SB_IDS_TRADE_BUY == IDS_TRADE_BUY && EE_SCREENS_SB_IDS_TRADE_SELL == IDS_TRADE_SELL &&
                  EE_SCREENS_SB_IDS_TRADE_FOR == IDS_TRADE_FOR &&
                  EE_SCREENS_SB_IDS_RESOURCE_NAME_BASE == IDS_RESOURCE_NAME_BASE &&
                  EE_SCREENS_SB_IDS_RESOURCE_NAME_GOLD == IDS_RESOURCE_NAME_GOLD &&
                  EE_SCREENS_SB_IDS_IMPROVEMENT_POWER == IDS_IMPROVEMENT_POWER && EE_SCREENS_SB_IDS_POP_COST == IDS_POP_COST,
              "status_button.rs's constants");

// The press's reads and calls (EE_SCREENS_SB_OP_*); the selected group's units by index, the button's node by its
// technology, a unit class by its table index
extern "C" int64_t ee_screenscpp_status_button(void* inPress, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIFMGStatusButtonPress& thePress = *static_cast<EEUIFMGStatusButtonPress*>(inPress);
    EEUIFMGStatusButton* theButton = thePress.mButton;
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(theButton->GetForm());
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* theGroup = thePlayer != NULL ? thePlayer->mSelectedGroup : NULL;   // read only by the ops that use it
    EEComplexUnit* theUnit = (inOp >= EE_SCREENS_SB_OP_UNIT && inOp <= EE_SCREENS_SB_OP_UNIT_POWER) ||
                                     inOp == EE_SCREENS_SB_OP_UNIT_PRIEST || inOp == EE_SCREENS_SB_OP_CONVERSION_COST
                                 ? theGroup->GetUnit(inA)
                                 : NULL;
    bool theNodeOp = (inOp >= EE_SCREENS_SB_OP_NODE_KIND && inOp <= EE_SCREENS_SB_OP_MOUSE_INSIDE) ||
                     inOp == EE_SCREENS_SB_OP_NODE_CALAMITY || inOp == EE_SCREENS_SB_OP_HELP_TEXT ||
                     inOp == EE_SCREENS_SB_OP_NODE_PRESENT || inOp == EE_SCREENS_SB_OP_NODE_NAME_ID;
    EETechTreeNode* theNode = theNodeOp && !theButton->mIsCommand ? thePlayer->GetTechTree()->Get(theButton->mTechnology) : NULL;
    EEUnitClass* theClass = (inOp >= EE_SCREENS_SB_OP_CLASS_PRESENT && inOp <= EE_SCREENS_SB_OP_CLASS_GATE) ||
                                    (inOp >= EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED && inOp <= EE_SCREENS_SB_OP_CLASS_NAME_ID)
                                ? gUnitClassTable[inA]
                                : NULL;
    EESoundObject* theSound;
    switch (inOp) {
    case EE_SCREENS_SB_OP_IS_COMMAND: return theButton->mIsCommand;
    case EE_SCREENS_SB_OP_BUTTON: return theButton->mButton;
    case EE_SCREENS_SB_OP_PANEL:
        if (inC != 0) gUIManager->GetMainGameForm()->GetEntityGroup(inA)->Enable();
        else gUIManager->GetMainGameForm()->GetEntityGroup(inA)->Disable();
        gUIManager->GetMainGameForm()->GetBackgroundByID(inB)->SetVisibility(inC != 0);
        break;
    case EE_SCREENS_SB_OP_MAIN_GROUP:
        if (inA == 0) gUIManager->Fn26FA6A();
        else gUIManager->Fn26FA55();
        break;
    case EE_SCREENS_SB_OP_GROUP_COUNT: return theGroup == NULL ? -1 : (int64_t)theGroup->GetNumUnits();
    case EE_SCREENS_SB_OP_UNIT: return theUnit != NULL;
    case EE_SCREENS_SB_OP_UNIT_TYPE: return theUnit->GetUnitClass()->mUnitType;
    case EE_SCREENS_SB_OP_UNIT_POWER: return inB != 0 ? theUnit->GetPower() : (long)theUnit->mPower;
    case EE_SCREENS_SB_OP_UNLOAD_COST: return gFortressUnloadCost;
    case EE_SCREENS_SB_OP_FIRST_SELECTED: return EEGetFirstSelectedUnit(thePress.mTransport) == true;
    case EE_SCREENS_SB_OP_FIRST_CATEGORY: return thePress.mTransport->GetUnitClass()->mCategory;
    case EE_SCREENS_SB_OP_FIRST_TYPE: return thePress.mTransport->GetUnitClass()->mUnitType;
    case EE_SCREENS_SB_OP_SEND: {
        NEMessage* theMessage;
        switch (inA) {
        case EE_SCREENS_SB_SEND_UNLOAD:
            theMessage = new EEMRecord(kMessageUnloadTransport, {thePress.mTransport->mUnitID, thePress.mTransport->GetTileX(), thePress.mTransport->GetTileY()});
            break;
        case EE_SCREENS_SB_SEND_STOP: theMessage = new EEMRecord(kMessageStopActiveGroup); break;
        case EE_SCREENS_SB_SEND_DISBAND: theMessage = new EEMRecord(kMessageDisband); break;
        case EE_SCREENS_SB_SEND_SELF_DESTRUCT: theMessage = new EEMRecord(kMessageSelfDestruct); break;
        case EE_SCREENS_SB_SEND_PATROL: theMessage = new EEMRecord(kMessagePatrol, {-1}); break;
        case EE_SCREENS_SB_SEND_REPLANT: theMessage = new EEMRecord(kMessageReplantFarms, {-1}); break;
        case EE_SCREENS_SB_SEND_CLEAR_AIRPORT: theMessage = new EEMRecord(kMessageClearAirportCommands); break;
        case EE_SCREENS_SB_SEND_EXPLORE: theMessage = new EEMRecord(kMessageExplore); break;
        case EE_SCREENS_SB_SEND_LOCK: theMessage = new EEMRecord(kMessageLockUnlock, {inB != 0}); break;
        case EE_SCREENS_SB_SEND_PACK: theMessage = new EEMRecord(kMessagePackUnpack, {inB != 0}); break;
        case EE_SCREENS_SB_SEND_FANATIC: theMessage = new EEMRecord(kMessageGoFanatic); break;
        case EE_SCREENS_SB_SEND_JIT: theMessage = new EEMRecord(kMessageJITManufacturing); break;
        case EE_SCREENS_SB_SEND_EXCHANGE: theMessage = new EEMRecord(kMessageExchangeResources, {(inB & 1) != 0, (long)(inB >> 1), inC != 0}); break;
        case EE_SCREENS_SB_SEND_RESEARCH:
            theMessage = new EEMRecord(kMessageResearchTechnology, {theButton->mUnitID, theButton->mTechnology, inB != 0});
            break;
        default:
            theMessage = new EEMRecord(kMessageRemoveTechnology, {theButton->mUnitID, theButton->mTechnology, false, inB != 0});
            break;
        }
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        break;
    }
    case EE_SCREENS_SB_OP_LOCK_CURSOR: theForm->mIsCursorLocked = true; break;
    case EE_SCREENS_SB_OP_POINTER: gMouse->mPointer = inA; break;
    case EE_SCREENS_SB_OP_UI_STATE: theForm->SetUIState(inA); break;
    case EE_SCREENS_SB_OP_SHOW_CANCEL: theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown); break;
    case EE_SCREENS_SB_OP_REFRESH_GROUP:
        if (gUIManager->GetMainGameForm())
            gUIManager->GetMainGameForm()->mRefreshGroup = true;
        break;
    case EE_SCREENS_SB_OP_START_PATROL: theForm->StartPatrol(); break;
    case EE_SCREENS_SB_OP_START_FLARE: theForm->StartFlare(); break;
    case EE_SCREENS_SB_OP_GAME_RESOURCES: return gGameResources;
    case EE_SCREENS_SB_OP_GAME_CLOCK: {
        float theClock = gPhysics->GetGameClock();
        uint32_t theBits;
        memcpy(&theBits, &theClock, 4);
        return theBits;
    }
    case EE_SCREENS_SB_OP_WORLD_LONG: return gWorldTable.GetLongValue((EEWorldConstant)inA);
    case EE_SCREENS_SB_OP_CHAT_STRING: {
        UWideString theString;
        gResource->GetString(inA, theString);
        gUserInterface->AddChatLogEntry(theString, 0);
        break;
    }
    case EE_SCREENS_SB_OP_CAN_AFFORD_TRADE: return EECanAffordTrade(inA);
    case EE_SCREENS_SB_OP_DECODE_TRADE: {
        long theResource;
        bool theSell;
        if (!EEDecodeTradeButton(inA, theResource, theSell) && inB == 0)   // (GetHelpString, b 1, ignores the result)
            return -1;
        return (int64_t)theResource * 2 + (theSell ? 1 : 0);
    }
    case EE_SCREENS_SB_OP_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_SB_OP_SOUND:
        theSound = EESoundObject::Get(inA);
        gSoundManager->Play(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        break;
    case EE_SCREENS_SB_OP_NODE_KIND: return theNode->mData->mKind;
    case EE_SCREENS_SB_OP_NODE_CLASS: return theNode->mData->mUnitClass;
    case EE_SCREENS_SB_OP_CLASS_PRESENT: return theClass != NULL;
    case EE_SCREENS_SB_OP_CLASS_LAND_BUILDING: return theClass->IsLandBuilding() && theClass->mCategory != kUnitCategoryNavalBuilding;
    case EE_SCREENS_SB_OP_CLASS_GATE: return theClass->mCategory == kUnitCategoryGate;
    case EE_SCREENS_SB_OP_NODE_MAKES_UNIT: return theNode->mData->mMakesUnit;
    case EE_SCREENS_SB_OP_BUILD_UNIT: theForm->BuildUnit(theButton->mUnitID, theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_START_WALL: theForm->StartWall(theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_START_GATE: theForm->StartGate(theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_CAN_PRODUCE:
        return thePlayer->CanProduce(theButton->mTechnology, theButton->mUnitType, inA, true, 3, &thePress.mReason) == true;
    case EE_SCREENS_SB_OP_TIME: return (uint32_t)TMGetTime();
    case EE_SCREENS_SB_OP_LAST_ERROR: return (uint32_t)theButton->mLastErrorTime;
    case EE_SCREENS_SB_OP_SET_LAST_ERROR: theButton->mLastErrorTime = (unsigned long)inA; break;
    case EE_SCREENS_SB_OP_CHAT_REASON: gUserInterface->AddChatLogEntry(thePress.mReason, 0); break;
    case EE_SCREENS_SB_OP_REASON_STRING: gResource->GetString(inA, thePress.mReason); break;
    case EE_SCREENS_SB_OP_CALAMITY_COST: {
        EECalamityClass* theCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(
            (EECalamityEnum)theNode->mData->mCalamity);
        return theCalamity != NULL ? theCalamity->GetCost() : 0;
    }
    case EE_SCREENS_SB_OP_NODE_CONTAINS: return theNode->Contains(inA);
    case EE_SCREENS_SB_OP_START_CALAMITY: theForm->StartCalamity(theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_BASE_BUTTON_UP: theButton->BaseButtonUp(); break;
    case EE_SCREENS_SB_OP_RELEASE_MOUSE: theButton->ReleaseCapturedMouse(); break;
    case EE_SCREENS_SB_OP_BUTTON_UP: theButton->ButtonUp(); break;
    case EE_SCREENS_SB_OP_MOUSE_INSIDE: return theButton->IsMouseInside();
    case EE_SCREENS_SB_OP_SET_SHOW_COUNT: theButton->mShowCount = inA != 0; break;
    case EE_SCREENS_SB_OP_COUNT: return theButton->mCount;
    case EE_SCREENS_SB_OP_COUNT_TEXT: {
        UWideString theString;
        sprintf(theString, "%ld", (long)inA);
        theButton->mBuildQueue->SetText(theString);
        break;
    }
    case EE_SCREENS_SB_OP_NODE_CALAMITY: return theNode->mData->GetCalamity();
    case EE_SCREENS_SB_OP_CALAMITY_CLASS_COST:
        return USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inA)->GetCost();
    case EE_SCREENS_SB_OP_IN_FANATIC_SET: return gUnitSetManager.GetSet(0x2f)->Contains((unsigned long)inA);
    case EE_SCREENS_SB_OP_UNIT_CLOAKED: return theGroup->GetUnit(inA)->IsCloaked();
    case EE_SCREENS_SB_OP_SET_AVAILABLE: theButton->mAvailable = inA != 0; break;
    case EE_SCREENS_SB_OP_HELP_TEXT: {
        UIHotKey* theHotKey = NULL;
        switch (inA) {
        case EE_SCREENS_SB_TEXT_STRING: gResource->GetString((unsigned long)inB, thePress.mText); break;
        case EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY: {
            EEButtonObject* theObject = EEButtonObject::Get(theButton->mButton);
            if (theObject == NULL || (theHotKey = theObject->GetHotKey()) == NULL)
                return -1;
            theHotKey->GetHotKeyName(thePress.mText);
            break;
        }
        case EE_SCREENS_SB_TEXT_NODE_HOT_KEY:
            if ((theHotKey = theNode->GetHotKey()) == NULL)
                return -1;
            theHotKey->GetHotKeyName(thePress.mText);
            break;
        case EE_SCREENS_SB_TEXT_UPGRADE_NAME:
            thePress.mText = thePlayer->mUnitClassUpgrades[gUnitClassTable[inB]->mUnitType]->mName;
            break;
        case EE_SCREENS_SB_TEXT_COST: theNode->GetCostString(thePress.mText, theButton->mUnitType, 1.0f); break;
        }
        return (int64_t)strlen(thePress.mText);
    }
    case EE_SCREENS_SB_OP_HELP_TEXT_READ: {
        size_t theLength = strlen(thePress.mText);
        memcpy(reinterpret_cast<char*>(inA), (const char*)thePress.mText, theLength < (size_t)inB ? theLength : (size_t)inB);
        return (int64_t)theLength;
    }
    case EE_SCREENS_SB_OP_HELP_OUT: {
        std::vector<char> theBytes(reinterpret_cast<const char*>(inA), reinterpret_cast<const char*>(inA) + inB);
        theBytes.push_back(0);
        *thePress.mOut = &theBytes[0];
        break;
    }
    case EE_SCREENS_SB_OP_TRADE_PRICE: return EEGetTradePrice(thePlayer, (long)inA, inB != 0);
    case EE_SCREENS_SB_OP_UNIT_PRIEST: return theUnit != NULL && theUnit->GetUnitClass()->IsPriest();
    case EE_SCREENS_SB_OP_CONVERSION_COST: return EEGetConversionCost(theUnit->mComplexUnit);
    case EE_SCREENS_SB_OP_NODE_PRESENT: return theNode != NULL;
    case EE_SCREENS_SB_OP_NODE_NAME_ID: return theNode->mData->mNameID;
    case EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED: return thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mName.GetLength() != 0;
    case EE_SCREENS_SB_OP_CLASS_POPULATION: return theClass->GetPopulationCost();
    case EE_SCREENS_SB_OP_CLASS_NAME_ID: return theClass->mNameStringID;
    case EE_SCREENS_SB_OP_HAS_OBJECT: return theButton->mButtonObject != NULL;
    case EE_SCREENS_SB_OP_STATE: return theButton->GetState();
    case EE_SCREENS_SB_OP_BITMAPS:
        return (theButton->GetNormalBitmap() == theButton->GetPressedBitmap() ? 1 : 0) |
               (theButton->GetFocusedBitmap() == theButton->GetPressedBitmap() ? 2 : 0) | (theButton->GetPressedBitmap() == NULL ? 4 : 0);
    case EE_SCREENS_SB_OP_ICON:
        if (inA == EE_SCREENS_BUTTON_ICON_NONE) {
            theButton->mDrawIcon = false;
        } else {
            if (inA == EE_SCREENS_BUTTON_ICON_PRESSED)
                theButton->mButtonObject->SetPressedBitmap(theButton->mIcon, theButton->GetRectangle(), theButton->GetScreenIndex());
            else
                theButton->mButtonObject->SetFaceBitmap(theButton->mIcon, theButton->GetRectangle(), theButton->GetScreenIndex());
            theButton->mDrawIcon = true;
        }
        break;
    case EE_SCREENS_SB_OP_BASE_COPY_DATA: return theButton->UIButton::CopyData();
    case EE_SCREENS_SB_OP_CAN_PRODUCE_QUIET:
        return thePlayer->CanProduce(theButton->mTechnology, theButton->mUnitType, 1, true, 3, NULL) == true;
    }
    return 0;
}

// FUNCTION: EXE 0x66e274
// Presses a command or tech button: runs the command, starts placing a building, wall or gate, queues the
// research (Shift: five), or starts a calamity when a selected unit can pay it; refusals beep and explain in the chat
// (ee_screens status_button.rs)
void EEUIFMGStatusButton::ButtonUp()
{
    EEUIFMGStatusButtonPress thePress = {this, NULL};
    ee_screens_status_button_up(&thePress);
}

// FUNCTION: EXE 0x66e71d
// Releases the button's button object
void EEUIFMGStatusButton::ReleaseButton()
{
    if (this->mButtonObject) {
        this->mButtonObject->Release();
        this->mButtonObject = NULL;
    }
}

// FUNCTION: EXE 0x66e734
// Snapshots the button for drawing: whether it is available, its icon and the queued count
// The availability of the button: a command button by its own test (mB148), else its technology through the
// player's WriteDetailsHook, or for a calamity with a cost whether a selected unit can pay it (each branch stores
// mAvailable; VC6 merges the stores)
long EEUIFMGStatusButton::CopyData()
{
    EEUIFMGStatusButtonPress thePress = {this, NULL};
    return (long)ee_screens_status_button_copy_data(&thePress);   // ee_screens status_button.rs copy_data
}

// FUNCTION: EXE 0x66eaf4
// Draws the button's icon, dithered when unavailable
// The icon (and the dither over an unavailable one at the last drawn rectangle) under the button
long EEUIFMGStatusButton::DrawPolygons()
{
    GERhombus theRhombus;
    if (this->mDrawIcon) {
        this->mIcon->SetColor(GEColor(1.0f, 1.0f, 1.0f));
        this->GetRasterizer()->Draw(this->mIcon, this->GetScreenIndex());
        if (!this->mAvailable) {
            this->mDither->SetLocation(this->GetCopiedDrawRectangle().GetTopLeft());
            this->GetRasterizer()->Draw(this->mDither, this->GetScreenIndex());
        }
    }
    return UIButton::DrawPolygons();
}

// FUNCTION: EXE 0x66ebb5
// Draws the queued count over the button
long EEUIFMGStatusButton::DrawTextA()
{
    if (this->mShowCount)
        this->GetForm()->DrawClippedText(this->GetRectangle(), this->mBuildQueue, this);
    return UIButton::DrawTextA();
}

// FUNCTION: EXE 0x66ebdf
// Builds the button's help text: market trade prices, command help (with conversion cost for priests) and
// hot key, or a tech's name, cost and description (ee_screens status_button.rs help_string)
bool EEUIFMGStatusButton::GetHelpString(UWideString& outString) const
{
    EEUIFMGStatusButtonPress thePress;
    thePress.mButton = const_cast<EEUIFMGStatusButton*>(this);
    thePress.mTransport = NULL;
    thePress.mOut = &outString;
    return ee_screens_status_button_help(&thePress);
}

// FUNCTION: EXE 0x66f16c
// Presses the button from its hot key
void EEUIFMGStatusButton::HotKeyDown()
{
    this->ButtonDown();
}

// FUNCTION: EXE 0x66f174
// Releases the button from its hot key
void EEUIFMGStatusButton::HotKeyUp()
{
    if (this->GetState() == kButtonStatePressed)
        this->ButtonUp();
}

// FUNCTION: EXE 0x66f186
// Presses a tech button on a right press
long EEUIFMGStatusButton::MouseRButtonDown()
{
    if (!this->mIsCommand) {
        this->CaptureMouse();
        UIButton::ButtonDown();
    }
    return 0;
}

// FUNCTION: EXE 0x66f1a4
// Runs a command button on a right release, or cancels one queued tech (Shift: five, for units only)
// (ee_screens status_button.rs)
long EEUIFMGStatusButton::MouseRButtonUp()
{
    EEUIFMGStatusButtonPress thePress = {this, NULL};
    ee_screens_status_button_right_up(&thePress);
    return 0;
}

// FUNCTION: EXE 0x66f2b0
// Creates a remove-technology message
// (inline ctor, out of line after its first user)


// FUNCTION: EXE 0x66f927
// Creates a clear-airport-commands message
// (EEMClearAirportCommands' inline ctor)

// FUNCTION: EXE 0x66f94b
// Creates a market trade message
// (EEMExchangeResources' inline ctor)

// FUNCTION: EXE 0x66f986
// Creates a go-fanatic message
// (EEMGoFanatic's inline ctor)

// FUNCTION: EXE 0x66f9aa
// Creates a just-in-time manufacturing message
// (EEMJITManufacturing's inline ctor)

// FUNCTION: EXE 0x66f9ce
// Creates a lock/unlock message
// (EEMLockUnlock's inline ctor)

// FUNCTION: EXE 0x66f9fb
// Creates a pack/unpack message
// (EEMPackUnpack's inline ctor)

// FUNCTION: EXE 0x66fa28
// Creates a patrol message
// (EEMPatrol's inline ctor)

// FUNCTION: EXE 0x66fa55
// Asks the main panel to refill its unit (clearing its flag)
void EEUIManager::Fn26FA55()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 0;
    theGroup->mReset = true;
}

// FUNCTION: EXE 0x66fa6a
// Asks the main panel to refill its unit (setting its flag)
void EEUIManager::Fn26FA6A()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 1;
    theGroup->mReset = true;
}

// FUNCTION: EXE 0x66fa82
// Makes the button a command button showing a button object
void EEUIFMGStatusButton::SetCommandButton(unsigned long inButton)
{
    this->ReleaseButton();
    this->mCount = 0;
    this->mIsCommand = true;
    this->mButton = inButton;
    this->mButtonObject = EEButtonObject::Get(inButton);
    this->mButtonObject->Acquire();
}

// FUNCTION: EXE 0x66faba
// Makes the button a tech button for a unit's technology, showing the tech's button
void EEUIFMGStatusButton::SetTechButton(long inTechnology, unsigned long inUnitType, unsigned long inUnitID)
{
    this->ReleaseButton();
    this->mIsCommand = false;
    this->mCount = 0;
    this->mTechnology = inTechnology;
    this->mUnitType = inUnitType;
    this->mUnitID = inUnitID;
    EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(inTechnology);
    this->mButtonObject = theNode->mButton;
    this->mButtonObject->Acquire();
}

// FUNCTION: EXE 0x66fb19
// Moves the button, its icon and its queue count text
void EEUIFMGStatusButton::SetLocation(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation(inLocation);
    theLocation.SetX(theLocation.GetX() + 3.0f);
    theLocation.SetY(theLocation.GetY() + 1.0f);
    this->mIcon->SetLocation(inLocation);
    this->mBuildQueue->SetLocation(theLocation);
    UIButton::SetLocation(inLocation);
}

// FUNCTION: EXE 0x66fb79
// Releases a pressed button when tabbing away
void EEUIFMGStatusButton::TabbedFrom()
{
    if (this->GetState() == kButtonStatePressed)
        this->HotKeyUp();
    else
        UIButton::TabbedFrom();
}

// FUNCTION: EXE 0x66fb8d
// Handles tabbing to the button
void EEUIFMGStatusButton::TabbedTo()
{
    UIButton::TabbedTo();
}

// FUNCTION: EXE 0x66fb93
// Ends the cinematic at its saved end time (sends the end-cinematic message)
// (EEMEndCinematic takes the end time as a float)
long EEUIEndCinematicModeEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageEndCinematic, {gSavedGameSpeed}), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x66fbca
// Creates an end-cinematic message
// (EEMEndCinematic's inline ctor)

// SYNTHETIC: EXE 0x66fbf7
// (EEMessage's scalar deleting destructor, ICF-folded with every EEM* class's that has no members to destroy)

// FUNCTION: EXE 0x66fc22
// Handles the mouse in an airport rally mode: previews and, on a click, sends the airport command at the
// clicked point (Shift keeps the mode)
// NEAR: theChance lives in memory in the original; the pressed block is placed after the epilogue
// (inCommand: EEMSetAirportCommand's command)
long EEModeAirport(void* inLocation, void* inForm, long inCommand, long inKind)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    long theTarget = -1;
    ISEntity* theKey;
    unsigned char theShift;
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured() && gViewport.GetViewRectangle().Contains(theLocation)) {
        gISKeyboard->GetEntity(kKeyShift, theKey);
        theShift = theKey->GetValue() > 0;
        gMouse->mPointer = 0xf;
        EEPointUnitOrTile(theLocation, theTile, thePoint, true);
        if (inCommand != 0xf) {
            EEUnit* theUnit = gUnitPicker.PickUnit(theLocation, 0);
            if (theUnit != NULL) {
                theTarget = theUnit->mUnitID;
                if (thePlayer != theUnit->mPlayer && theUnit->mPlayer->mEnemies[thePlayer->mPlayerID] &&
                    EEIsSeenBy(static_cast<EEComplexUnit*>(theUnit), thePlayer, true)) {
                    float theChance = 0.0f;
                    long theClass = theUnit->GetUnitClass()->mFamily;
                    if (theUnit->GetUnitClass()->CanBeAttacked())
                        theChance = EEDamageFactor(theClass, inKind);
                    if (theChance > 0.0f)
                        gMouse->mPointer = 1;
                }
                theUnit->Dereference();
            } else
                theTarget = -1;
        }
        if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustUnpressed()) {
            if (theShift)
                return false;
            if (!sB5652F0)
                return false;
            sB5652F0 = false;
        } else {
            sB5652F0 = theShift;
            gCommunications->ProcessLocalUserInput(
                new EEMRecord(kMessageSetAirportCommand, {GE2DPoint(thePoint.GetX(), thePoint.GetY()), inCommand, theTarget, theShift}),
                kSlotNone);
            if (theShift)
                return false;
        }
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
        gMouse->mPointer = 0;
    } else {
        gMouse->mPointer = 0;
    }
    return false;
}

// FUNCTION: EXE 0x66fe14
// Creates a set-airport-command message
// (EEMSetAirportCommand's inline ctor)

// FUNCTION: EXE 0x66fe64
// Handles the mouse in atomic bomber rally mode (airport command 0xa)
long EEModeAtomicBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xa, 0);
}

// FUNCTION: EXE 0x66fe79
// Handles the mouse in paratrooper rally mode (airport command 0x14)
long EEModeParatrooperRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0x14, 0);
}

// FUNCTION: EXE 0x66fe8e
// Handles the mouse in bomber rally mode (airport command 9)
long EEModeBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 9, 0);
}

// FUNCTION: EXE 0x66fea3
// Handles the mouse in fighter rally mode (airport command 8)
long EEModeFighterRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 8, 0xc);
}

// FUNCTION: EXE 0x66feb8
// Handles the mouse in timed atomic bomber rally mode (airport command 0xc)
long EEModeTimedAtomicBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xc, 0);
}

// FUNCTION: EXE 0x66fecd
// Handles the mouse in timed bomber rally mode (airport command 0xd)
long EEModeTimedBomberRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xd, 0);
}

// FUNCTION: EXE 0x66fee2
// Handles the mouse in timed fighter rally mode (airport command 0xe)
long EEModeTimedFighterRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xe, 0xc);
}

// FUNCTION: EXE 0x66fef7
// Handles the mouse in timed rally mode (airport command 0xf)
long EEModeTimedRally(void* inLocation, void* inForm)
{
    return EEModeAirport(inLocation, inForm, 0xf, 0xc);
}

// FUNCTION: EXE 0x66ff0c
// Handles the mouse in attack-ground mode: range cursor, then an attack-ground order at the clicked point
// NEAR: the range test's x87 shape and block layout
long EEModeAttackGround(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustPressed()) {
        if (EEPointUnitOrTile(theLocation, theTile, thePoint, true)) {
            if (theTile->IsSeenBy(thePlayer)) {
                if (EEGetFirstSelectedUnit(theUnit) == true) {
                    gMouse->mPointer = theUnit->mComplexUnit->DistanceSquared3D(thePoint) >= UMSquare(theUnit->GetUnitClass()->mMinRange) ? 1 : 3;
                } else {
                    gMouse->mPointer = 0;
                }
            } else {
                gMouse->mPointer = 7;
            }
        }
        return false;
    }
    if (!EEPointUnitOrTile(theLocation, theTile, thePoint, true) || !theTile->IsSeenBy(thePlayer))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (EEGetFirstSelectedUnit(theUnit) == true) {
        if (theUnit->mComplexUnit->DistanceSquared3D(thePoint) >= UMSquare(theUnit->GetUnitClass()->mMinRange))
            gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageAttackGround, {thePoint.GetX(), thePoint.GetY()}), kSlotNone);
        EEMakeCalamityEffect(thePoint, theTarget);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    if (theTarget)
        theTarget->Dereference();
    return false;
}

// FUNCTION: EXE 0x67012b
// Handles the mouse in build mode: previews the building ghost and places it on a click
// NEAR: the ghost loop (out-of-line vector operator[]/_Ptrit operator+ in the original)
long EEModeBuild(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE2DPoint theTile(0.0f, 0.0f);
    ISEntity* theKey;
    if (!theForm->GetIsMouseCaptured()) {
        EEPreviewPlacement(theForm->GetMouseLocation());
        gMouse->mPointer = 0;
        if (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetLMouseButton()->GetValue())
            return false;
        if (UIManager::GetLMouseButton()->JustUnpressed()) {
            gWorld.mGhostUnitsLock->Enter();
            for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
                EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(gWorld.mGhostUnits[i]);
                if (!theGhost->mCanPlace &&
                    ((theGhost->mUnitClass->mUnitType == kUnitTypeFarm || theGhost->mUnitClass->mUnitType == kUnitTypeRoboticFarm) &&
                         gWorld.GetNumGhostUnits() == 1 ||
                     theGhost->mUnitClass->mUnitType != kUnitTypeFarm && theGhost->mUnitClass->mUnitType != kUnitTypeRoboticFarm))
                    goto leave;
            }
            gISKeyboard->GetEntity(kKeyShift, theKey);
            theForm->mBuild->mIsQueued = theKey->GetValue() > 0;
            theTile.SetX((float)(static_cast<EEGhostUnit*>(gWorld.mGhostUnits[0]))->mTileX);
            theTile.SetY((float)((EEGhostUnit*)*(gWorld.mGhostUnits.begin() + 0))->mTileY);
            theForm->mBuild->Build(theTile);
            if (theKey->GetValue() == 0) {
                gWorld.ClearGhostUnits();
                theForm->SetUIState(0);
            }
        leave:
            gWorld.mGhostUnitsLock->Leave();
        } else if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->GetValue() &&
                   UIManager::GetRMouseButton()->JustUnpressed()) {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
            theForm->SetUIState(0);
        }
    }
    return false;
}

// FUNCTION: EXE 0x670343
// Handles the mouse in gate placement mode
long EEModeGate(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured()) {
        gMouse->mPointer = 0;
        theForm->mBuildGate.Update(*static_cast<const GE2DPoint*>(inLocation));
    }
    return false;
}

// FUNCTION: EXE 0x670379
// Handles the mouse in wall placement mode
long EEModeWall(void* inLocation, void* inForm)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured()) {
        gMouse->mPointer = 0;
        theForm->mBuildWall.Update(*static_cast<const GE2DPoint*>(inLocation));
    }
    return false;
}

// FUNCTION: EXE 0x6703af
// Handles the mouse in flare mode: a click on the map sends a flare there and leaves the mode
long EEModeFlare(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    long theX = -10000;
    long theY = -10000;
    EETile* theTile = NULL;
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 0x16;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustPressed())
        return false;
    if (EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true)) {
        EEOrderAtCursor(theLocation, NULL);
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSendFlare, {theX, theY}), kSlotNone);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    return false;
}

// FUNCTION: EXE 0x67048e
// Creates a send-flare message
// (inline ctor: ee/EEMessage.h)

// FUNCTION: EXE 0x6704c9
// Handles the mouse in garrison mode: shows whether the unit under the cursor can take the selection, and
// garrisons on a click
// NEAR: the original reads the view rectangle through an out-of-line vector operator[]
long EEModeGarrison(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    long theX;
    long theY;
    long theMessage;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!theView.Contains(theLocation))
        return false;
    if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetLMouseButton()->JustPressed())
        return false;
    if (!EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (theTarget != NULL) {
        EEComplexUnit* theComplex = theTarget->mComplexUnit;
        if (EEGetFirstSelectedUnit(theUnit) == true) {
            EEComplexUnit* theOwn = theUnit->mComplexUnit;
            if (theOwn && theComplex && theComplex->mIsBuilt && theOwn->mPlayer == theComplex->mPlayer &&
                theComplex->GetUnitClass()->mGarrisonCapacity != theComplex->mGarrisonCount) {
                gCommunications->ProcessLocalUserInput(
                    new EEMRecord(kMessageLoadGarrison, {theTarget->GetUnitID(), theOwn->mUnitID, 0}), kSlotNone);
                EEOrderAtCursor(theLocation, theComplex);
                goto done;
            }
            if (theComplex && theComplex->GetUnitClass()->mGarrisonCapacity)
                theMessage = 0x7c3a;
            else
                theMessage = 0x7c41;
        } else {
            theMessage = 0x7c41;
        }
        {
            U2DPoint<long> thePoint;
            thePoint.mX = theX;
            thePoint.mY = theY;
            gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), theMessage, false);
        }
    done:
        theTarget->Dereference();
    } else {
        U2DPoint<long> thePoint;
        thePoint.mX = theX;
        thePoint.mY = theY;
        gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), 0x7c41, false);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    return false;
}

// FUNCTION: EXE 0x670703
// Handles the mouse in unload mode: unloads the selected transport at the clicked point
// NEAR: the original reads the view rectangle through out-of-line begin()/operator+
long EEModeUnload(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    EETile* theTile;
    long theX;
    long theY;
    if (theForm->GetIsMouseCaptured() || !theView.Contains(theLocation))
        return false;
    if (!UIManager::GetRMouseButton()->JustPressed() && !UIManager::GetLMouseButton()->JustPressed())
        return false;
    if (!EEPointUnitOrTileXY(theLocation, theTile, theX, theY, true))
        return false;
    EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
    if (EEGetFirstSelectedUnit(theUnit) == true) {
        if (theUnit) {
            gCommunications->ProcessLocalUserInput(
                new EEMRecord(kMessageUnloadTransport, {theUnit->GetComplexUnit()->GetUnitID(), theX, theY}), kSlotNone);
            EEOrderAtCursor(theLocation, theTarget);
        }
    } else {
        U2DPoint<long> thePoint;
        thePoint.mX = theX;
        thePoint.mY = theY;
        gWorld.GetCurrentPlayer()->mEvents.AddEvent(thePoint, 2, gWorld.GetCurrentPlayer(), 0x7c3b, false);
    }
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    if (theTarget)
        theTarget->Dereference();
    return false;
}

// FUNCTION: EXE 0x670898
// Handles the mouse in heal mode: a click on a unit the healer's set can heal sends a heal order
// NEAR: GetViewport inline budget, register choice
long EEModeHeal(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    EEComplexUnit* theUnit = NULL;
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 0x12;
    if (!theForm->GetIsMouseCaptured() && theView.Contains(theLocation)) {
        if (EEGetFirstSelectedUnit(theUnit)) {
            EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
            EEComplexUnit* theComplex;
            if (theTarget != NULL && (theComplex = theTarget->mComplexUnit) != NULL) {
                if (gUnitSetManager.Contains(theComplex->GetUnitClass()->GetUnitType(), theUnit->mStats->mWonderUnitSet)) {
                    if (!UIManager::GetLMouseButton()->JustUnpressed() &&
                        !UIManager::GetRMouseButton()->JustUnpressed())
                        goto done;
                    gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHealUnit, {theComplex->mUnitID}), kSlotNone);
                } else {
                    gMouse->mPointer = 0x13;
                    goto done;
                }
            } else {
                if (!UIManager::GetRMouseButton()->JustUnpressed())
                    goto done;
            }
            theForm->SetUIState(0);
            theForm->mIsCursorLocked = false;
        done:
            if (theTarget)
                theTarget->Dereference();
        } else {
            theForm->SetUIState(0);
            theForm->mIsCursorLocked = false;
        }
    }
    return false;
}

// FUNCTION: EXE 0x6709ee
// Handles the mouse in convert mode: shows whether a selected unit can convert the unit under the cursor
// (and is in range), and orders it on a click (Shift keeps the mode)
// NEAR: view rectangle access and the loop shape
long EEModeConvert(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    bool theInRange = false;
    bool theFound = false;
    ISEntity* theKey;
    EEComplexUnit* theFirst = thePlayer->mSelectedGroup->GetFirstUnit();
    theForm->mIsCursorLocked = true;
    gMouse->mPointer = 10;
    if (theForm->GetIsMouseCaptured())
        return false;
    if (!theView.Contains(theLocation))
        return false;
    if (theFirst != NULL) {
        EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
        EEComplexUnit* theComplex;
        if (theTarget == NULL || (theComplex = theTarget->GetComplexUnit()) == NULL) {
            if (!UIManager::GetRMouseButton()->JustUnpressed()) {
                gMouse->mPointer = 0x13;
                goto done;
            }
        } else {
            for (unsigned long i = 0;; i++) {
                if (!(i < thePlayer->mSelectedGroup->GetNumUnits()))
                    break;
                if (theFound)
                    break;
                if (theInRange) {
                    gMouse->mPointer = 0x10;
                    goto done;
                }
                theFound = EECanConvert(thePlayer->mSelectedGroup->GetUnit(i), theComplex, theInRange);
            }
            if (!theFound) {
                if (theInRange)
                    gMouse->mPointer = 0x10;
                else
                    gMouse->mPointer = 0x13;
                goto done;
            }
            if (!UIManager::GetLMouseButton()->JustUnpressed() && !UIManager::GetRMouseButton()->JustUnpressed())
                goto done;
            gISKeyboard->GetEntity(kKeyShift, theKey);
            gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageAttackUnit, {theComplex->mUnitID, theKey->GetValue() > 0, true}),
                                                   kSlotNone);
            if (theKey->GetValue())
                goto done;
        }
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
done:
        if (theTarget)
            theTarget->Dereference();
    } else {
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
    }
    return false;
}

// FUNCTION: EXE 0x670c08
// Handles the mouse in calamity mode: previews the calamity at the cursor and casts it on a click
// NEAR: GetViewport expands vector::operator[] (the original calls it), epilogue placement
long EEModeCalamity(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    EEComplexUnit* theUnit = NULL;
    const GERectangle& theView = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
    theForm->mIsCursorLocked = true;
    if (theForm->GetIsMouseCaptured())
        return false;
    EEPreviewPlacement(theForm->GetMouseLocation());
    if (theView.Contains(theLocation)) {
        long theCheck = theForm->mCalamity->Check(theLocation);
        if (theCheck == 0) {
            if (EEGetFirstSelectedUnit(theUnit) && theUnit->mHitPoints > 0) {
                gMouse->mPointer = 0xd;
                if (theUnit->mUnitID == theForm->mCalamity->mCasterID) {
                    if (!UIManager::GetLMouseButton()->JustUnpressed() &&
                        !UIManager::GetRMouseButton()->JustUnpressed())
                        return false;
                    theForm->mCalamity->Cast(theLocation);
                }
            }
        } else if (theCheck == 1) {
            if (EEGetFirstSelectedUnit(theUnit)) {
                if (theUnit->mHitPoints > 0)
                    gMouse->mPointer = 0xe;
                else
                    gMouse->mPointer = 0;
                return false;
            }
        } else if (theCheck == 2) {
            if (EEGetFirstSelectedUnit(theUnit)) {
                if (theUnit->mHitPoints > 0)
                    gMouse->mPointer = 0x11;
                else
                    gMouse->mPointer = 0;
                return false;
            }
        } else
            return false;
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        theForm->SetUIState(0);
        theForm->mIsCursorLocked = false;
    } else
        gMouse->mPointer = 0;
    return false;
}

// FUNCTION: EXE 0x670d77
// Handles the mouse in the normal mode: selection, camera and the unit editing hack
long EEModeNormal(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    GE2DPoint theTile(0.0f, 0.0f);
    EETile* theTarget;
    bool theInView = theForm->IsInGameView(theLocation);
    if (theForm->GetIsMouseCaptured())
        return false;
    if (UIManager::GetLMouseButton()->JustPressed()) {
        if (!theForm->GetEntityGroup(1)->GetIsEnabled()) {
            theForm->mCurrentGroup = 0;
            theForm->SetEntityGroup(1);
        }
        gWorld.mCameraControl.ClearUnitA();
        gWorld.mCameraControl.ClearUnitB();
        if (theInView) {
            if (EEPointUnitOrTile(theLocation, theTarget, thePoint, true)) {
                theTile.SetX(thePoint.GetX());
                theTile.SetY(thePoint.GetY());
                theForm->mUnitHack->Fn157269(theTile);
            }
            static_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits))->StartSelection(theLocation);
        }
    } else if (!UIManager::GetLMouseButton()->GetValue()) {
        if (UIManager::GetLMouseButton()->JustUnpressed()) {
            bool theFound = EEPointUnitOrTile(theLocation, theTarget, thePoint, true);
            if (theInView && theFound) {
                theTile.SetX(thePoint.GetX());
                theTile.SetY(thePoint.GetY());
                theForm->mUnitHack->PlaceUnit(theTile);
            }
        } else if (UIManager::GetRMouseButton()->JustPressed()) {
            if (!theForm->GetEntityGroup(1)->GetIsEnabled()) {
                theForm->mCurrentGroup = 0;
                theForm->SetEntityGroup(1);
            }
            if (theInView)
                (static_cast<EEUICCommandUnits*>(theForm->GetControl(kMainGameCommandUnits)))->StartCommand(theLocation);
        } else if (!UIManager::GetRMouseButton()->GetValue()) {
            UIManager::GetRMouseButton()->JustUnpressed();
        }
    }
    return false;
}

// FUNCTION: EXE 0x670f20
// Handles the mouse in rally point mode: sets the selected buildings' rally point at the clicked point
// theShift is a byte (`setg` into it, `setne` when it is copied to the bool sB5652F1)
long EEModeRally(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    ISEntity* theKey;
    EEComplexUnit* theUnit = NULL;
    bool theDone = false;
    unsigned char theShift;
    theForm->mIsCursorLocked = true;
    if (!theForm->GetIsMouseCaptured() && gViewport.GetViewRectangle().Contains(theLocation)) {
        gMouse->mPointer = 0xf;
        gISKeyboard->GetEntity(kKeyShift, theKey);
        theShift = theKey->GetValue() > 0;
        if (!UIManager::GetLMouseButton()->JustPressed() && !UIManager::GetRMouseButton()->JustUnpressed()) {
            if (theShift)
                return false;
            if (!sB5652F1)
                return false;
            sB5652F1 = false;
            goto finish;
        }
        sB5652F1 = theShift;
        if (EEGetFirstSelectedUnit(theUnit)) {
            if (theUnit->GetUnitClass()->mCanSetRallyPoint) {
                EEPointUnitOrTile(theLocation, theTile, thePoint, true);
                EEUnit* theTarget = gUnitPicker.PickUnit(theLocation, 0);
                long theTargetID;
                if (theTarget != NULL) {
                    theTargetID = theTarget->mUnitID;
                    theTarget->Dereference();
                } else
                    theTargetID = -1;
                EEComplexUnit* theComplex = theUnit->mComplexUnit;
                gCommunications->ProcessLocalUserInput(
                    new EEMRecord(kMessageSetRallyGoal, {GE2DPoint(thePoint.GetX(), thePoint.GetY()), theTargetID, theComplex->GetUnitID(), theShift}),
                    kSlotNone);
            }
        } else
            theDone = true;
        if (!theShift)
            theDone = true;
    } else {
        gMouse->mPointer = 0;
    }
    if (theDone != true)
        return false;
finish:
    theForm->SetUIState(0);
    theForm->mIsCursorLocked = false;
    gMouse->mPointer = 0;
    return false;
}

// FUNCTION: EXE 0x6710d0
// Starts a drag selection on a left press in the game view (stops following a unit)
long EEModeCamera(void* inLocation, void* inForm)
{
    const GE2DPoint& theLocation = *static_cast<const GE2DPoint*>(inLocation);
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    bool theInView = gViewport.GetViewRectangle().Contains(theLocation);
    if (theForm->GetIsMouseCaptured())
        return false;
    if (UIManager::GetLMouseButton()->JustPressed()) {
        gWorld.mCameraControl.Follow(NULL, false, 1.0f, 0.0f, 0.0f);
        if (theInView)
            (static_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits)))->StartSelection(theLocation);
    } else if (!UIManager::GetLMouseButton()->GetValue()) {
        UIManager::GetLMouseButton()->JustUnpressed();
    }
    return false;
}

// FUNCTION: EXE 0x67115c
// Handles the keys in cinematic mode: Escape or F1 ends it, F3 pauses, F9 takes a screenshot
static bool EECinematicKeyPressed(unsigned long inKey)
{
    ISEntity* theKey;
    return gISKeyboard->GetEntity(inKey, theKey) == 0 && theKey->GetValue() && theKey->JustPressed();
}

bool EEModeKeyboardCinematic(void* inForm)
{
    ISEntity* theKey;
    // either key ends the cinematic (each is tested, not only the first one found)
    if ((EECinematicKeyPressed(kKeyEscape) || EECinematicKeyPressed(kKeyF1)) && !gCommunications->GetIsGamePaused()) {
        UIEvent* theEvent = new EEUIEndCinematicModeEvent;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    } else if (gISKeyboard->GetEntity(kKeyF3, theKey) == 0 && theKey->JustPressed() && !gWorld.GetScenarioManager()->mIsEndingCinematic) {
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_MG_PAUSE);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    } else if (gISKeyboard->GetEntity(kKeyF9, theKey) == 0 && theKey->JustPressed()) {
        gUserInterface->PostScreenShot(-1, true, false);
    }
    return true;
}

// SYNTHETIC: EXE 0x671265
// (EEUIEndCinematicModeEvent's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x671290
// Runs the keyboard test of the current interface mode, else the form's keys
// the keyboard tests take the form only (EEUIFMGKeyboardTest)
long EEUIFMainGame::UpdateKeyboard()
{
    long theResult = 0;
    // (the tests return bool; the original keeps the result in edi)
    if (!this->mKeyboardTests[this->mUIState](this))
        theResult = UIForm::UpdateKeyboard();
    return theResult;
}

// FUNCTION: EXE 0x6712b8
// Runs the mouse: the unit hack, the form's controls, then the current interface mode's mouse test
// returns the last UpdateMouse/mouse-test result (the original never resets eax after `xor eax, eax`)
long EEUIFMainGame::UpdateMouse()
{
    long theResult = 0;
    this->mB5F0 = false;
    if (this->mUIState != 0x11) {
        this->mUnitHack->Update(this->GetMouseLocation());
        if (this->GetMouseCaptured() == NULL) {
            theResult = UIForm::UpdateMouse();
            if (this->GetMouseCaptured() == NULL)
                theResult = this->mMouseTests[this->mUIState]((void*)&this->GetMouseLocation(), this);
        } else
            theResult = UIForm::UpdateMouse();
    }
    return theResult;
}
