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
#include "ee/EEUIBSavedGroup.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUIFMGErrorLog.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIFMGStatusButton.h"
#include "lle/UIFormManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEStringIDs.h"
// the command buttons' part of the host (EEUIFMGStatusButton.cpp)
int64_t EEStatusButtonOp(EEUIFMGStatusButton* inButton, EEComplexUnit* inFirst, UWideString& ioText,
                         UWideString* outString, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC);
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (Open passes mIsButtonBlinking)
void EEPostGraphicsAll();                   // 0x10fc0c (Open) (provisional name)
class EEMiniMap;
EEMiniMap* gMiniMap;                        // 0x564888 (ee/EEMiniMap.h; .bss between this unit's statics and EEUIFMGStatusBox's)
bool gUnitSelected;                         // 0x56488c (a unit is selected: the main form draws backgrounds 0x6f..0x71)
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

// One call of the form's work in Rust (ee_screens main_form.rs, host EEUIFMainGame::HostOp): the form, the
// picked unit, the held text, the chat line's colour, name, text and flags, the dialog message
struct EEUIFMGFormCall {
    EEUIFMainGame* mForm;                   // first: main_form_ffi.rs reads it (the toolkit's direct path)
    EEUnit* mPicked;
    UWideString mText;
    const GEColor* mColor;
    UWideString mName;
    UWideString mChat;
    unsigned long mFlags;
    const EESMDialogMessage* mMessage;
    EETechTreeNode* mNode;                  // the start modes' tech tree node, unit, caster, calamity
    EEUnit* mUnit;
    EEUnit* mCaster;
    EECalamityClass* mCalamity;
    EEMedia* mOldMedia;                     // UpdateBackgroundMedia's old and new media
    EEMedia* mMedia;
    EEComplexUnit* mFirst;                  // the mouse modes' first selected unit and pointed tile
    EETile* mTile;
    EEUIBuildWall* mWall;                   // the placement whose Update runs (the game form's or the editor's)
    EEUIBuildGate* mGate;
    EEUIFMGStatusButton* mButton;           // a command button's press, CopyData or help string (its result)
    UWideString* mOut;
    explicit EEUIFMGFormCall(EEUIFMainGame* inForm)
        : mForm(inForm), mPicked(NULL), mColor(NULL), mFlags(0), mMessage(NULL), mNode(NULL), mUnit(NULL),
          mCaster(NULL), mCalamity(NULL), mOldMedia(NULL), mMedia(NULL), mFirst(NULL), mTile(NULL),
          mWall(NULL), mGate(NULL), mButton(NULL), mOut(NULL) {}
};
static_assert(offsetof(EEUIFMGFormCall, mForm) == 0, "main_form_ffi.rs reads the form as the record's first word");
static_assert(offsetof(EEUIFMGFormCall, mText) == 2 * sizeof(void*) &&
                  offsetof(EEUIFMGFormCall, mName) == 3 * sizeof(void*) + sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mChat) == 3 * sizeof(void*) + 2 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mButton) == 15 * sizeof(void*) + 3 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mOut) == 16 * sizeof(void*) + 3 * sizeof(UWideString),
              "hud_direct.rs FormCallHead: the held text, the chat line's name and text, the button, the help");

// Runs a member that is Rust's (EE_SCREENS_MF_DO_*: Activate, Deactivate, Open, Close, DrawPolygons and the
// original's UpdateBackgroundMedia, ShowAllBackgrounds, SetPauseText, PauseInformation, ResumeInformation,
// SetChatTop, HideAllBackgrounds, HideMessages, RefreshSavedGroups, UpdateIconBar: ee_screens main_form.rs)
int64_t EEUIFMainGame::Rust(uint32_t inWhich, int64_t inA)
{
    EEUIFMGFormCall theCall(this);
    return ee_screens_main_form_member(&theCall, inWhich, inA);
}

// A command button's member in Rust (EE_SCREENS_SB_DO_*: ButtonUp, MouseRButtonUp, CopyData, GetHelpString into
// outString; ee_screens status_button.rs over this host)
int64_t EEUIFMainGame::ButtonCall(EEUIFMGStatusButton* inButton, uint32_t inWhich, UWideString* outString)
{
    EEUIFMGFormCall theCall(this);
    theCall.mButton = inButton;
    theCall.mOut = outString;
    return ee_screens_status_button_member(&theCall, inWhich);
}

// Runs the keyboard mode of the current interface state (the cinematic's keys), else the form's keys
long EEUIFMainGame::UpdateKeyboard()
{
    return (long)this->Rust(EE_SCREENS_MF_DO_KEYBOARD, this->mKeyboardModes[this->mUIState]);
}

void EEUIFMainGame::ShowAllBackgrounds()
{
    this->Rust(EE_SCREENS_MF_DO_SHOW_BACKGROUNDS);
}

void EEUIFMainGame::HideAllBackgrounds()
{
    this->Rust(EE_SCREENS_MF_DO_HIDE_BACKGROUNDS);
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

// Builds a gate at a location facing a direction
void EEUIFMainGame::BuildGate(const GE2DPoint& inLocation, long inDirection)
{
    this->mBuild->BuildGate(inLocation, inDirection);
}

// Builds a wall between two points
void EEUIFMainGame::BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd)
{
    this->mBuild->BuildWall(inStart, inEnd);
}


// Shows a text in the main game form's title label
void EEUIFMainGame::SetStatusText(const UWideString& inText)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetControl(kMainGameTitle));

    if (theLabel != NULL)
        theLabel->SetText(inText);
}

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
void EEUIFMainGame::SetErrorString(const UWideString& inText, unsigned long inIndex)
{
    this->mErrorCriticalSection->Enter();
    this->mStrings[inIndex] = inText;
    this->mStringsChanged = true;
    this->mErrorCriticalSection->Leave();
}

// Opens the form: sound latency and volume, centred mouse, empty error log, the cancel-mode event, the
// backgrounds and the civilization builder button's blinking
void EEUIFMainGame::Open()
{
    this->Rust(EE_SCREENS_MF_DO_OPEN);
}

// Reads the information and civilization builder blinking flags from a saved game (version 0.0xd2 on)
long EEUIFMainGame::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = kErrorOK;

    if (inMajorVersion > 1 || (inMajorVersion == kErrorOK && inMinorVersion >= 0xd2)) {
        theResult = inFile.Read(this->mInfoBlinking);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsButtonBlinking);
    }
    return theResult;
}

// Writes the information and civilization builder blinking flags to the saved game
long EEUIFMainGame::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mInfoBlinking);

    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsButtonBlinking);
    return theResult;
}


// Starts placing a building from a unit's tech when it can be produced; otherwise beeps and explains in
// the chat log (ee_screens main_form.rs, as the other start modes)
void EEUIFMainGame::BuildUnit(unsigned long inUnitID, long inTechnology)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_build_unit(&theCall, (int64_t)inUnitID, inTechnology);
}

// Starts placing a gate for a tech (gate placement mode)
void EEUIFMainGame::StartGate(long inTechnology)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_start(&theCall, 0, inTechnology);
}

// Starts placing a wall for a tech (wall placement mode)
void EEUIFMainGame::StartWall(long inTechnology)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_start(&theCall, 1, inTechnology);
}

// Starts casting the calamity of a tech with the first selected unit: at once for self-targeted ones,
// otherwise in calamity mode (with a volcano ghost for tech 0x3f9)
void EEUIFMainGame::StartCalamity(long inTechnology)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_start(&theCall, 2, inTechnology);
}


// Enters flare mode when units are selected
void EEUIFMainGame::StartFlare()
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_start(&theCall, 3, 0);
}

// Enters patrol mode when units are selected
void EEUIFMainGame::StartPatrol()
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_start(&theCall, 4, 0);
}

// Switches the interface mode, clearing ghosts and ending a wall or gate placement
void EEUIFMainGame::SetUIState(long inUIState)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_ui_state(&theCall, inUIState);
}



#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIEChatLogEvents.h"

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
    case EE_SCREENS_MF_OP_MD_UI_STATE:
        if (inB == 0)
            theForm->SetUIState((long)inA);
        else
            theCall.mWall != NULL ? theCall.mWall->Cancel() : theCall.mGate->Cancel();
        return 0;
    case EE_SCREENS_MF_OP_MD_BUTTONS: {
        ISEntity* theL = UIManager::GetLMouseButton();
        ISEntity* theR = UIManager::GetRMouseButton();
        return (theL->JustPressed() ? 1 : 0) | (theL->GetValue() ? 2 : 0) | (theL->JustUnpressed() ? 4 : 0) |
               (theR->JustPressed() ? 8 : 0) | (theR->GetValue() ? 16 : 0) | (theR->JustUnpressed() ? 32 : 0);
    }
    case EE_SCREENS_MF_OP_MD_SHIFT: {
        ISEntity* theKey;
        gISKeyboard->GetEntity(kKeyShift, theKey);
        return theKey->GetValue();
    }
    case EE_SCREENS_MF_OP_MD_VIEW:
        if (inA != 0)
            return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(theLocation);
        return gViewport.GetViewRectangle().Contains(theLocation);
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
    case EE_SCREENS_MF_OP_MD_UNIT:
        switch (inA) {
        case EE_SCREENS_MF_UNIT_ID: return thePicked->mUnitID;
        case EE_SCREENS_MF_UNIT_HOSTILE_SEEN:
            return thePlayer != thePicked->mPlayer && thePicked->mPlayer->mEnemies[thePlayer->mPlayerID] &&
                   EEIsSeenBy(static_cast<EEComplexUnit*>(thePicked), thePlayer, true);
        case EE_SCREENS_MF_UNIT_FAMILY: return thePicked->GetUnitClass()->mFamily;
        case EE_SCREENS_MF_UNIT_CAN_BE_ATTACKED: return thePicked->GetUnitClass()->CanBeAttacked();
        case EE_SCREENS_MF_UNIT_IS_WALL: return thePicked->GetUnitClass()->mIsWall;
        case EE_SCREENS_MF_UNIT_TILE_X: return thePicked->GetTileX();
        case EE_SCREENS_MF_UNIT_TILE_Y: return thePicked->GetTileY();
        case EE_SCREENS_MF_UNIT_HAS_COMPLEX: return theComplex != NULL;
        case EE_SCREENS_MF_UNIT_COMPLEX_BUILT: return theComplex->mIsBuilt;
        case EE_SCREENS_MF_UNIT_COMPLEX_CAPACITY: return theComplex->GetUnitClass()->mGarrisonCapacity != 0;
        case EE_SCREENS_MF_UNIT_COMPLEX_ROOM: return theComplex->GetUnitClass()->mGarrisonCapacity != theComplex->mGarrisonCount;
        case EE_SCREENS_MF_UNIT_IN_WONDER_SET:
            return gUnitSetManager.Contains(theComplex->GetUnitClass()->GetUnitType(), theFirst->mStats->mWonderUnitSet);
        case EE_SCREENS_MF_UNIT_SAME_PLAYER: return theFirst->mComplexUnit->mPlayer == theComplex->mPlayer;
        }
        return 0;
    case EE_SCREENS_MF_OP_MD_DAMAGE: ioIo->f[0] = EEDamageFactor((long)inA, (long)inB); return 0;
    case EE_SCREENS_MF_OP_MD_FIRST:
        if (inA != 0)
            return (theCall.mFirst = thePlayer->mSelectedGroup->GetFirstUnit()) != NULL;
        theCall.mFirst = NULL;
        return (EEGetFirstSelectedUnit(theCall.mFirst) == true ? 1 : 0) | (theCall.mFirst != NULL ? 2 : 0);
    case EE_SCREENS_MF_OP_MD_FIRST_FIELD:
        switch (inA) {
        case EE_SCREENS_MF_FIRST_ALIVE: return theFirst->mHitPoints > 0;
        case EE_SCREENS_MF_FIRST_HAS_COMPLEX: return theFirst->mComplexUnit != NULL;
        case EE_SCREENS_MF_FIRST_IN_RANGE:
            return theFirst->mComplexUnit->DistanceSquared3D(GE3DPoint(ioIo->f[0], ioIo->f[1], ioIo->f[2])) >=
                   UMSquare(theFirst->GetUnitClass()->mMinRange);
        case EE_SCREENS_MF_FIRST_CAN_RALLY: return theFirst->GetUnitClass()->mCanSetRallyPoint;
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
    case EE_SCREENS_MF_OP_MD_EVENT: {
        U2DPoint<long> thePoint;
        thePoint.mX = (long)ioIo->n[0];
        thePoint.mY = (long)ioIo->n[1];
        thePlayer->mEvents.AddEvent(thePoint, 2, thePlayer, (long)inA, false);
        return 0;
    }
    case EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT:
        EEMakeCalamityEffect(GE3DPoint(ioIo->f[0], ioIo->f[1], ioIo->f[2]), thePicked);
        return 0;
    case EE_SCREENS_MF_OP_MD_PREVIEW: EEPreviewPlacement(theForm->GetMouseLocation()); return 0;
    case EE_SCREENS_MF_OP_MD_GHOST: {
        if (inA < 0)
            return gWorld.GetNumGhostUnits();
        EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(gWorld.mGhostUnits[(size_t)inA]);
        return inB == EE_SCREENS_MF_GHOST_CAN_PLACE ? (int64_t)theGhost->mCanPlace
             : inB == EE_SCREENS_MF_GHOST_TYPE      ? (int64_t)theGhost->mUnitClass->mUnitType
             : inB == EE_SCREENS_MF_GHOST_TILE_X    ? (int64_t)theGhost->mTileX : (int64_t)theGhost->mTileY;
    }
    case EE_SCREENS_MF_OP_MD_BUILD:
        theForm->mBuild->mIsQueued = inA != 0;
        theForm->mBuild->Build(theLocation);
        return 0;
    case EE_SCREENS_MF_OP_MD_CALAMITY_CHECK: return theForm->mCalamity->Check(theLocation);
    case EE_SCREENS_MF_OP_MD_IN_GAME_VIEW: return theForm->IsInGameView(theLocation);
    case EE_SCREENS_MF_OP_MD_ENTITY_ENABLED: return theForm->GetEntityGroup((unsigned long)inA)->GetIsEnabled();
    case EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP: theForm->SetEntityGroup((long)inA); return 0;
    case EE_SCREENS_MF_OP_MD_CAMERA:
        if (inA == 0) {
            gWorld.mCameraControl.ClearUnitA();
            gWorld.mCameraControl.ClearUnitB();
        } else
            gWorld.mCameraControl.Follow(NULL, false, 1.0f, 0.0f, 0.0f);
        return 0;
    case EE_SCREENS_MF_OP_MD_HACK:
        if (inA == 2)
            theForm->mUnitHack->Update(theLocation);
        else
            inA == 0 ? theForm->mUnitHack->Fn157269(theLocation) : theForm->mUnitHack->PlaceUnit(theLocation);
        return 0;
    case EE_SCREENS_MF_OP_MD_MOUSE_AT:
        ioIo->f[0] = theForm->GetMouseLocation().GetX(), ioIo->f[1] = theForm->GetMouseLocation().GetY();
        return theForm->mMouseModes[theForm->mUIState];
    case EE_SCREENS_MF_OP_MD_KEY: {
        ISEntity* theKey;
        if (gISKeyboard->GetEntity((unsigned long)inA, theKey) != 0)
            return 0;
        return 1 | (theKey->GetValue() ? 2 : 0) | (theKey->JustPressed() ? 4 : 0);
    }
    case EE_SCREENS_MF_OP_MD_CINEMATIC: {
        if (inA == EE_SCREENS_MF_CINE_ENDING)
            return gWorld.GetScenarioManager()->mIsEndingCinematic;
        if (inA == EE_SCREENS_MF_CINE_SCREENSHOT) {
            gUserInterface->PostScreenShot(-1, true, false);
            return 0;
        }
        UIEvent* theEvent = inA == EE_SCREENS_MF_CINE_END ? new EEUIEndCinematicModeEvent : EECreateScreenEvent(EE_SCREENS_MG_PAUSE);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_MF_OP_MD_START:
        if (inA == 0)
            static_cast<EEUICSelectUnits*>(theForm->GetControl(kMainGameSelectUnits))->StartSelection(theForm->GetMouseLocation());
        else
            static_cast<EEUICCommandUnits*>(theForm->GetControl(kMainGameCommandUnits))->StartCommand(theForm->GetMouseLocation());
        return 0;
    case EE_SCREENS_MF_OP_MD_BUILD_WALL:
        inA == 0 ? theCall.mWall->Build() : theCall.mGate->Build(theLocation, (long)inB);
        return 0;
    case EE_SCREENS_MF_OP_MD_GATE_CLEAR: theCall.mGate->ClearGhostUnits(); return 0;
    case EE_SCREENS_MF_OP_MD_GATE_PREVIEW: {
        long theDirection = (long)ioIo->n[0];
        EEGhostUnit::CreateGate(0.0f, theCall.mGate->mUnitClass, gWorld.GetPlayer(theCall.mGate->mPlayer));
        EEGhostUnit::PreviewGate(theLocation, theDirection);
        ioIo->n[0] = theDirection;
        return 0;
    }
    case EE_SCREENS_MF_OP_MD_PLACEMENT:
        if (inA == 0)
            theForm->mBuildWall.Update(theForm->GetMouseLocation());
        else
            theForm->mBuildGate.Update(theForm->GetMouseLocation());
        return 0;
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
    case EE_SCREENS_MF_OP_ERROR_LOCK:
        inA != 0 ? theForm->mErrorCriticalSection->Enter() : theForm->mErrorCriticalSection->Leave();
        return 0;
    case EE_SCREENS_MF_OP_SET_UNIT_SELECTED: gUnitSelected = inA != 0; return 0;
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
        static_cast<EEUIFMGErrorLog*>(theForm->GetControl(kMainGameErrorLog))->SetLine((unsigned long)inA,
                                                                                    theForm->mStrings[inA]);
        return 0;
    case EE_SCREENS_MF_OP_VIEW_RECT: {
        GERectangle theView = gViewport.GetViewRectangle();
        ioIo->f[0] = theView.GetLeft(), ioIo->f[1] = theView.GetTop();
        ioIo->f[2] = theView.GetRight(), ioIo->f[3] = theView.GetBottom();
        return 0;
    }
    case EE_SCREENS_MF_OP_POLL: return theForm->mPoll->Poll();
    case EE_SCREENS_MF_OP_GROUP: {
        EEUIFMGGroup* theGroup = theForm->mGroups[theForm->mCurrentGroup];
        if (inA == EE_SCREENS_MF_GROUP_RESET)
            theGroup->Reset();
        else if (inA == EE_SCREENS_MF_GROUP_REFRESH)
            theGroup->Refresh();
        else if (inA == EE_SCREENS_MF_GROUP_SELECT)
            theGroup->Select();
        else
            return theGroup->WantsRefresh() == true;
        return 0;
    }
    case EE_SCREENS_MF_OP_SCROLL: theForm->mScroll->Update(gMouse->mPosition); return 0;
    case EE_SCREENS_MF_OP_PICK:
        theCall.mPicked = gUnitPicker.PickUnit(gMouse->mPosition, 0);
        return theCall.mPicked == NULL ? 0 : theCall.mPicked->mUnitClass->mIsWall ? 2 : 1;
    case EE_SCREENS_MF_OP_PICKED_TILE: ioIo->n[0] = theCall.mPicked->mTileX; ioIo->n[1] = theCall.mPicked->mTileY; return 0;
    case EE_SCREENS_MF_OP_MOUSE_TILE: {
        EETile* theOther;
        long theX = 0, theY = 0;
        EEPointUnitOrTileXY(gMouse->mPosition, theOther, theX, theY, true);
        ioIo->n[0] = theX, ioIo->n[1] = theY;
        return 0;
    }
    case EE_SCREENS_MF_OP_RELEASE: theCall.mPicked->Dereference(); theCall.mPicked = NULL; return 0;
    case EE_SCREENS_MF_OP_FIND_GATE_WALL: {
        EETileCoordinate theTile;
        theTile.mX = (long)inA, theTile.mY = (long)inB;
        long theDirection = (long)ioIo->n[0];
        bool theFound = EEFindGateWall(theTile, theDirection);
        ioIo->n[0] = theDirection;
        return theFound;
    }
    case EE_SCREENS_MF_OP_PREVIEW_GATE: {
        long theDirection = (long)ioIo->n[0];
        EEGhostUnit::CreateGate(0.0f, theForm->mBuildGate.mUnitClass, gWorld.mPlayers[theForm->mBuildGate.mPlayer]);
        EEGhostUnit::PreviewGate(gMouse->mPosition, theDirection);
        ioIo->n[0] = theDirection;
        return 0;
    }
    case EE_SCREENS_MF_OP_GHOST_LOCK: inA != 0 ? gWorld.mGhostUnitsLock->Enter() : gWorld.mGhostUnitsLock->Leave(); return 0;
    case EE_SCREENS_MF_OP_CLEAR_GHOSTS: gWorld.ClearGhostUnits(); return 0;
    case EE_SCREENS_MF_OP_WALL_START:
        ioIo->f[0] = theForm->mBuildWall.mStart.GetX(), ioIo->f[1] = theForm->mBuildWall.mStart.GetY();
        return 0;
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
    case EE_SCREENS_MF_OP_COUNTDOWN: {
        unsigned long theValue = 0;
        EEPlayer* theWinner = gWorld.mTeams[inA].mVictoryCondition->GetWonderCountdown(theValue);
        ioIo->n[0] = theValue;
        return (int64_t)(intptr_t)theWinner;
    }
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
    case EE_SCREENS_MF_OP_FPS: ioIo->f[0] = gRasterizer->GetFramesPerSecond(theForm->GetManager()->GetScreen()); return 0;
    case EE_SCREENS_MF_OP_HAS_COMMS: return gCommunications != NULL;
    case EE_SCREENS_MF_OP_COMMS_PAUSED: return gCommunications->GetIsGamePaused();
    case EE_SCREENS_MF_OP_HAS_TEAMS: return gWorld.mTeams.size() > 0;
    case EE_SCREENS_MF_OP_SET_MUTED: gSoundManager->SetIsMuted(inA != 0); return 0;
    case EE_SCREENS_MF_OP_MUTED: return gSoundManager->GetIsMuted();
    case EE_SCREENS_MF_OP_PAUSE_TEXT: theForm->Rust(EE_SCREENS_MF_DO_PAUSE_TEXT, inA); return 0;
    case EE_SCREENS_MF_OP_LAG_STATUS: return gWorld.mLagStatus;
    case EE_SCREENS_MF_OP_LAG_COLOR:
        static_cast<UIBitmap*>(theForm->GetControl((unsigned long)inA))->GetBitmap()->SetColor(gWorld.mPlayers[inB]->mColor);
        return 0;
    case EE_SCREENS_MF_OP_HAS_INSTRUCTIONS: return gWorld.mScenarioManager->mMediaStrings.mInstructions.GetLength() > 0;
    case EE_SCREENS_MF_OP_LOG_ENTRIES: return EEUIECLEFormManager::GetEntryCount();
    case EE_SCREENS_MF_OP_UPDATE_CURSOR: theForm->UpdateCursorKind(); return 0;
    case EE_SCREENS_MF_OP_INFO_SET_TEXT: {
        UITLScrolling* theText = static_cast<UITLScrolling*>(theForm->GetControl(kMainGameDialogMessage));
        theText->SetText(theCall.mMessage->mText);
        theText->SetDisplayMode(kDisplayModeShown);
        return 0;
    }
    case EE_SCREENS_MF_OP_INFO_HAS_BUTTON: {
        // (a class past the table is no portrait)
        unsigned long theType = theCall.mMessage->mUnitType;
        return theType < (unsigned long)EEUnitClassCount() && gUnitClassTable[theType] != NULL &&
               gWorld.GetCurrentPlayer()->mUnitClassUpgrades[theType]->mButton != NULL;
    }
    case EE_SCREENS_MF_OP_INFO_PORTRAIT: {
        UIBitmap* thePortrait = static_cast<UIBitmap*>(theForm->GetControl(kMainGameDialogPortrait));
        EEButtonObject* theButton = gWorld.GetCurrentPlayer()->mUnitClassUpgrades[theCall.mMessage->mUnitType]->mButton;
        GEBitmap* theBitmap = new GEBitmap;
        theButton->SetFaceBitmap(theBitmap, thePortrait->GetRectangle(), thePortrait->GetScreenIndex());
        thePortrait->SetBitmap(theBitmap);
        thePortrait->SetDisplayMode(kDisplayModeShown);
        thePortrait->SetBgOpacity(thePortrait->GetBgOpacity());
        theBitmap->Dereference();
        return 0;
    }
    case EE_SCREENS_MF_OP_INFO_SOUND: {
        unsigned long theSampleID, theLength;
        EELoadSound("Sounds\\" + UString((const char*)theCall.mMessage->mSound), 0x31, theSampleID, theLength, 1);
        ioIo->n[0] = theSampleID, ioIo->n[1] = theLength;
        return 0;
    }
    case EE_SCREENS_MF_OP_PLAY_TIME: return gSoundManager->GetTotalPlayTimeInMilli((unsigned long)inA);
    case EE_SCREENS_MF_OP_INFO_DURATION: return theCall.mMessage->mDuration;
    case EE_SCREENS_MF_OP_CHAT_TOP: theForm->Rust(EE_SCREENS_MF_DO_CHAT_TOP, inA); return 0;
    case EE_SCREENS_MF_OP_HIDE_MESSAGES: theForm->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); return 0;
    case EE_SCREENS_MF_OP_CREATE_ERROR_LOCK:
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theForm->mErrorCriticalSection) == keseOK, 633);
        return 0;
    case EE_SCREENS_MF_OP_CREATE_GROUP: theForm->CreateGroup((unsigned long)inA); return 0;
    case EE_SCREENS_MF_OP_NEW_UNIT_HACK: theForm->mUnitHack = new EEUIFMGUnitHack(theForm); return 0;
    case EE_SCREENS_MF_OP_NEW_POLL:
        theForm->mPoll = new TMPoll((unsigned long)inA, 0, false);
        theForm->mPoll->Start();
        return 0;
    case EE_SCREENS_MF_OP_WINDOW_SIZE:
        ioIo->n[0] = inA == 0 ? gGameWindowWidth : gGameWindow->GetWidth();
        ioIo->n[1] = inA == 0 ? gGameWindowHeight : gGameWindow->GetHeight();
        return 0;
    case EE_SCREENS_MF_OP_NEW_SCROLL:
        theForm->mScroll = new EEAScroll;
        theForm->mScroll->mEdges = GERectangle(ioIo->f[0], ioIo->f[1], ioIo->f[2], ioIo->f[3]);
        return 0;
    case EE_SCREENS_MF_OP_NEW_ACTION:
        if (inA == 0)
            theForm->mMouseClick = new EEAMouseClick;
        else if (inA == 1)
            theForm->mLasso = new EEALasso;
        else if (inA == 2)
            theForm->mBuild = new EEABuild;
        else
            theForm->mCalamity = new EEACalamity;
        return 0;
    case EE_SCREENS_MF_OP_WHEEL_ZOOM:
        theForm->mWheelZoom = EECreateScreenEvent((unsigned long)inA);
        theForm->SetMouseWheelMoveEvent(theForm->mWheelZoom);
        return 0;
    case EE_SCREENS_MF_OP_GRAB_SCROLL:
        theForm->mGrabScroll = EECreateGrabScroll((unsigned long)inA);
        theForm->SetMouseMiddleButtonPressedEvent(theForm->mGrabScroll);
        theForm->SetMouseMiddleButtonUnpressedEvent(theForm->mGrabScroll);
        return 0;
    case EE_SCREENS_MF_OP_ADD_HOT_KEY: {
        UIEvent* theEvents[4];
        for (int i = 0; i < 4; i++) {
            uint32_t theCode = (uint32_t)((uint64_t)ioIo->n[i / 2] >> (i % 2 * 32));
            theEvents[i] = theCode == EE_SCREENS_MF_NO_EVENT ? NULL : EECreateScreenEvent(theCode);
        }
        theForm->AddHotKey(inA & 0xffff, inA >> 16 & 0xffff, inA >> 32 & 0xffff, inA >> 48 & 0xffff, theEvents[0],
                           theEvents[1], theEvents[2], theEvents[3], (unsigned long)(uint32_t)inB, (inB >> 32) != 0);
        return 0;
    }
    case EE_SCREENS_MF_OP_UPDATE_JIT:
        if (gUserInterface != NULL)
            gUserInterface->UpdateJITManufacturing();
        return 0;
    case EE_SCREENS_MF_OP_BUILD_TERMINATE:
        inA == 0 ? ((EEUIBuildWall&)theForm->mBuildWall).Terminate() : ((EEUIBuildGate&)theForm->mBuildGate).Terminate();
        return 0;
    case EE_SCREENS_MF_OP_NODE:
        theCall.mNode = gWorld.GetCurrentPlayer()->mTechnologies->Get((long)inA);
        return theCall.mNode != NULL;
    case EE_SCREENS_MF_OP_NODE_CLASS:
        return theCall.mNode != NULL && gUnitClassTable[theCall.mNode->mData->mUnitClass] != NULL;
    case EE_SCREENS_MF_OP_UNIT: {
        RTSUnit* theUnit = gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inA);
        theCall.mUnit = static_cast<EEUnit*>(theUnit);
        return theUnit == NULL ? 0 : theCall.mUnit->mComplexUnit == NULL ? 1 : 2;
    }
    case EE_SCREENS_MF_OP_CAN_PRODUCE:
        return gWorld.GetCurrentPlayer()->CanProduce((long)inA, theCall.mUnit->mComplexUnit->mUnitClass->GetUnitType(), 1,
                                                     true, 3, &theCall.mText);
    case EE_SCREENS_MF_OP_PLAY_SOUND:
        gSoundManager->Play(EESoundObject::Get((unsigned long)inA)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_SCREENS_MF_OP_GHOST:
        EEGhostUnit::Create(theForm->GetMouseLocation(),
                            inA == 0 ? theCall.mNode->mData->mUnitClass
                                     : theCall.mCalamity->mUnitClassIndex,
                            gWorld.GetCurrentPlayer(), 0.0f, false);
        return 0;
    case EE_SCREENS_MF_OP_CHAT_LOG_ERROR: gUserInterface->AddChatLogEntry(theCall.mText, 0); return 0;
    case EE_SCREENS_MF_OP_GATE_BEGIN:
        theForm->mBuildGate.mTechnology = (long)inA;
        theForm->mBuildGate.mWallType = theCall.mNode->GetUnitClass(0);
        theForm->mBuildGate.SetUnitClass(gUnitClassTable[theCall.mNode->mData->mUnitClass]);
        theForm->mBuildGate.Initialize(gWorld.GetCurrentPlayer()->mPlayerID);
        return 0;
    case EE_SCREENS_MF_OP_WALL_BEGIN:
        theForm->mBuildWall.mTechnology = (long)inA;
        theForm->mBuildWall.Initialize(gWorld.GetCurrentPlayer()->mPlayerID);
        return 0;
    case EE_SCREENS_MF_OP_SELECTED_COUNT: return gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits();
    case EE_SCREENS_MF_OP_CASTER: {
        EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
        theCall.mCaster = theGroup->GetNumUnits() != 0 ? theGroup->GetUnit(0) : NULL;
        return theCall.mCaster != NULL;
    }
    case EE_SCREENS_MF_OP_NODE_CALAMITY: return theCall.mNode->mData->mCalamity;
    case EE_SCREENS_MF_OP_CALAMITY_CLASS:
        theCall.mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inA);
        return theCall.mCalamity != NULL;
    case EE_SCREENS_MF_OP_CALAMITY_SET:
        theForm->mCalamity->SetCalamity((long)inA, theCall.mCalamity);
        theForm->mCalamity->SetCasterID(theCall.mCaster->mUnitID);
        return theCall.mCalamity->GetTargetKind();
    case EE_SCREENS_MF_OP_CALAMITY_CAST: theForm->mCalamity->Cast(theForm->GetMouseLocation()); return 0;
    case EE_SCREENS_MF_OP_REFRESH_MAIN_FORM:
        if (gUIManager->mMainGameForm != NULL)
            gUIManager->mMainGameForm->mRefreshGroup = true;
        return 0;
    case EE_SCREENS_MF_OP_MOUSE_FREEZE: gMouse->mIsFrozen = inA != 0; return 0;
    case EE_SCREENS_MF_OP_MOUSE_RESET:
        gMouse->mShiftMode = false;
        gMouse->mPointer = 0;
        gMouse->SetBounds(GERectangle(ioIo->f[0], ioIo->f[1], ioIo->f[2], ioIo->f[3]));
        return 0;
    case EE_SCREENS_MF_OP_SCROLL_EDGES:
        theForm->mScroll->mEdges = GERectangle(ioIo->f[0], ioIo->f[1], ioIo->f[2], ioIo->f[3]);
        return 0;
    case EE_SCREENS_MF_OP_USER_PAUSED: return gWorld.mUserPaused;
    case EE_SCREENS_MF_OP_IS_SERVER: return gCommunications->IsServer() == true;
    case EE_SCREENS_MF_OP_UNPAUSE:
        dynamic_cast<EEServer*>(gCommunications)->UnpauseGame();
        gUserInterface->PostPauseInformation(false);
        return 0;
    case EE_SCREENS_MF_OP_GRAPHICS_UI:
        if (gGraphics != NULL) {
            gGraphics->PostClearRenderObjects();
            gGraphics->PostPushRenderObject(gU56534C);
        }
        return 0;
    case EE_SCREENS_MF_OP_LATENCY: gSoundManager->SetLatency((unsigned long)inA); return 0;
    case EE_SCREENS_MF_OP_MASTER_VOLUME: gSoundManager->SetMasterVolume(gMasterVolume); return 0;
    case EE_SCREENS_MF_OP_MOUSE_POSITION: gMouse->SetPosition(GE2DPoint(ioIo->f[0], ioIo->f[1])); return 0;
    case EE_SCREENS_MF_OP_CANCEL_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_MG_CANCEL_UI_MODE);
        theForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_MF_OP_BLINKING:
        if (inA == 0)
            EESetCivBuilderButtonBlinking(inB != 0);
        else
            EEUISCIFormManager::Get()->SetBlinking(inB != 0);
        return 0;
    case EE_SCREENS_MF_OP_FILL_HACK: theForm->mUnitHack->FillComboBox(); return 0;
    case EE_SCREENS_MF_OP_MAIN_GROUP:
        if (inA == 0)
            static_cast<EEUIFMGMainGroup*>(theForm->mGroups[0])->Open(theForm);
        else
            static_cast<EEUIFMGMainGroup*>(theForm->mGroups[0])->mL20 = 0;
        return 0;
    case EE_SCREENS_MF_OP_HAS_TECHS: return gWorld.GetCurrentPlayer()->mTechnologies != NULL;
    case EE_SCREENS_MF_OP_TEXT_EPOCH_TITLE:
        gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), theCall.mText);
        return 0;
    case EE_SCREENS_MF_OP_UNIT_INFO_BACKGROUND: theForm->mUnitInfoBackground = theForm->GetBackgroundByID(0x64); return 0;
    case EE_SCREENS_MF_OP_POST_GRAPHICS: EEPostGraphicsAll(); gGraphics->PostFadeIn(0); return 0;
    case EE_SCREENS_MF_OP_MAIN_ENTITY_GROUP:
        inB != 0 ? gUIManager->GetMainGameForm()->GetEntityGroup(inA)->Enable()
                 : gUIManager->GetMainGameForm()->GetEntityGroup(inA)->Disable();
        return 0;
    case EE_SCREENS_MF_OP_MAIN_BACKGROUND:
        gUIManager->GetMainGameForm()->GetBackgroundByID((unsigned long)inA)->SetVisibility(inB != 0);
        return 0;
    case EE_SCREENS_MF_OP_MEDIA_OLD: return (theCall.mOldMedia = EEMedia::Get(theForm->mMediaID)) != NULL;
    case EE_SCREENS_MF_OP_PLAYER_MEDIA_ID: return gWorld.GetCurrentPlayer()->mTechnologies->mMediaID;
    case EE_SCREENS_MF_OP_USING_CARBON: return gWorld.mUsingCarbon;
    case EE_SCREENS_MF_OP_MEDIA: return (theCall.mMedia = EEMedia::Get((long)inA)) != NULL;
    case EE_SCREENS_MF_OP_MEDIA_LOAD: inA != 0 ? theCall.mMedia->LoadMedia() : theCall.mOldMedia->UnloadMedia(); return 0;
    case EE_SCREENS_MF_OP_BG_TEXTURE:
        static_cast<UIBitmapBackground*>(theForm->GetBackgroundByID((unsigned long)inA))
            ->SetBitmapTexture(static_cast<EEGraphicObject*>(theCall.mMedia)->mTextures[inB]);
        return 0;
    case EE_SCREENS_MF_OP_POST_SHOW_DIALOG: gPhysics->PostShowDialogMessage(); return 0;
    case EE_SCREENS_MF_OP_SAVED_GROUP_REFRESH:
    case EE_SCREENS_MF_OP_ICON_BAR_UPDATE: {
        UIControl* theControl = theForm->GetControl(inOp == EE_SCREENS_MF_OP_ICON_BAR_UPDATE ? kMainGameUnitIconBar : (unsigned long)inA);
        if (theControl == NULL)
            return 0;
        if (inOp == EE_SCREENS_MF_OP_SAVED_GROUP_REFRESH)
            dynamic_cast<EEUIBSavedGroup*>(theControl)->Refresh();
        else
            dynamic_cast<EEUICUnitIconBar*>(theControl)->Update();
        return 1;
    }
    case EE_SCREENS_MF_OP_DRAW_BACKGROUND: {
        UIBackground* theBackground = theForm->GetBackgroundByID((unsigned long)inA);
        theBackground->DrawBackground(theBackground->GetRectangle());
        return 0;
    }
    case EE_SCREENS_MF_OP_MAIN_UI_STATE: return gUIManager->mMainGameForm->mUIState;
    case EE_SCREENS_MF_OP_UNIT_SELECTED: return gUnitSelected;
    default:
        if (inOp >= EE_SCREENS_MF_OP_SB)
            return EEStatusButtonOp(theCall.mButton, theCall.mFirst, theCall.mText, theCall.mOut, inOp - EE_SCREENS_MF_OP_SB,
                                    inA, inB, ioIo->n[0]);
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
        return EEIsSeenBy(theTarget, theRead.mPlayer, true);
    case EE_SCREENS_CURSOR_OP_T_DISABLED:
        return theTarget->mTileNode != NULL && theTarget->mComplexUnit->mIsDisabled &&
               !theTarget->mComplexUnit->IsProtected();
    case EE_SCREENS_CURSOR_OP_T_TARGETABLE:
        return EEUUGetIsUnitClassTargetable(NULL, theTarget->mUnitClass);
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
    EEUIFMGFormCall theCall(gUIManager->mMainGameForm);
    theCall.mWall = this;
    ee_screens_wall_update(&theCall, reinterpret_cast<EeScreensBuildWallFields*>(&this->mState), inLocation.GetX(),
                           inLocation.GetY());
}

// Tracks the mouse in gate placement (ee_screens mouse_modes.rs gate_update): a gate ghost on the wall under the
// mouse, built on a left click, cancelled on a right click
void EEUIBuildGate::Update(const GE2DPoint& inLocation)
{
    EEUIFMGFormCall theCall(gUIManager->mMainGameForm);
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

// ICF-folded empty members (no address of their own: 0x157269 / 0xe123d are their twins)
void EEUIFMainGame::FnE123D()
{
}
