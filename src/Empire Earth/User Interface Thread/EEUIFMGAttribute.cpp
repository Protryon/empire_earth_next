// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGAttribute.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFMGAttribute). The main game form's attribute button: 0x26783d..0x267ad9 (ctor, ??_G, dtor, CopyData,
// DrawTextA, GetIconRectangle; EEUIFMGErrorLog's ctor at 0x267ad9). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/GEText.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"

// Creates the main game form's attribute upgrade button with a level text in its corner
EEUIFMGAttribute::EEUIFMGAttribute(GEFont* inFont, UIForm* inForm, const GERectangle& inRectangle,
                                   unsigned long inIndex, unsigned long inId, const GEColor& inTextColor,
                                   bool inScenarioEditor, unsigned long inBorderThickness)
    : EEUIBUpgrade(inId, inRectangle.GetTopLeft(), (unsigned long)inRectangle.GetWidth(),
                   (unsigned long)inRectangle.GetHeight(), inForm, inIndex, inScenarioEditor, inBorderThickness),
      mShowLevel(false)
{
    this->mLevelText = new GEText(inFont);
    this->mLevelText->SetIsTransparent(true);
    this->mLevelText->SetFgColor(inTextColor);
    this->mLevelText->SetLocation(this->GetRectangle().GetLeft() + this->GetBorderThickness() * 2.0f,
                            this->GetRectangle().GetTop() + this->GetBorderThickness() * 2.0f);
}

// Destroys the attribute button and its level text
EEUIFMGAttribute::~EEUIFMGAttribute()
{
    delete this->mLevelText;
}

// Snapshots the button for drawing; shows the number of queued upgrades as its level text
long EEUIFMGAttribute::CopyData()
{
    long theResult = EEUIBUpgrade::CopyData();
    if (this->mNumQueued > 0) {
        char theBuffer[32];
        PlatIToA(this->mNumQueued, theBuffer, 10);
        this->mLevelText->SetText(UWideString(theBuffer));
        this->mShowLevel = true;
    } else
        this->mShowLevel = false;
    return theResult;
}

// Draws the button and, when shown, its level text
long EEUIFMGAttribute::DrawTextA()
{
    float theBottom = this->mLevelText->GetLocation().GetY();
    float theRight = this->mLevelText->GetLocation().GetX();
    theBottom += this->GetHeight();
    theRight += this->GetWidth();
    GERectangle theRectangle(GE2DPoint(this->mLevelText->GetLocation()), GE2DPoint(theRight, theBottom));
    long theResult = EEUIBUpgrade::DrawTextA();
    if (this->mShowLevel)
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mLevelText, this);
    return theResult;
}

// Returns the icon's rectangle: the whole button, or a 17-pixel strip at its left in free camera mode
GERectangle EEUIFMGAttribute::GetIconRectangle() const
{
    GERectangle theRectangle;
    if (!gFreeCamera)
        theRectangle = this->GetRectangle();
    else
        theRectangle = GERectangle(GE2DPoint(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop()), 17.0f,
                                   this->GetRectangle().GetHeight());
    return theRectangle;
}
