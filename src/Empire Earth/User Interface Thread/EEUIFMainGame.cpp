// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.cpp (__FILE__ 0x262e73). The in-game form: 0x26272b..0x26783d
// (the default player names' init chain first). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "lle/UITLScrolling.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIBitmap.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EmpireEarth.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIFMGStatusButton.h"
#include "lle/UIFormManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEStringIDs.h"
// the command buttons' part of the host (EEUIFMGStatusButton.cpp)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (Open passes mIsButtonBlinking)
void EEPostGraphicsAll();                   // 0x10fc0c (Open) (provisional name)
class EEMiniMap;
extern EEMiniMap* gMiniMap;                 // 0x564888 (ee/EEMiniMap.h; stored in EEGlobals.cpp)
extern RTSRenderObject* gU56534C;           // 0x56534c (the render object pushed when the form closes)
extern float& gMasterVolume;                 // 0x530830 (the master volume Open restores)

// the information panel's message is the scenario's dialog message (EEUserInterface's message 0x422 hands over an
// EESMDialogMessage; ShowInformation 0x26533e read it through the 32-bit offsets of a stand-in class, whose fields
// lined up there and not on x64: the portrait's class was read from the duration, the scroll time from the unit id)

// chain at 0x66272b (wrapper)
//   object 0x8372c0  ctor None  dtor __imp_??1UWideString@@UAE@XZ
//   inline ctor: mov esi, dword ptr [0x8372c0]
//   object bound as __imp_??0UWideString@@QAE@PBD@Z
UWideString gDefaultPlayerNames[32] = {
    "Player 0",
    "Player 1",
    "Player 2",
    "Player 3",
    "Player 4",
    "Player 5",
    "Player 6",
    "Player 7",
    "Player 8",
    "Player 9",
    "Player 10",
    "Player 11",
    "Player 12",
    "Player 13",
    "Player 14",
    "Player 15",
    "Player 16",
    "Player 17",
    "Player 18",
    "Player 19",
    "Player 20",
    "Player 21",
    "Player 22",
    "Player 23",
    "Player 24",
    "Player 25",
    "Player 26",
    "Player 27",
    "Player 28",
    "Player 29",
    "Player 30",
    "Player 31"
};

// The unit hack whole in Rust (ee_screens unit_hack.rs): the original's members 0x2628e5 (ctor: the sorted
// class names), 0x262a91 (dtor), 0x262aa5 PlaceUnit, 0x262c6f FillComboBox, 0x262cdc LoadUnitData, 0x262d66
// Update, 0x262e29 GetSelection, and the two message ctors' COMDATs 0x262c24 (EEMHackANewUnit) and 0x262d32
// (EEMLoadUnitData). The host answers its ops (EE_SCREENS_UH_OP_*): the class table, the object and player lists,
// the keys, the mouse, the messages.
extern "C" int32_t ee_screenscpp_unit_hack(void* inForm, uint32_t inOp, int32_t inA, int32_t inB, float* ioPoint,
                                           uint32_t* ioSelection, uint8_t* ioText, size_t inLength)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    // the object list only for its ops: the class table's are asked from the form's constructor, before its
    // controls exist
    UIComboBox* theObjects = inOp >= EE_SCREENS_UH_OP_COMBO_CLEAR && inOp <= EE_SCREENS_UH_OP_COMBO_SELECTION
                                 ? static_cast<UIComboBox*>(theForm->GetControl(kMainGameTempObjectList))
                                 : NULL;

    switch (inOp) {
    case EE_SCREENS_UH_OP_CLASS_COUNT:
        return (int32_t)EEUnitClassCount();
    case EE_SCREENS_UH_OP_CLASS: {
        EEUnitClass* theClass = gUnitClassTable[inA];
        if (theClass == NULL)
            return 0;
        return 1 | (theClass->mIsEnabled ? 2 : 0) | (theClass->mIsPlayerPlaceable ? 4 : 0);
    }
    case EE_SCREENS_UH_OP_CLASS_CATEGORY:
        return gUnitClassTable[inA]->mCategory;
    case EE_SCREENS_UH_OP_CLASS_TYPE:
        return gUnitClassTable[inA]->mUnitType;
    case EE_SCREENS_UH_OP_CLASS_NAME: {
        UWideString theName((const char*)gUnitClassTable[inA]->mName);
        size_t theLength = theName.GetTCHARLength();
        if (theLength <= inLength)
            memcpy(ioText, (const char*)theName, theLength);
        return (int32_t)theLength;
    }
    case EE_SCREENS_UH_OP_COMBO_CLEAR:
        theObjects->DeleteAllEntries();
        return 0;
    case EE_SCREENS_UH_OP_COMBO_APPEND: {
        char* theBytes = new char[inLength + 1];
        memcpy(theBytes, ioText, inLength);
        theBytes[inLength] = 0;
        theObjects->AppendLine(UWideString(theBytes));
        delete[] theBytes;
        return 0;
    }
    case EE_SCREENS_UH_OP_COMBO_SELECT_FIRST: {
        // port: the first line is selected without the box's change event. The original's event sent EEMLoadUnitData
        // from the UI thread as the main game form opened, racing the game's start: in the loading's last communications
        // ticks the client ran it as lobby input, a tick later the physics ran it as a game input (one more input
        // counted: the out-of-synch checksum, EE-SIM's AI trace "inputs=" and its capture's NMEX/NMIM/NMDC/USCK
        // differed by run under load). Its only effect was the sender's hack class and that class's media preloaded
        // for every player; the hack's own placing sends the listed class, and a later change still sends the message.
        UIEvent* theEvent = theObjects->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theEvent != NULL) {
            theEvent->Reference();
            theObjects->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        }
        theObjects->SetCurrentSelection(0);
        if (theEvent != NULL) {
            theObjects->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
            theEvent->Dereference();
        }
        return 0;
    }
    case EE_SCREENS_UH_OP_COMBO_SELECTION: {
        unsigned long theSelection;
        theObjects->GetCurrentSelection(theSelection);
        return (int32_t)theSelection;
    }
    case EE_SCREENS_UH_OP_LIST_SELECTION: {
        unsigned long theSelection = *ioSelection;
        long theResult = static_cast<UIComboBox*>(theForm->GetControl(inA == 1 ? kMainGameTempObjectList
                                                                                 : kMainGameTempPlayerList))
                             ->GetListBox()->GetCurrentSelection(theSelection);
        *ioSelection = (uint32_t)theSelection;
        return (int32_t)theResult;
    }
    case EE_SCREENS_UH_OP_TILE:
        return gWorld.mMap->GetTile((long)ioPoint[0], (long)ioPoint[1]) != NULL;
    case EE_SCREENS_UH_OP_KEY: {
        ISEntity* theKey;
        gISKeyboard->GetEntity(inA == 0 ? kKeyShift : kKeyControl, theKey);
        return (int32_t)theKey->GetValue();
    }
    case EE_SCREENS_UH_OP_PLAYER_SKIPPED:
        return gWorld.mPlayers[inA] == NULL || gWorld.mPlayers[inA] == gWorld.GetCurrentPlayer();
    case EE_SCREENS_UH_OP_LOCAL_PLAYER:
        return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID;
    case EE_SCREENS_UH_OP_SEND_PLACE: {
        long theType = inA;
        gCommunications->ProcessLocalUserInput(
            new EEMRecord(kMessageHackANewUnit, {theType, GE2DPoint(ioPoint[0], ioPoint[1]), (unsigned char)inB}), kSlotNone);
        return 0;
    }
    case EE_SCREENS_UH_OP_SEND_LOAD:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadUnitData, {inA, true}), kSlotNone);
        return 0;
    case EE_SCREENS_UH_OP_IN_VIEW:
        return gViewport.GetViewRectangle().Contains(GE2DPoint(ioPoint[0], ioPoint[1]));
    case EE_SCREENS_UH_OP_LEFT_BUTTON:
        return inA == 0 ? UIManager::GetLMouseButton()->JustPressed() : UIManager::GetLMouseButton()->JustUnpressed();
    case EE_SCREENS_UH_OP_POINT: {
        GE3DPoint thePoint;
        EETile* theLocation;
        bool theFound = EEPointUnitOrTile(GE2DPoint(ioPoint[0], ioPoint[1]), theLocation, thePoint, true);
        ioPoint[0] = thePoint.GetX();
        ioPoint[1] = thePoint.GetY();
        return theFound;
    }
    }
    return 0;
}

EEUIFMGUnitHack::EEUIFMGUnitHack(EEUIFMainGame* inForm)
{
    this->mForm = inForm;
    this->mRecord = ee_screens_unit_hack_new(inForm);
}

EEUIFMGUnitHack::~EEUIFMGUnitHack()
{
    ee_screens_unit_hack_free(this->mRecord);
}

void EEUIFMGUnitHack::PlaceUnit(const GE2DPoint& inTile)
{
    ee_screens_unit_hack_call(this->mRecord, this->mForm, EE_SCREENS_UH_PLACE_UNIT, inTile.GetX(), inTile.GetY());
}

void EEUIFMGUnitHack::FillComboBox()
{
    ee_screens_unit_hack_call(this->mRecord, this->mForm, EE_SCREENS_UH_FILL_COMBO_BOX, 0.0f, 0.0f);
}

void EEUIFMGUnitHack::LoadUnitData()
{
    ee_screens_unit_hack_call(this->mRecord, this->mForm, EE_SCREENS_UH_LOAD_UNIT_DATA, 0.0f, 0.0f);
}

void EEUIFMGUnitHack::Update(const GE2DPoint& inLocation)
{
    ee_screens_unit_hack_call(this->mRecord, this->mForm, EE_SCREENS_UH_UPDATE, inLocation.GetX(), inLocation.GetY());
}

// (EEUIFMGFormCall, one call of the form's work in Rust: ee/EEUIFMainGame.h)
static_assert(offsetof(EEUIFMGFormCall, mForm) == 0, "main_form_ffi.rs reads the form as the record's first word");
static_assert(offsetof(EEUIFMGFormCall, mText) == 2 * sizeof(void*) &&
                  offsetof(EEUIFMGFormCall, mName) == 3 * sizeof(void*) + sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mChat) == 3 * sizeof(void*) + 2 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mButton) == 15 * sizeof(void*) + 3 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mOut) == 16 * sizeof(void*) + 3 * sizeof(UWideString),
              "hud_direct.rs FormCallHead: the held text, the chat line's name and text, the button, the help");


// Runs the keyboard mode of the current interface state (the cinematic's keys), else the form's keys
long EEUIFMainGame::UpdateKeyboard()
{
    return (long)this->Rust(EE_SCREENS_MF_DO_KEYBOARD, this->mKeyboardModes[this->mUIState]);
}

// Creates the main game form: its panel groups, mouse actions (scroll, zoom, click, lasso, build, calamity),
// the mouse and keyboard tests of every interface mode, the unit hack, the hot keys (ee_screens main_form.rs
// construct; it reads no control: they are made after it)
EEUIFMainGame::EEUIFMainGame(UIManager* inManager, const UIDbDataForms& inData)
    : UIForm(inManager, inData, true), EeScreensMainFormFields()
{
    ee_screens_main_form_fields(this, static_cast<EeScreensMainFormFields*>(this));
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_construct(&theCall);
}

// Destroys the main game form, its actions, panel groups, poll and unit hack
EEUIFMainGame::~EEUIFMainGame()
{
    unsigned long i;

    this->mGrabScroll->Dereference();
    EEReleaseGrabScroll(0);
    this->mWheelZoom->Dereference();
    this->mScroll->Dereference();
    this->mMouseClick->Dereference();
    this->mLasso->Dereference();
    TSScheduler::DestroyCriticalSection(this->mErrorCriticalSection);
    for (i = 0; i < 6; i++)
        delete this->mGroups[i];
    delete this->mPoll;
    this->mBuild->Dereference();
    this->mCalamity->Dereference();
    delete this->mUnitHack;
}

// Activates the form: frees the mouse, resumes a game paused only by leaving it, refreshes the saved groups
void EEUIFMainGame::Activate()
{
    this->Rust(EE_SCREENS_MF_DO_ACTIVATE);
}


// Shows a chat message in the next free chat line (scrolling the lines up when full), wrapping the rest of a
// long message onto further lines (ee_screens main_form.rs)
void EEUIFMainGame::DisplayChatLine(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                    unsigned long inFlags)
{
    EEUIFMGFormCall theCall(this);
    theCall.mColor = &inColor;
    theCall.mName = inName;
    theCall.mChat = inText;
    theCall.mFlags = inFlags;
    ee_screens_main_form_chat_line(&theCall);
}

// (BuildGate and BuildWall: the placements call the build action, ee/EEARust.h)


// Closes the form: hides the messages, gives the graphics thread the UI render object back and unloads the
// background media
void EEUIFMainGame::Close()
{
    this->Rust(EE_SCREENS_MF_DO_CLOSE);
}

// Deactivates the form: frees and resets the mouse and leaves any interface mode
void EEUIFMainGame::Deactivate()
{
    this->Rust(EE_SCREENS_MF_DO_DEACTIVATE);
}

// Snapshots the form for drawing: selection state, error log, status refresh each poll, the panel group, edge
// scrolling and the gate or wall preview of the current mode (ee_screens main_form.rs)
long EEUIFMainGame::CopyData()
{
    EEUIFMGFormCall theCall(this);
    return (long)ee_screens_main_form_copy_data(&theCall);
}

// Creates one of the six panel groups (main, statistics, chat, diplomacy, options, civilization builder)
void EEUIFMainGame::CreateGroup(unsigned long inIndex)
{
    switch (inIndex) {
    case 0:
        this->mGroups[0] = new EEUIFMGMainGroup(this);
        break;
    case EE_SCREENS_GROUP_STATISTICS:
    case EE_SCREENS_GROUP_CHAT:
    case EE_SCREENS_GROUP_DIPLOMACY:
    case EE_SCREENS_GROUP_OPTIONS:
    case EE_SCREENS_GROUP_CIV_BUILDER:
        this->mGroups[inIndex] = new EEUIFMGGRust(this, inIndex);
        break;
    }
}

// Shows a chat message when the form has its chat lines
void EEUIFMainGame::DisplayChat(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                unsigned long inFlags, unsigned long inArg5)
{
    if (this->GetControl(kMainGameChatLine7) != NULL)
        this->DisplayChatLine(inColor, inName, inText, inFlags);
}

// Shows an information message: scrolling text, the speaker's portrait and its sound; hides the messages
// without a text control (ee_screens main_form.rs)
void EEUIFMainGame::ShowInformation(EEUIFMGInformation* inInformation)
{
    EEUIFMGFormCall theCall(this);
    theCall.mMessage = reinterpret_cast<const EESMDialogMessage*>(inInformation);
    ee_screens_main_form_show_information(&theCall);
}

// Draws the form's fixed background panels (the unit panels only with a selection)
long EEUIFMainGame::DrawPolygons()
{
    return (long)this->Rust(EE_SCREENS_MF_DO_DRAW);
}

// Switches the bottom panel to another entity group and selects its panel group (ee_screens main_form.rs)
void EEUIFMainGame::SetEntityGroup(long inGroup)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_entity_group(&theCall, inGroup);
}



// Tells whether a screen point is in the game view and not under the unit information panel
bool EEUIFMainGame::IsInGameView(const GE2DPoint& inLocation)
{
    bool theOutside = true;

    if (this->mUnitInfoBackground != NULL && this->mUnitInfoBackground->GetIsVisible())
        theOutside = !this->mUnitInfoBackground->GetRectangle().Contains(inLocation);
    if (gViewport.GetViewRectangle().Contains(inLocation) && theOutside)
        return true;
    return false;
}

// Stores an error log line for the next snapshot
// Opens the form: sound latency and volume, centred mouse, empty error log, the cancel-mode event, the
// backgrounds and the civilization builder button's blinking
void EEUIFMainGame::Open()
{
    this->Rust(EE_SCREENS_MF_DO_OPEN);
}



// (BuildUnit and the start modes StartGate, StartWall, StartCalamity, StartFlare, StartPatrol: inline in
// ee/EEUIFMainGame.h)

// Switches the interface mode, clearing ghosts and ending a wall or gate placement



#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EECombatGoals.h"
#include "rust/sim/units.h"

static_assert(EE_SCREENS_MF_CTL_WIN_LOSE == kMainGameWinLose && EE_SCREENS_MF_CTL_CANCEL_UI_MODE == kMainGameCancelUIMode &&
                  EE_SCREENS_MF_SOUND_BUTTON1 == kSoundButton1 && EE_SCREENS_MF_SOUND_BUTTON2 == kSoundButton2 && EE_SCREENS_MF_CTL_POPULATION == kMainGamePopulation &&
                  EE_SCREENS_MF_CTL_MINI_MAP == kMainGameMiniMap && EE_SCREENS_MF_CTL_CHAT_LINE1 == kMainGameChatLine1 &&
                  EE_SCREENS_MF_CTL_CHAT_LINE7 == kMainGameChatLine7 && EE_SCREENS_MF_CTL_GAME_CLOCK == kMainGameGameClock &&
                  EE_SCREENS_MF_CTL_BRIEFING == kMainGameGameInfoBriefing &&
                  EE_SCREENS_MF_CTL_DIALOG_PORTRAIT == kMainGameDialogPortrait &&
                  EE_SCREENS_MF_CTL_BRIEFING_ALERT == kMainGameLargeScenBriefingAlert &&
                  EE_SCREENS_MF_CTL_PREVIOUS_MESSAGES == kMainGamePreviousMessages &&
                  EE_SCREENS_MF_IDS_FPS == IDS_FPS && EE_SCREENS_MF_IDS_GAME_PAUSED == IDS_GAME_PAUSED,
              "main_form.rs' controls and strings");

// The Rust's calls on the form, its controls and the world (EE_SCREENS_MF_OP_*), each the original's call
extern "C" int64_t ee_screenscpp_main_form(void* inCall, uint32_t inOp, int64_t inA, int64_t inB,
                                           EeScreensMainFormIo* ioIo)
{
    return EEUIFMainGame::HostOp(inCall, inOp, inA, inB, ioIo);
}

// ---- the mouse modes' ops (ee_screens mouse_modes.rs, EE_SCREENS_MF_OP_MD_*): the picked unit, the first selected
// unit and the pointed tile held in the call; io.f[0..2] the mouse location where an op takes one
static_assert(EE_SCREENS_MF_TYPE_FARM == kUnitTypeFarm && EE_SCREENS_MF_TYPE_ROBOTIC_FARM == kUnitTypeRoboticFarm &&
                  EE_SCREENS_MF_KEY_ESCAPE == kKeyEscape && EE_SCREENS_MF_KEY_F1 == kKeyF1 && EE_SCREENS_MF_KEY_F3 == kKeyF3 &&
                  EE_SCREENS_MF_KEY_F9 == kKeyF9,
              "mouse_modes.rs' farms and keys");
static int64_t EEMouseModeOp(EEUIFMGFormCall& theCall, uint32_t inOp, int64_t inA, int64_t inB, EeScreensMainFormIo* ioIo)
{
    EEUIFMainGame* theForm = theCall.mForm;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    GE2DPoint theLocation(ioIo->f[0], ioIo->f[1]);
    EEUnit* thePicked = theCall.mPicked;
    EEComplexUnit* theFirst = theCall.mFirst;
    EEComplexUnit* theComplex = thePicked != NULL ? thePicked->mComplexUnit : NULL;
    switch (inOp) {
    case EE_SCREENS_MF_OP_MD_CAPTURED: return theForm->GetIsMouseCaptured();
    case EE_SCREENS_MF_OP_MD_POINTER: gMouse->mPointer = (long)inA; return 0;
    case EE_SCREENS_MF_OP_MD_POINT: {
        if (inA == 0) {
            GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
            bool theFound = EEPointUnitOrTile(theLocation, theCall.mTile, thePoint, true);
            ioIo->f[0] = thePoint.GetX(), ioIo->f[1] = thePoint.GetY(), ioIo->f[2] = thePoint.GetZ();
            return theFound;
        }
        long theX = -10000, theY = -10000;
        bool theFound = EEPointUnitOrTileXY(theLocation, theCall.mTile, theX, theY, true);
        ioIo->n[0] = theX, ioIo->n[1] = theY;
        return theFound;
    }
    case EE_SCREENS_MF_OP_MD_TILE_SEEN: return theCall.mTile->IsSeenBy(thePlayer);
    case EE_SCREENS_MF_OP_MD_PICK: return (theCall.mPicked = gUnitPicker.PickUnit(theLocation, 0)) != NULL;
    case EE_SCREENS_MF_OP_MD_UNIT:   // (its plain fields, and the first unit's, are hud_views.rs mouse_mode's)
        switch (inA) {
        case EE_SCREENS_MF_UNIT_CAN_BE_ATTACKED: return thePicked->GetUnitClass()->CanBeAttacked();
        case EE_SCREENS_MF_UNIT_IN_WONDER_SET:
            return gUnitSetManager.Contains(theComplex->GetUnitClass()->GetUnitType(), theFirst->mStats->mWonderUnitSet);
        }
        return 0;
    case EE_SCREENS_MF_OP_MD_DAMAGE: ioIo->f[0] = EEDamageFactor((long)inA, (long)inB); return 0;
    case EE_SCREENS_MF_OP_MD_FIRST_FIELD:
        switch (inA) {
        case EE_SCREENS_MF_FIRST_IS_CASTER: return theFirst->mUnitID == theForm->mCalamity->mCasterID;
        }
        return 0;
    case EE_SCREENS_MF_OP_MD_CAN_CONVERT: {
        bool theInRange = inB != 0;
        bool theFound = EECanConvert(thePlayer->mSelectedGroup->GetUnit((unsigned long)inA), thePicked->GetComplexUnit(), theInRange);
        return (theFound ? 1 : 0) | (theInRange ? 2 : 0);
    }
    case EE_SCREENS_MF_OP_MD_SEND: {
        NEMessage* theMessage;
        switch (inA) {
        case EE_SCREENS_MF_SEND_LOAD_GARRISON:
            theMessage = new EEMRecord(kMessageLoadGarrison, {thePicked->GetUnitID(), theFirst->mComplexUnit->mUnitID, 0});
            break;
        case EE_SCREENS_MF_SEND_UNLOAD:
            theMessage = new EEMRecord(kMessageUnloadTransport,
                                       {theFirst->GetComplexUnit()->GetUnitID(), (long)ioIo->n[0], (long)ioIo->n[1]});
            break;
        case EE_SCREENS_MF_SEND_HEAL: theMessage = new EEMRecord(kMessageHealUnit, {theComplex->mUnitID}); break;
        case EE_SCREENS_MF_SEND_CONVERT: theMessage = new EEMRecord(kMessageAttackUnit, {theComplex->mUnitID, inB != 0, true}); break;
        default:   // EE_SCREENS_MF_SEND_RALLY (the airport's, the attack ground's and the flare's are hud_sends.rs')
            theMessage = new EEMRecord(kMessageSetRallyGoal, {GE2DPoint(ioIo->f[0], ioIo->f[1]), (long)ioIo->n[0],
                                                              theFirst->mComplexUnit->GetUnitID(), (unsigned char)ioIo->n[1]});
            break;
        }
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        return 0;
    }
    case EE_SCREENS_MF_OP_MD_ORDER_AT:
        EEOrderAtCursor(theLocation, inA == 0 ? NULL : inA == 1 ? thePicked : static_cast<EEUnit*>(theComplex));
        return 0;
    case EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT:
        EEMakeCalamityEffect(GE3DPoint(ioIo->f[0], ioIo->f[1], ioIo->f[2]), thePicked);
        return 0;
    case EE_SCREENS_MF_OP_MD_PREVIEW: EEPreviewPlacement(theForm->GetMouseLocation()); return 0;
    case EE_SCREENS_MF_OP_MD_CALAMITY_CHECK: return theForm->mCalamity->Check(theLocation);
    case EE_SCREENS_MF_OP_MD_IN_GAME_VIEW: return theForm->IsInGameView(theLocation);
    case EE_SCREENS_MF_OP_MD_ENTITY_ENABLED: return theForm->GetEntityGroup((unsigned long)inA)->GetIsEnabled();
    case EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP: theForm->SetEntityGroup((long)inA); return 0;
    case EE_SCREENS_MF_OP_MD_GATE_CLEAR: theCall.mGate->ClearGhostUnits(); return 0;
    }
    return 0;
}
// ---- (the mouse modes' ops end)

// (a member: UIForm's GetMouseCaptured and CopyData are protected). The plain world reads and the controls' display
// modes and dirty flags are hud_views.rs' (the world's views, the toolkit's direct path)
int64_t EEUIFMainGame::HostOp(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, EeScreensMainFormIo* ioIo)
{
    EEUIFMGFormCall& theCall = *static_cast<EEUIFMGFormCall*>(inCall);
    EEUIFMainGame* theForm = theCall.mForm;
    UITextLabel* theLabel = NULL;
    if (inOp == EE_SCREENS_MF_OP_LABEL_PLAYER_COLOR)
        theLabel = static_cast<UITextLabel*>(theForm->GetControl((unsigned long)inA));
    switch (inOp) {
    case EE_SCREENS_MF_OP_FIELD:   // (the fields of EeScreensMainFormFields are main_form_fields.rs')
    case EE_SCREENS_MF_OP_SET_FIELD: {
        bool theSet = inOp == EE_SCREENS_MF_OP_SET_FIELD;
#define EE_MF_FIELD(id, member) \
    case id: \
        if (theSet) \
            member = (decltype(member))inB; \
        return (int64_t)member;
        switch (inA) {
            EE_MF_FIELD(EE_SCREENS_MF_FIELD_GATE_DIRECTION, theForm->mBuildGate.mDirection)
            EE_MF_FIELD(EE_SCREENS_MF_FIELD_WALL_STATE, theForm->mBuildWall.mState)
            case EE_SCREENS_MF_FIELD_TOOLTIP_SHADOW:
                if (theSet)
                    theForm->SetDrawToolTipShadow(inB != 0);
                return (int64_t)theForm->GetDrawToolTipShadow();
            EE_MF_FIELD(EE_SCREENS_MF_FIELD_BUILD_TECH, theForm->mBuild->mTechnology)
        }
#undef EE_MF_FIELD
        return 0;
    }
    case EE_SCREENS_MF_OP_LOG_LINE:
        static_cast<EEUIShellList*>(theForm->GetControl(kMainGameErrorLog))->SetLine((unsigned long)inA,
                                                                                  theForm->mStrings[inA]);
        return 0;
    case EE_SCREENS_MF_OP_POLL: return theForm->mPoll->Poll();
    case EE_SCREENS_MF_OP_SCROLL: theForm->mScroll->Update(gMouse->mPosition); return 0;
    case EE_SCREENS_MF_OP_FIND_GATE_WALL: {
        EETileCoordinate theTile;
        theTile.mX = (long)inA, theTile.mY = (long)inB;
        long theDirection = (long)ioIo->n[0];
        bool theFound = EEFindGateWall(theTile, theDirection);
        ioIo->n[0] = theDirection;
        return theFound;
    }
    case EE_SCREENS_MF_OP_CLEAR_GHOSTS: gWorld.ClearGhostUnits(); return 0;
    case EE_SCREENS_MF_OP_PREVIEW_WALL: {
        const int32_t* theXY = reinterpret_cast<const int32_t*>(inB);
        std::vector<EETileCoordinate> theTiles;
        for (int64_t i = 0; i < inA; i++) {
            EETileCoordinate theTile;
            theTile.mX = theXY[2 * i], theTile.mY = theXY[2 * i + 1];
            theTiles.push_back(theTile);
        }
        EEUIBuildWall* theWall = theCall.mWall != NULL ? theCall.mWall : &theForm->mBuildWall;
        EEPreviewWall(theTiles, theWall->mTechnology, theWall->mPlayer);
        return 0;
    }
    case EE_SCREENS_MF_OP_PREVIEW_PLACEMENT: EEPreviewPlacement(gMouse->mPosition); return 0;
    case EE_SCREENS_MF_OP_POPULATION: return gWorld.GetCurrentPlayer()->GetTotalPopulation(false, false);
    case EE_SCREENS_MF_OP_POPULATION_LIMIT: return gWorld.GetCurrentPlayer()->GetPopulationLimit(false);
    case EE_SCREENS_MF_OP_NUM_TEAMS: return gWorld.GetNumTeams();
    case EE_SCREENS_MF_OP_LABEL_PLAYER_COLOR:
        theLabel->GetGEText()->SetFgColor(reinterpret_cast<EEPlayer*>(inB)->mColor);
        return 0;
    case EE_SCREENS_MF_OP_TEXT_WONDER: {
        UWideString theFormat;
        gResource->GetString(IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY, theFormat);
        sprintf(theCall.mText, theFormat, (const char*)reinterpret_cast<EEPlayer*>(inA)->mName, (unsigned long)inB);
        return 0;
    }
    case EE_SCREENS_MF_OP_TEXT_TIME: theCall.mText = gPhysics->GetTimeString(); return 0;
    case EE_SCREENS_MF_OP_HAS_COMMS: return gCommunications != NULL;
    case EE_SCREENS_MF_OP_COMMS_PAUSED: return gCommunications->GetIsGamePaused();
    case EE_SCREENS_MF_OP_HAS_TEAMS: return gWorld.mTeams.size() > 0;
    case EE_SCREENS_MF_OP_PAUSE_TEXT: theForm->Rust(EE_SCREENS_MF_DO_PAUSE_TEXT, inA); return 0;
    case EE_SCREENS_MF_OP_LAG_STATUS: return gWorld.mLagStatus;
    case EE_SCREENS_MF_OP_LAG_COLOR:
        static_cast<UIBitmap*>(theForm->GetControl((unsigned long)inA))->GetBitmap()->SetColor(gWorld.mPlayers[inB]->mColor);
        return 0;
    case EE_SCREENS_MF_OP_HAS_INSTRUCTIONS: return gWorld.mScenarioManager->mMediaStrings.mInstructions.GetLength() > 0;
    case EE_SCREENS_MF_OP_UPDATE_CURSOR: theForm->UpdateCursorKind(); return 0;
    case EE_SCREENS_MF_OP_INFO_DURATION: return theCall.mMessage->mDuration;
    case EE_SCREENS_MF_OP_CHAT_TOP: theForm->Rust(EE_SCREENS_MF_DO_CHAT_TOP, inA); return 0;
    case EE_SCREENS_MF_OP_HIDE_MESSAGES: theForm->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); return 0;
    case EE_SCREENS_MF_OP_CREATE_ERROR_LOCK:
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theForm->mErrorCriticalSection) == keseOK, 633);
        return 0;
    case EE_SCREENS_MF_OP_CREATE_GROUP: theForm->CreateGroup((unsigned long)inA); return 0;
    case EE_SCREENS_MF_OP_NEW_UNIT_HACK: theForm->mUnitHack = new EEUIFMGUnitHack(theForm); return 0;
    case EE_SCREENS_MF_OP_BUILD_TERMINATE:
        inA == 0 ? ((EEUIBuildWall&)theForm->mBuildWall).Terminate() : ((EEUIBuildGate&)theForm->mBuildGate).Terminate();
        return 0;
    case EE_SCREENS_MF_OP_NODE:
        theCall.mNode = gWorld.GetCurrentPlayer()->mTechnologies->Get((long)inA);
        return theCall.mNode != NULL;
    case EE_SCREENS_MF_OP_NODE_CLASS:
        return theCall.mNode != NULL && gUnitClassTable[theCall.mNode->mData->mUnitClass] != NULL;
    case EE_SCREENS_MF_OP_CAN_PRODUCE:
        return gWorld.GetCurrentPlayer()->CanProduce((long)inA, theCall.mUnit->mComplexUnit->mUnitClass->GetUnitType(), 1,
                                                     true, 3, &theCall.mText);
    case EE_SCREENS_MF_OP_PLAY_SOUND:
        gSoundManager->Play(EESoundObject::Get((unsigned long)inA)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_SCREENS_MF_OP_CHAT_LOG_ERROR: gUserInterface->AddChatLogEntry(theCall.mText, 0); return 0;
    case EE_SCREENS_MF_OP_SELECTED_COUNT: return gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits();
    case EE_SCREENS_MF_OP_NODE_CALAMITY: return theCall.mNode->mData->mCalamity;
    case EE_SCREENS_MF_OP_CALAMITY_CLASS:
        theCall.mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inA);
        return theCall.mCalamity != NULL;
    case EE_SCREENS_MF_OP_CALAMITY_CAST: theForm->mCalamity->Cast(theForm->GetMouseLocation()); return 0;
    case EE_SCREENS_MF_OP_MOUSE_FREEZE: gMouse->mIsFrozen = inA != 0; return 0;
    case EE_SCREENS_MF_OP_USER_PAUSED: return gWorld.mUserPaused;
    case EE_SCREENS_MF_OP_IS_SERVER: return gCommunications->IsServer() == true;
    case EE_SCREENS_MF_OP_MASTER_VOLUME: gSoundManager->SetMasterVolume(gMasterVolume); return 0;
    case EE_SCREENS_MF_OP_MOUSE_POSITION: gMouse->SetPosition(GE2DPoint(ioIo->f[0], ioIo->f[1])); return 0;
    case EE_SCREENS_MF_OP_FILL_HACK: theForm->mUnitHack->FillComboBox(); return 0;
    case EE_SCREENS_MF_OP_HAS_TECHS: return gWorld.GetCurrentPlayer()->mTechnologies != NULL;
    case EE_SCREENS_MF_OP_TEXT_EPOCH_TITLE:
        gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), theCall.mText);
        return 0;
    case EE_SCREENS_MF_OP_UNIT_INFO_BACKGROUND: theForm->mUnitInfoBackground = theForm->GetBackgroundByID(0x64); return 0;
    case EE_SCREENS_MF_OP_MEDIA_OLD: return (theCall.mOldMedia = EEMedia::Get(theForm->mMediaID)) != NULL;
    case EE_SCREENS_MF_OP_PLAYER_MEDIA_ID: return gWorld.GetCurrentPlayer()->mTechnologies->mMediaID;
    case EE_SCREENS_MF_OP_USING_CARBON: return gWorld.mUsingCarbon;
    case EE_SCREENS_MF_OP_MEDIA: return (theCall.mMedia = EEMedia::Get((long)inA)) != NULL;
    case EE_SCREENS_MF_OP_BG_TEXTURE:
        static_cast<UIBitmapBackground*>(theForm->GetBackgroundByID((unsigned long)inA))
            ->SetBitmapTexture(static_cast<EEGraphicObject*>(theCall.mMedia)->mTextures[inB]);
        return 0;
    case EE_SCREENS_MF_OP_POST_SHOW_DIALOG: gPhysics->PostShowDialogMessage(); return 0;
    default:
        return EEMouseModeOp(theCall, inOp, inA, inB, ioIo);
    }
    return 0;
}

// The cursor's reads for one UpdateCursorKind (ee_screens cursor.rs asks them through ee_screenscpp_cursor)
struct EEUIFMGCursorRead {
    EEUIFMainGame* mForm;
    EEPlayer* mPlayer;
    EESelectedGroup* mGroup;
    EEComplexUnit* mFirst;
    EEUnit* mTarget;
};
static_assert(sizeof(EEUIFMGCursorRead) == 5 * sizeof(void*), "hud_direct.rs CursorRead");

// The world's answers to the cursor's ops (EE_SCREENS_CURSOR_OP_*): the target, the selection by index, an air
// base's cargo by index; the positions' floats into outF
extern "C" int32_t ee_screenscpp_cursor(void* inContext, uint32_t inOp, uint32_t inA, uint32_t inB, float* outF)
{
    EEUIFMGCursorRead& theRead = *static_cast<EEUIFMGCursorRead*>(inContext);
    EEUnit* theTarget = theRead.mTarget;
    EEComplexUnit* theUnit = inOp >= EE_SCREENS_CURSOR_OP_SEL_KIND && inOp <= EE_SCREENS_CURSOR_OP_SEL_CONVERT ? theRead.mGroup->GetUnit(inA) : NULL;
    EEComplexUnit* theCargo = inOp >= EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK ? theRead.mGroup->GetUnit(inA)->GetComplexUnit()->mCargo[inB] : NULL;

    switch (inOp) {
    case EE_SCREENS_CURSOR_OP_MOUSE_OK:
        return theRead.mForm->GetMouseOverControl() == NULL ||
               theRead.mForm->GetMouseOverControl() == theRead.mForm->GetControl(kMainGameCommandUnits);
    case EE_SCREENS_CURSOR_OP_FLAGS:
        return (int32_t)theRead.mGroup->mKindFlags;
    case EE_SCREENS_CURSOR_OP_IN_VIEW:
        return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(gMouse->mPosition);
    case EE_SCREENS_CURSOR_OP_PICK:
        theRead.mTarget = gUnitPicker.PickUnit(gMouse->mPosition, inA);
        return theRead.mTarget != NULL;
    case EE_SCREENS_CURSOR_OP_RELEASE:
        theTarget->Dereference();
        return 0;
    case EE_SCREENS_CURSOR_OP_SEEN:
        return EECombatIsSeenBy(theTarget, theRead.mPlayer, true);
    case EE_SCREENS_CURSOR_OP_T_DISABLED:
        return theTarget->mTileNode != NULL && theTarget->mComplexUnit->mIsDisabled &&
               !theTarget->mComplexUnit->IsProtected();
    case EE_SCREENS_CURSOR_OP_T_TARGETABLE:
        return ee_units_uu_targetable(0, (uint64_t)(uintptr_t)theTarget->mUnitClass, 0);
    case EE_SCREENS_CURSOR_OP_AIM: {
        // EEGetAttackRangeKind's aim: the model's default point scaled and placed, else the tile's centre
        GE3DPoint thePoint, theAim;
        if (theTarget->GetComplexUnit() != NULL && theTarget->GetModel()->mModel != NULL) {
            float theScale = theTarget->mStats->GetScale();
            thePoint = theTarget->GetModel()->mDefaultPoint;
            thePoint.SetX(thePoint.GetX() * theScale);
            thePoint.SetY(thePoint.GetY() * theScale);
            thePoint.SetZ(thePoint.GetZ() * theScale);
            theTarget->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(thePoint, theAim);
        } else {
            theAim.SetX((float)theTarget->GetTileX() + 0.5f);
            theAim.SetY((float)theTarget->GetTileY() + 0.5f);
            theAim.SetZ(theTarget->mTile->GetHeight());
        }
        outF[0] = theAim.GetX();
        outF[1] = theAim.GetY();
        outF[2] = theAim.GetZ();
        return 0;
    }
    case EE_SCREENS_CURSOR_OP_CAN_GATHER:
        return theRead.mGroup->FindUnitWithFlags(inA)->CanGather(theTarget, theTarget->GetTileX(),
                                                                  theTarget->GetTileY(), false, false);
    case EE_SCREENS_CURSOR_OP_CAN_HEAL:
        return EECanHeal(theRead.mFirst, theTarget->mComplexUnit);
    case EE_SCREENS_CURSOR_OP_SEL_AIR_BASE:
        return theUnit->GetUnitClass()->IsAirBase();
    case EE_SCREENS_CURSOR_OP_SEL_CRUSADER:
        return theUnit->mUnitClass->IsCrusader();
    case EE_SCREENS_CURSOR_OP_SEL_PRIEST:
        return theUnit->mUnitClass->IsPriest();
    case EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK:
        return theUnit->GetComplexUnit()->CanAttackClass(theTarget->GetUnitClass());
    case EE_SCREENS_CURSOR_OP_SEL_CONVERT: {
        bool theShielded = inB != 0;
        bool theResult = EECanConvert(theUnit, theTarget->mComplexUnit, theShielded);
        return (theResult ? 1 : 0) | (theShielded ? 2 : 0);
    }
    case EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK:
        return theCargo->CanAttackClass(theTarget->GetUnitClass());
    }
    return 0;
}

// Picks the mouse cursor for the unit under it from what the selection can do to it (ee_screens cursor.rs, with
// GetAttackStance, GetTransportCommand, GetRepairCommand, GetCommandOnUnit, Fn265D7A and EEGetAttackRangeKind)
void EEUIFMainGame::UpdateCursorKind()
{
    EEUIFMGCursorRead theRead;
    theRead.mForm = this;
    theRead.mPlayer = gWorld.GetCurrentPlayer();
    theRead.mGroup = theRead.mPlayer->mSelectedGroup;
    theRead.mFirst = theRead.mGroup->GetFirstUnit();
    theRead.mTarget = NULL;
    gMouse->mPointer = ee_screens_cursor_kind(&theRead);
}



// the wall's and the gate's members from mState / mUnitClass on as mouse_modes.rs' records
static_assert(offsetof(EEUIBuildWall, mPlayer) - offsetof(EEUIBuildWall, mState) == offsetof(EeScreensBuildWallFields, player) &&
                  offsetof(EEUIBuildWall, mEnd) - offsetof(EEUIBuildWall, mState) == offsetof(EeScreensBuildWallFields, end) &&
                  offsetof(EEUIBuildGate, mPlayer) - offsetof(EEUIBuildGate, mUnitClass) == offsetof(EeScreensBuildGateFields, player) &&
                  offsetof(EEUIBuildGate, mTileX) - offsetof(EEUIBuildGate, mUnitClass) == offsetof(EeScreensBuildGateFields, tile_x),
              "the placements' records");

// Tracks the mouse in wall placement (ee_screens mouse_modes.rs wall_update): a left click sets the start, then
// builds each segment and chains from its end; a right click cancels; moving previews the wall ghosts
void EEUIBuildWall::Update(const GE2DPoint& inLocation)
{
    EEUIFMGFormCall theCall(gUIManager->GetMainGameForm());
    theCall.mWall = this;
    ee_screens_wall_update(&theCall, reinterpret_cast<EeScreensBuildWallFields*>(&this->mState), inLocation.GetX(),
                           inLocation.GetY());
}

// Tracks the mouse in gate placement (ee_screens mouse_modes.rs gate_update): a gate ghost on the wall under the
// mouse, built on a left click, cancelled on a right click
void EEUIBuildGate::Update(const GE2DPoint& inLocation)
{
    EEUIFMGFormCall theCall(gUIManager->GetMainGameForm());
    theCall.mGate = this;
    ee_screens_gate_update(&theCall, reinterpret_cast<EeScreensBuildGateFields*>(&this->mUnitClass), inLocation.GetX(),
                           inLocation.GetY());
}

// Runs the mouse: the unit hack, the form's controls, then the current interface mode's mouse (ee_screens
// mouse_modes.rs update_mouse); returns the last UpdateMouse result (the modes answer 0)
long EEUIFMainGame::UpdateMouse()
{
    return (long)this->Rust(EE_SCREENS_MF_DO_MOUSE);
}


// ---- the shims of the moved cases (UI lane round 7: ee_game screens main_form_calls.rs, game_cpp.rs), each one
// C++ statement on the host's call record or the world
#include "ee/EEScreensShims.h"
static EEUIFMGFormCall* R(uintptr_t inRec) { return reinterpret_cast<EEUIFMGFormCall*>(inRec); }
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// the keyboard's key (NULL when the keyboard has none)
static ISEntity* EEKey(unsigned long inKey)
{
    ISEntity* theKey = NULL;
    return gISKeyboard->GetEntity(inKey, theKey) != 0 ? NULL : theKey;
}
static EETile* sShimTile;   // the mouse tile's other out (a local of the original case)
// shims5
extern "C" void ee_screenscpp_mf_set_ui_state(uintptr_t inRec, int32_t inState)
{ R(inRec)->mForm->SetUIState(inState); }
extern "C" bool ee_screenscpp_mf_has_wall(uintptr_t inRec) { return R(inRec)->mWall != NULL; }
extern "C" void ee_screenscpp_mf_wall_cancel(uintptr_t inRec) { R(inRec)->mWall->Cancel(); }
extern "C" void ee_screenscpp_mf_gate_cancel(uintptr_t inRec) { R(inRec)->mGate->Cancel(); }
extern "C" bool ee_screenscpp_mouse_left_just_pressed() { return UIManager::GetLMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_left_down() { return UIManager::GetLMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_left_just_unpressed() { return UIManager::GetLMouseButton()->JustUnpressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_pressed() { return UIManager::GetRMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_down() { return UIManager::GetRMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_unpressed()
{ return UIManager::GetRMouseButton()->JustUnpressed() != 0; }
extern "C" int32_t ee_screenscpp_key_shift_value() { return (int32_t)EEKey(kKeyShift)->GetValue(); }
extern "C" bool ee_screenscpp_key_exists(uint32_t inKey) { return EEKey(inKey) != NULL; }
extern "C" bool ee_screenscpp_key_value(uint32_t inKey) { return EEKey(inKey)->GetValue() != 0; }
extern "C" bool ee_screenscpp_key_just_pressed(uint32_t inKey) { return EEKey(inKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_rasterizer_view_contains(float inX, float inY)
{ return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_picked_owned_by_current(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer() == R(inRec)->mPicked->mPlayer; }
extern "C" bool ee_screenscpp_picked_owner_hostile(uintptr_t inRec)
{ return R(inRec)->mPicked->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] != 0; }
extern "C" bool ee_screenscpp_picked_seen_by_current(uintptr_t inRec)
{ return EECombatIsSeenBy(static_cast<EEComplexUnit*>(R(inRec)->mPicked), gWorld.GetCurrentPlayer(), true); }
extern "C" bool ee_screenscpp_mf_first_from_group(uintptr_t inRec)
{ return (R(inRec)->mFirst = gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit()) != NULL; }
extern "C" void ee_screenscpp_mf_first_clear(uintptr_t inRec) { R(inRec)->mFirst = NULL; }
extern "C" bool ee_screenscpp_mf_first_selected(uintptr_t inRec)
{ return EEGetFirstSelectedUnit(R(inRec)->mFirst) == true; }
extern "C" bool ee_screenscpp_mf_has_first(uintptr_t inRec) { return R(inRec)->mFirst != NULL; }
extern "C" float ee_screenscpp_first_distance_squared(uintptr_t inRec, float inX, float inY, float inZ)
{ return R(inRec)->mFirst->mComplexUnit->DistanceSquared3D(GE3DPoint(inX, inY, inZ)); }
extern "C" float ee_screenscpp_first_min_range(uintptr_t inRec) { return R(inRec)->mFirst->GetUnitClass()->mMinRange; }
extern "C" void ee_screenscpp_player_add_event(int64_t inX, int64_t inY, int32_t inKind)
{ gWorld.GetCurrentPlayer()->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, gWorld.GetCurrentPlayer(), inKind, false); }
extern "C" uint32_t ee_screenscpp_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" bool ee_screenscpp_ghost_can_place(uint32_t inGhost)
{ return static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mCanPlace; }
extern "C" uint32_t ee_screenscpp_ghost_type(uint32_t inGhost)
{ return (uint32_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mUnitClass->mUnitType; }
extern "C" int64_t ee_screenscpp_ghost_tile_x(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mTileX; }
extern "C" int64_t ee_screenscpp_ghost_tile_y(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mTileY; }
extern "C" void ee_screenscpp_build_set_queued(uintptr_t inRec, bool inQueued)
{ R(inRec)->mForm->mBuild->mIsQueued = inQueued; }
extern "C" void ee_screenscpp_build_at(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mBuild->Build(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_camera_clear_unit_a() { gWorld.mCameraControl.ClearUnitA(); }
extern "C" void ee_screenscpp_camera_clear_unit_b() { gWorld.mCameraControl.ClearUnitB(); }
extern "C" void ee_screenscpp_hack_update(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->Update(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_point(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->Fn157269(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_place(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->PlaceUnit(GE2DPoint(inX, inY)); }
extern "C" float ee_screenscpp_mf_mouse_x(uintptr_t inRec) { return R(inRec)->mForm->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mf_mouse_y(uintptr_t inRec) { return R(inRec)->mForm->GetMouseLocation().GetY(); }
extern "C" int64_t ee_screenscpp_mf_mouse_mode(uintptr_t inRec)
{ return (int64_t)R(inRec)->mForm->mMouseModes[R(inRec)->mForm->mUIState]; }
extern "C" bool ee_screenscpp_cinematic_ending() { return gWorld.GetScenarioManager()->mIsEndingCinematic; }
extern "C" void ee_screenscpp_post_cinematic_screen_shot() { gUserInterface->PostScreenShot(-1, true, false); }
extern "C" uintptr_t ee_screenscpp_end_cinematic_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEUIEndCinematicModeEvent); }
extern "C" void ee_screenscpp_mf_start_selection(uintptr_t inRec)
{ static_cast<EEUICSelectUnits*>(R(inRec)->mForm->GetControl(kMainGameSelectUnits))->StartSelection(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_start_command(uintptr_t inRec)
{ static_cast<EEUICCommandUnits*>(R(inRec)->mForm->GetControl(kMainGameCommandUnits))->StartCommand(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_wall_build(uintptr_t inRec) { R(inRec)->mWall->Build(); }
extern "C" void ee_screenscpp_mf_gate_build(uintptr_t inRec, float inX, float inY, int32_t inDirection)
{ R(inRec)->mGate->Build(GE2DPoint(inX, inY), (long)inDirection); }
extern "C" void ee_screenscpp_mf_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, R(inRec)->mGate->mUnitClass, gWorld.GetPlayer(R(inRec)->mGate->mPlayer)); }
extern "C" void ee_screenscpp_gate_preview(float inX, float inY, long* inDirection)
{ EEGhostUnit::PreviewGate(GE2DPoint(inX, inY), *inDirection); }
extern "C" void ee_screenscpp_mf_wall_update(uintptr_t inRec)
{ R(inRec)->mForm->mBuildWall.Update(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_gate_update(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.Update(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_error_lock_enter(uintptr_t inRec) { R(inRec)->mForm->mErrorCriticalSection->Enter(); }
extern "C" void ee_screenscpp_mf_error_lock_leave(uintptr_t inRec) { R(inRec)->mForm->mErrorCriticalSection->Leave(); }
extern "C" void ee_screenscpp_mf_group_reset(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Reset(); }
extern "C" void ee_screenscpp_mf_group_refresh(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Refresh(); }
extern "C" void ee_screenscpp_mf_group_select(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Select(); }
extern "C" bool ee_screenscpp_mf_group_wants_refresh(uintptr_t inRec)
{ return R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->WantsRefresh() == true; }
extern "C" uintptr_t ee_screenscpp_mf_pick(uintptr_t inRec)
{ return (uintptr_t)(R(inRec)->mPicked = gUnitPicker.PickUnit(gMouse->mPosition, 0)); }
extern "C" bool ee_screenscpp_mf_picked_is_wall(uintptr_t inRec) { return R(inRec)->mPicked->mUnitClass->mIsWall; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_x(uintptr_t inRec) { return (int64_t)R(inRec)->mPicked->mTileX; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_y(uintptr_t inRec) { return (int64_t)R(inRec)->mPicked->mTileY; }
extern "C" void ee_screenscpp_mouse_tile(long* inX, long* inY)
{ EEPointUnitOrTileXY(gMouse->mPosition, sShimTile, *inX, *inY, true); }
extern "C" void ee_screenscpp_mf_picked_dereference(uintptr_t inRec) { R(inRec)->mPicked->Dereference(); }
extern "C" void ee_screenscpp_mf_picked_clear(uintptr_t inRec) { R(inRec)->mPicked = NULL; }
extern "C" void ee_screenscpp_mf_form_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, R(inRec)->mForm->mBuildGate.mUnitClass, gWorld.mPlayers[R(inRec)->mForm->mBuildGate.mPlayer]); }
extern "C" void ee_screenscpp_gate_preview_at_mouse(long* inDirection)
{ EEGhostUnit::PreviewGate(gMouse->mPosition, *inDirection); }
extern "C" float ee_screenscpp_mf_wall_start_x(uintptr_t inRec) { return R(inRec)->mForm->mBuildWall.mStart.GetX(); }
extern "C" float ee_screenscpp_mf_wall_start_y(uintptr_t inRec) { return R(inRec)->mForm->mBuildWall.mStart.GetY(); }
extern "C" uintptr_t ee_screenscpp_team_wonder_countdown(uint32_t inTeam, unsigned long* inValue)
{ return (uintptr_t)gWorld.mTeams[inTeam].mVictoryCondition->GetWonderCountdown(*inValue); }
extern "C" void ee_screenscpp_mf_info_set_text(uintptr_t inRec)
{ static_cast<UITLScrolling*>(R(inRec)->mForm->GetControl(kMainGameDialogMessage))->SetText(R(inRec)->mMessage->mText); }
extern "C" uintptr_t ee_screenscpp_mf_control(uintptr_t inRec, uint32_t inControl)
{ return (uintptr_t)R(inRec)->mForm->GetControl(inControl); }
extern "C" uint32_t ee_screenscpp_mf_info_unit_type(uintptr_t inRec) { return (uint32_t)R(inRec)->mMessage->mUnitType; }
extern "C" uint32_t ee_screenscpp_unit_class_count() { return (uint32_t)EEUnitClassCount(); }
extern "C" bool ee_screenscpp_unit_class_exists(uint32_t inKind) { return gUnitClassTable[inKind] != NULL; }
extern "C" bool ee_screenscpp_player_class_has_button(uint32_t inKind)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[inKind]->mButton != NULL; }
extern "C" void ee_screenscpp_mf_info_set_face(uintptr_t inRec, uintptr_t inBitmap, uintptr_t inPortrait)
{ gWorld.GetCurrentPlayer()->mUnitClassUpgrades[R(inRec)->mMessage->mUnitType]->mButton->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inPortrait)->GetRectangle(), C(inPortrait)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ui_bitmap_set_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIBitmap*>(C(inControl))->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_control_refresh_opacity(uintptr_t inControl)
{ C(inControl)->SetBgOpacity(C(inControl)->GetBgOpacity()); }
extern "C" void ee_screenscpp_mf_info_load_sound(uintptr_t inRec, unsigned long* inSample, unsigned long* inLength)
{ EELoadSound("Sounds\\" + UString((const char*)R(inRec)->mMessage->mSound), 0x31, *inSample, *inLength, 1); }
extern "C" void ee_screenscpp_mf_poll_new(uintptr_t inRec, uint32_t inMilliseconds)
{ R(inRec)->mForm->mPoll = new TMPoll((unsigned long)inMilliseconds, 0, false); }
extern "C" void ee_screenscpp_mf_poll_start(uintptr_t inRec) { R(inRec)->mForm->mPoll->Start(); }
extern "C" uint32_t ee_screenscpp_game_size_width() { return (uint32_t)gGameWindowWidth; }
extern "C" uint32_t ee_screenscpp_game_size_height() { return (uint32_t)gGameWindowHeight; }
extern "C" void ee_screenscpp_mf_scroll_new(uintptr_t inRec) { R(inRec)->mForm->mScroll = new EEAScroll; }
extern "C" void ee_screenscpp_mf_scroll_set_edges(uintptr_t inRec, float inLeft, float inTop, float inRight, float inBottom)
{ R(inRec)->mForm->mScroll->mEdges = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mf_mouse_click_new(uintptr_t inRec) { R(inRec)->mForm->mMouseClick = new EEAMouseClick; }
extern "C" void ee_screenscpp_mf_lasso_new(uintptr_t inRec) { R(inRec)->mForm->mLasso = new EEALasso; }
extern "C" void ee_screenscpp_mf_build_new(uintptr_t inRec) { R(inRec)->mForm->mBuild = new EEABuild; }
extern "C" void ee_screenscpp_mf_calamity_new(uintptr_t inRec) { R(inRec)->mForm->mCalamity = new EEACalamity; }
extern "C" void ee_screenscpp_mf_wheel_zoom_new(uintptr_t inRec, uint32_t inCode)
{ R(inRec)->mForm->mWheelZoom = EECreateScreenEvent((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_wheel_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseWheelMoveEvent(R(inRec)->mForm->mWheelZoom); }
extern "C" void ee_screenscpp_mf_grab_scroll_new(uintptr_t inRec, uint32_t inCode)
{ R(inRec)->mForm->mGrabScroll = EECreateGrabScroll((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_middle_pressed_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseMiddleButtonPressedEvent(R(inRec)->mForm->mGrabScroll); }
extern "C" void ee_screenscpp_mf_set_middle_unpressed_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseMiddleButtonUnpressedEvent(R(inRec)->mForm->mGrabScroll); }
extern "C" void ee_screenscpp_mf_add_hot_key(uintptr_t inRec, uint32_t inKey, uint32_t inModifier1, uint32_t inModifier2, uint32_t inModifier3, uintptr_t inEvent1, uintptr_t inEvent2, uintptr_t inEvent3, uintptr_t inEvent4, uint32_t inId, bool inFlag)
{ R(inRec)->mForm->AddHotKey(inKey, inModifier1, inModifier2, inModifier3, reinterpret_cast<UIEvent*>(inEvent1), reinterpret_cast<UIEvent*>(inEvent2), reinterpret_cast<UIEvent*>(inEvent3), reinterpret_cast<UIEvent*>(inEvent4), (unsigned long)inId, inFlag); }
extern "C" void ee_screenscpp_mf_unit_load(uintptr_t inRec, uint32_t inUnit)
{ R(inRec)->mUnit = static_cast<EEUnit*>(gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inUnit)); }
extern "C" uintptr_t ee_screenscpp_mf_unit(uintptr_t inRec) { return (uintptr_t)R(inRec)->mUnit; }
extern "C" void ee_screenscpp_mf_ghost_of_node(uintptr_t inRec)
{ EEGhostUnit::Create(R(inRec)->mForm->GetMouseLocation(), R(inRec)->mNode->mData->mUnitClass, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_ghost_of_calamity(uintptr_t inRec)
{ EEGhostUnit::Create(R(inRec)->mForm->GetMouseLocation(), R(inRec)->mCalamity->mUnitClassIndex, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_gate_set_technology(uintptr_t inRec, int32_t inTechnology)
{ R(inRec)->mForm->mBuildGate.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_gate_wall_type_from_node(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.mWallType = R(inRec)->mNode->GetUnitClass(0); }
extern "C" void ee_screenscpp_mf_gate_class_from_node(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.SetUnitClass(gUnitClassTable[R(inRec)->mNode->mData->mUnitClass]); }
extern "C" void ee_screenscpp_mf_gate_initialize(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.Initialize(gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" void ee_screenscpp_mf_wall_set_technology(uintptr_t inRec, int32_t inTechnology)
{ R(inRec)->mForm->mBuildWall.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_wall_initialize(uintptr_t inRec)
{ R(inRec)->mForm->mBuildWall.Initialize(gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" uint32_t ee_screenscpp_selected_count()
{ return (uint32_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" void ee_screenscpp_mf_caster_first(uintptr_t inRec)
{ R(inRec)->mCaster = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0); }
extern "C" void ee_screenscpp_mf_caster_clear(uintptr_t inRec) { R(inRec)->mCaster = NULL; }
extern "C" bool ee_screenscpp_mf_has_caster(uintptr_t inRec) { return R(inRec)->mCaster != NULL; }
extern "C" void ee_screenscpp_mf_calamity_set(uintptr_t inRec, int32_t inCalamity)
{ R(inRec)->mForm->mCalamity->SetCalamity((long)inCalamity, R(inRec)->mCalamity); }
extern "C" void ee_screenscpp_mf_calamity_set_caster(uintptr_t inRec)
{ R(inRec)->mForm->mCalamity->SetCasterID(R(inRec)->mCaster->mUnitID); }
extern "C" int64_t ee_screenscpp_mf_calamity_target_kind(uintptr_t inRec)
{ return (int64_t)R(inRec)->mCalamity->GetTargetKind(); }
extern "C" void ee_screenscpp_mouse_shift_off() { gMouse->mShiftMode = false; }
extern "C" void ee_screenscpp_mouse_set_pointer(int32_t inPointer) { gMouse->mPointer = (long)inPointer; }
extern "C" void ee_screenscpp_mouse_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ gMouse->SetBounds(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_server_unpause(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->UnpauseGame(); }
extern "C" uintptr_t ee_screenscpp_graphics() { return (uintptr_t)gGraphics; }
extern "C" void ee_screenscpp_gfx_clear_render_objects() { gGraphics->PostClearRenderObjects(); }
extern "C" void ee_screenscpp_mf_cancel_button_up(uintptr_t inRec, uintptr_t inEvent)
{ R(inRec)->mForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_cancel_right_button_up(uintptr_t inRec, uintptr_t inEvent)
{ R(inRec)->mForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_intro_set_blinking(bool inBlinking)
{ EEUISCIFormManager::Get()->SetBlinking(inBlinking); }
extern "C" void ee_screenscpp_mf_main_group_open(uintptr_t inRec)
{ static_cast<EEUIFMGMainGroup*>(R(inRec)->mForm->mGroups[0])->Open(R(inRec)->mForm); }
extern "C" void ee_screenscpp_mf_main_group_clear_l20(uintptr_t inRec)
{ static_cast<EEUIFMGMainGroup*>(R(inRec)->mForm->mGroups[0])->mL20 = 0; }
extern "C" void ee_screenscpp_post_graphics_all() { EEPostGraphicsAll(); }
extern "C" void ee_screenscpp_gfx_fade_in(uint32_t inValue) { gGraphics->PostFadeIn(inValue); }
extern "C" void ee_screenscpp_mf_media_load(uintptr_t inRec) { R(inRec)->mMedia->LoadMedia(); }
extern "C" void ee_screenscpp_mf_old_media_unload(uintptr_t inRec) { R(inRec)->mOldMedia->UnloadMedia(); }
extern "C" void ee_screenscpp_shell_op(uintptr_t inControl, uint32_t inOp) { EEUIShell::Op(C(inControl), inOp); }
extern "C" uintptr_t ee_screenscpp_mf_background(uintptr_t inRec, uint32_t inId)
{ return (uintptr_t)R(inRec)->mForm->GetBackgroundByID((unsigned long)inId); }
extern "C" void ee_screenscpp_background_draw(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->DrawBackground(reinterpret_cast<UIBackground*>(inBackground)->GetRectangle()); }
extern "C" float ee_screenscpp_mf_fps(uintptr_t inRec)
{ return gRasterizer->GetFramesPerSecond(R(inRec)->mForm->GetManager()->GetScreen()); }
// end shims5
