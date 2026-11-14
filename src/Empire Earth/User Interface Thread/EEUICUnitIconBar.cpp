// C:\EEX\Empire Earth\User Interface Thread\EEUICUnitIconBar.cpp (file name inferred: no __FILE__; RTTI class
// EEUICUnitIconBar). The main game form's selected-units icon bar: 0x259f7f..0x25b2ce (ctor, ??_G, dtor, the members,
// the message ctors after their first users, vector<EEButtonObject*>::push_back last; EEUICSelectUnits ends at
// 0x259f7f, EEUICWonNet* follows). Unit flags: the exe module set.
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
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/GELine.h"
#include "lle/UIControlsTable.h"
#include "ee/EEUICUnitIconBar.h"

#include "rust/ee_screens.h"   // ee_screens_icon_bar_* (ee_screens icon_bar.rs)

static unsigned long sIconBarUpSound;           // 0x56458c (sound 0x208)
static unsigned long sIconBarDownSound;         // 0x564590 (sound 0x207)

// FUNCTION: EXE 0x659f7f
// Creates the selected-units icon bar with its icon frame bitmap and click sounds (its state and icon rectangle:
// ee_screens icon_bar.rs)
EEUICUnitIconBar::EEUICUnitIconBar(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
    : UIControl(kControlTypeCustom, inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), 0), mIcon(new GEBitmap)
{
    float theIcon[4];
    this->mState = ee_screens_icon_bar_new(this->GetRectangle().GetWidth(), this->GetRectangle().GetHeight(), theIcon);
    this->mBackground =
        inTable.CreateNormalBitmap(inForm->GetManager(), GERectangle(theIcon[0], theIcon[1], theIcon[2], theIcon[3]));
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound) {
        theSound->LoadMedia();
        sIconBarDownSound = theSound->GetSampleID();
    }
    theSound = EESoundObject::Get(kSoundButton2);
    if (theSound) {
        theSound->LoadMedia();
        sIconBarUpSound = theSound->GetSampleID();
    }
}

// SYNTHETIC: EXE 0x65a178
// (the scalar deleting destructor, emitted with the vtable; slot EEUICUnitIconBar+0x0)

// FUNCTION: EXE 0x65a194
// Destroys the icon bar, unloading its sounds and releasing its bitmaps and buttons
EEUICUnitIconBar::~EEUICUnitIconBar()
{
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound)
        theSound->UnloadMedia();
    theSound = EESoundObject::Get(kSoundButton2);
    if (theSound)
        theSound->UnloadMedia();
    this->mIcon->Dereference();
    this->mBackground->Dereference();
    ee_screens_icon_bar_free(this->mState, this);
}

// FUNCTION: EXE 0x65a258
// FUNCTION: EXE 0x65a32f
// FUNCTION: EXE 0x65a954
// FUNCTION: EXE 0x65a9e2
// FUNCTION: EXE 0x65aa43
// FUNCTION: EXE 0x65aa9b
// FUNCTION: EXE 0x65ae44
// FUNCTION: EXE 0x65afa5
// FUNCTION: EXE 0x65b010
// The snapshot, the drawing, the clicks and the refill from the selection (and the original's GetUnit,
// GetIconIndex, GetUnitIDs): ee_screens icon_bar.rs over the host below
long EEUICUnitIconBar::CopyData() { return ee_screens_icon_bar_copy_data(this->mState, this); }
long EEUICUnitIconBar::DrawPolygons() { return ee_screens_icon_bar_draw(this->mState, this); }
long EEUICUnitIconBar::MouseLButtonUp() { return ee_screens_icon_bar_mouse_l_up(this->mState, this); }
long EEUICUnitIconBar::MouseRButtonUp() { return ee_screens_icon_bar_mouse_r_up(this->mState, this); }
long EEUICUnitIconBar::MouseMove(const GE2DPoint& inLocation) { return ee_screens_icon_bar_mouse_move(this->mState, this); }
void EEUICUnitIconBar::Update() { ee_screens_icon_bar_update(this->mState, this); }

// FUNCTION: EXE 0x65ad92
// Creates a scramble-aircraft message for a unit and a tile
// (EEMScrambleAircraft::EEMScrambleAircraft(long, long, long), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65adcd
// Creates a select-these-units message from a list of ids
// (EEMSelectTheseUnits::EEMSelectTheseUnits(std::vector<long>&), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65ae3b
// Captures the mouse on a right press (also the left press, ICF-folded)
long EEUICUnitIconBar::MouseRButtonDown()
{
    this->CaptureMouse();
    return 0;
}

long EEUICUnitIconBar::MouseLButtonDown()
{
    this->CaptureMouse();
    return 0;
}

// FUNCTION: EXE 0x65af3d
// Creates an immediate cargo unload message for a transport and a cargo unit
// (EEMImmediateCargoUnload::EEMImmediateCargoUnload(long, long, bool), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65af78
// Creates a remove-from-selection message for a unit
// (EEMRemoveUnitFromSelectedGroup::EEMRemoveUnitFromSelectedGroup(long), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65b2a2
// Appends a button to a vector (std::vector<EEButtonObject*>::push_back)
// (std::vector<EEButtonObject*>::push_back, a template COMDAT closing the unit)

// The bar's host (icon_bar.rs EE_SCREENS_IB_OP_*): units and buttons as handles, ioF the op's floats
extern "C" int64_t ee_screenscpp_icon_bar(void* inBar, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    return EEUICUnitIconBar::HostOp(static_cast<EEUICUnitIconBar*>(inBar), inOp, inA, inB, ioF);
}

int64_t EEUICUnitIconBar::HostOp(EEUICUnitIconBar* inBar, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(inA);
    EEButtonObject* theButton = reinterpret_cast<EEButtonObject*>(inA);
    NEMessage* theMessage = NULL;
    switch (inOp) {
    case EE_SCREENS_IB_OP_GROUP_COUNT:
        return theGroup ? (int64_t)theGroup->GetNumUnits() : -1;
    case EE_SCREENS_IB_OP_GROUP_UNIT:
        return (int64_t)theGroup->GetUnit((unsigned long)inA);
    case EE_SCREENS_IB_OP_CARGO_COUNT:
        return (int64_t)theUnit->GetNumCargoUnits();
    case EE_SCREENS_IB_OP_CARGO:
        return (int64_t)theUnit->GetCargo((unsigned long)inB);
    case EE_SCREENS_IB_OP_UNIT_ID:
        return theUnit->GetUnitID();
    case EE_SCREENS_IB_OP_UNIT_TYPE:
        return theUnit->GetUnitClass()->mUnitType;
    case EE_SCREENS_IB_OP_CLASS: {
        EEUnitClass* theClass = theUnit->GetUnitClass();
        return (theClass->IsTransport() ? EE_SCREENS_IB_CLASS_TRANSPORT : 0) |
               (theClass->IsAirBase() ? EE_SCREENS_IB_CLASS_AIR_BASE : 0) |
               (theClass->mIsParatrooperBomber ? EE_SCREENS_IB_CLASS_PARATROOPER_BOMBER : 0) | (int64_t)theClass->mFamily << 8;
    }
    case EE_SCREENS_IB_OP_QUEUED_GOAL:
        return theUnit->GetQueuedGoal((unsigned long)inB) != NULL;
    case EE_SCREENS_IB_OP_NO_GOALS:
        return theUnit->GetQueueGoals(1).size() == 0 || theUnit->mGoalQueues[1].GetFront() == NULL;
    case EE_SCREENS_IB_OP_TILE:
        return (int64_t)(uint32_t)theUnit->GetTileX() | (int64_t)theUnit->GetTileY() << 32;
    case EE_SCREENS_IB_OP_HIT_POINTS:
        return theUnit->GetHitPoints();
    case EE_SCREENS_IB_OP_MAX_HIT_POINTS:
        return theUnit->mStats->mMaxHitPoints;
    case EE_SCREENS_IB_OP_BUTTON:
        return (int64_t)theUnit->mStats->mButton;
    case EE_SCREENS_IB_OP_SELECT_THESE: {
        const int32_t* theIDs = reinterpret_cast<const int32_t*>(inA);
        std::vector<long> theList(theIDs, theIDs + inB);
        theMessage = new EEMSelectTheseUnits(theList);
        break;
    }
    case EE_SCREENS_IB_OP_SELECT:
        theMessage = new EEMSelectUnit((unsigned long)inA);
        break;
    case EE_SCREENS_IB_OP_UNLOAD:
        theMessage = new EEMImmediateCargoUnload((long)inA, (long)inB, false);
        break;
    case EE_SCREENS_IB_OP_SCRAMBLE:
        theMessage = new EEMScrambleAircraft((long)inA, (long)(int32_t)inB, (long)(inB >> 32));
        break;
    case EE_SCREENS_IB_OP_REMOVE:
        theMessage = new EEMRemoveUnitFromSelectedGroup((long)inA);
        break;
    case EE_SCREENS_IB_OP_SOUND:
        gSoundManager->Play(inA == 0 ? sIconBarDownSound : sIconBarUpSound, 1.0f, 0.0f, 1.0f, false, NULL);
        break;
    case EE_SCREENS_IB_OP_DIRTY:
        inBar->SetDirty(true);
        break;
    case EE_SCREENS_IB_OP_CTRL:
        return UIControl::ControlKeyPressed();
    case EE_SCREENS_IB_OP_SHIFT:
        return UIControl::ShiftKeyPressed();
    case EE_SCREENS_IB_OP_MOUSE:
        ioF[0] = inBar->GetMouseLocation().GetX();
        break;
    case EE_SCREENS_IB_OP_CONTAINS:
        return inBar->GetRectangle().Contains(inBar->GetMouseLocation());
    case EE_SCREENS_IB_OP_RECT:
    case EE_SCREENS_IB_OP_DRAW_RECT: {
        GERectangle theRect = inOp == EE_SCREENS_IB_OP_RECT ? inBar->GetRectangle() : inBar->GetCopiedDrawRectangle();
        ioF[0] = theRect.GetLeft();
        ioF[1] = theRect.GetTop();
        ioF[2] = theRect.GetRight();
        ioF[3] = theRect.GetBottom();
        break;
    }
    case EE_SCREENS_IB_OP_RELEASE_MOUSE:
        inBar->ReleaseCapturedMouse();
        break;
    case EE_SCREENS_IB_OP_ACQUIRE:
        theButton->Acquire();
        break;
    case EE_SCREENS_IB_OP_RELEASE:
        theButton->Release();
        break;
    case EE_SCREENS_IB_OP_COPY_BASE:
        return inBar->UIControl::CopyData();
    case EE_SCREENS_IB_OP_DRAW_BASE:
        return inBar->UIControl::DrawPolygons();
    case EE_SCREENS_IB_OP_ICON:
        inBar->mIcon->SetTexture(theButton->GetTexture(), inBar->GetScreenIndex());
        theButton->SetFaceBitmap(inBar->mIcon, GERectangle(ioF[2], ioF[3], ioF[4], ioF[5]), gScreenIndex);
        inBar->mIcon->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        inBar->GetRasterizer()->Draw(inBar->mIcon, inBar->GetScreenIndex());
        break;
    case EE_SCREENS_IB_OP_RHOMBUS: {
        GERhombus theRhombus;
        theRhombus.SetColor(GEColor(inA == 1 ? 1.0f : 0.0f, inA == 2 ? 1.0f : 0.0f, 0.0f));
        theRhombus.SetRectangle(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]));
        inBar->GetRasterizer()->Draw(&theRhombus, inBar->GetScreenIndex());
        break;
    }
    case EE_SCREENS_IB_OP_FRAME:
        inBar->mBackground->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        return inBar->GetRasterizer()->Draw(inBar->mBackground, inBar->GetScreenIndex());
    case EE_SCREENS_IB_OP_LINE: {
        GELine theLine;
        theLine.SetStartPoint(GE2DPoint(ioF[0], ioF[1]));
        theLine.SetEndPoint(GE2DPoint(ioF[2], ioF[3]));
        theLine.SetColor(GEColor(1.0f, 0.0f, 0.0f));
        return gRasterizer->Draw(&theLine, 1, inBar->GetScreenIndex());
    }
    }
    if (theMessage != NULL)
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    return 0;
}
