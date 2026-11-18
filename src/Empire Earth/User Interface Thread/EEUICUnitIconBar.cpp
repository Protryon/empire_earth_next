// C:\EEX\Empire Earth\User Interface Thread\EEUICUnitIconBar.cpp (file name inferred: no __FILE__; RTTI class
// EEUICUnitIconBar). The main game form's selected-units icon bar: 0x259f7f..0x25b2ce (ctor, ??_G, dtor, the members,
// the message ctors after their first users, vector<EEButtonObject*>::push_back last; EEUICSelectUnits ends at
// 0x259f7f, EEUICWonNet* follows). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/GELine.h"
#include "lle/UIControlsTable.h"
#include "ee/EEUICUnitIconBar.h"


static unsigned long sIconBarUpSound;           // 0x56458c (sound 0x208)
static unsigned long sIconBarDownSound;         // 0x564590 (sound 0x207)

// Creates the selected-units icon bar with its icon frame bitmap and click sounds (its state and icon rectangle:
// ee_screens icon_bar.rs)
EEUICUnitIconBar::EEUICUnitIconBar(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
    : EEUIRustControl(inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), 0), mIcon(new GEBitmap)
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

// The snapshot, the drawing, the clicks and the refill from the selection (and the original's GetUnit,
// GetIconIndex, GetUnitIDs): ee_screens icon_bar.rs over the host below
long EEUICUnitIconBar::CopyData() { return ee_screens_icon_bar_copy_data(this->mState, this); }
long EEUICUnitIconBar::DrawPolygons() { return ee_screens_icon_bar_draw(this->mState, this); }
long EEUICUnitIconBar::MouseLButtonUp() { return ee_screens_icon_bar_mouse_l_up(this->mState, this); }
long EEUICUnitIconBar::MouseRButtonUp() { return ee_screens_icon_bar_mouse_r_up(this->mState, this); }
long EEUICUnitIconBar::MouseMove(const GE2DPoint& inLocation) { return ee_screens_icon_bar_mouse_move(this->mState, this); }
void EEUICUnitIconBar::Update() { ee_screens_icon_bar_update(this->mState, this); }

// Captures the mouse on a right press (also the left press, ICF-folded)
long EEUICUnitIconBar::MouseRButtonDown()
{
    this->CaptureMouse();
    return 0;
}

// The bar's own ops (icon_bar.rs EE_SCREENS_IB_OP_*, EEUIRustControl's host the toolkit's): units and buttons as
// handles, ioF the op's floats; the selection's and the units' reads are hud_views.rs'
int64_t EEUICUnitIconBar::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EEUICUnitIconBar* inBar = this;
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(inA);
    EEButtonObject* theButton = reinterpret_cast<EEButtonObject*>(inA);
    NEMessage* theMessage = NULL;
    switch (inOp - EE_SCREENS_HC_CLASS) {
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
    case EE_SCREENS_IB_OP_SELECT_THESE: {
        const int32_t* theIDs = reinterpret_cast<const int32_t*>(inA);
        std::vector<long> theList(theIDs, theIDs + inB);
        theMessage = new EEMRecord(kMessageSelectTheseUnits, {theList});
        break;
    }
    case EE_SCREENS_IB_OP_SOUND:
        gSoundManager->Play(inA == 0 ? sIconBarDownSound : sIconBarUpSound, 1.0f, 0.0f, 1.0f, false, NULL);
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
    case EE_SCREENS_IB_OP_ACQUIRE:
        theButton->Acquire();
        break;
    case EE_SCREENS_IB_OP_RELEASE:
        theButton->Release();
        break;
    case EE_SCREENS_IB_OP_ICON:
        inBar->mIcon->SetTexture(theButton->GetTexture(), inBar->GetScreenIndex());
        theButton->SetFaceBitmap(inBar->mIcon, GERectangle(ioF[2], ioF[3], ioF[4], ioF[5]), gScreenIndex);
        inBar->mIcon->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        inBar->GetRasterizer()->Draw(inBar->mIcon, inBar->GetScreenIndex());
        break;
    case EE_SCREENS_IB_OP_FRAME:
        inBar->mBackground->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        return inBar->GetRasterizer()->Draw(inBar->mBackground, inBar->GetScreenIndex());
    }
    if (theMessage != NULL)
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    return 0;
}
