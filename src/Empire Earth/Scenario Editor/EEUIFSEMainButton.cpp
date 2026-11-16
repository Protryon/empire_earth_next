// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEMainButton.cpp (file name inferred: no __FILE__). The editor's tab
// buttons: 0x1a147f..0x1a18f1. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainButton.h"

// Creates a 32x32 editor tab button whose normal and pressed images are cells of the button texture
EEUIFSEMainButton::EEUIFSEMainButton(unsigned long inId, unsigned char inFlag, GETexture* inTexture,
                                     const GE2DPoint& inNormalCell, const GE2DPoint& inPressedCell,
                                     const GE2DPoint& inPosition, EEUIFScenarioEditor* inEditor, long inTab,
                                     unsigned long inIndex, bool inIsChecked)
    : UIRadioButton(inId, UWideString(""), gU531930, GERectangle(inPosition, 32.0f, 32.0f), inEditor, inIndex, 0, false,
                    inIsChecked, false)
{
    float theXScale;
    float theYScale;
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;

    this->mNormalBitmap2 = new GEBitmap;
    this->mTab = inTab;
    this->mNormalBitmap2->SetTexture(inTexture, gScreenIndex);
    theTopLeft.SetU(inNormalCell.GetX() / inTexture->GetWidth());
    theTopLeft.SetV(inNormalCell.GetY() / inTexture->GetHeight());
    theBottomRight.SetU((inNormalCell.GetX() + 32.0f) / inTexture->GetWidth());
    theBottomRight.SetV((inNormalCell.GetY() + 32.0f) / inTexture->GetHeight());
    theTopRight.SetU(theBottomRight.GetU());
    theTopRight.SetV(theTopLeft.GetV());
    theBottomLeft.SetU(theTopLeft.GetU());
    theBottomLeft.SetV(theBottomRight.GetV());
    this->mNormalBitmap2->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    theYScale = 32.0f / inTexture->GetHeight();
    theXScale = 32.0f / inTexture->GetWidth();
    this->mNormalBitmap2->SetXYScale(theXScale, theYScale);

    this->mPressedBitmap2 = new GEBitmap;
    this->mPressedBitmap2->SetTexture(inTexture, gScreenIndex);
    theTopLeft.SetU(inPressedCell.GetX() / inTexture->GetWidth());
    theTopLeft.SetV(inPressedCell.GetY() / inTexture->GetHeight());
    theBottomRight.SetU((inPressedCell.GetX() + 32.0f) / inTexture->GetWidth());
    theBottomRight.SetV((inPressedCell.GetY() + 32.0f) / inTexture->GetHeight());
    theTopRight.SetU(theBottomRight.GetU());
    theTopRight.SetV(theTopLeft.GetV());
    theBottomLeft.SetU(theTopLeft.GetU());
    theBottomLeft.SetV(theBottomRight.GetV());
    this->mPressedBitmap2->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    theYScale = 32.0f / inTexture->GetHeight();
    theXScale = 32.0f / inTexture->GetWidth();
    this->mPressedBitmap2->SetXYScale(theXScale, theYScale);

    this->SetNormalBitmap(this->mNormalBitmap2);
    this->SetPressedBitmap(this->mPressedBitmap2);
    this->SetBitmapDrawOffset(GE2DPoint(0.0f, 0.0f));
    this->mEditor = inEditor;
    this->mFlag = inFlag;
}

// Destroys the tab button, releasing its two bitmaps
EEUIFSEMainButton::~EEUIFSEMainButton()
{
    this->mNormalBitmap2->Dereference();
    this->mPressedBitmap2->Dereference();
}

// Checks the tab button and selects its tab (posted through the user interface when there is one), showing it pressed
void EEUIFSEMainButton::Check()
{
    UIRadioButton::Check();
    if (gUserInterface)
        gUserInterface->PostSelectEditorTab(this->mFlag, this->mTab);
    else
        this->mEditor->SelectTab(this->mFlag, this->mTab);
    this->SetPressedBitmap(this->mPressedBitmap2);
    this->SetNormalBitmap(this->mPressedBitmap2);
}

// Checks the tab button and selects its tab directly on the editor, showing it pressed
void EEUIFSEMainButton::CheckDirect()
{
    UIRadioButton::Check();
    this->mEditor->SelectTab(this->mFlag, this->mTab);
    this->SetPressedBitmap(this->mPressedBitmap2);
    this->SetNormalBitmap(this->mPressedBitmap2);
}

// Unchecks the tab button and restores its normal image
void EEUIFSEMainButton::UnCheck()
{
    UIRadioButton::UnCheck();
    this->SetPressedBitmap(this->mPressedBitmap2);
    this->SetNormalBitmap(this->mNormalBitmap2);
}
