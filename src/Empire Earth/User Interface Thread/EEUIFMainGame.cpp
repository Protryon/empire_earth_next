// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.cpp (__FILE__ 0x262e73). The in-game form: 0x26272b..0x26783d
// (the default player names' init chain first). Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
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
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
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
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "ee/EETileCoordinate.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEAScroll.h"
#include "ee/EEAMouseClick.h"
#include "ee/EEALasso.h"
#include "ee/EEABuild.h"
#include "ee/EEACalamity.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCVolcano.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // EE_SCREENS_MG_*, EE_SCREENS_CAM_*: the main game form's events (main_game.rs, camera.rs)
#include "ee/EEUICChatLine.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUIFMGErrorLog.h"
#include "ee/EEUIFMGUnitHack.h"
#include "lle/UIFormManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEStringIDs.h"
// the status buttons' availability tests (EEUIFMGStatusButton.cpp's range) (provisional names)
long EEModeAtomicBomberRally(void* inLocation, void* inForm);
long EEModeParatrooperRally(void* inLocation, void* inForm);
long EEModeBomberRally(void* inLocation, void* inForm);
long EEModeFighterRally(void* inLocation, void* inForm);
long EEModeTimedAtomicBomberRally(void* inLocation, void* inForm);
long EEModeTimedBomberRally(void* inLocation, void* inForm);
long EEModeTimedFighterRally(void* inLocation, void* inForm);
long EEModeTimedRally(void* inLocation, void* inForm);
long EEModeAttackGround(void* inLocation, void* inForm);
long EEModeBuild(void* inLocation, void* inForm);
long EEModeGate(void* inLocation, void* inForm);
long EEModeWall(void* inLocation, void* inForm);
long EEModeFlare(void* inLocation, void* inForm);
long EEModeGarrison(void* inLocation, void* inForm);
long EEModeUnload(void* inLocation, void* inForm);
long EEModeHeal(void* inLocation, void* inForm);
long EEModeConvert(void* inLocation, void* inForm);
long EEModeCalamity(void* inLocation, void* inForm);
long EEModeNormal(void* inLocation, void* inForm);
long EEModeRally(void* inLocation, void* inForm);
long EEModeCamera(void* inLocation, void* inForm);
bool EEModeKeyboardCinematic(void* inForm);
bool EEModeKeyboardNone(void* inForm);  // `return false` (ICF-folded)
bool EEFn27C0B4B(void* inForm);   // another `return false` (the ctor reloads its address: a different
                                             // function, ICF-folded into the same body)
long EEModeMouseNone(void* inLocation, void* inForm);  // `return 0` (ICF-folded)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);  // 0x23eb4c
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
#include "ee/EESMDialogMessage.h"

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
};   // 0x564608 (provisional name)
// SYNTHETIC: EXE 0x66272b   (wrapper)
// SYNTHETIC: EXE 0x662735   (ctor)
// SYNTHETIC: EXE 0x6628be   (registrar)
// SYNTHETIC: EXE 0x6628ca   (dtor)

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
    case EE_SCREENS_UH_OP_COMBO_SELECT_FIRST:
        theObjects->SetCurrentSelection(0);
        return 0;
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
            new EEMHackANewUnit(theType, GE2DPoint(ioPoint[0], ioPoint[1]), (unsigned char)inB), kSlotNone);
        return 0;
    }
    case EE_SCREENS_UH_OP_SEND_LOAD:
        gCommunications->ProcessLocalUserInput(new EEMLoadUnitData(inA, true), kSlotNone);
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

// FUNCTION: EXE 0x662e73
// Creates the main game form: its panel groups, mouse actions (scroll, zoom, click, lasso, build, calamity),
// the mouse and keyboard tests of every interface mode, the unit hack and the error strings
EEUIFMainGame::EEUIFMainGame(UIManager* inManager, const UIDbDataForms& inData) : UIForm(inManager, inData, true)
{
    unsigned long i;

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mErrorCriticalSection) == keseOK, 633);
    this->mInfoBlinking = false;
    this->mIsButtonBlinking = false;
    this->mDrawToolTipShadow = false;
    this->mIsCursorLocked = false;
    this->mUIState = 0;
    this->mEntityGroup = 0;
    this->mCurrentGroup = 0;
    for (i = 0; i < 6; i++)
        this->CreateGroup(i);
    this->mUnitHack = new EEUIFMGUnitHack(this);
    this->mElapsed = 0;
    this->mPoll = new TMPoll(200, 0, false);
    this->mPoll->Start();
    this->mScroll = new EEAScroll;
    this->mScroll->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindowWidth - 1.0f, (float)gGameWindowHeight - 1.0f);
    this->mMouseClick = new EEAMouseClick;
    this->mLasso = new EEALasso;
    this->mBuild = new EEABuild;
    this->mCalamity = new EEACalamity;
    this->mMediaID = -1;
    this->mEntityGroupSet = false;
    this->mStringsChanged = false;
    this->mClockMode = 0;
    this->GetEntityGroup(0)->Enable();
    this->SetEntityGroup(1);
    this->GetEntityGroup(9)->Disable();
    this->GetEntityGroup(8)->Disable();
    this->mMouseTests[0] = EEModeNormal;
    this->mMouseTests[9] = EEModeBomberRally;
    this->mMouseTests[8] = EEModeFighterRally;
    this->mMouseTests[10] = EEModeAtomicBomberRally;
    this->mMouseTests[1] = EEModeBuild;
    this->mMouseTests[2] = EEModeRally;
    this->mMouseTests[3] = EEModeUnload;
    this->mMouseTests[7] = EEModeGarrison;
    this->mMouseTests[5] = EEModeCalamity;
    this->mMouseTests[6] = EEModeWall;
    this->mMouseTests[11] = EEModeGate;
    this->mMouseTests[12] = EEModeTimedAtomicBomberRally;
    this->mMouseTests[13] = EEModeTimedBomberRally;
    this->mMouseTests[14] = EEModeTimedFighterRally;
    this->mMouseTests[15] = EEModeTimedRally;
    this->mMouseTests[4] = EEModeCamera;
    this->mMouseTests[16] = EEModeHeal;
    this->mMouseTests[17] = EEModeMouseNone;
    this->mMouseTests[18] = EEModeFlare;
    this->mMouseTests[19] = EEModeAttackGround;
    this->mMouseTests[20] = EEModeParatrooperRally;
    this->mMouseTests[21] = EEModeConvert;
    this->mKeyboardTests[0] = EEModeKeyboardNone;
    this->mKeyboardTests[9] = EEModeKeyboardNone;
    this->mKeyboardTests[8] = EEModeKeyboardNone;
    this->mKeyboardTests[10] = EEModeKeyboardNone;
    this->mKeyboardTests[1] = EEModeKeyboardNone;
    this->mKeyboardTests[2] = EEModeKeyboardNone;
    this->mKeyboardTests[3] = EEModeKeyboardNone;
    this->mKeyboardTests[7] = EEModeKeyboardNone;
    this->mKeyboardTests[5] = EEModeKeyboardNone;
    this->mKeyboardTests[6] = EEModeKeyboardNone;
    this->mKeyboardTests[11] = EEModeKeyboardNone;
    this->mKeyboardTests[12] = EEModeKeyboardNone;
    this->mKeyboardTests[13] = EEModeKeyboardNone;
    this->mKeyboardTests[14] = EEModeKeyboardNone;
    this->mKeyboardTests[15] = EEModeKeyboardNone;
    this->mKeyboardTests[4] = EEFn27C0B4B;
    this->mKeyboardTests[16] = EEModeKeyboardNone;
    this->mKeyboardTests[17] = EEModeKeyboardCinematic;
    this->mKeyboardTests[18] = EEModeKeyboardNone;
    this->mKeyboardTests[19] = EEModeKeyboardNone;
    this->mKeyboardTests[20] = EEModeKeyboardNone;
    this->mKeyboardTests[21] = EEModeKeyboardNone;
    this->mWheelZoom = EECreateScreenEvent(EE_SCREENS_CAM_WHEEL_ZOOM);
    this->SetMouseWheelMoveEvent(this->mWheelZoom);
    this->mGrabScroll = EECreateGrabScroll(0);
    this->SetMouseMiddleButtonPressedEvent(this->mGrabScroll);
    this->SetMouseMiddleButtonUnpressedEvent(this->mGrabScroll);
    for (i = 0; i < 10; i++) {
        this->AddHotKey(i + 2, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_SELECT_GROUP + i), NULL, NULL, EECreateScreenEvent(EE_SCREENS_HK_SELECT_GROUP_ADD + i),
                  0, false);
        this->AddHotKey(i + 2, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_ADD_TO_GROUP + i), NULL, NULL, NULL, 0, false);
        this->AddHotKey(i + 2, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_CREATE_GROUP + i), NULL, NULL, NULL, 0, false);
        this->AddHotKey(i + 2, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_SELECT_GROUP_ADD + i), NULL, NULL, NULL, 0, false);
    }
    this->AddHotKey(kKeyS, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_SCOUT), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyD, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_STAND_GROUND), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyG, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_GUARD), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyA, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyB, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 0), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyA, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 1), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 2), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyD, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 3), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyS, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 4), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyW, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 5), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyPrior, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_PREVIOUS_MESSAGES), NULL, NULL, NULL, 0, true);
    this->AddHotKey(kKeyB, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 0), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyA, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 1), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 2), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyD, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 3), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyS, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 4), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyW, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_AIRCRAFT + 5), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyUp, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_SCROLL_UP), NULL, NULL, 0, true);
    this->AddHotKey(kKeyDown, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_SCROLL_DOWN), NULL, NULL, 0, true);
    this->AddHotKey(kKeyLeft, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_SCROLL_LEFT), NULL, NULL, 0, true);
    this->AddHotKey(kKeyRight, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_SCROLL_RIGHT), NULL, NULL, 0, true);
    this->AddHotKey(kKeyShift, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_ENTER_SHIFT), NULL, EECreateScreenEvent(EE_SCREENS_MG_EXIT_SHIFT), NULL, 0, true);
    this->AddHotKey(kKeyDelete, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_KILL), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyDelete, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_KILL_ALL), NULL, NULL, NULL, 0, false);
    // New: Backspace deletes too (a Mac keyboard's "delete" key is Backspace; it has no forward Delete key)
    this->AddHotKey(kKeyBack, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_KILL), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyBack, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_KILL_ALL), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyAdd, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_FASTER), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeySubtract, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_SLOWER), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyEscape, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_CANCEL_UPDATE_STATE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyReturn, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_CHAT), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyNumpadEnter, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_CHAT), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyRBracket, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_ZOOM_IN), NULL, NULL, 0, true);
    this->AddHotKey(kKeyLBracket, kKeyNone, kKeyNone, kKeyNone, NULL, EECreateScreenEvent(EE_SCREENS_CAM_ZOOM_OUT), NULL, NULL, 0, true);
    this->AddHotKey(kKeyComma, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(0, false, true, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyComma, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(0, false, true, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyComma, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(0, false, true, true)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyGrave, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_CAM_DISABLE_MOUSE_MOVE), EECreateScreenEvent(EE_SCREENS_CAM_ROTATE), EECreateScreenEvent(EE_SCREENS_CAM_ENABLE_MOUSE_MOVE), NULL, 0, true);
    this->AddHotKey(kKeyPeriod, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_FOLLOW), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyA, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeArcheryRange, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyB, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeBarracks, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyC, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeSiegeFactory, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF, kKeyAlt, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_FLARE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeTankFactory, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyG, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeGranary, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyH, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeTownCenter, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyI, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeMissileBase, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyN, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeSettlement, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyQ, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeAirport, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyR, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCyberFactory, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyS, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeStable, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyV, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeNavyYard, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyX, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCyberLaboratory, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyY, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeTemple, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyZ, kKeyControl, kKeyAlt, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_ATTACK), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyZ, kKeyControl, kKeyShift, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_ATTACK_ALLIES), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyTab, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCitizen, true, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyTab, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCitizen, true, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyTab, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCitizen, true, false, true)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeySpace, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_HK_PLAYER_EVENT), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF1, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_ENTER_EDITOR), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF2, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_PERSPECTIVE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF3, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_PAUSE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyBackslash, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeCpMissileBase, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeySlash, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypePriestTower, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyM, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeMarketCP, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyU, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeUniversity, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyZ, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeHospital, false, false, false)), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF4, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_QUICK_SAVE), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF4, kKeyShift, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_QUICK_LOAD), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF4, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_AUTO_LOAD), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF5, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_TOGGLE_HIDDEN), NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyF10, kKeyNone, kKeyNone, kKeyNone, NULL, NULL, EECreateScreenEvent(EE_SCREENS_MG_DISPLAY_OPTIONS), NULL, 0, false);
    this->AddHotKey(kKeyF11, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(EE_SCREENS_MG_TOGGLE_CLOCK), NULL, NULL, NULL, 0, false);
    this->SetHotKeyMode(0);
    this->mB5F0 = false;
    this->mRefreshGroup = true;
}

// FUNCTION: EXE 0x66443e
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664469
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664494
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6644bf
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6644ea
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664515
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664540
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x66456b
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664596
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6645c1
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6645ec
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664617
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664642
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x66466d
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664698
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6646c3
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x6646ee
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664719
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x664744
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x66476f
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// FUNCTION: EXE 0x66479a
// Destroys a main game form event (scalar deleting destructor, emitted after its first user, the form's ctor)

// SYNTHETIC: EXE 0x6647c5
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFMainGame+0x0)

// FUNCTION: EXE 0x6647e1
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

// FUNCTION: EXE 0x6648d4
// Activates the form: frees the mouse, resumes a game paused only by leaving it, refreshes the saved groups
void EEUIFMainGame::Activate()
{
    UWideString theString;

    gMouse->mIsFrozen = false;
    this->mScroll->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f,
                                  (float)gGameWindow->GetHeight() - 1.0f);
    UIForm::Activate();
    if (!gWorld.mUserPaused && gCommunications->GetIsGamePaused() == true && gCommunications->IsServer() == true) {
        dynamic_cast<EEServer*>(gCommunications)->UnpauseGame();
        gUserInterface->PostPauseInformation(false);
    }
    this->mIsCursorLocked = false;
    this->RefreshSavedGroups();
}

// FUNCTION: EXE 0x6649a2
// Shows a chat message in the next free chat line (scrolling the lines up when full), wrapping the rest of a
// long message onto further lines
void EEUIFMainGame::DisplayChatLine(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                    unsigned long inFlags)
{
    UWideString theRest;
    UWideString theName(inName);
    unsigned long theShown;
    long i = 0x6a;
    bool theFree;

    if (this->GetControl(kMainGameChatLine7)->GetDisplayMode() == kDisplayModeHidden) {
        for (; i >= 0x65; i--) {
            theFree = this->GetControl(i)->GetDisplayMode() == kDisplayModeHidden;
            if (!theFree) {
                theShown = static_cast<EEUICChatLine*>(this->GetControl(i + 1))->Display(inColor, inName, inText, inFlags);
                break;
            }
        }
        if (theFree != true)
            goto theCheck;
        theShown = static_cast<EEUICChatLine*>(this->GetControl(kMainGameChatLine1))->Display(inColor, inName, inText, inFlags);
    } else {
        for (; i >= 0x65; i--) {
            theFree = this->GetControl(i)->GetDisplayMode() == kDisplayModeHidden;
            if (theFree == true)
                break;
        }
        if (theFree == true) {
            for (; i < 0x6b; i++)
                (static_cast<EEUICChatLine*>(this->GetControl(i)))->Copy(*static_cast<EEUICChatLine*>(this->GetControl(i + 1)));
        }
        if (!theFree) {
            for (i = 0x65; i < 0x6b; i++)
                (static_cast<EEUICChatLine*>(this->GetControl(i)))->Copy(*static_cast<EEUICChatLine*>(this->GetControl(i + 1)));
        }
        theShown = static_cast<EEUICChatLine*>(this->GetControl(i))->Display(inColor, inName, inText, inFlags);
    }
theCheck:
    if (theShown < inText.GetLength()) {
        const unsigned char* theNext = PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), theShown);
        if (PlatMbsNBCmp(theNext, (const unsigned char*)"\n", 1) == 0)
            theNext = PlatMbsInc(theNext);
        theRest = reinterpret_cast<const char*>(theNext);
        theName = "";
        this->DisplayChatLine(inColor, theName, theRest, inFlags);
    }
}

// FUNCTION: EXE 0x664b1f
// Builds a gate at a location facing a direction
void EEUIFMainGame::BuildGate(const GE2DPoint& inLocation, long inDirection)
{
    this->mBuild->BuildGate(inLocation, inDirection);
}

// FUNCTION: EXE 0x664b35
// Builds a wall between two points
void EEUIFMainGame::BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd)
{
    this->mBuild->BuildWall(inStart, inEnd);
}

// FUNCTION: EXE 0x664b4b
// Loads the interface background media of the local player's epoch (the carbon one when carbon is used),
// unloading the previous one, and redraws the form
void EEUIFMainGame::UpdateBackgroundMedia(bool inOnlyIfVisible)
{
    EEMedia* theOld = EEMedia::Get(this->mMediaID);
    unsigned long theID = 0x67;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();

    if (thePlayer != NULL && thePlayer->mTechnologies != NULL &&
        (!inOnlyIfVisible || this->GetManager()->IsFormVisible(this))) {
        this->mMediaID = gWorld.GetCurrentPlayer()->mTechnologies->mMediaID;
        if (gWorld.mUsingCarbon)
            this->mMediaID = 0x377;
        EEMedia* theMedia = EEMedia::Get(this->mMediaID);
        if (theMedia != NULL) {
            unsigned long i;
            theMedia->LoadMedia();
            if (theOld != NULL)
                theOld->UnloadMedia();
            for (i = 4; i != 0; i--) {
                static_cast<UIBitmapBackground*>(this->GetBackgroundByID(theID))->SetBitmapTexture(static_cast<EEGraphicObject*>(theMedia)->mTextures[8]);
                theID++;
            }
            static_cast<UIBitmapBackground*>(this->GetBackgroundByID(0x64))->SetBitmapTexture(static_cast<EEGraphicObject*>(theMedia)->mTextures[8]);
            for (i = 0x12; i != 0; i--) {
                static_cast<UIBitmapBackground*>(this->GetBackgroundByID(theID))->SetBitmapTexture(static_cast<EEGraphicObject*>(theMedia)->mTextures[4]);
                theID++;
            }
        }
        if (this->GetManager()->GetCurrentForm() == this) {
            this->SetAllControlsDirty();
            this->GetManager()->AddDirtyRectangle(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(),
                                                    (float)gGameWindow->GetHeight()), this);
        }
    } else {
        if (theOld != NULL)
            theOld->UnloadMedia();
        this->mMediaID = -1;
    }
}

// FUNCTION: EXE 0x664cd5
// Shows a text in the main game form's title label
void EEUIFMainGame::SetStatusText(const UWideString& inText)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetControl(kMainGameTitle));

    if (theLabel != NULL)
        theLabel->SetText(inText);
}

// FUNCTION: EXE 0x664cf1
// Closes the form: hides the messages, gives the graphics thread the UI render object back and unloads the
// background media
void EEUIFMainGame::Close()
{
    this->HideMessages();
    if (gGraphics != NULL) {
        gGraphics->PostClearRenderObjects();
        gGraphics->PostPushRenderObject(gU56534C);
    }
    this->UpdateBackgroundMedia(false);
    gSoundManager->SetLatency(200);
    UIForm::Close();
}

// FUNCTION: EXE 0x664d40
// Deactivates the form: frees and resets the mouse and leaves any interface mode
void EEUIFMainGame::Deactivate()
{
    gMouse->mIsFrozen = false;
    gMouse->mShiftMode = false;
    gMouse->mPointer = 0;
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    this->mIsCursorLocked = true;
    this->SetUIState(0);
    UIForm::Deactivate();
}

// FUNCTION: EXE 0x664dc4
// Snapshots the form for drawing: selection state, error log, status refresh each poll, the panel group, edge
// scrolling and the gate or wall preview of the current mode
long EEUIFMainGame::CopyData()
{
    GERectangle theView = gViewport.GetViewRectangle();
    unsigned long theElapsed;
    long theDirection;
    EETile* theOther;

    this->mErrorCriticalSection->Enter();
    if (gWorld.GetCurrentPlayer() != NULL &&
        ((gWorld.GetCurrentPlayer()->mSelectedGroup != NULL &&
          gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit() != NULL) ||
         gWorld.GetCurrentPlayer()->mSelectedViewedUnit != NULL))
        gUnitSelected = true;
    else
        gUnitSelected = false;
    if (this->mStringsChanged) {
        unsigned long i;
        for (i = 0; i < 0x24; i++) {
            EEUIFMGErrorLog* theLog = static_cast<EEUIFMGErrorLog*>(this->GetControl(kMainGameErrorLog));
            theLog->SetLine(i, this->mStrings[i]);
        }
        this->mStringsChanged = false;
    }
    this->mErrorCriticalSection->Leave();
    if (this->GetManager()->GetCurrentForm() != this) {
        theView.SetRight(theView.GetRight() - 1.0f);
        theView.SetBottom(theView.GetBottom() - 1.0f);
        this->GetManager()->AddDirtyRectangle(theView, this);
    }
    theElapsed = this->mPoll->Poll();
    if (theElapsed > 0) {
        this->mElapsed += theElapsed;
        this->RefreshStatus();
    }
    if (this->mRefreshGroup) {
        this->mGroups[this->mCurrentGroup]->Reset();
        this->mGroups[this->mCurrentGroup]->Refresh();
        this->mRefreshGroup = false;
    } else if (this->mGroups[this->mCurrentGroup]->WantsRefresh() == true || theElapsed > 0) {
        this->mGroups[this->mCurrentGroup]->Refresh();
    }
    if (this->GetMouseCaptured() == NULL && this->mUIState != 0x11)
        this->mScroll->Update(gMouse->mPosition);
    switch (this->mUIState) {
    case 0xb: {
        EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
        long theX;
        long theY;
        if (theUnit != NULL && theUnit->mUnitClass->mIsWall) {
            theX = theUnit->mTileX;
            theY = theUnit->mTileY;
        } else {
            EEPointUnitOrTileXY(gMouse->mPosition, theOther, theX, theY, true);
        }
        if (theUnit != NULL)
            theUnit->Dereference();
        theDirection = this->mBuildGate.mDirection;
        EETileCoordinate theTile;
        theTile.mX = theX;
        theTile.mY = theY;
        if (EEFindGateWall(theTile, theDirection)) {
            EEGhostUnit::CreateGate(0.0f, this->mBuildGate.mUnitClass, gWorld.mPlayers[this->mBuildGate.mPlayer]);
            EEGhostUnit::PreviewGate(gMouse->mPosition, theDirection);
            this->mBuildGate.mDirection = theDirection;
        } else {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
        }
        break;
    }
    case 6: {
        std::vector<EETileCoordinate> theTiles;
        std::vector<GE2DPoint> thePoints;
        GE2DPoint theEnd;
        EETile* theUnit;
        long theX;
        long theY;
        unsigned long i;
        EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
        theEnd.SetX((float)theX);
        theEnd.SetY((float)theY);
        if (this->mBuildWall.mState == 0) {
            EETileCoordinate theTile;
            theEnd.SetX(theEnd.GetX() + 0.5f);
            theEnd.SetY(theEnd.GetY() + 0.5f);
            theTiles.clear();
            theTile.mX = (long)theEnd.GetX();
            theTile.mY = (long)theEnd.GetY();
            theTiles.push_back(theTile);
            gWorld.mGhostUnitsLock->Enter();
            EEPreviewWall(theTiles, this->mBuildWall.mTechnology, this->mBuildWall.mPlayer);
            gWorld.mGhostUnitsLock->Leave();
        }
        if (this->mBuildWall.mState == 1) {
            theEnd.SetX(theEnd.GetX() + 0.5f);
            theEnd.SetY(theEnd.GetY() + 0.5f);
            theTiles.clear();
            this->mBuildWall.mStart.InterpolateTo(theEnd, 1.0f, thePoints);
            for (i = 0; i < thePoints.size(); i++) {
                EETileCoordinate theTile;
                theTile.mX = (long)thePoints[i].GetX();
                theTile.mY = (long)thePoints[i].GetY();
                theTiles.push_back(theTile);
            }
            gWorld.mGhostUnitsLock->Enter();
            EEPreviewWall(theTiles, this->mBuildWall.mTechnology, this->mBuildWall.mPlayer);
            gWorld.mGhostUnitsLock->Leave();
        }
        break;
    }
    case 5:
        EEPreviewPlacement(gMouse->mPosition);
        break;
    default:   // every state but 6 and 0xb (orig 264f61..264f74: sub 5 je / jne -> 0x26520f)
        EEPreviewPlacement(gMouse->mPosition);
        break;
    }
    return UIForm::CopyData();
}

// FUNCTION: EXE 0x66522b
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

// FUNCTION: EXE 0x665317
// Shows a chat message when the form has its chat lines
void EEUIFMainGame::DisplayChat(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                unsigned long inFlags, unsigned long inArg5)
{
    if (this->GetControl(kMainGameChatLine7) != NULL)
        this->DisplayChatLine(inColor, inName, inText, inFlags);
}

// FUNCTION: EXE 0x66533e
// Shows an information message: scrolling text, the speaker's portrait and its sound; hides the messages
// without a text control
void EEUIFMainGame::ShowInformation(EEUIFMGInformation* inInformation)
{
    const EESMDialogMessage* theMessage = reinterpret_cast<const EESMDialogMessage*>(inInformation);
    UITLScrolling* theText = static_cast<UITLScrolling*>(this->GetControl(kMainGameDialogMessage));
    UIBitmap* thePortrait = static_cast<UIBitmap*>(this->GetControl(kMainGameDialogPortrait));
    unsigned long theSampleID;
    unsigned long theLength;

    if (theText != NULL) {
        theText->SetText(theMessage->mText);
        theText->SetDisplayMode(kDisplayModeShown);
        if (thePortrait != NULL) {
            EEButtonObject* theButton;
            // (a class past the table is no portrait)
            if (theMessage->mUnitType < (unsigned long)EEUnitClassCount() && gUnitClassTable[theMessage->mUnitType] != NULL &&
                (theButton = gWorld.GetCurrentPlayer()->mUnitClassUpgrades[theMessage->mUnitType]->mButton) != NULL) {
                GEBitmap* theBitmap = new GEBitmap;
                theButton->SetFaceBitmap(theBitmap, thePortrait->GetRectangle(), thePortrait->GetScreenIndex());
                thePortrait->SetBitmap(theBitmap);
                thePortrait->SetDisplayMode(kDisplayModeShown);
                thePortrait->SetBgOpacity(thePortrait->GetBgOpacity());
                theBitmap->Dereference();
            } else {
                thePortrait->SetDisplayMode(kDisplayModeHidden);
            }
        }
        EELoadSound("Sounds\\" + UString((const char*)theMessage->mSound), 0x31, theSampleID, theLength, 1);
        if (!UIsNone32(theLength))
            theText->ScrollText(gSoundManager->GetTotalPlayTimeInMilli(theSampleID) + 300, theMessage->mDuration);
        else
            theText->ScrollText(theMessage->mDuration, 0);
        this->SetChatTop(UMFloatToLong(theText->GetRectangle().GetBottom() - theText->GetRectangle().mTop + 5.0f));
    } else
        this->HideMessages();
}

// FUNCTION: EXE 0x66550c
// Draws the form's fixed background panels (the unit panels only with a selection)
long EEUIFMainGame::DrawPolygons()
{
    if (gUIManager->mMainGameForm->mUIState != 0x11) {
        this->GetBackgroundByID(0x74)->DrawBackground(this->GetBackgroundByID(0x74)->GetRectangle());
        this->GetBackgroundByID(0x73)->DrawBackground(this->GetBackgroundByID(0x73)->GetRectangle());
        this->GetBackgroundByID(0x72)->DrawBackground(this->GetBackgroundByID(0x72)->GetRectangle());
        if (gUnitSelected) {
            this->GetBackgroundByID(0x6f)->DrawBackground(this->GetBackgroundByID(0x6f)->GetRectangle());
            this->GetBackgroundByID(0x70)->DrawBackground(this->GetBackgroundByID(0x70)->GetRectangle());
            this->GetBackgroundByID(0x71)->DrawBackground(this->GetBackgroundByID(0x71)->GetRectangle());
        }
    }
    return UIForm::DrawPolygons();
}

// FUNCTION: EXE 0x6655da
// Switches the bottom panel to another entity group and selects its panel group
void EEUIFMainGame::SetEntityGroup(long inGroup)
{
    if (this->mEntityGroupSet)
        this->GetEntityGroup(this->mEntityGroup)->Disable();
    this->GetEntityGroup(inGroup)->Enable();
    this->mEntityGroupSet = true;
    this->mEntityGroup = inGroup;
    this->mGroups[this->mCurrentGroup]->Select();
    this->SetUIState(0);
    this->GetManager()->AddDirtyRectangle(GERectangle(0.0f, gViewport.GetViewRectangle().GetBottom(),
                                            (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()), this);
    this->SetAllControlsDirty();
}

// FUNCTION: EXE 0x66568e
// Shows every background panel except the unit panels and enables the first two entity groups
void EEUIFMainGame::ShowAllBackgrounds()
{
    unsigned long i;

    for (i = 0x67; i < 0x7d; i++) {
        if (i != 0x64 && i != 0x6b && i != 0x6f && i != 0x70 && i != 0x71)
            this->GetBackgroundByID(i)->SetVisibility(true);
    }
    this->GetBackgroundByID(0x65)->SetVisibility(true);
    this->GetBackgroundByID(0x90)->SetVisibility(true);
    this->GetBackgroundByID(0x8f)->SetVisibility(true);
    this->GetBackgroundByID(0x91)->SetVisibility(true);
    for (i = 0; i < 2; i++)
        this->GetEntityGroup(i)->Enable();
    this->GetManager()->RedrawAll();
}

// FUNCTION: EXE 0x66572f
// Shows the victory or defeat text
void EEUIFMainGame::SetPauseText(bool inPaused)
{
    UWideString theText;
    UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetControl(kMainGameWinLose));

    if (inPaused)
        gResource->GetString(IDS_YOU_ARE_VICTORIOUS, theText);
    else
        gResource->GetString(IDS_YOU_HAVE_BEEN_DEFEATED, theText);
    theLabel->SetText(theText);
    theLabel->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x665a06
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

// FUNCTION: EXE 0x665ff6
// Stores an error log line for the next snapshot
void EEUIFMainGame::SetErrorString(const UWideString& inText, unsigned long inIndex)
{
    this->mErrorCriticalSection->Enter();
    this->mStrings[inIndex] = inText;
    this->mStringsChanged = true;
    this->mErrorCriticalSection->Leave();
}

// FUNCTION: EXE 0x666038
// Opens the form: sound latency and volume, centred mouse, empty error log, the cancel-mode event, the
// backgrounds and the civilization builder button's blinking
void EEUIFMainGame::Open()
{
    UWideString theText;
    unsigned long i;
    UIEvent* theEvent;

    gSoundManager->SetLatency(100);
    gSoundManager->SetMasterVolume(gMasterVolume);
    gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() / 2), (float)(gGameWindow->GetHeight() / 2)));
    UIForm::Open();
    theText = "";
    for (i = 0; i < 0x24; i++)
        (static_cast<UILBString*>(this->GetControl(kMainGameErrorLog)))->AppendLine(theText);
    theEvent = EECreateScreenEvent(EE_SCREENS_MG_CANCEL_UI_MODE);
    this->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    this->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, theEvent);
    theEvent->Dereference();
    EESetCivBuilderButtonBlinking(this->mIsButtonBlinking);
    EEUISCIFormManager::Get()->SetBlinking(this->mInfoBlinking);
    this->mUnitHack->FillComboBox();
    static_cast<EEUIFMGMainGroup*>(this->mGroups[0])->Open(this);
    this->UpdateBackgroundMedia(false);
    this->FnE123D();
    this->GetControl(kMainGameMiniMap)->SetDisplayMode(kDisplayModeShown);
    this->mCurrentGroup = 0;
    this->SetEntityGroup(1);
    this->GetEntityGroup(3)->Enable();
    this->FnE123D();
    if (gWorld.GetCurrentPlayer() != NULL && gWorld.GetCurrentPlayer()->mTechnologies != NULL)
        gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), theText);
    else
        theText = "";
    this->SetStatusText(theText);
    static_cast<EEUIFMGMainGroup*>(this->mGroups[0])->mL20 = 0;
    this->mUnitInfoBackground = this->GetBackgroundByID(0x64);
    EEPostGraphicsAll();
    gGraphics->PostFadeIn(0);
    gUIManager->GetMainGameForm()->GetEntityGroup(9)->Disable();
    gUIManager->GetMainGameForm()->GetBackgroundByID(0x97)->SetVisibility(false);
    gUIManager->GetMainGameForm()->GetEntityGroup(8)->Disable();
    gUIManager->GetMainGameForm()->GetBackgroundByID(0x98)->SetVisibility(false);
}

// FUNCTION: EXE 0x6662ac
// Destroys a cancel-mode event (scalar deleting destructor)
// (an inline COMDAT after its first user, Open)

// FUNCTION: EXE 0x6662d7
// Pauses the scrolling information message
void EEUIFMainGame::PauseInformation()
{
    UITLScrolling* theText = static_cast<UITLScrolling*>(this->GetControl(kMainGameDialogMessage));

    if (theText != NULL && theText->GetDisplayMode() == kDisplayModeShown)
        theText->PauseScrolling();
}

// FUNCTION: EXE 0x6662f4
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

// FUNCTION: EXE 0x66633a
// Writes the information and civilization builder blinking flags to the saved game
long EEUIFMainGame::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mInfoBlinking);

    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsButtonBlinking);
    return theResult;
}

// FUNCTION: EXE 0x666367
// Resumes the scrolling information message
void EEUIFMainGame::ResumeInformation()
{
    UITLScrolling* theText = static_cast<UITLScrolling*>(this->GetControl(kMainGameDialogMessage));

    if (theText != NULL && theText->GetDisplayMode() == kDisplayModeShown)
        theText->ResumeScrolling();
}

// FUNCTION: EXE 0x666384
// Starts placing a building from a unit's tech when it can be produced; otherwise beeps and explains in
// the chat log
void EEUIFMainGame::BuildUnit(unsigned long inUnitID, long inTechnology)
{
    UWideString theError;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(inTechnology);
    RTSUnit* theUnit = thePlayer->GetUnit(inUnitID);

    if (theUnit == NULL || theNode == NULL || static_cast<EEUnit*>(theUnit)->mComplexUnit == NULL)
        return;
    if (thePlayer->CanProduce(inTechnology, static_cast<EEUnit*>(theUnit)->mComplexUnit->mUnitClass->GetUnitType(), 1, true, 3,
                            &theError)) {
        gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        this->mBuild->mTechnology = inTechnology;
        this->SetUIState(1);
        EEGhostUnit::Create(this->GetMouseLocation(), theNode->mData->mUnitClass, thePlayer, 0.0f, false);
    } else {
        gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        gUserInterface->AddChatLogEntry(theError, 0);
    }
}

// FUNCTION: EXE 0x6664b4
// Starts placing a gate for a tech (gate placement mode)
void EEUIFMainGame::StartGate(long inTechnology)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(inTechnology);
    EEUnitClass* theClass = gUnitClassTable[theNode->mData->mUnitClass];
    long theWallType = theNode->GetUnitClass(0);

    if (theClass == NULL || theNode == NULL)
        return;
    this->mBuild->mTechnology = inTechnology;
    this->mBuildGate.mTechnology = inTechnology;
    this->mBuildGate.mWallType = theWallType;
    this->mBuildGate.SetUnitClass(theClass);
    EEUIBuildGate* theTool = &this->mBuildGate;
    theTool->Initialize(thePlayer->mPlayerID);

    this->SetUIState(0xb);
    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x666572
// Creates the build action with no tech chosen
EEABuild::EEABuild()
{
    this->mMessage = NULL;
    this->mTechnology = kTechNone;
    this->mIsQueued = 0;
}

// FUNCTION: EXE 0x666594
// Starts placing a wall for a tech (wall placement mode)
void EEUIFMainGame::StartWall(long inTechnology)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(inTechnology);

    if (gUnitClassTable[theNode->mData->mUnitClass] == NULL || theNode == NULL)
        return;
    this->mBuild->mTechnology = inTechnology;
    this->mBuildWall.mTechnology = inTechnology;
    EEUIBuildWall* theTool = &this->mBuildWall;
    theTool->Initialize(thePlayer->mPlayerID);

    this->SetUIState(6);
    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x66662d
// Starts casting the calamity of a tech with the first selected unit: at once for self-targeted ones,
// otherwise in calamity mode (with a volcano ghost for tech 0x3f9)
void EEUIFMainGame::StartCalamity(long inTechnology)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(inTechnology);
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    EEUnit* theCaster;

    theCaster = theGroup->GetNumUnits() != 0 ? theGroup->GetUnit(0) : NULL;
    if (theCaster != NULL && theNode != NULL && theNode->mData->mCalamity > 0) {
        EECalamityEnum theType = (EECalamityEnum)theNode->mData->mCalamity;
        EECalamityClass* theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theType);
        if (theClass != NULL) {
            this->mCalamity->SetCalamity(inTechnology, theClass);
            this->mCalamity->SetCasterID(theCaster->mUnitID);
            if (theClass->GetTargetKind() == 1) {
                this->mCalamity->Cast(this->GetMouseLocation());
            } else {
                this->SetUIState(5);
                this->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
                if (inTechnology == 0x3f9)
                    EEGhostUnit::Create(this->GetMouseLocation(),
                                        dynamic_cast<EECCVolcano*>(theClass)->GetUnitClassIndex(), thePlayer, 0.0f, false);
                if (gUIManager->mMainGameForm != NULL)
                    gUIManager->mMainGameForm->mRefreshGroup = true;
            }
        }
    }
}

// FUNCTION: EXE 0x66676e
// Moves the chat lines down by an offset (below the information message)
void EEUIFMainGame::SetChatTop(long inTop)
{
    unsigned long i;

    for (i = 0x65; i < 0x6c; i++) {
        EEUICChatLine* theLine = static_cast<EEUICChatLine*>(this->GetControl(i));
        if (theLine != NULL)
            theLine->SetShadowOffset(inTop);
    }
}

// FUNCTION: EXE 0x66679a
// Enters flare mode when units are selected
void EEUIFMainGame::StartFlare()
{
    if (!(gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() > 0))
        return;
    this->SetUIState(0x15);
    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    this->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
    if (gUIManager->mMainGameForm != NULL)
        gUIManager->mMainGameForm->mRefreshGroup = true;
}

// FUNCTION: EXE 0x66681d
// Enters patrol mode when units are selected
void EEUIFMainGame::StartPatrol()
{
    if (!(gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() > 0))
        return;
    this->SetUIState(0x10);
    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    this->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
    if (gUIManager->mMainGameForm != NULL)
        gUIManager->mMainGameForm->mRefreshGroup = true;
}

// FUNCTION: EXE 0x6668a0
// Enables the hot key groups of a hot key mode
void EEUIFMainGame::SetHotKeyMode(long inMode)
{
    this->mHotKeyMode = inMode;
    this->SetHotKeyGroupEnabled(2, true);
    switch (this->mHotKeyMode) {
    case 0:
        this->SetHotKeyGroupEnabled(0, true);
        this->SetHotKeyGroupEnabled(1, false);
        break;
    case 1:
        this->SetHotKeyGroupEnabled(0, false);
        this->SetHotKeyGroupEnabled(1, true);
        break;
    case 2:
        this->SetHotKeyGroupEnabled(0, true);
        this->SetHotKeyGroupEnabled(1, true);
        break;
    }
}

// FUNCTION: EXE 0x6668ee
// Switches the interface mode, clearing ghosts and ending a wall or gate placement
void EEUIFMainGame::SetUIState(long inUIState)
{
    gWorld.mGhostUnitsLock->Enter();
    gWorld.ClearGhostUnits();
    gWorld.mGhostUnitsLock->Leave();
    this->GetManager()->ForceUIUpdate();
    if (gUserInterface != NULL)
        gUserInterface->UpdateJITManufacturing();
    if (this->mUIState == 6)
        ((EEUIBuildWall&)this->mBuildWall).Terminate();
    else if (this->mUIState == 0xb)
        ((EEUIBuildGate&)this->mBuildGate).Terminate();
    this->mUIState = inUIState;
}

// FUNCTION: EXE 0x666960
// Hides every background panel and disables the first three entity groups
void EEUIFMainGame::HideAllBackgrounds()
{
    unsigned long i;

    for (i = 0; i < 0x16; i++)
        this->GetBackgroundByID(i + 0x67)->SetVisibility(false);
    this->GetBackgroundByID(0x64)->SetVisibility(false);
    this->GetBackgroundByID(0x65)->SetVisibility(false);
    this->GetBackgroundByID(0x90)->SetVisibility(false);
    this->GetBackgroundByID(0x8f)->SetVisibility(false);
    this->GetBackgroundByID(0x91)->SetVisibility(false);
    for (i = 0; i < 3; i++)
        this->GetEntityGroup(i)->Disable();
}

// FUNCTION: EXE 0x6669e8
// Hides the information message and its portrait and moves the chat lines back up
void EEUIFMainGame::HideMessages()
{
    UITLScrolling* theText = static_cast<UITLScrolling*>(this->GetControl(kMainGameDialogMessage));
    UIControl* thePortrait = this->GetControl(kMainGameDialogPortrait);

    if (theText != NULL) {
        theText->StopScrolling();
        theText->SetDisplayMode(kDisplayModeHidden);
    } else {
        gPhysics->PostShowDialogMessage();
    }
    if (thePortrait != NULL)
        thePortrait->SetDisplayMode(kDisplayModeHidden);
    this->SetChatTop(0);
}

#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIEChatLogEvents.h"

// Original status row: player pointer, victory condition pointer, unsigned remaining seconds.
// The original sort compares only +8 as unsigned (267521).
struct EEUIFMGVictoryStatus {
    EEPlayer* mPlayer;
    EEVictoryCondition* mCondition;
    unsigned long mValue;
    bool operator<(const EEUIFMGVictoryStatus& inOther) const { return mValue < inOther.mValue; }
};

// FUNCTION: EXE 0x666a3b
// Refreshes the status bar: resources, population, wonder countdowns, clock or frame rate, pause and
// victory texts, the lag icons, the briefing and previous-messages buttons, then the cursor
void EEUIFMainGame::RefreshStatus()
{
    std::vector<EEUIFMGVictoryStatus> theRows;
    UWideString thePopulation, theText, theFormat;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    this->FnE123D();
    for (long i = 0; i < 5; i++) {
        long theValue = thePlayer->mResourceCaps[i + 2];
        UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetControl(i + 6));
        if (theValue != atoi(theLabel->GetText(0)))
            theLabel->SetText(theValue);
    }
    UITextLabel* thePopulationLabel = static_cast<UITextLabel*>(this->GetControl(kMainGamePopulation));
    sprintf(thePopulation, "%ld/%ld", thePlayer->GetTotalPopulation(false, false), thePlayer->GetPopulationLimit(false));
    if (thePopulationLabel->GetText(0) != thePopulation)
        thePopulationLabel->SetText(thePopulation);
    if (gWorld.mVictoryEnabled == true) {
        for (unsigned long t = 0; t < gWorld.GetNumTeams(); t++) {
            EEVictoryCondition* theCondition = gWorld.mTeams[t].mVictoryCondition;
            unsigned long theValue;
            EEPlayer* theWinner = theCondition->GetWonderCountdown(theValue);
            if (theWinner == NULL)
                continue;
            EEUIFMGVictoryStatus theRow;
            theRow.mPlayer = theWinner;
            theRow.mCondition = theCondition;
            theRow.mValue = theValue;
            theRows.push_back(theRow);
        }
        std::sort(theRows.begin(), theRows.end());
        unsigned long theRow;
        long theControl = 0x0e;
        for (theRow = 0; theRow < theRows.size(); theRow++) {
            if (theControl >= 0x13)
                break;
            UITextLabel* theLabel = static_cast<UITextLabel*>(this->GetControl(theControl));
            theLabel->SetDisplayMode(kDisplayModeShown);
            theLabel->GetGEText()->SetFgColor(theRows[theRow].mPlayer->mColor);
            gResource->GetString(IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY, theFormat);
            sprintf(theText, theFormat, (const char*)(theRows[theRow]).mPlayer->mName,
                    theRows[theRow].mValue);
            theLabel->SetText(theText);
            theControl++;
        }
        if (theRow < 5)
            for (long theUnused = theRow + 0xe; theUnused < 0x13; theUnused++)
                this->GetControl(theUnused)->SetDisplayMode(kDisplayModeHidden);
    }
    UITextLabel* theTimeLabel = static_cast<UITextLabel*>(this->GetControl(kMainGameGameClock));
    if (!gWorld.mInCinematic) {
        switch (this->mClockMode) {
        case 1:
            theTimeLabel->SetDisplayMode(kDisplayModeShown);
            theTimeLabel->SetText(gPhysics->GetTimeString());
            break;
        case 2: {
            theTimeLabel->SetDisplayMode(kDisplayModeShown);
            gResource->GetString(IDS_FPS, theFormat);
            float theFPS = gRasterizer->GetFramesPerSecond(this->GetManager()->GetScreen());
            unsigned long theRounded = (unsigned long)UMCeil(theFPS);
            sprintf(theText, "%s: %ld", (const char*)theFormat, theRounded);
            theTimeLabel->SetText(theText);
            break;
        }
        default:
            theTimeLabel->SetDisplayMode(kDisplayModeHidden);
            break;
        }
    } else
        theTimeLabel->SetDisplayMode(kDisplayModeHidden);
    if (gCommunications != NULL && gCommunications->GetIsGamePaused() && gWorld.mTeams.size() > 0) {
        UITextLabel* thePauseLabel = static_cast<UITextLabel*>(this->GetControl(kMainGameWinLose));
        gResource->GetString(IDS_GAME_PAUSED, theText);
        thePauseLabel->SetText(theText);
        thePauseLabel->SetDisplayMode(kDisplayModeShown);
        gSoundManager->SetIsMuted(true);
    } else if (thePlayer->mGameResult != 0) {
        gSoundManager->SetIsMuted(true);
        this->SetPauseText(thePlayer->mGameResult == 1);
    } else {
        this->GetControl(kMainGameWinLose)->SetDisplayMode(kDisplayModeHidden);
        if (gSoundManager->GetIsMuted())
            gSoundManager->SetIsMuted(false);
    }
    if (this->mElapsed > 2) {
        this->GetControl(kMainGameMiniMap)->SetDirty(true);
        this->mElapsed = 0;
    }
    unsigned short theMask = gWorld.mLagStatus;
    unsigned long theBit = 1;
    for (long thePlayerIndex = 1, theIcon = 0xfc; theIcon >= 0xf5; thePlayerIndex++, theIcon--) {
        UIControl* theControl = this->GetControl(theIcon);
        if ((theMask & theBit) != 0) {
            static_cast<UIBitmap*>(theControl)->GetBitmap()->SetColor(gWorld.mPlayers[thePlayerIndex]->mColor);
            theControl->SetDisplayMode(kDisplayModeShown);
        } else
            theControl->SetDisplayMode(kDisplayModeHidden);
        theBit <<= 1;
    }
    if (gWorld.mScenarioManager->mMediaStrings.mInstructions.GetLength() > 0)
        this->GetControl(kMainGameGameInfoBriefing)->SetDisplayMode(kDisplayModeShown);
    else {
        this->GetControl(kMainGameGameInfoBriefing)->SetDisplayMode(kDisplayModeHidden);
        this->GetControl(kMainGameLargeScenBriefingAlert)->SetDisplayMode(kDisplayModeHidden);
    }
    this->GetControl(kMainGamePreviousMessages)->SetDisplayMode(EEUIECLEFormManager::GetEntryCount() > 0);
    if (!this->mIsCursorLocked)
        this->UpdateCursorKind();
}

// The cursor's reads for one UpdateCursorKind (ee_screens cursor.rs asks them through ee_screenscpp_cursor)
struct EEUIFMGCursorRead {
    EEUIFMainGame* mForm;
    EEPlayer* mPlayer;
    EESelectedGroup* mGroup;
    EEComplexUnit* mFirst;
    EEUnit* mTarget;
};

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
    case EE_SCREENS_CURSOR_OP_FIRST:
        return theRead.mFirst == NULL ? 0 : theRead.mFirst->mHitPoints > 0 ? 1 : 2;
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
    case EE_SCREENS_CURSOR_OP_T_KIND:
        return (int32_t)theTarget->mUnitClass->mKindFlags;
    case EE_SCREENS_CURSOR_OP_T_DISABLED:
        return theTarget->mTileNode != NULL && theTarget->mComplexUnit->mIsDisabled &&
               !theTarget->mComplexUnit->IsProtected();
    case EE_SCREENS_CURSOR_OP_FIRST_MEDIC:
        return theRead.mFirst->mUnitClass->mIsMedic;
    case EE_SCREENS_CURSOR_OP_T_TARGETABLE:
        return EEUUGetIsUnitClassTargetable(NULL, theTarget->mUnitClass);
    case EE_SCREENS_CURSOR_OP_T_OWNER:
        if (theTarget->mPlayer == theRead.mPlayer)
            return 0;
        return theRead.mPlayer->mEnemies[theTarget->mPlayer->mPlayerID] != 0 ? 1 : 2;
    case EE_SCREENS_CURSOR_OP_T_HP:
        return theTarget->mHitPoints;
    case EE_SCREENS_CURSOR_OP_T_MAX_HP:
        return theTarget->mStats->mMaxHitPoints;
    case EE_SCREENS_CURSOR_OP_T_ATTACK:
        return theTarget->mStats->mAttack > 0.0f;
    case EE_SCREENS_CURSOR_OP_T_TYPE:
        return theTarget->mUnitClass->mUnitType;
    case EE_SCREENS_CURSOR_OP_T_TOWER:
        return theTarget->mUnitClass->mIsTower;
    case EE_SCREENS_CURSOR_OP_T_CATEGORY:
        return theTarget->mUnitClass->mCategory;
    case EE_SCREENS_CURSOR_OP_T_ENTERABLE:
        return theTarget->mTileNode != NULL && theTarget->mComplexUnit->mIsBuilt;
    case EE_SCREENS_CURSOR_OP_T_COMPLEX:
        return theTarget->mComplexUnit != NULL;
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
    case EE_SCREENS_CURSOR_OP_SEL_COUNT:
        return (int32_t)theRead.mGroup->GetNumUnits();
    case EE_SCREENS_CURSOR_OP_SEL_KIND:
        return (int32_t)theUnit->mUnitClass->mKindFlags;
    case EE_SCREENS_CURSOR_OP_SEL_ATTACK:
        return theUnit->mStats->GetAttack() > 0.0f;
    case EE_SCREENS_CURSOR_OP_SEL_BUILT:
        return theUnit->mComplexUnit->mIsBuilt;
    case EE_SCREENS_CURSOR_OP_SEL_ON_MAP:
        return theUnit->mTileNode != NULL;
    case EE_SCREENS_CURSOR_OP_SEL_CARGO:
        return (int32_t)theUnit->GetComplexUnit()->GetNumCargoUnits();
    case EE_SCREENS_CURSOR_OP_SEL_TYPE:
        return theUnit->GetUnitClass()->mUnitType;
    case EE_SCREENS_CURSOR_OP_SEL_AIR_BASE:
        return theUnit->GetUnitClass()->IsAirBase();
    case EE_SCREENS_CURSOR_OP_SEL_CRUSADER:
        return theUnit->mUnitClass->IsCrusader();
    case EE_SCREENS_CURSOR_OP_SEL_PRIEST:
        return theUnit->mUnitClass->IsPriest();
    case EE_SCREENS_CURSOR_OP_SEL_COMPLEX:
        return theUnit->GetComplexUnit() != NULL;
    case EE_SCREENS_CURSOR_OP_SEL_POSITION: {
        const GE3DPoint& thePosition = theUnit->GetComplexUnit()->mPhysicalModel.GetLocation().GetPosition();
        outF[0] = thePosition.GetX();
        outF[1] = thePosition.GetY();
        outF[2] = thePosition.GetZ();
        return 0;
    }
    case EE_SCREENS_CURSOR_OP_SEL_MIN_RANGE:
        outF[0] = theUnit->GetUnitClass()->mMinRange;
        return 0;
    case EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK:
        return theUnit->GetComplexUnit()->CanAttackClass(theTarget->GetUnitClass());
    case EE_SCREENS_CURSOR_OP_SEL_CONVERT: {
        bool theShielded = inB != 0;
        bool theResult = EECanConvert(theUnit, theTarget->mComplexUnit, theShielded);
        return (theResult ? 1 : 0) | (theShielded ? 2 : 0);
    }
    case EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK:
        return theCargo->CanAttackClass(theTarget->GetUnitClass());
    case EE_SCREENS_CURSOR_OP_CARGO_ATTACK:
        return theCargo->mStats->GetAttack() > 0.0f;
    case EE_SCREENS_CURSOR_OP_CARGO_POSITION: {
        const GE3DPoint& thePosition = theCargo->mPhysicalModel.GetLocation().GetPosition();
        outF[0] = thePosition.GetX();
        outF[1] = thePosition.GetY();
        outF[2] = thePosition.GetZ();
        return 0;
    }
    case EE_SCREENS_CURSOR_OP_CARGO_MIN_RANGE:
        outF[0] = theCargo->GetUnitClass()->mMinRange;
        return 0;
    }
    return 0;
}

// FUNCTION: EXE 0x666f8b
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

// FUNCTION: EXE 0x66716e
// Refreshes the ten saved-group buttons
void EEUIFMainGame::RefreshSavedGroups()
{
    unsigned long i;

    for (i = 0xd4; i <= 0xdd; i++) {
        if (this->GetControl(i) != NULL)
            dynamic_cast<EEUIBSavedGroup*>(this->GetControl(i))->Refresh();
    }
}

// FUNCTION: EXE 0x6671b0
// Refreshes the selected-units icon bar
void EEUIFMainGame::UpdateIconBar()
{
    if (this->GetControl(kMainGameUnitIconBar) != NULL)
        dynamic_cast<EEUICUnitIconBar*>(this->GetControl(kMainGameUnitIconBar))->Update();
}

// FUNCTION: EXE 0x6671df
// Creates the calamity casting action
// (EEACalamity's ctor: inline in EEACalamity.h, emitted here out of line)

// FUNCTION: EXE 0x66720f
// Appends a row to the status update's countdown vector (std::vector::push_back, ICF-shared)
// (vector<12-byte score entry>::push_back: a template COMDAT, ICF-folded with vector<EEATCCandidate>'s)

// FUNCTION: EXE 0x66723b
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort over the status update's score entries: template COMDATs 0x26723b..0x26782d, emitted once 0x266a3b is
// written)

// FUNCTION: EXE 0x6672e6
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x66730b
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6673d0
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x667408
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6674b1
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6674eb
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x667521
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x667616
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x66763e
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6676a0
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6676c5
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x667742
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x66779f
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x6677ed
// Sorts the wonder countdown rows of the status update (std::sort template COMDAT)
// (std::sort COMDAT)

// FUNCTION: EXE 0x66782d
// Returns an error log line by index (std::vector<UWideString>::operator[])
// (vector<UWideString>::operator[]: a template COMDAT; CopyData calls it out of line)

// ICF-folded empty members (no address of their own: 0x157269 / 0xe123d are their twins)
void EEUIFMainGame::FnE123D()
{
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor eax, eax; ret` body of EEPlagueFunctor::InfectsAlliesOnly)
bool EEFn27C0B4B(void *)
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor eax, eax; ret` body of EEPlagueFunctor::InfectsAlliesOnly)
bool EEModeKeyboardNone(void *)
{
    return false;
}

// ICF-FOLDED: EXE 0x4fca98
// (no address of its own: folded into the shared `xor eax, eax; ret` body of EEUnitGoal::GetOutOfSynchChecksum)
long EEModeMouseNone(void *,void *)
{
    return 0;
}
