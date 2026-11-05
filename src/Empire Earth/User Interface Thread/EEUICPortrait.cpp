// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.cpp (file name inferred: no __FILE__; RTTI class
// EEUICPortrait). The main game form's selected-unit portrait: 0x25861c..0x25952f (ctor, ??_G, dtor, then the members
// in alphabetical order with EEPlayer's two inline lookups after their first users; EEUICSelectUnits' init chains at
// 0x25952f). Unit flags: the exe module set.
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
#include "ee/EEUICPortrait.h"
#include "ee/EEStringIDs.h"
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
// Returns the bar rectangles; with both bars the research bar is the top half
void EEUICPortrait::GetBarRectangles(GERectangle& outFirst, GERectangle& outSecond) const
{
    outFirst = this->GetRectangle();
    outSecond = this->GetRectangle();
    if (this->mShowSecondBar == true && this->mShowFirstBar == true) {
        outSecond.SetBottom(outSecond.GetHeight() * 0.5f + outSecond.GetTop());
        outFirst.SetTop(outSecond.GetTop() + 1.0f);
    }
}

// FUNCTION: EXE 0x658987
// Draws the portrait and each shown progress bar with the red remainder after it
long EEUICPortrait::DrawPolygons()
{
    GERectangle theRectangle;
    long theResult = UIControl::DrawPolygons();
    theResult = this->GetRasterizer()->Draw(this->mPortrait, this->GetScreenIndex());
    if (this->mShowFirstBar) {
        theResult = this->GetRasterizer()->Draw(this->mFirstBar, this->GetScreenIndex());
        theRectangle.SetLeft(this->mFirstBar->GetRectangle().GetRight());
        theRectangle.SetRight(this->GetCopiedDrawRectangle().GetRight() + 1.0f);
        theRectangle.SetTop(this->mFirstBar->GetRectangle().GetTop());
        theRectangle.SetBottom(this->mFirstBar->GetRectangle().GetBottom());
        this->mBackground->SetRectangle(theRectangle);
        theResult = this->GetRasterizer()->Draw(this->mBackground, this->GetScreenIndex());
    }
    if (!this->mShowSecondBar)
        return theResult;
    theResult = this->GetRasterizer()->Draw(this->mSecondBar, this->GetScreenIndex());
    theRectangle.SetLeft(this->mSecondBar->GetRectangle().GetRight());
    theRectangle.SetRight(this->GetCopiedDrawRectangle().GetRight() + 1.0f);
    theRectangle.SetTop(this->mSecondBar->GetRectangle().GetTop());
    theRectangle.SetBottom(this->mSecondBar->GetRectangle().GetBottom());
    this->mBackground->SetRectangle(theRectangle);
    theResult = this->GetRasterizer()->Draw(this->mBackground, this->GetScreenIndex());
    return theResult;
}

// FUNCTION: EXE 0x658ad0
// Draws the improvement level when it is above 0
long EEUICPortrait::DrawTextA()
{
    this->mDrawCount--;
    long theResult = 0;
    if (this->mLevel > 0)
        theResult = this->GetForm()->DrawClippedText(this->GetCopiedDrawRectangle(), this->mText, this);
    return theResult;
}

// FUNCTION: EXE 0x658af7
// Builds the help text for the bar under the mouse: cancel the research (buildings) or the improvement
bool EEUICPortrait::GetHelpString(UWideString& outString) const
{
    GERectangle theFirst;
    GERectangle theSecond;
    UWideString theName;
    UWideString theFormat;
    UWideString theResult;
    bool theFound = true;
    if (this->mShowSecondBar == true && this->mShowFirstBar == true) {
        this->GetBarRectangles(theFirst, theSecond);
        if (theSecond.Contains(this->GetMouseLocation()) && this->mUnit->GetUnitClass()->IsBuilding() == true) {
            gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH, theFormat);
            long theID = this->mUnit->GetComplexUnit()->mCurrentResearch;
            EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(theID);
            if (theNode) {
                if (theNode->mData->mMakesUnit == true)
                    gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theName);
                else
                    gResource->GetString(theNode->mData->mNameID, theName);
                sprintf(theResult, theFormat, (const char*)theName);
            } else
                theFound = false;
        } else {
            gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT, theFormat);
            this->mUnit->mStats->GetImprovementName(theName,
                                    this->mUnit->GetPlayer()->GetResearchedImprovement(this->mUnit->GetUnitClass()->GetUnitType()));
            sprintf(theResult, theFormat, (const char*)theName);
        }
    } else if (this->mShowSecondBar == true) {
        gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT, theFormat);
        this->mUnit->mStats->GetImprovementName(theName, this->mUnit->GetPlayer()->GetResearchedImprovement(this->mUnit->GetUnitClass()->GetUnitType()));
        sprintf(theResult, theFormat, (const char*)theName);
    } else if (this->mShowFirstBar == true && this->mUnit->GetUnitClass()->IsBuilding() == true) {
        gResource->GetString(IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH, theFormat);
        long theID = this->mUnit->GetComplexUnit()->mCurrentResearch;
        EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->GetTechTree()->Get(theID);
        if (theNode) {
            if (theNode->mData->mMakesUnit == true)
                gResource->GetString(gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID, theName);
            else
                gResource->GetString(theNode->mData->mNameID, theName);
            sprintf(theResult, theFormat, (const char*)theName);
        } else
            theFound = false;
    } else
        theFound = false;
    outString = theResult;
    return theFound;
}

// FUNCTION: EXE 0x658cb8
// Returns the improvement the player is researching for a unit type
// (EEPlayer::GetResearchedImprovement, inline in EEPlayer.h; emitted after its first user)

// FUNCTION: EXE 0x658d02
// On a click on the portrait, cancels the research (top bar of a building) or the improvement in progress
// NEAR: register assignment (orig: this esi, the complex unit ebx, the technology edi and in memory)
long EEUICPortrait::MouseLButtonDown()
{
    GERectangle theFirst;
    GERectangle theSecond;
    if (this->mUnit && this->mUnit->mHitPoints > 0 && this->mUnit->mTileNode) {
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        long theTechnology = theUnit->mCurrentResearch;
        long theImprovement = theUnit->mPlayer->GetResearchedImprovement(theUnit->GetUnitClass()->mUnitType);
        if (theTechnology != kTechNone) {
            if (theImprovement != 0x10) {
                this->GetBarRectangles(theFirst, theSecond);
                if (this->mShowFirstBar && theSecond.Contains(this->GetMouseLocation())) {
                    if (this->mUnit->GetUnitClass()->IsBuilding()) {
                        gCommunications->ProcessLocalUserInput(
                            new EEMRemoveTechnology(theUnit->mUnitID, theTechnology, true), kSlotNone);
                        gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                    }
                } else if (this->mShowSecondBar) {
                    gCommunications->ProcessLocalUserInput(new EEMRemoveImprovement(theUnit->mUnitID, theImprovement),
                                                           kSlotNone);
                    gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
                }
            } else if (this->mUnit->GetUnitClass()->IsBuilding()) {
                gCommunications->ProcessLocalUserInput(new EEMRemoveTechnology(theUnit->GetUnitID(), theTechnology, true),
                                                       kSlotNone);
                gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
            }
        } else if (theImprovement != 0x10) {
            gCommunications->ProcessLocalUserInput(new EEMRemoveImprovement(theUnit->mUnitID, theImprovement), kSlotNone);
            gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        }
    }
    return 0;
}

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
// Shows a unit's portrait (button face), taking references, and updates the bars
void EEUICPortrait::SetUnit(EEUnit* inUnit)
{
    if (this->mUnit) {
        this->mButton->Release();
        this->mUnit->Dereference();
    }
    this->mUnit = inUnit;
    if (!this->mUnit)
        return;
    this->mUnit->Reference();
    this->mButton = this->mUnit->mStats->mButton;
    this->mButton->Acquire();
    this->mPortrait->SetTexture(this->mButton->GetTexture(), gScreenIndex);
    this->mButton->SetFaceBitmap(this->mPortrait, this->GetRectangle(), gScreenIndex);
    this->mShowFirstBar = false;
    this->mShowSecondBar = false;
    this->SetDirty(true);
    this->Update();
}

// FUNCTION: EXE 0x659028
// Updates the portrait for the local player's unit: research and improvement progress bars and the
// improvement level (capped by world table entry 47)
// NEAR: register and slot choice; the both-bars branch expands GERhombus::SetRectangle and
// GetTopLeft calls the imported GE2DPoint ctors in the original)
void EEUICPortrait::Update()
{
    GERectangle theRectangle;
    unsigned long theOldLevel = this->mLevel;
    bool theOldSecond = this->mShowSecondBar;
    bool theOldFirst = this->mShowFirstBar;
    EEComplexUnit* theUnit = this->mUnit->GetComplexUnit();
    if (theUnit != NULL) {
        if (theUnit->GetPlayer() == gWorld.GetCurrentPlayer()) {
            if (theUnit->mProductionTime > 0) {
                this->mShowFirstBar = true;
                this->SetDirty(true);
            } else if (this->mShowFirstBar) {
                this->mShowFirstBar = false;
                this->SetDirty(true);
            }
            long theImprovement = theUnit->GetPlayer()->GetResearchedImprovement(theUnit->GetUnitClass()->mUnitType);
            if (theImprovement != 0x10) {
                this->mShowSecondBar = true;
                this->SetDirty(true);
            } else if (this->mShowSecondBar) {
                this->mShowSecondBar = false;
                this->SetDirty(true);
            }
            if (this->mShowSecondBar == true && this->mShowFirstBar == true) {
                EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->mTechnologies->Get(theUnit->mCurrentResearch);
                float theScale;
                if (theNode->mData->mMakesUnit && this->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass])
                    theScale = this->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass]->mBuildTimeScale;
                else
                    theScale = 1.0f;
                long theTotal = (long)(theNode->GetBuildTime() * theScale + 0.5f);
                GE2DPoint theTopLeft = this->GetRectangle().GetTopLeft();
                theRectangle.SetTop(theTopLeft.GetY());
                theRectangle.SetLeft(theTopLeft.GetX());
                theRectangle.SetBottom(this->GetRectangle().GetHeight() * 0.5f + theRectangle.GetTop() + 1.0f);
                theRectangle.SetRight(this->GetRectangle().GetWidth() * ((float)(theTotal - theUnit->mProductionTime) / theTotal) +
                                      theTopLeft.GetX());
                this->mFirstBar->SetRectangle(theRectangle);
                long theMaximum;
                if (theUnit->mStats->mImprovement.GetLevel(theImprovement) == 1)
                    theMaximum = gWorldTable.GetLongValue(theImprovement + 0x39);
                else
                    theMaximum = gWorldTable.GetLongValue2(theImprovement + 0x39);
                theRectangle.SetLeft(this->GetRectangle().GetLeft());
                float theMaximumF = theMaximum;
                theRectangle.SetRight((theMaximumF - theUnit->GetPlayer()->GetImprovementProgress(
                                                         theUnit->GetUnitClass()->mUnitType)) / theMaximumF *
                                      this->GetRectangle().GetWidth() + this->GetRectangle().GetLeft() + 1.0f);
                theRectangle.SetTop(this->GetRectangle().GetHeight() * 0.5f + this->GetRectangle().GetTop());
                theRectangle.SetBottom(this->GetRectangle().GetBottom() + 1.0f);
                this->mSecondBar->SetRectangle(theRectangle);
            } else if (this->mShowSecondBar == true) {
                long theMaximum;
                if (theUnit->mStats->mImprovement.GetLevel(theImprovement) == 1)
                    theMaximum = gWorldTable.GetLongValue(theImprovement + 0x39);
                else
                    theMaximum = gWorldTable.GetLongValue2(theImprovement + 0x39);
                theRectangle.SetLeft(this->GetRectangle().GetLeft());
                float theMaximumF = theMaximum;
                theRectangle.SetRight((theMaximumF - theUnit->GetPlayer()->GetImprovementProgress(
                                                         theUnit->GetUnitClass()->mUnitType)) / theMaximumF *
                                      this->GetRectangle().GetWidth() + this->GetRectangle().GetLeft() + 1.0f);
                theRectangle.SetTop(this->GetRectangle().GetTop());
                theRectangle.SetBottom(this->GetRectangle().GetBottom() + 1.0f);
                this->mSecondBar->SetRectangle(theRectangle);
            } else if (this->mShowFirstBar == true) {
                EETechTreeNode* theNode = gWorld.GetCurrentPlayer()->mTechnologies->Get(theUnit->mCurrentResearch);
                if (theNode) {
                    float theScale;
                    if (theNode->mData->mMakesUnit && this->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass])
                        theScale = this->mUnit->GetPlayer()->mUnitClassUpgrades[theNode->mData->mUnitClass]->mBuildTimeScale;
                    else
                        theScale = 1.0f;
                    long theTotal = (long)(theNode->GetBuildTime() * theScale + 0.5f);
                    theRectangle.SetTop(this->GetRectangle().GetTop());
                    theRectangle.SetLeft(this->GetRectangle().GetLeft());
                    theRectangle.SetBottom(this->GetRectangle().GetHeight() + theRectangle.GetTop());
                    theRectangle.SetRight(this->GetRectangle().GetWidth() * ((float)(theTotal - theUnit->mProductionTime) / theTotal) +
                                          this->GetRectangle().GetLeft());
                    this->mFirstBar->SetRectangle(theRectangle);
                }
            }
        }
        this->mLevel = theUnit->mStats->mImprovement.mTotalCost;
    } else
        this->mLevel = 0;
    if (this->mLevel > gWorldTable.GetLongValue(47))
        this->mLevel = gWorldTable.GetLongValue(47);
    if (this->mLevel != theOldLevel) {
        UWideString* theText = new UWideString;
        if (this->mLevel > 0)
            sprintf(*theText, "%ld", this->mLevel);
        this->mText->SetText(*theText);
        delete theText;
    }
    if (theOldSecond != this->mShowSecondBar || theOldFirst != this->mShowFirstBar)
        this->GetForm()->UpdateHelpString();
}

// FUNCTION: EXE 0x6594e8
// Returns the player's improvement progress for a unit type
// (EEPlayer::GetImprovementProgress, inline in EEPlayer.h; emitted after its first user)
