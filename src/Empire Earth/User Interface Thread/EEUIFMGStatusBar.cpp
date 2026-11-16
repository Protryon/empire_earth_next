// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBar.cpp (__FILE__ 0x4cdcb0, ESVERIFY lines 125/148). An
// improvement button drawn as an icon, a status bar and two value labels (RTTI EEUIFMGStatusBar > EEUIBUpgrade;
// vtable 0x44acc4): 0x2691b2..0x269ca8 (ctor, ??_G, GEFontBitmap's local ??_G, dtor, the overrides, MoveIcon).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "lle/UIStatusBar.h"
#include "ee/EEUIFMGStatusBar.h"
#include "rust/ee_screens.h"   // ee_screens_upgrade_bar_mouse's constants (ee_screens upgrade.rs)

const float kScenarioEditorInset = 0.43f;
const float kInset = 0.46f;

// Creates an improvement button drawn as an icon, a green status bar and two value labels (and a queued
// count), laid out for the main game or the scenario editor
EEUIFMGStatusBar::EEUIFMGStatusBar(unsigned long inId, unsigned long inIndex, UILayoutUtility* inLayout,
                                   unsigned long inScreenWidth, unsigned long inScreenHeight, const GERectangle& inRectangle,
                                   UIForm* inForm, bool inScenarioEditor, bool inEnabled, bool inArg10)
    : EEUIBUpgrade(inId, inRectangle.GetTopLeft(), (unsigned long)inRectangle.GetWidth(),
                   (unsigned long)inRectangle.GetHeight(), inForm, inIndex, inScenarioEditor, inScenarioEditor == true),
      mEnabled(inEnabled)
{
    float theInset;
    float theIconSize;
    float theBarLeft;
    this->mPressed = false;
    if (inScenarioEditor) {
        theIconSize = theInset = inRectangle.GetHeight() * kScenarioEditorInset;
        theBarLeft = inRectangle.GetLeft() + this->GetBorderThickness();
    } else if (inArg10) {
        theIconSize = theInset = inRectangle.GetHeight() * kInset;
        theBarLeft = inRectangle.GetLeft() + this->GetBorderThickness();
    } else {
        theInset = inRectangle.GetHeight() * kInset;
        theIconSize = min(inRectangle.GetWidth(), inRectangle.GetHeight());
        theIconSize -= this->GetBorderThickness() * 2.0f + 0.125f;
        theBarLeft = inRectangle.GetLeft() + theIconSize;
    }
    GERectangle theRectangle;
    float theLeft = inRectangle.GetLeft() + this->GetBorderThickness();
    theRectangle.SetLeft(theLeft);
    theRectangle.SetTop(inRectangle.GetTop() + this->GetBorderThickness());
    theRectangle.SetRight(theLeft + theIconSize);
    theRectangle.SetBottom(theRectangle.GetTop() + theIconSize);
    this->mIcon = new UIBitmap(this->GetId(), NULL, theRectangle, inForm, this->GetIndex());
    theRectangle.SetLeft(theBarLeft);
    theRectangle.SetTop(inRectangle.GetTop() + theInset);
    theRectangle.SetRight(inRectangle.GetRight() - (this->GetBorderThickness() + 2.0f));
    theRectangle.SetBottom(inRectangle.GetBottom() - theInset);
    this->mBar = new UIStatusBar(this->GetId(), theRectangle, inForm, this->GetIndex(), 0);
    ESVERIFY_AT(this->mBar, 125);

    this->mBar->SetStatusBarColor(GEColor(0.0f, 1.0f, 0.0f));
    this->mBar->SetBgColor(GEColor(0.5f, 0.5f, 0.5f));
    this->mBar->SetBgOpacity(1.0f);
    theRectangle.SetLeft(inRectangle.GetLeft() + this->GetBorderThickness());
    theRectangle.SetTop(this->mBar->GetRectangle().GetBottom() + 2.0f);
    theRectangle.SetRight(inRectangle.GetRight() - this->GetBorderThickness());
    theRectangle.SetBottom(inRectangle.GetBottom() - this->GetBorderThickness());
    GEFont* theFont = UIFontTable::CreateClosestFont(1, (unsigned long)theRectangle.GetHeight());
    if (theFont && !theFont->GetFontBitmap() && !inForm->GetUseGDIForText()) {
        GEFontBitmap* theBitmap = new GEFontBitmap(inForm->GetManager()->GetRasterizer(), theFont,
                                                   inForm->GetManager()->GetScreen());
        theBitmap->Dereference();
    }
    this->mValue = new UITextLabel(UWideString("888"), theFont, this->GetId(), theRectangle, inForm, 0, this->GetIndex(), 0);
    ESVERIFY_AT(this->mValue, 148);
    this->mValue->SetAlignment(kTextAlignRight);
    GEText* theText = this->mValue->GetGEText();
    theText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theText->SetIsTransparent(true);
    theRectangle.SetLeft(inRectangle.GetLeft() + this->GetBorderThickness());
    theRectangle.SetTop(inRectangle.GetTop() + this->GetBorderThickness() + 2.0f);
    theRectangle.SetRight(inRectangle.GetRight() - this->GetBorderThickness());
    theRectangle.SetBottom(this->mBar->GetRectangle().GetTop());
    theFont = UIFontTable::CreateClosestFont(1, (unsigned long)theRectangle.GetHeight());
    if (theFont && !theFont->GetFontBitmap() && !inForm->GetUseGDIForText()) {
        GEFontBitmap* theBitmap = new GEFontBitmap(inForm->GetManager()->GetRasterizer(), theFont,
                                                   inForm->GetManager()->GetScreen());
        theBitmap->Dereference();
    }
    this->mValue2 = new UITextLabel(UWideString("888"), theFont, this->GetId(), theRectangle, inForm, 0, this->GetIndex(), 0);
    theFont->Dereference();
    this->mValue2->SetAlignment(kTextAlignRight);
    theText = this->mValue2->GetGEText();
    theText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theText->SetIsTransparent(true);
    this->mBitmap140 = new GEBitmap();
    this->mBitmap144 = new GEBitmap();
    this->mCount = new GEText(theFont);
    this->mCount->SetLocation(this->GetRectangle().GetLeft() + this->GetBorderThickness() * 2.0f,
                        this->GetRectangle().GetTop() + this->GetBorderThickness() * 2.0f);
    this->mCount->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mCount->SetIsTransparent(true);
    this->mShowCount = false;
    if (inScenarioEditor) {
        this->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
        this->SetBgOpacity(1.0f);
    }
}

// Destroys the status bar button and its parts
EEUIFMGStatusBar::~EEUIFMGStatusBar()
{
    delete this->mBar;
    delete this->mIcon;
    delete this->mValue;
    delete this->mValue2;
    this->mBitmap140->Dereference();
    this->mBitmap144->Dereference();
    delete this->mCount;
}

// Snapshots the button and its parts for drawing; shows the number of queued improvements
long EEUIFMGStatusBar::CopyData()
{
    EEUIBUpgrade::CopyData();
    this->mBar->CopyData();
    this->mIcon->CopyData();
    this->mValue->CopyData();
    long theResult = this->mValue2->CopyData();
    if (this->mNumQueued > 0) {
        char theBuffer[32];
        PlatIToA(this->mNumQueued, theBuffer, 10);
        this->mCount->SetText(UWideString(theBuffer));
        this->mShowCount = true;
    } else
        this->mShowCount = false;
    this->mCopiedEnabled = this->mEnabled;
    return theResult;
}

// Draws the button face (when enabled), the icon, the bar and the labels
long EEUIFMGStatusBar::DrawPolygons()
{
    if (this->mCopiedEnabled)
        UIButton::DrawPolygons();
    this->mIcon->DrawPolygons();
    this->mBar->DrawPolygons();
    this->mValue->DrawPolygons();
    return this->mValue2->DrawPolygons();
}

// Draws the parts' texts, the second label when it has text, and the queued count
// NEAR: the original copies the count's location y first and stores the rectangle's top later (schedule only)
long EEUIFMGStatusBar::DrawTextA()
{
    GE2DPoint theBottomRight = this->mCount->GetLocation();
    theBottomRight.mY += this->GetHeight();
    theBottomRight.mX += this->GetWidth();
    GERectangle theRectangle(this->mCount->GetLocation(), theBottomRight);
    UIControl::DrawTextA();
    this->mBar->DrawTextA();
    this->mIcon->DrawTextA();
    long theResult = this->mValue->DrawTextA();
    if (this->mValue2->GetText(0).GetLength() > 0)
        theResult = this->mValue2->DrawTextA();
    if (this->mShowCount)
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mCount, this);
    return theResult;
}

// The presses and the drag: inside an enabled button the icon nudged down-right as pressed (left and right presses),
// back on the release, following the mouse while captured; then the button's own (ee_screens upgrade.rs bar_mouse over
// the improvement button's host)
long EEUIFMGStatusBar::MouseLButtonDown() { return this->BarMouse(EE_SCREENS_UP_BAR_L_DOWN, NULL); }
long EEUIFMGStatusBar::MouseLButtonUp() { return this->BarMouse(EE_SCREENS_UP_BAR_L_UP, NULL); }
long EEUIFMGStatusBar::MouseMove(const GE2DPoint& inLocation) { return this->BarMouse(EE_SCREENS_UP_BAR_MOVE, &inLocation); }
long EEUIFMGStatusBar::MouseRButtonDown() { return this->BarMouse(EE_SCREENS_UP_BAR_R_DOWN, NULL); }
long EEUIFMGStatusBar::MouseRButtonUp() { return this->BarMouse(EE_SCREENS_UP_BAR_R_UP, NULL); }
