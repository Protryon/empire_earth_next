// C:\EEX\Empire Earth\Scenario Editor\EESEUnit.cpp (file name inferred: no __FILE__; RTTI class EESEUnit). The scenario
// editor's unit tab: 0x199afd..0x19cc39. Its helper controls first, in definition order (EEUIFSELBUnitList,
// EEUIFSELBCargoList, EEUIFSEBUnitType, EEUIFSEBRotation, EEUIFSEBRemoveCargo: not alphabetical, so one unit), then
// the tab's ctor, the inline action ctors it refused, the members, and EEUIBuildWall's ctor COMDAT (first user: the
// ctor) with the build modes' ??_Gs. After EESEUITechListBox.cpp, before EEUIFScenarioEditor.cpp (0x19cc39). Unit
// flags: the exe module set.
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
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/USingleton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEARust.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEPlayer.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EESEARust.h"
#include "ee/EESEUICCommandUnits.h"
#include "ee/EESELoadThread.h"
#include "ee/EEMiniMap.h"
#include "rust/game/screens.h"
#include "ee/EEUIShell.h"


// cdecl helpers of other units (provisional names; types inferred)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);  // 0x23eb4c (wall ghosts)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2: a gate fits on inTile's wall
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the ghost at the mouse)
extern unsigned long& gTexturePixelDepth;

// The upgrade panel is a real 0x60-byte class (new at 0x59b976 in the tab's ctor); both its ctor counters
// are mutable
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFSEUpgradePanel.h"
#include "ee/EEStringIDs.h"






// The sort radio buttons' event (RTTI EEUIFSEESortUnitList > UIEvent; vtable 0x444e88; new 0xc): an EESEERust
// (ee_screens editor_events.rs)

extern GEFont* gSeplScreenFont;   // the screen's font while the tab makes its controls (EESERBUnitTypes.cpp)

// (the unit tab's controls are shell kinds: ee_game shells_unit.rs)

// The unit type buttons' cell width: a plain (non-folded) .rdata float in the original (0x444e7c), read by the ctor
// only (provisional name)
static const float kSEUButtonWidth = 35.0f;

// The shared control maker's unit tab lists (EESERBUnitTypes.cpp SeplMake)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
{
    std::vector<UWideString> theNone;
    if (inKind == EE_SCREENS_SEPL_LIST_UNIT_TYPES)
        return new EEUIShellList(EE_SCREENS_SK_UNIT_CLASS_LIST, {}, inID, theNone, inFont, false,
                                 GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 3);
    return new EEUIShellList(EE_SCREENS_SK_CARGO_LIST, {}, inID, theNone, inFont, false,
                             GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 3);
}


// ---- the unit tab's host (editor_unit.rs; EE_SCREENS_SEUN_OP_*): the tab's own controls, the actions, the world,
// the mouse, the build tools and the unit classes

static_assert(EE_SCREENS_SEUN_IDS_ALPHA == IDS_ALPHA && EE_SCREENS_SEUN_IDS_ALPHA + 1 == IDS_SE_EPOCH &&
                  EE_SCREENS_SEUN_IDS_ALPHA + 2 == IDS_FAMILY && EE_SCREENS_SEUN_IDS_CARGO == IDS_CARGO &&
                  EE_SCREENS_SEUN_IDS_SORT == IDS_SORT && EE_SCREENS_SEUN_IDS_WORLD_PLAYER == IDS_WORLD_PLAYER,
              "the strings");
static_assert(EE_SCREENS_SEUN_PALETTE_LAND == kUnitPaletteLandUnits && EE_SCREENS_SEUN_PALETTE_AIR == kUnitPaletteAirUnits &&
                  EE_SCREENS_SEUN_PALETTE_NAVAL == kUnitPaletteNavalUnits &&
                  EE_SCREENS_SEUN_PALETTE_STRUCTURES == kUnitPaletteStructures &&
                  EE_SCREENS_SEUN_PALETTE_HEROES == kUnitPaletteHeroes &&
                  EE_SCREENS_SEUN_PALETTE_RESOURCES == kUnitPaletteResources,
              "the palettes");

namespace {

EESEARust* SeunAction(void* inAction)
{
    return static_cast<EESEARust*>(static_cast<EEAction*>(inAction));
}

// one unit type button with its normal and pressed bitmaps (the tab's ctor's loop body)
void SeunTypeButton(void** outMade, EEUIFScenarioEditor* inForm, UIEvent* inEvent, const int32_t* i, const float* f)
{
    GETexture* theTexture = inForm->GetButtonTexture();
    float theSize = (float)theTexture->GetWidth();
    GETextureCoordinate theTopLeft, theTopRight, theBottomLeft, theBottomRight;
    float theYScale = 25.0f / theSize;
    float theXScale = kSEUButtonWidth / theSize;
    GE2DPoint thePosition((float)(unsigned long)i[3], 0.0f);
    GEBitmap* theNormal = new GEBitmap;
    GEBitmap* thePressed = new GEBitmap;
    GEBitmap* theBitmaps[2] = {theNormal, thePressed};
    theNormal->SetTexture(theTexture, inForm->GetManager()->GetScreen());
    thePressed->SetTexture(theTexture, inForm->GetManager()->GetScreen());
    for (int k = 0; k < 2; k++) {
        float theX = f[k * 2];
        float theY = f[k * 2 + 1];
        theTopLeft.SetU(theX / theSize);
        theTopLeft.SetV(theY / theSize);
        theBottomRight.SetU((theX + kSEUButtonWidth) / theSize);
        theBottomRight.SetV((theY + 25.0f) / theSize);
        theTopRight.SetU(theBottomRight.GetU());
        theTopRight.SetV(theTopLeft.GetV());
        theBottomLeft.SetU(theTopLeft.GetU());
        theBottomLeft.SetV(theBottomRight.GetV());
        theBitmaps[k]->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    }
    theNormal->SetXYScale(theXScale, theYScale);
    thePressed->SetXYScale(theXScale, theYScale);
    long theIndex = i[1];
    UIRadioButton* theButton = new EEUIShellRadio(EE_SCREENS_SK_UNIT_TYPE_BUTTON, {}, theIndex + 0x406, UWideString(""),
                                                  gU531930, GERectangle(thePosition, 35.0f, 25.0f), inForm,
                                                  (unsigned long)i[0], 0, true, theIndex == 0, false);
    theButton->SetUpCheckedBitmap(thePressed);
    theButton->SetDownCheckedBitmap(thePressed);
    theButton->SetDownUncheckedBitmap(thePressed);
    theButton->SetUpUncheckedBitmap(theNormal);
    theButton->SetFocusedBitmap(NULL);
    theButton->SetBitmapDrawOffset(GE2DPoint(0.0f, 0.0f));
    theButton->SetTriggerEvent(kControlTriggerButtonUp, inEvent);
    theButton->SetStringTableID((unsigned long)i[2]);
    inForm->AddControl(theButton, 6);
    outMade[0] = theButton;
    outMade[1] = theNormal;
    outMade[2] = thePressed;
}

// Update's wall ghosts: the wall tool's first tile, or the tiles from its start to the mouse
void SeunPreviewWall(EESEUnit* inTab)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<GE2DPoint> thePoints;
    EETile* theUnit;
    long theX;
    long theY;
    GE2DPoint theLocation;
    EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
    theLocation.SetXY((float)theX, (float)theY);
    if (inTab->mBuildWall.mState == 0) {
        theLocation.SetX(theLocation.GetX() + 0.5f);
        theLocation.SetY(theLocation.GetY() + 0.5f);
        theTiles.clear();
        theTiles.push_back(EETileCoordinate((long)theLocation.GetX(), (long)theLocation.GetY()));
        gWorld.mGhostUnitsLock->Enter();
        EEPreviewWall(theTiles, inTab->mBuildWall.mTechnology, inTab->mBuildWall.mPlayer);
        gWorld.mGhostUnitsLock->Leave();
    }
    if (inTab->mBuildWall.mState == 1) {
        theLocation.SetX(theLocation.GetX() + 0.5f);
        theLocation.SetY(theLocation.GetY() + 0.5f);
        theTiles.clear();
        inTab->mBuildWall.mStart.InterpolateTo(theLocation, 1.0f, thePoints);
        for (unsigned long i = 0; i < thePoints.size(); i++) {
            GE2DPoint& thePoint = thePoints[i];
            theTiles.push_back(EETileCoordinate((long)thePoint.GetX(), (long)thePoint.GetY()));
        }
        gWorld.mGhostUnitsLock->Enter();
        EEPreviewWall(theTiles, inTab->mBuildWall.mTechnology, inTab->mBuildWall.mPlayer);
        gWorld.mGhostUnitsLock->Leave();
    }
}

// Update's gate: a gate ghost on the wall under the mouse, else no ghost
void SeunPreviewGate(EESEUnit* inTab)
{
    EETile* theUnit;
    long theX;
    long theY;
    EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
    long theDirection = inTab->mBuildGate.mDirection;
    EETileCoordinate theTile(theX, theY);
    if (EEFindGateWall(theTile, theDirection)) {
        EEGhostUnit::CreateGate(0.0f, inTab->mBuildGate.mUnitClass, gWorld.GetPlayer(inTab->mBuildGate.mPlayer));
        EEGhostUnit::PreviewGate(gMouse->mPosition, theDirection);
        inTab->mBuildGate.mDirection = theDirection;
    } else {
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
    }
}

// SelectUnitType's look-up: the player, the list line's class and its technology's node (the original's order)
struct SeunPick {
    EEPlayer* mPlayer;
    EEUnitClass* mClass;
    EETechTreeNode* mNode;
};

SeunPick SeunPickClass(UIControl* inList, long inPlayer, unsigned long inLine)
{
    SeunPick thePick;
    thePick.mPlayer = gWorld.GetPlayer(inPlayer);
    long theType = (long)EEUIShell::Op(inList, EE_SCREENS_SKOP_UL_TYPE_AT, inLine);
    EEUnitClass* theTypeClass = gUnitClassTable[theType];
    long theTech;
    if (theTypeClass != NULL)
        theTech = theTypeClass->mTechnology;
    else
        theTech = 100000;
    thePick.mNode = thePick.mPlayer->GetTechTree()->Get(theTech);
    thePick.mClass = gUnitClassTable[theType];
    return thePick;
}

EEUIFScenarioEditor* SeunEditor(void* inControl)
{
    return static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(inControl)->GetForm());
}

}   // namespace

// The ops (editor_unit.rs; p: three handles, i: four integers, f: four floats). The class table's count and
// placement flags are Rust's (ee_game editor_world.rs).
extern "C" int32_t ee_screenscpp_seun(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESEUnit* theTab = static_cast<EESEUnit*>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UILBString* theList = static_cast<UILBString*>(p[0]);
    UIControl* theShell = static_cast<UIControl*>(p[0]);
    void** theOut = static_cast<void**>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEUN_OP_SORT_EVENT: *theOut = static_cast<UIEvent*>(new EESEERust(kSEESortUnitList)); return 0;
    case EE_SCREENS_SEUN_OP_TYPE_BUTTON:
        SeunTypeButton(theOut, static_cast<EEUIFScenarioEditor*>(theForm), static_cast<UIEvent*>(p[2]), i, f);
        return 0;
    case EE_SCREENS_SEUN_OP_REMOVE_CARGO: {
        UWideString theText;
        gResource->GetString(IDS_SE_REMOVE, theText);
        UIButton* t = new EEUIShellButton(EE_SCREENS_SK_REMOVE_CARGO, {}, 0x3f4, NULL, NULL, NULL,
                                          GERectangle(GE2DPoint(f[0], 707.0f), 100.0f, 25.0f), theForm, (unsigned long)i[0], 2);
        t->SetText(theText, gSeplScreenFont);
        t->SetBgOpacity(1.0f);
        GEText* theGEText = t->GetGEText();
        theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
        theGEText->SetIsTransparent(true);
        theForm->AddControl(t, 6);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ROTATION: {
        UIButton* t = new EEUIShellButton(EE_SCREENS_SK_ROTATION, {}, 0x3f5, NULL, NULL, NULL,
                                          GERectangle(GE2DPoint(500.0f, 633.0f), 50.0f, 50.0f), theForm, (unsigned long)i[0], 0);
        t->SetStringTableID(IDS_ROTATE);
        theForm->AddControl(t, 6);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ACTION: {
        EEAction* theAction = NULL;
        switch (i[0]) {
        case EE_SCREENS_SEUN_ACTION_CLICK: theAction = new EESEAMouseClick; break;
        case EE_SCREENS_SEUN_ACTION_LASSO: theAction = new EESEALasso; break;
        case EE_SCREENS_SEUN_ACTION_PLACE_UNIT: theAction = new EESEARust(kSEAPlaceUnit); break;
        case EE_SCREENS_SEUN_ACTION_PLACE_GATE: theAction = new EESEARust(kSEAPlaceGate); break;
        case EE_SCREENS_SEUN_ACTION_PLACE_WALL: theAction = new EESEARust(kSEAPlaceWall); break;
        default: theAction = new EESEARust(kSEAMoveUnit); break;
        }
        *theOut = theAction;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_SELECT_UNITS: {
        void* const* theTools = static_cast<void* const*>(p[2]);
        EEUICSelectUnits* t = new EEUICSelectUnits(
            theForm, 0x3f6, (unsigned long)i[0], static_cast<EESEAMouseClick*>(static_cast<EEAction*>(theTools[0])),
            static_cast<EESEALasso*>(static_cast<EEAction*>(theTools[1])));
        theForm->AddControl(t, 0);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_COMMAND_UNITS: {
        EESEUICCommandUnits* t = new EESEUICCommandUnits(
            theForm, 0x3f7, (unsigned long)i[0], static_cast<EESEAMouseClick*>(static_cast<EEAction*>(p[2])));
        theForm->AddControl(t, 0);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_BACKGROUND: {
        GETextureDescription theDescription = {1, 0, gTexturePixelDepth, 0, false, false, true, true};
        theDescription.mCreateMIPMaps = false;
        GETexture* theTexture = gTextureManager->LoadTexture("textures\\scenunit", theDescription);
        theTexture->SetFrame(0);
        theTexture->SetStageAlphaMode(kStageOpTexture, -1);
        theTexture->SetStageColorMode(kStageOpTexture, -1);
        theTexture->SetStageMinFilterMode(0, -1);
        theTexture->SetStageMagFilterMode(0, -1);
        theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
        theTexture->SetStageMipMapBias(-1.0f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
        GEBitmap* theBackground = new GEBitmap;
        theBackground->SetTexture(theTexture, gScreenIndex);
        *theOut = theBackground;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_UPGRADE_PANEL: {
        unsigned long theID = 0x41a;
        unsigned long theIndex = (unsigned long)i[0];
        *theOut = new EEUIFSEUpgradePanel(theID, theIndex, static_cast<EEUIFScenarioEditor*>(theForm));
        i[0] = (int32_t)theIndex;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_RELEASE:
        switch (i[0]) {
        case 0: static_cast<EEAction*>(p[0])->Dereference(); break;
        case 1: static_cast<GEBitmap*>(p[0])->Dereference(); break;
        case 2: static_cast<UIEvent*>(p[0])->Dereference(); break;
        default: delete static_cast<EEUIFSEUpgradePanel*>(p[0]); break;
        }
        return 0;
    case EE_SCREENS_SEUN_OP_PLACE_WALL: {
        GE2DPoint theStart(theTab->mBuildWall.mStart);
        GE2DPoint theEnd(theTab->mBuildWall.mEnd);
        ee_screens_sew_wall(SeunAction(p[1])->mAction, theStart.GetX(), theStart.GetY(), theEnd.GetX(), theEnd.GetY(),
                            i[0], theTab->mBuildWall.mTechnology);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_PLACE_GATE:
        ee_screens_sew_gate(SeunAction(p[1])->mAction, theTab->mBuildGate.mTileX, theTab->mBuildGate.mTileY, i[0],
                            theTab->mBuildGate.mTechnology, theTab->mBuildGate.mDirection);
        return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_GHOSTS:
        if (i[0])
            gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        if (i[0])
            gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS:
        EEClearGateHighlights(gWorld.GetPlayer(theTab->mBuildGate.mPlayer), theTab->mBuildGate.mWallType);
        return 0;
    case EE_SCREENS_SEUN_OP_PREVIEW:
        if (i[0] == 0)
            EEPreviewPlacement(gMouse->mPosition);
        else if (i[0] == 1)
            SeunPreviewWall(theTab);
        else
            SeunPreviewGate(theTab);
        return 0;
    case EE_SCREENS_SEUN_OP_GAME_VIEW:
        if (i[0])
            EESetGameViewRectangle(GERectangle(225.0f, 0.0f, 1024.0f, 606.0f));
        else
            EESetGameViewRectangle(GERectangle(0.0f, 0.0f, 1024.0f, 606.0f));
        gB529066 = false;
        gCameraFollowing = false;
        gB529068 = false;
        if (gMiniMap != NULL)
            gMiniMap->RefreshView();
        gUIManager->RedrawAll();
        gUIManager->GetCurrentForm()->SetAllControlsDirty();
        return 0;
    case EE_SCREENS_SEUN_OP_REDRAW: static_cast<UIControl*>(p[0])->GetForm()->GetManager()->RedrawAll(); return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_SELECTION: SeunEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEUN_OP_MOVING: return ee_screens_sew_moving(SeunAction(p[0])->mAction) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_CANCEL_MOVE: ee_screens_sew_cancel(SeunAction(p[0])->mAction); return 0;
    case EE_SCREENS_SEUN_OP_PICK_UP: ee_screens_sew_pick_up(SeunAction(p[0])->mAction, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_PUT_DOWN: ee_screens_sew_put_down(SeunAction(p[0])->mAction); return 0;
    case EE_SCREENS_SEUN_OP_PLACE_UNIT: ee_screens_sew_place_unit(SeunAction(p[0])->mAction, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_SHOW_UNIT:
        static_cast<EEUIFSEUpgradePanel*>(p[0])->ShowUnit(static_cast<EEUnit*>(p[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_DRAW: {
        GERectangle theRectangle;
        if (theTab->mPanelRectangle.And(*static_cast<const GERectangle*>(p[2]), theRectangle))
            theTab->mForm->DrawBackground(static_cast<GEBitmap*>(p[1]), theRectangle,
                                          GERectangle(0.0f, 0.0f, 255.0f, 255.0f), theTab->mPanelRectangle);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ANGLE: {
        uint32_t theBits = (uint32_t)EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_ROTATION_ANGLE);
        memcpy(&f[0], &theBits, 4);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_TYPE_MODE: return ee_screens_shell_unit_type_mode();
    case EE_SCREENS_SEUN_OP_DIRTY: {
        EESEUnit* theShell = static_cast<EESEUnit*>(p[1]);
        theShell->mPanelRectangle.SetLeft(0.0f);
        theShell->mPanelRectangle.SetTop(0.0f);
        theShell->mPanelRectangle.SetRight(224.0f);
        theShell->mPanelRectangle.SetBottom(605.0f);
        UIForm* theComboForm = static_cast<UIControl*>(p[0])->GetForm();
        theComboForm->GetManager()->AddDirtyRectangle(theShell->mPanelRectangle, theComboForm);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_LIST_TOP: theList->SetScrollBarCurrentRange(0); return 0;
    case EE_SCREENS_SEUN_OP_CANCEL_BUILD:
        if (i[0])
            theTab->mBuildGate.Cancel();
        else
            theTab->mBuildWall.Cancel();
        return 0;
    case EE_SCREENS_SEUN_OP_PLACE_INFO: {
        SeunPick thePick = SeunPickClass(theShell, i[0], (unsigned long)i[1]);
        long theID = thePick.mPlayer->GetPlayerID();
        i[1] = !((theID != 0 || !thePick.mClass->mIsGaiaPlaceable) &&
                 (!(theID > 0) || !thePick.mClass->mIsPlayerPlaceable));
        if (thePick.mNode != NULL && thePick.mNode->mData->mKind == 6)
            i[2] = EE_SCREENS_SEUN_KIND_WALL;
        else if (thePick.mClass != NULL && thePick.mClass->mCategory == kUnitCategoryGate)
            i[2] = EE_SCREENS_SEUN_KIND_GATE;
        else
            i[2] = EE_SCREENS_SEUN_KIND_UNIT;
        return (int32_t)theID;
    }
    case EE_SCREENS_SEUN_OP_START_BUILD: {
        SeunPick thePick = SeunPickClass(static_cast<UIControl*>(p[1]), i[0], (unsigned long)i[1]);
        if (i[2] == 0) {
            theTab->mBuildWall.mTechnology = thePick.mNode->mData->mID;
            theTab->mBuildWall.Initialize(thePick.mPlayer->GetPlayerID());
        } else {
            theTab->mBuildGate.mTechnology = thePick.mNode->mData->mID;
            theTab->mBuildGate.mWallType = thePick.mNode->GetUnitClass(0);
            theTab->mBuildGate.mUnitClass = thePick.mClass;
            theTab->mBuildGate.Initialize(thePick.mPlayer->GetPlayerID());
        }
        return 0;
    }
    case EE_SCREENS_SEUN_OP_POST_GHOST: {
        long theType = (long)EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_UL_TYPE_AT, (uint32_t)i[0]);
        SeunEditor(p[0])->GetLoadThread()->Post402(reinterpret_cast<void*>(theType),
                                               reinterpret_cast<void*>((long)i[1]));
        return 0;
    }
    case EE_SCREENS_SEUN_OP_CARGO_CLEAR:
        EEUIShell::Op(theShell, EE_SCREENS_SKOP_CARGO_CLEAR);
        return 0;
    case EE_SCREENS_SEUN_OP_SELECTED_COUNT: return (int32_t)SeunEditor(p[1])->GetSelectedUnitCount();
    case EE_SCREENS_SEUN_OP_CARGO_COUNT: {
        EEComplexUnit* theUnit = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit;
        return theUnit != NULL ? (int32_t)theUnit->GetNumCargoUnits() : 0;
    }
    case EE_SCREENS_SEUN_OP_CARGO_ADD:
    {
        EEUnit* theUnit = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit->mCargo[(unsigned long)i[1]];
        EEUIShell::Op(theShell, EE_SCREENS_SKOP_CARGO_ADD, (int64_t)theUnit);
    }
        return 0;
    case EE_SCREENS_SEUN_OP_LIST_SORT: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_SORT, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_LIST_EMPTY: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_CLEAR); return 0;
    case EE_SCREENS_SEUN_OP_LIST_ADD: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_ADD, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_IN_VIEW: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1])) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_MOUSE: {
        int32_t theButtons = 0;
        if (UIManager::GetLMouseButton()->JustDoublePressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_DOUBLE;
        if (UIManager::GetLMouseButton()->JustPressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_PRESSED;
        if (UIManager::GetLMouseButton()->JustUnpressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_RELEASED;
        if (UIManager::GetLMouseButton()->GetValue() != 0) theButtons |= EE_SCREENS_SEUN_MOUSE_L_DOWN;
        if (UIManager::GetRMouseButton()->JustPressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_R_PRESSED;
        if (UIManager::GetRMouseButton()->JustUnpressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_R_RELEASED;
        return theButtons;
    }
    case EE_SCREENS_SEUN_OP_PICK: {
        EEUnit* theUnit = gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), 0);
        if (theUnit == NULL)
            return -1;
        int32_t theID = (int32_t)theUnit->mUnitID;
        i[1] = theUnit->mUnitClass->mCategory == kUnitCategoryGate;
        theUnit->Dereference();
        return theID;
    }
    case EE_SCREENS_SEUN_OP_HOLD_GATE: {
        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)(uint32_t)i[0]));
        theTab->mBuildGate.mTechnology = theUnit->mUnitClass->mTechnology;
        theTab->mBuildGate.mWallType = theUnit->GetTechTreeNode()->GetUnitClass(0);
        theTab->mBuildGate.mUnitClass = theUnit->mUnitClass;
        theUnit->Kill(0, 0, 0);
        theTab->mBuildGate.Initialize(theUnit->mPlayer->mPlayerID);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_BUILD_UPDATE:
        if (i[0])
            theTab->mBuildGate.Update(GE2DPoint(f[0], f[1]));
        else
            theTab->mBuildWall.Update(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_SELECTION:
        static_cast<EEUICSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_COMMAND:
        static_cast<EESEUICCommandUnits*>(p[0])->StartCommand(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_LOCK:
        if (i[0])
            gWorld.mGhostUnitsLock->Enter();
        else
            gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEUN_OP_KEYS: {
        ISEntity* theKey;
        int32_t theKeys = 0;
        gISKeyboard->GetEntity(kKeyDelete, theKey);
        if (theKey->JustPressed()) {
            theKeys |= 1;
            gISKeyboard->GetEntity(kKeyShift, theKey);
            if (theKey->GetValue() != 0)
                theKeys |= 2;
        }
        return theKeys;
    }
    case EE_SCREENS_SEUN_OP_DELETE_SELECTION: {
        EESEARust* theAction = new EESEARust(kSEADeleteUnit, NULL, -1, gWorld.GetCurrentPlayer()->GetPlayerID(), i[0] != 0);
        theAction->Dereference();
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ROTATION_KEYS: static_cast<UIControl*>(p[0])->UpdateKeyboard(); return 0;
    }
    return 0;
}

// the editor's gate and wall placement modes (once EESETSUICBPlayer.cpp): the unit tab places or ends them
void EESEUIBuildGate::Build(const GE2DPoint& inLocation, long inDirection)
{
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_GATE);
}

void EESEUIBuildGate::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_END_GATE);
    thePlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
}

void EESEUIBuildWall::Build()
{
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_WALL);
}

void EESEUIBuildWall::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_END_WALL);
    long theTechnology = this->mTechnology;
    thePlayer->UnloadUnitClassMedia(thePlayer->mTechnologies->GetData(theTechnology)->mUnitClass);
}
