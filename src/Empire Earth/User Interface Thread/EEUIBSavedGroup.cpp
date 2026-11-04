// C:\EEX\Empire Earth\User Interface Thread\EEUIBSavedGroup.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBSavedGroup). The main game form's saved-group buttons: 0x255c2a..0x25619b (ctor, ??_G, dtor, the overrides,
// EEMSavedGroupClear's inline ctor after its first user, then Refresh and SetUnit; the EEUIBuildGate family follows at
// 0x25619b). Unit flags: the exe module set.
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
#include "lle/UIControlsTable.h"
#include "ee/EEUIBSavedGroup.h"

static unsigned long sSavedGroupUpSound;        // 0x564568 (sound 0x208)
static unsigned long sSavedGroupDownSound;      // 0x56456c (sound 0x207)

// FUNCTION: EXE 0x655c2a
// Creates a saved-group button with its pressed bitmaps, an empty portrait and the two button sounds
EEUIBSavedGroup::EEUIBSavedGroup(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
    : UIButton(inTable.GetID(), NULL, NULL, NULL, inRectangle, inForm, inTable.GetFormIndex(), 0),
      mHasUnit(false), mDrawPortrait(false)
{
    this->mPortrait = new GEBitmap;
    GEBitmap* theBitmap = inTable.CreateNormalBitmap(inForm->GetManager(), inRectangle);
    this->SetNormalBitmap(theBitmap);
    theBitmap->Dereference();
    theBitmap = inTable.CreatePressedBitmap(inForm->GetManager(), inRectangle);
    this->SetPressedBitmap(theBitmap);
    theBitmap->Dereference();
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound) {
        theSound->LoadMedia();
        sSavedGroupDownSound = theSound->GetSampleID();
    }
    theSound = EESoundObject::Get(kSoundButton2);
    if (theSound) {
        theSound->LoadMedia();
        sSavedGroupUpSound = theSound->GetSampleID();
    }
    this->mTexture = NULL;
}

// SYNTHETIC: EXE 0x655d57
// (the scalar deleting destructor, emitted with the vtable; slot EEUIBSavedGroup+0x0)

// FUNCTION: EXE 0x655d73
// Destroys the saved-group button, releasing its portrait and texture and unloading its sounds
EEUIBSavedGroup::~EEUIBSavedGroup()
{
    this->mPortrait->Dereference();
    if (this->mTexture) {
        this->mTexture->Dereference();
        this->mTexture = NULL;
    }
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound)
        theSound->UnloadMedia();
    EESoundObject::Get(kSoundButton2)->UnloadMedia();
}

// FUNCTION: EXE 0x655dd5
// Snapshots the button for drawing: whether to draw the unit portrait, with its texture
long EEUIBSavedGroup::CopyData()
{
    this->mDrawPortrait = this->mHasUnit;
    if (this->mDrawPortrait)
        this->mPortrait->SetTexture(this->mTexture, this->GetScreenIndex());
    return UIButton::CopyData();
}

// FUNCTION: EXE 0x655e0b
// Draws the group's unit portrait under the button
long EEUIBSavedGroup::DrawPolygons()
{
    if (this->mDrawPortrait) {
        this->mPortrait->SetLocation(this->mBitmapLocation);
        this->GetRasterizer()->Draw(this->mPortrait, this->GetScreenIndex());
    }
    return UIButton::DrawPolygons();
}

// FUNCTION: EXE 0x655e66
// On a double click, sends a select-saved-group message with its double-click flag set (if it has units)
long EEUIBSavedGroup::MouseLButtonDoublePressed()
{
    unsigned long theIndex = this->GetIndex();
    if (gWorld.GetCurrentPlayer()) {
        if (this->mHasUnit == true)
            gCommunications->ProcessLocalUserInput(new EEMSavedGroupSelect(theIndex - 0xd4, true), kSlotNone);
        gSoundManager->Play(sSavedGroupDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
    }
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x655ee7
// On a click, adds the selection to the group (shift), saves the selection as the group (empty group) or
// selects the group, and plays the button sound
// NEAR: register permutation only (orig: this edi, player esi, message ebx); declaration order and NULL tests do not move it
long EEUIBSavedGroup::MouseLButtonUp()
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned char theGroup = this->GetIndex() - 0xd4;
    EEMessage* theMessage = NULL;
    if (thePlayer) {
        if (UIControl::ShiftKeyPressed()) {
            theMessage = new EEMSavedGroupAddSelection(theGroup);
            if (!this->mHasUnit)
                this->SetUnit(thePlayer->mSelectedGroup->GetFirstUnit());
        } else if (!this->mHasUnit) {
            theMessage = new EEMSavedGroupCreate(theGroup);
            this->SetUnit(thePlayer->mSelectedGroup->GetFirstUnit());
        } else
            theMessage = new EEMSavedGroupSelect(theGroup, false);
        gSoundManager->Play(sSavedGroupDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    }
    return UIButton::MouseLButtonUp();
}

// FUNCTION: EXE 0x655fce
// On a right click, clears the saved group and plays the release sound
long EEUIBSavedGroup::MouseRButtonUp()
{
    unsigned long theIndex = this->GetIndex();
    if (this->mHasUnit == true) {
        this->mHasUnit = false;
        gCommunications->ProcessLocalUserInput(new EEMSavedGroupClear(theIndex - 0xd4), kSlotNone);
    } else
        this->mHasUnit = false;
    UIButton::MouseLButtonUp();
    gSoundManager->Play(sSavedGroupUpSound, 1.0f, 0.0f, 1.0f, false, NULL);
    return 0;
}

// FUNCTION: EXE 0x65604f
// Creates a clear-saved-group message for a group index
// (inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65607c
// Shows the first unit of the player's saved group, or no portrait when the group is empty
void EEUIBSavedGroup::Refresh()
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (!thePlayer)
        return;
    EEGroup* theGroup = thePlayer->mGroups[this->GetIndex() - 0xd4];
    if (theGroup && theGroup->GetNumUnits() > 0)
        this->SetUnit(theGroup->GetFirstUnit());
    else {
        // Test before SetUnit(NULL), which clears mHasUnit (the redraw after it could never run).
        bool theHadUnit = this->mHasUnit;
        this->SetUnit(NULL);
        if (theHadUnit) {
            this->SetDirty(true);
        }
    }
}

// FUNCTION: EXE 0x6560e1
// Shows a unit's portrait on the button (marked dirty when the portrait changes; always, once it had one)
void EEUIBSavedGroup::SetUnit(EEComplexUnit* inUnit)
{
    if (this->mTexture) {
        this->mTexture->Dereference();
        this->mTexture = NULL;
    }
    if (inUnit) {
        EEButtonObject* theButton = inUnit->mStats->mButton;
        theButton->SetFaceBitmap(this->mPortrait, this->GetRectangle(), gScreenIndex);
        if (!this->mHasUnit) {
            this->mHasUnit = true;
            this->mPortraitX = theButton->GetCellU();
            this->mPortraitY = theButton->GetCellV();
            this->mTexture = theButton->GetTexture();
            this->SetDirty(true);
        } else if (this->mTexture != theButton->GetTexture() || theButton->GetCellU() != this->mPortraitX ||
                   theButton->GetCellV() != this->mPortraitY) {
            this->mPortraitX = theButton->GetCellU();
            this->mPortraitY = theButton->GetCellV();
            this->mTexture = theButton->GetTexture();
            this->SetDirty(true);
        }
        this->mTexture->Reference();
    } else
        this->mHasUnit = false;
}
