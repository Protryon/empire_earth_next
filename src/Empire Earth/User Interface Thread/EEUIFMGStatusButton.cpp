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
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIControlsTable.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitSetManager.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"   // ee_screens_button_*, ee_screens_command_*: the command panel (ee_screens command_panel.rs)
extern float& gSavedGameSpeed;               // 0x5318b0 (the cinematic's end time) (provisional name)
extern unsigned short gFortressUnloadCost;   // 0x4c8fa4 (= 100, a word: EEUCFortress.cpp defines it)
extern long& gGameResources;                 // 0x53123c (4: a game kind that delays JIT manufacturing) (provisional name)
// 0x20d571 / 0x20da86: EEGetConversionCost / EECanConvert (ee/EEAreaUtilities.h; EEUCBasicPriest.cpp)

long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);  // 0x23cc42 (EEMessage.cpp's)
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

// Destroys the command button and its parts
EEUIFMGStatusButton::~EEUIFMGStatusButton()
{
    this->mIcon->Dereference();
    delete this->mBuildQueue;
    this->mDither->Dereference();
    this->ReleaseButton();
}

// The command buttons' part of the main game form's host (ee_screens status_button_ffi.rs; EE_SCREENS_MF_OP_SB +
// EE_SCREENS_SB_OP_*): the button, the first selected unit (the unload's transport), the form's held text
// (CanProduce's reason, the help text's pieces) and the help string's result; the selected group's units by index,
// the button's node by its technology, a unit class by its table index. The ops the form's host answers itself
// (the pointer, the UI state, the cursor lock, the refresh, the sounds, the panels, the first selected unit, the
// chat line, the strings, the text's read) are its own.
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

int64_t EEStatusButtonOp(EEUIFMGStatusButton* theButton, EEComplexUnit* inFirst, UWideString& ioText,
                         UWideString* outString, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
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
    switch (inOp) {
    case EE_SCREENS_SB_OP_IS_COMMAND: return theButton->mIsCommand;
    case EE_SCREENS_SB_OP_BUTTON: return theButton->mButton;
    case EE_SCREENS_SB_OP_MAIN_GROUP:
        if (inA == 0) gUIManager->Fn26FA6A();
        else gUIManager->Fn26FA55();
        break;
    case EE_SCREENS_SB_OP_UNIT_POWER: return inB != 0 ? theUnit->GetPower() : (long)theUnit->mPower;
    case EE_SCREENS_SB_OP_UNLOAD_COST: return gFortressUnloadCost;
    case EE_SCREENS_SB_OP_FIRST_CATEGORY: return inFirst->GetUnitClass()->mCategory;
    case EE_SCREENS_SB_OP_FIRST_TYPE: return inFirst->GetUnitClass()->mUnitType;
    case EE_SCREENS_SB_OP_SEND: {   // (the sends of the op's arguments are hud_sends.rs')
        NEMessage* theMessage;
        switch (inA) {
        case EE_SCREENS_SB_SEND_UNLOAD:
            theMessage = new EEMRecord(kMessageUnloadTransport, {inFirst->mUnitID, inFirst->GetTileX(), inFirst->GetTileY()});
            break;
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
    case EE_SCREENS_SB_OP_SHOW_CANCEL: theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown); break;
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
    case EE_SCREENS_SB_OP_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_SB_OP_NODE_KIND: return theNode->mData->mKind;
    case EE_SCREENS_SB_OP_NODE_CLASS: return theNode->mData->mUnitClass;
    case EE_SCREENS_SB_OP_CLASS_LAND_BUILDING: return theClass->IsLandBuilding() && theClass->mCategory != kUnitCategoryNavalBuilding;
    case EE_SCREENS_SB_OP_NODE_MAKES_UNIT: return theNode->mData->mMakesUnit;
    case EE_SCREENS_SB_OP_BUILD_UNIT: theForm->BuildUnit(theButton->mUnitID, theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_START_WALL: theForm->StartWall(theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_START_GATE: theForm->StartGate(theButton->mTechnology); break;
    case EE_SCREENS_SB_OP_CAN_PRODUCE:
        return thePlayer->CanProduce(theButton->mTechnology, theButton->mUnitType, inA, true, 3, &ioText) == true;
    case EE_SCREENS_SB_OP_TIME: return (uint32_t)TMGetTime();
    case EE_SCREENS_SB_OP_LAST_ERROR: return (uint32_t)theButton->mLastErrorTime;
    case EE_SCREENS_SB_OP_SET_LAST_ERROR: theButton->mLastErrorTime = (unsigned long)inA; break;
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
        case EE_SCREENS_SB_TEXT_STRING: gResource->GetString((unsigned long)inB, ioText); break;
        case EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY: {
            EEButtonObject* theObject = EEButtonObject::Get(theButton->mButton);
            if (theObject == NULL || (theHotKey = theObject->GetHotKey()) == NULL)
                return -1;
            theHotKey->GetHotKeyName(ioText);
            break;
        }
        case EE_SCREENS_SB_TEXT_NODE_HOT_KEY:
            if ((theHotKey = theNode->GetHotKey()) == NULL)
                return -1;
            theHotKey->GetHotKeyName(ioText);
            break;
        case EE_SCREENS_SB_TEXT_UPGRADE_NAME:
            ioText = thePlayer->mUnitClassUpgrades[gUnitClassTable[inB]->mUnitType]->mName;
            break;
        case EE_SCREENS_SB_TEXT_COST: theNode->GetCostString(ioText, theButton->mUnitType, 1.0f); break;
        }
        return (int64_t)strlen(ioText);
    }
    case EE_SCREENS_SB_OP_HELP_OUT: {
        std::vector<char> theBytes(reinterpret_cast<const char*>(inA), reinterpret_cast<const char*>(inA) + inB);
        theBytes.push_back(0);
        *outString = &theBytes[0];
        break;
    }
    case EE_SCREENS_SB_OP_TRADE_PRICE: return EEGetTradePrice(thePlayer, (long)inA, inB != 0);
    case EE_SCREENS_SB_OP_UNIT_PRIEST: return theUnit != NULL && theUnit->GetUnitClass()->IsPriest();
    case EE_SCREENS_SB_OP_CONVERSION_COST: return EEGetConversionCost(theUnit->mComplexUnit);
    case EE_SCREENS_SB_OP_NODE_PRESENT: return theNode != NULL;
    case EE_SCREENS_SB_OP_NODE_NAME_ID: return theNode->mData->mNameID;
    case EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED: return thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mName.GetLength() != 0;
    case EE_SCREENS_SB_OP_CLASS_POPULATION: return theClass->GetPopulationCost();
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

// Presses a command or tech button: runs the command, starts placing a building, wall or gate, queues the
// research (Shift: five), or starts a calamity when a selected unit can pay it; refusals beep and explain in the chat
// (ee_screens status_button.rs)
void EEUIFMGStatusButton::ButtonUp()
{
    static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_UP);
}

// Releases the button's button object
void EEUIFMGStatusButton::ReleaseButton()
{
    if (this->mButtonObject) {
        this->mButtonObject->Release();
        this->mButtonObject = NULL;
    }
}

// Snapshots the button for drawing: whether it is available, its icon and the queued count
// The availability of the button: a command button by its own test (mB148), else its technology through the
// player's WriteDetailsHook, or for a calamity with a cost whether a selected unit can pay it (each branch stores
// mAvailable; VC6 merges the stores)
long EEUIFMGStatusButton::CopyData()
{
    return (long)static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_COPY_DATA);
}

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

// Draws the queued count over the button
long EEUIFMGStatusButton::DrawTextA()
{
    if (this->mShowCount)
        this->GetForm()->DrawClippedText(this->GetRectangle(), this->mBuildQueue, this);
    return UIButton::DrawTextA();
}

// Builds the button's help text: market trade prices, command help (with conversion cost for priests) and
// hot key, or a tech's name, cost and description (ee_screens status_button.rs help_string)
bool EEUIFMGStatusButton::GetHelpString(UWideString& outString) const
{
    EEUIFMGStatusButton* theButton = const_cast<EEUIFMGStatusButton*>(this);
    return static_cast<EEUIFMainGame*>(theButton->GetForm())->ButtonCall(theButton, EE_SCREENS_SB_DO_HELP, &outString) != 0;
}

// Presses the button from its hot key
void EEUIFMGStatusButton::HotKeyDown()
{
    this->ButtonDown();
}

// Releases the button from its hot key
void EEUIFMGStatusButton::HotKeyUp()
{
    if (this->GetState() == kButtonStatePressed)
        this->ButtonUp();
}

// Presses a tech button on a right press
long EEUIFMGStatusButton::MouseRButtonDown()
{
    if (!this->mIsCommand) {
        this->CaptureMouse();
        UIButton::ButtonDown();
    }
    return 0;
}

// Runs a command button on a right release, or cancels one queued tech (Shift: five, for units only)
// (ee_screens status_button.rs)
long EEUIFMGStatusButton::MouseRButtonUp()
{
    static_cast<EEUIFMainGame*>(this->GetForm())->ButtonCall(this, EE_SCREENS_SB_DO_RIGHT_UP);
    return 0;
}

// Asks the main panel to refill its unit (clearing its flag)
void EEUIManager::Fn26FA55()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 0;
    theGroup->mReset = true;
}

// Asks the main panel to refill its unit (setting its flag)
void EEUIManager::Fn26FA6A()
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(this->mMainGameForm->mGroups[0]);
    theGroup->mL1C = 1;
    theGroup->mReset = true;
}

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

// Releases a pressed button when tabbing away
void EEUIFMGStatusButton::TabbedFrom()
{
    if (this->GetState() == kButtonStatePressed)
        this->HotKeyUp();
    else
        UIButton::TabbedFrom();
}

// Handles tabbing to the button
void EEUIFMGStatusButton::TabbedTo()
{
    UIButton::TabbedTo();
}

// Ends the cinematic at its saved end time (sends the end-cinematic message)
// (EEMEndCinematic takes the end time as a float)
long EEUIEndCinematicModeEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageEndCinematic, {gSavedGameSpeed}), kSlotNone);
    return kErrorOK;
}
