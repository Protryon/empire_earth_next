// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.cpp (file name inferred: no __FILE__; RTTI class
// EEUICPortrait). The main game form's selected-unit portrait: 0x25861c..0x25952f (ctor, ??_G, dtor, then the members
// in alphabetical order with EEPlayer's two inline lookups after their first users; EEUICSelectUnits' init chains at
// 0x25952f). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
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
#include "ee/EEUICPortrait.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"   // ee_screens_portrait_* (ee_screens portrait.rs)
// FUNCTION: EXE 0x65861c
// Creates the selected-unit portrait with its research (green) and improvement (yellow) progress bars over a
// red background, and a level text
EEUICPortrait::EEUICPortrait(UIForm* inForm, const GERectangle& inRectangle, unsigned long inId,
                             unsigned long inIndex, bool inShowLevel)
    : UIControl(kControlTypeCustom, inId, inRectangle, inForm, inIndex, 0)
{
    this->mPortrait = new GEBitmap;
    this->mPortrait->SetLocation(inRectangle.GetTopLeft());
    this->mFirstBar = new GERhombus;
    this->mFirstBar->SetColor(GEColor(0.15f, 1.0f, 0.15f));
    this->mFirstBar->SetOpacity(0.5f);
    this->mSecondBar = new GERhombus;
    this->mSecondBar->SetColor(GEColor(1.0f, 1.0f, 0.15f));
    this->mSecondBar->SetOpacity(0.5f);
    this->mBackground = new GERhombus;
    this->mBackground->SetRectangle(inRectangle);
    this->mBackground->SetColor(GEColor(1.0f, 0.15f, 0.15f));
    this->mBackground->SetOpacity(0.5f);
    this->mShowFirstBar = false;
    this->mShowSecondBar = false;
    this->mUnit = NULL;
    this->mLevel = 0;
    this->mShowLevel = inShowLevel;
    this->mText = new GEText(UIFontTable::CreateClosestFont(0x36,
                                                      inRectangle.GetHeight() * 0.5f));
    this->mText->SetAlignment(kTextAlignRight);
    this->mText->SetIsTransparent(true);
    this->mText->SetFgColor(GEColor());
    this->mText->SetLocation(inRectangle.GetTopRight());
}

// SYNTHETIC: EXE 0x658862
// (the scalar deleting destructor, emitted with the vtable; slot EEUICPortrait+0x0)

// FUNCTION: EXE 0x65887e
// Destroys the portrait, releasing its unit and button
EEUICPortrait::~EEUICPortrait()
{
    delete this->mFirstBar;
    delete this->mBackground;
    delete this->mSecondBar;
    delete this->mText;
    if (this->mUnit) {
        this->mButton->Release();
        this->mUnit->Dereference();
    }
    this->mPortrait->Dereference();
}

// FUNCTION: EXE 0x65891c
// FUNCTION: EXE 0x658987
// FUNCTION: EXE 0x658af7
// FUNCTION: EXE 0x658d02
// FUNCTION: EXE 0x659028
// The bars' rectangles, the drawing, the tooltip, the click and the update from the unit (the original's
// GetBarRectangles, DrawPolygons, GetHelpString, MouseLButtonDown and Update): ee_screens portrait.rs over the host
// below
long EEUICPortrait::DrawPolygons() { return ee_screens_portrait_draw(this); }
long EEUICPortrait::MouseLButtonDown() { return ee_screens_portrait_mouse_down(this); }
void EEUICPortrait::Update() { ee_screens_portrait_update(this); }

// the tooltip being built (the UI thread's)
static UWideString* sPortraitHelp;

bool EEUICPortrait::GetHelpString(UWideString& outString) const
{
    UWideString theResult;
    sPortraitHelp = &theResult;
    bool theFound = ee_screens_portrait_help(const_cast<EEUICPortrait*>(this));
    outString = theResult;
    return theFound;
}

// FUNCTION: EXE 0x658ad0
// Draws the improvement level when it is above 0
long EEUICPortrait::DrawTextA()
{
    this->UIControl::DrawTextA();   // the base's draw count
    long theResult = 0;
    if (this->mLevel > 0)
        theResult = this->GetForm()->DrawClippedText(this->GetCopiedDrawRectangle(), this->mText, this);
    return theResult;
}

// FUNCTION: EXE 0x658cb8
// Returns the improvement the player is researching for a unit type
// (EEPlayer::GetResearchedImprovement, inline in EEPlayer.h; emitted after its first user)

// FUNCTION: EXE 0x658f5d
// Updates the help text while the mouse moves over a portrait showing both bars
long EEUICPortrait::MouseMove(const GE2DPoint& inLocation)
{
    if (this->mShowFirstBar && this->mShowSecondBar)
        this->GetForm()->UpdateHelpString();
    return 0;
}

// FUNCTION: EXE 0x658f7d
// Ignores right presses on the portrait
// (inline in the class)

// FUNCTION: EXE 0x658f82
// Shows a unit's portrait (button face), taking references, and updates the bars (ee_screens portrait.rs)
void EEUICPortrait::SetUnit(EEUnit* inUnit)
{
    ee_screens_portrait_set_unit(this, inUnit);
}

// FUNCTION: EXE 0x6594e8
// Returns the player's improvement progress for a unit type
// (EEPlayer::GetImprovementProgress, inline in EEPlayer.h; emitted after its first user)

// The portrait's host (portrait.rs EE_SCREENS_PT_OP_*): units and nodes as handles, ioF the op's floats
extern "C" int64_t ee_screenscpp_portrait(void* inPortrait, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    return EEUICPortrait::HostOp(static_cast<EEUICPortrait*>(inPortrait), inOp, inA, inB, ioF);
}

int64_t EEUICPortrait::HostOp(EEUICPortrait* inPortrait, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(inA);
    EETechTreeNode* theNode = reinterpret_cast<EETechTreeNode*>(inA);
    GERhombus* theBars[3] = {inPortrait->mFirstBar, inPortrait->mSecondBar, inPortrait->mBackground};
    GERectangle theRect;
    UWideString theFormat;
    UWideString theName;
    float theFloat;
    uint32_t theBits;
    NEMessage* theMessage = NULL;
    switch (inOp) {
    case EE_SCREENS_PT_OP_HAS_UNIT: return inPortrait->mUnit != NULL;
    case EE_SCREENS_PT_OP_RELEASE:
        inPortrait->mButton->Release();
        inPortrait->mUnit->Dereference();
        return 0;
    case EE_SCREENS_PT_OP_SET_UNIT: inPortrait->mUnit = reinterpret_cast<EEUnit*>(inA); return 0;
    case EE_SCREENS_PT_OP_ACQUIRE:
        inPortrait->mUnit->Reference();
        inPortrait->mButton = inPortrait->mUnit->mStats->mButton;
        inPortrait->mButton->Acquire();
        return 0;
    case EE_SCREENS_PT_OP_FACE:
        inPortrait->mPortrait->SetTexture(inPortrait->mButton->GetTexture(), gScreenIndex);
        inPortrait->mButton->SetFaceBitmap(inPortrait->mPortrait, inPortrait->GetRectangle(), gScreenIndex);
        return 0;
    case EE_SCREENS_PT_OP_STATE:
        return (int64_t)inPortrait->mLevel | (int64_t)inPortrait->mShowFirstBar << 32 | (int64_t)inPortrait->mShowSecondBar << 33;
    case EE_SCREENS_PT_OP_SET_STATE:
        inPortrait->mLevel = (unsigned long)(inA & 0xffffffff);
        inPortrait->mShowFirstBar = (inA >> 32 & 1) != 0;
        inPortrait->mShowSecondBar = (inA >> 33 & 1) != 0;
        return 0;
    case EE_SCREENS_PT_OP_COMPLEX:
        return (int64_t)inPortrait->mUnit->GetComplexUnit();
    case EE_SCREENS_PT_OP_LOCAL:
        return theUnit->GetPlayer() == gWorld.GetCurrentPlayer();
    case EE_SCREENS_PT_OP_PRODUCTION:
        return theUnit->mProductionTime;
    case EE_SCREENS_PT_OP_DIRTY:
        inPortrait->SetDirty(true);
        return 0;
    case EE_SCREENS_PT_OP_RESEARCHED:
        return theUnit->GetPlayer()->GetResearchedImprovement(theUnit->GetUnitClass()->GetUnitType());
    case EE_SCREENS_PT_OP_RESEARCH:
        return theUnit->mCurrentResearch;
    case EE_SCREENS_PT_OP_NODE:
        return (int64_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inA);
    case EE_SCREENS_PT_OP_SCALE:
        if (!theNode->mData->mMakesUnit || !inPortrait->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass])
            return -1;
        memcpy(&theBits, &inPortrait->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass]->mBuildTimeScale, 4);
        return theBits;
    case EE_SCREENS_PT_OP_BUILD_TIME:
        return theNode->GetBuildTime();
    case EE_SCREENS_PT_OP_RECT:
    case EE_SCREENS_PT_OP_BAR:
    case EE_SCREENS_PT_OP_DRAW_RECT:
        theRect = inOp == EE_SCREENS_PT_OP_RECT ? inPortrait->GetRectangle()
                : inOp == EE_SCREENS_PT_OP_BAR  ? theBars[inA]->GetRectangle() : inPortrait->GetCopiedDrawRectangle();
        ioF[0] = theRect.GetLeft();
        ioF[1] = theRect.GetTop();
        ioF[2] = theRect.GetRight();
        ioF[3] = theRect.GetBottom();
        return 0;
    case EE_SCREENS_PT_OP_SET_BAR:
        theBars[inA]->SetRectangle(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]));
        return 0;
    case EE_SCREENS_PT_OP_IMPROVEMENT_LEVEL:
        return theUnit->GetStats()->mImprovement.GetLevel((long)inB);
    case EE_SCREENS_PT_OP_IMPROVEMENT_TIME:
        return inA == 1 ? gWorldTable.GetLongValue(EEWorldImprovementTime((long)inB))
                        : gWorldTable.GetLongValue2(EEWorldImprovementTime((long)inB));
    case EE_SCREENS_PT_OP_PROGRESS:
        theFloat = theUnit->GetPlayer()->GetImprovementProgress(theUnit->GetUnitClass()->GetUnitType());
        memcpy(&theBits, &theFloat, 4);
        return theBits;
    case EE_SCREENS_PT_OP_TOTAL_COST:
        return theUnit->GetStats()->mImprovement.mTotalCost;
    case EE_SCREENS_PT_OP_POINTS_MAX:
        return gWorldTable.GetLongValue(kWorldUnitImprovementPointsMax);
    case EE_SCREENS_PT_OP_TEXT:
        inPortrait->mText->SetText(UWideString(std::string(reinterpret_cast<const char*>(inA), (size_t)inB).c_str()));
        return 0;
    case EE_SCREENS_PT_OP_UPDATE_HELP:
        inPortrait->GetForm()->UpdateHelpString();
        return 0;
    case EE_SCREENS_PT_OP_DRAW_BASE:
        return inPortrait->UIControl::DrawPolygons();
    case EE_SCREENS_PT_OP_DRAW:
        if (inA == 0)
            return inPortrait->GetRasterizer()->Draw(inPortrait->mPortrait, inPortrait->GetScreenIndex());
        return inPortrait->GetRasterizer()->Draw(inA == 3 ? theBars[2] : theBars[inA - 1], inPortrait->GetScreenIndex());
    case EE_SCREENS_PT_OP_CONTAINS:
        return GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]).Contains(inPortrait->GetMouseLocation());
    case EE_SCREENS_PT_OP_BUILDING:
        return inPortrait->mUnit->GetUnitClass()->IsBuilding();
    case EE_SCREENS_PT_OP_HELP_RESEARCH:
        gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH, theFormat);
        if (theNode->mData->mMakesUnit == true)
            gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theName);
        else
            gResource->GetString(theNode->mData->mNameID, theName);
        sprintf(*sPortraitHelp, theFormat, (const char*)theName);
        return 0;
    case EE_SCREENS_PT_OP_HELP_IMPROVEMENT:
        gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT, theFormat);
        inPortrait->mUnit->mStats->GetImprovementName(
            theName, inPortrait->mUnit->GetPlayer()->GetResearchedImprovement(inPortrait->mUnit->GetUnitClass()->GetUnitType()));
        sprintf(*sPortraitHelp, theFormat, (const char*)theName);
        return 0;
    case EE_SCREENS_PT_OP_ALIVE:
        return inPortrait->mUnit && inPortrait->mUnit->mHitPoints > 0 && inPortrait->mUnit->mTileNode;
    case EE_SCREENS_PT_OP_COMPLEX_FIELD:
        return (int64_t)(EEComplexUnit*)inPortrait->mUnit->mComplexUnit;
    case EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY:
        theMessage = new EEMRemoveTechnology(theUnit->GetUnitID(), (long)inB, true);
        break;
    case EE_SCREENS_PT_OP_REMOVE_IMPROVEMENT:
        theMessage = new EEMRecord(kMessageRemoveImprovement, {theUnit->GetUnitID(), (long)inB});
        break;
    case EE_SCREENS_PT_OP_SOUND:
        gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    }
    if (theMessage != NULL)
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    return 0;
}
