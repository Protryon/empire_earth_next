// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETerrain.cpp (__FILE__ 0x4c561c, the ctor's ESVERIFYs). The scenario
// editor's terrain tab: 0x1aaad4..0x1ac48b (the list boxes' draw/copy members, the paint type check, the paint
// control's CreatePaintAction, the change events, the tab's ctor, then the list boxes' inline COMDATs). After
// EESEATestScenario.cpp, before EEUIFSETRBPaintType.cpp.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "lle/UIButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UIRadioButton.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEBitmap.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EESEAPaint.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"

#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "lle/UResourceFile.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EESETSConditionControls.h" // declares the gU531934 font
#include "ee/EEMouse.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEStringIDs.h"
// The terrain name order (0x1aee71): compares the localized terrain names without case. It has no state and
// returns via ret 8 (provisional name).
struct EESETerrainNameLess {
    bool operator()(EETerrainTable* inFirst, EETerrainTable* inSecond)
    {
        UWideString theFirst, theSecond;
        gResource->GetString(inFirst->mNameID, theFirst);
        gResource->GetString(inSecond->mNameID, theSecond);
        return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)theFirst),
                        reinterpret_cast<const unsigned char*>((const char*)theSecond)) < 0;
    }
};

// Draws each color set line as a row of colored swatches clipped to the list box, then the list box itself
long EESEUITerrainColorLB::DrawPolygons()
{
    GERectangle theRectangle;
    GERectangle theLineRectangle;
    GERhombus theRhombus;
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        theLineRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theLineRectangle.SetBottom(theLineRectangle.GetTop() + (float)this->GetLineHeight() - 1.0f);
        unsigned long theWidth = (unsigned long)(this->GetCopyTextRectangle().GetWidth() / (float)(this->mCopyLineArray[i]).size());
        unsigned long theXOffset;
        unsigned long j;
        for (j = 0, theXOffset = 0; j < (this->mCopyLineArray[i]).size(); j++, theXOffset += theWidth) {
            theLineRectangle.SetLeft(this->GetCopyTextRectangle().GetLeft() + (float)theXOffset);
            theLineRectangle.SetRight(theLineRectangle.GetLeft() + (float)theWidth - 1.0f);
            if (theLineRectangle.And(this->GetCopyTextRectangle(), theRectangle) != true)
                break;
            theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
            theRectangle.SetBottom(theRectangle.GetBottom() + 1.0f);
            theRhombus.SetRectangle(theRectangle);
            theRhombus.SetColor((this->mCopyLineArray[i])[j]);
            this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        }
    }
    return UIListBox<std::vector<GEColor> >::DrawPolygons();
}

// Copies the list for drawing, moving the preview texture references to the new copy and framing the selection
long EESEUITerrainTypeLB::CopyData()
{
    unsigned long i;
    for (i = 0; i < this->mCopyLineArray.size(); i++) {
        if (this->mCopyLineArray[i].mTexture != NULL) {
            this->mCopyLineArray[i].mTexture->Dereference();
            this->mCopyLineArray[i].mTexture = NULL;
        }
    }
    long theResult = UIListBox<EESEUITTData>::CopyData();
    *this->mCopyText = *this->mText;
    if (this->GetCopiedHasSelection() == true) {
        this->SetCopiedHasFocus(false);
        this->mIsSelectionFramed = true;
        this->SetCopiedFocusedRectColor(this->GetFocusedRectColor());
    } else
        this->mIsSelectionFramed = false;
    for (unsigned long j = 0; j < this->mCopyLineArray.size(); j++) {
        if (this->mCopyLineArray[j].mTexture != NULL)
            this->mCopyLineArray[j].mTexture->Reference();
    }
    return theResult;
}

// Draws the list box, each line's 16x16 terrain preview and a frame around the selected line
long EESEUITerrainTypeLB::DrawPolygons()
{
    GELine theLine(GE2DPoint(), GE2DPoint(), this->GetCopiedFocusedRectColor(), this->GetCopiedFocusedRectColor(), 1.0f);
    GERectangle theSource;
    GERectangle theDestination;
    long theResult = UIListBox<EESEUITTData>::DrawPolygons();
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        GETexture* theTexture = (this->mCopyLineArray[i]).mTexture;
        if (theTexture == NULL)
            continue;
        this->mBitmap->SetTexture(theTexture, this->GetScreenIndex());
        theSource = GERectangle(0.0f, 0.0f, (float)theTexture->GetWidth(), (float)theTexture->GetHeight());
        theDestination.SetLeft(this->GetCopyTextRectangle().GetLeft());
        theDestination.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theDestination.SetRight(theDestination.GetLeft() + 16.0f - 1.0f);
        theDestination.SetBottom(theDestination.GetTop() + 16.0f - 1.0f);
        if (!theDestination.And(this->GetCopyTextRectangle()))
            continue;
        this->mBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetScreenIndex());
        this->mBitmap->SetLocation(theDestination.GetTopLeft());
        theResult = this->GetRasterizer()->Draw(this->mBitmap, this->GetScreenIndex());
    }
    if (this->mIsSelectionFramed == true) {
        theDestination = this->GetCopiedSelectedBackground()->GetRectangle();
        if (theDestination.And(this->GetCopyTextRectangle()) == true) {
            theLine.SetStartPoint(theDestination.GetTopLeft());
            theLine.SetEndPoint(theDestination.GetTopRight());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetEndPoint(theDestination.GetBottomLeft());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetStartPoint(theDestination.GetBottomRight());
            this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            theLine.SetEndPoint(theDestination.GetTopRight());
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
        }
    }
    return theResult;
}

// Sets the preview texture of a terrain line, releasing the old one and referencing the new one
void EESEUITerrainTypeLB::SetTexture(GETexture* inTexture, unsigned long inLine)
{
    if (this->mData[inLine].mTexture != NULL)
        this->mData[inLine].mTexture->Dereference();
    this->mData[inLine].mTexture = inTexture;
    if (inTexture != NULL)
        this->mData[inLine].mTexture->Reference();
}

// Draws the visible terrain names to the right of their previews; returns the last draw result
long EESEUITerrainTypeLB::DrawTextA()
{
    GERectangle theRectangle;
    unsigned long theYOffset = 0;
    unsigned long theHeight = this->GetLineHeight();
    unsigned long i = 0;
    this->DrawScrollBarText();
    this->UIControl::DrawTextA();   // the base's draw count
    theRectangle = this->GetCopyTextRectangle();
    theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
    theRectangle.SetLeft(theRectangle.GetLeft() + 16.0f);
    long theResult = 0;
    for (; i < this->GetDrawCount(); i++) {
        theRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)theYOffset);
        theRectangle.SetBottom(theRectangle.GetTop() + (float)theHeight);
        this->mCopyText->SetLocation(GE2DPoint(theRectangle.GetLeft(), theRectangle.GetTop()));
        this->mCopyText->SetText(this->mCopyLineArray[i].mName);
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mCopyText, this);
        theYOffset += theHeight;
    }
    return theResult;
}

// Checks the paint type button and shows (terrain) or hides (bulldozer) the terrain type, color and preview controls
void EEUIFSETRBPaintType::Check()
{
    UIRadioButton::Check();
    switch (this->GetId()) {
    case kSETerrainTerrainButton:
        this->GetForm()->GetControlById(kSETerrainTerrainTypeLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSETerrainTerrainType)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSETerrainTerrainColorTypeLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSETerrainTerrainColorType)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSETerrainBitmap)->SetDisplayMode(kDisplayModeShown);
        break;
    case kSETerrainMountainsButton:
        this->GetForm()->GetControlById(kSETerrainTerrainTypeLabel)->SetDisplayMode(kDisplayModeHidden);
        this->GetForm()->GetControlById(kSETerrainTerrainType)->SetDisplayMode(kDisplayModeHidden);
        this->GetForm()->GetControlById(kSETerrainTerrainColorTypeLabel)->SetDisplayMode(kDisplayModeHidden);
        this->GetForm()->GetControlById(kSETerrainTerrainColorType)->SetDisplayMode(kDisplayModeHidden);
        this->GetForm()->GetControlById(kSETerrainBitmap)->SetDisplayMode(kDisplayModeHidden);
        break;
    }
}

// Creates the paint action: terrain with the selected type and color (NULL without a type) or bulldozer;
// without a color selection it indexes the color list with the terrain's color type id
EESEAPaint* EEUIFSETerrainPaint::CreatePaintAction()
{
    UIRadioButton* theTerrainButton = static_cast<UIRadioButton*>(this->GetForm()->GetControlById(kSETerrainTerrainButton));
    if (theTerrainButton->GetIsChecked()) {
        EESEUITerrainTypeLB* theTypeList = static_cast<EESEUITerrainTypeLB*>(this->GetForm()->GetControlById(kSETerrainTerrainType));
        EESEUITerrainColorLB* theColorList = static_cast<EESEUITerrainColorLB*>(this->GetForm()->GetControlById(kSETerrainTerrainColorType));
        unsigned long theType;
        unsigned long theColor;
        if (theTypeList->GetCurrentSelection(theType) == 0) {
            if (theColorList->GetCurrentSelection(theColor) != 0)
                theColor = this->mTerrain->mTerrains[theType]->mColorType;
            this->mIsCentered = true;
            // The color type id used as a list position can lie past the filtered color list: an element
            // past the end counts as color id 0 instead of being read from beyond the vector.
            long theColorID = 0;
            if (theColor < this->mTerrain->mColorTypes.size())
                theColorID = this->mTerrain->mColorTypes[theColor]->mID;
            return new EESEAPaintRust(kSEAPaintTerrain, this->mTerrain->mTerrains[theType]->mID, theColorID);
        }
    } else {
        this->mIsCentered = false;
        return new EESEAPaintRust(kSEAPaintBulldozer);
    }
    return NULL;
}

// Tints the preview bitmap's corners with the first, one-third, two-thirds and last colors of the selected color set
long EEUIETerrainColorChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIBitmap* theBitmap = static_cast<UIBitmap*>(inForm->GetControlById(kSETerrainBitmap));
    EESEUITerrainColorLB* theList = static_cast<EESEUITerrainColorLB*>(inEntity);
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) != 0)
        return kErrorOK;
    if (theList->GetData(theSelection).size() > 0) {
        GEBitmap* theColors = theBitmap->GetBitmap();
        unsigned long theThird = theList->GetData(theSelection).size() > 2 ?
            (theList->GetData(theSelection).size() - 2) / 3 : 0;
        theColors->SetColorPerVertex(theList->GetData(theSelection)[0],
            theList->GetData(theSelection)[theThird],
            theList->GetData(theSelection)[theList->GetData(theSelection).size() - theThird - 1],
            theList->GetData(theSelection)[theList->GetData(theSelection).size() - 1]);
        theBitmap->SetDirty(true);
    }
    return kErrorOK;
}

// Shows the selected terrain's preview, selects its color set and makes it the tab's selected terrain
long EEUIETerrainTypeChange::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    GERectangle theSource;
    EESEUITerrainColorLB* theColors = static_cast<EESEUITerrainColorLB*>(inForm->GetControlById(kSETerrainTerrainColorType));
    UIBitmap* theBitmap = static_cast<UIBitmap*>(inForm->GetControlById(kSETerrainBitmap));
    EESEUITerrainTypeLB* theTypes = static_cast<EESEUITerrainTypeLB*>(inEntity);
    unsigned long theSelection;
    if (theTypes->GetCurrentSelection(theSelection) == 0) {
        GEBitmap* theImage = theBitmap->GetBitmap();
        if (theTypes->GetData(theSelection).mTexture != 0) {
            theImage->SetTexture(theTypes->GetData(theSelection).mTexture, theTypes->GetScreenIndex());
            theSource = GERectangle(0.0f, 0.0f, (float)theTypes->GetData(theSelection).mTexture->GetWidth(),
                                    (float)theTypes->GetData(theSelection).mTexture->GetHeight());
            theImage->ComputeStretchedCoordsAndScale(theBitmap->GetRectangle(), theBitmap->GetRectangle(),
                                                    theSource, theTypes->GetScreenIndex());
            theColors->ChangeSelectionIndex(theTypes->GetData(theSelection).mColorSet, false, true);
            theColors->FireTriggerEvent(kControlTriggerSelectionChanged);
            theBitmap->SetDirty(true);
        }
        this->mTerrain->SelectTerrain(theSelection);
    }
    return kErrorOK;
}

// Creates the terrain tab: terrain/bulldozer buttons, the name-sorted terrain type list, color set list, paint control
// and preview bitmap; loads the terrain previews
EEUIFSETerrain::EEUIFSETerrain(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes,
                               unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    std::vector<EESEUITTData> theTypes;
    std::vector<std::vector<GEColor> > theColors;
    // The original also constructs/destroys an empty vector<UWideString> here.
    std::vector<UWideString> theUnusedStrings;
    UWideString theText;
    GE2DPoint theLocation;
    unsigned long i;
    for (i = 0; i < EETerrainTypeTable::GetCount(); i++) {
        if (EETerrainTypeTable::Get(i) != NULL && EETerrainTypeTable::Get(i)->mIsInEditor) {
            EETerrainTypeTable* theType = EETerrainTypeTable::Get(i);
            this->mColorTypes.push_back(theType);
        }
    }
    for (i = 0; i < EETerrainTable::GetNumTables(); i++) {
        if (EETerrainTable::Get(i) != NULL && EETerrainTable::Get(i)->mIsInEditor) {
            EETerrainTable* theTerrain = EETerrainTable::Get(i);
            this->mTerrains.push_back(theTerrain);
        }
    }
    std::sort(this->mTerrains.begin(), this->mTerrains.end(), EESETerrainNameLess());
    this->mSelectedTerrain = 0;
    theLocation.SetXY(115.0f, 621.0f);
    gResource->GetString(IDS_SE_TERRAIN, theText);
    this->mTerrainButton = new EEUIFSETRBPaintType(kSETerrainTerrainButton, theText, theLocation, inFont, inForm, ioIndex, true);
    this->mTerrainButton->GetTextLabel()->SetBgOpacity(0.4f);
    ioIndex++;
    GEText* theGEText = this->mTerrainButton->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theLocation.SetX(115.0f);
    theLocation.SetY(this->mTerrainButton->GetRectangle().GetBottom() + 5.0f);
    gResource->GetString(IDS_BULLDOZER, theText);
    this->mMountainsButton = new EEUIFSETRBPaintType(kSETerrainMountainsButton, theText, theLocation, inFont, inForm, ioIndex, false);
    this->mMountainsButton->GetTextLabel()->SetBgOpacity(0.4f);
    ioIndex++;
    theGEText = this->mMountainsButton->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    inForm->AddControl(this->mMountainsButton, 3);
    theLocation.SetXY(250.0f, 621.0f);
    gResource->GetString(IDS_TERRAIN_TYPE, theText);
    this->mTerrainTypeLabel = new UITextLabel(theText, inFont, kSETerrainTerrainTypeLabel,
        GERectangle(theLocation, 200.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mTerrainTypeLabel->SetBgOpacity(0.4f);
    ioIndex++;
    this->mTerrainTypeLabel->SetAlignment(kTextAlignCenter);
    this->mTerrainTypeLabel->GetGEText()->SetIsTransparent(true);
    this->mTerrainTypeLabel->GetGEText()->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    inForm->AddControl(this->mTerrainTypeLabel, 3);
    theLocation.SetX(455.0f);
    gResource->GetString(IDS_TERRAIN_COLOR, theText);
    this->mTerrainColorTypeLabel = new UITextLabel(theText, inFont, kSETerrainTerrainColorTypeLabel,
        GERectangle(theLocation, 120.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    this->mTerrainColorTypeLabel->SetBgOpacity(0.4f);
    ioIndex++;
    this->mTerrainColorTypeLabel->SetAlignment(kTextAlignCenter);
    this->mTerrainColorTypeLabel->GetGEText()->SetIsTransparent(true);
    this->mTerrainColorTypeLabel->GetGEText()->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    inForm->AddControl(this->mTerrainColorTypeLabel, 3);
    theTypes.resize(this->mTerrains.size());
    for (i = 0; i < this->mTerrains.size(); i++) {
        gResource->GetString(this->mTerrains[i]->mNameID, theText);
        theTypes[i].mName = theText;
        theTypes[i].mTexture = NULL;
        if (EETerrainTypeTable::Get(this->mTerrains[i]->mColorType) == NULL)
            continue;
        for (long j = 0; j < (long)this->mColorTypes.size(); j++) {
            if (this->mColorTypes[j] == EETerrainTypeTable::Get(this->mTerrains[i]->mColorType)) {
                theTypes[i].mColorSet = j;
                break;
            }
        }
    }
    theLocation.SetXY(this->mTerrainTypeLabel->GetRectangle().GetLeft(),
                       this->mTerrainTypeLabel->GetRectangle().GetBottom() + 5.0f);
    this->mTerrainType = new EESEUITerrainTypeLB(kSETerrainTerrainType, gU531934, theTypes,
        GERectangle(theLocation, (float)this->mTerrainTypeLabel->GetWidth(), 85.0f), inForm, ioIndex, 2);
    if (!this->mTerrainType)
        ESVerify("C:\\EEX\\Empire Earth\\Scenario Editor\\EEUIFSETerrain.cpp", 0x356, "this->mTerrainType");
    UIEvent* theEvent = new EEUIETerrainTypeChange(this);
    this->mTerrainType->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    ioIndex++;
    this->mTerrainType->mText->SetIsTransparent(true);
    this->mTerrainType->mText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    this->mTerrainType->GetSelectedBackground()->SetOpacity(0.0f);
    this->mTerrainType->SetBgOpacity(0.4f);
    inForm->AddControl(this->mTerrainType, 3);
    theLocation.SetX(this->mTerrainColorTypeLabel->GetRectangle().GetLeft());
    theLocation.SetY(this->mTerrainColorTypeLabel->GetRectangle().GetBottom() + 5.0f);
    theColors.resize(this->mColorTypes.size());
    for (i = 0; i < this->mColorTypes.size(); i++) {
        for (long color = this->mColorTypes[i]->mFirstColor; color <= this->mColorTypes[i]->mLastColor; color++)
            theColors[i].push_back(EETerrainColorTable::Get(color)->mColor);
    }
    this->mTerrainColorType = new EESEUITerrainColorLB(kSETerrainTerrainColorType, theColors, 16, false,
        GERectangle(theLocation, (float)this->mTerrainColorTypeLabel->GetWidth(), 85.0f), inForm, ioIndex, 2);
    if (!this->mTerrainColorType)
        ESVerify("C:\\EEX\\Empire Earth\\Scenario Editor\\EEUIFSETerrain.cpp", 900, "this->mTerrainColorType");
    theEvent = new EEUIETerrainColorChange;
    this->mTerrainColorType->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mTerrainColorType->ChangeSelectionIndex(this->mTerrainType->Begin()[0].mColorSet, false, true);
    ioIndex++;
    this->mTerrainColorType->GetSelectedBackground()->SetOpacity(0.0f);
    this->mTerrainColorType->SetBgOpacity(0.4f);
    inForm->AddControl(this->mTerrainColorType, 3);
    theLocation.SetXY(0.0f, 0.0f);
    this->mPaint = new EEUIFSETerrainPaint(inScroll, inBrushes, 205, theLocation, 1024, 606, inForm, ioIndex, this);
    ioIndex++;
    inForm->AddControl(this->mPaint, 3);
    inForm->AddControl(this->mTerrainButton, 3);
    theLocation.SetX(this->mTerrainColorType->GetRectangle().GetRight() + 10.0f);
    theLocation.SetY(this->mTerrainColorType->GetRectangle().GetHeight() * 0.5f + this->mTerrainColorType->GetRectangle().GetTop() - 32.0f);
    GEBitmap* theBitmap = new GEBitmap;
    this->mBitmap = new UIBitmap(kSETerrainBitmap, theBitmap, GERectangle(theLocation, 64.0f, 64.0f), inForm, ioIndex);
    theBitmap->Dereference();
    ioIndex++;
    inForm->AddControl(this->mBitmap, 3);
    EELoadTerrainPreviews();
}

// Creates a paint type radio button 125 wide and as tall as the larger of its font and the radio button image
EEUIFSETRBPaintType::EEUIFSETRBPaintType(unsigned long inID, const UWideString& inText,
    const GE2DPoint& inLocation, GEFont* inFont, UIForm* inForm, unsigned long inIndex, bool inChecked)
    : UIRadioButton(inID, inText, inFont,
        GERectangle(inLocation, 125.0f,
            (float)inFont->GetHeight() < inForm->GetManager()->GetRadioOn()->GetHeight()
                ? inForm->GetManager()->GetRadioOn()->GetHeight() : (float)inFont->GetHeight()),
        inForm, inIndex, 0, false, inChecked, false)
{
}

// Destroys the terrain tab and its controls and releases the terrain previews
EEUIFSETerrain::~EEUIFSETerrain()
{
    delete this->mTerrainButton;
    delete this->mMountainsButton;
    delete this->mTerrainType;
    delete this->mTerrainTypeLabel;
    delete this->mPaint;
    delete this->mTerrainColorType;
    delete this->mTerrainColorTypeLabel;
    delete this->mBitmap;
    EEReleaseTerrainPreviews();
}

// Leaves the tab: asks the load thread to unload the selected terrain and drops every preview texture
void EEUIFSETerrain::Leave()
{
    EESEUITerrainTypeLB* theList = static_cast<EESEUITerrainTypeLB*>(this->mTerrainType->GetForm()->GetControlById(kSETerrainTerrainType));
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) == 0 && theSelection < this->mTerrains.size()) {
        EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mTerrainType->GetForm());
        theEditor->GetLoadThread()->Post409(reinterpret_cast<void*>(this->mTerrains[theSelection]->mID));
    }
    for (theSelection = 0; theSelection < this->mTerrains.size(); theSelection++)
        this->mTerrainType->SetTexture(NULL, theSelection);
}

// Moves the brush highlight to the mouse while the editor is the current form
void EEUIFSETerrain::Update()
{
    UIForm* theForm = this->mTerrainType->GetForm();
    if (theForm == theForm->GetManager()->GetCurrentForm())
        this->MouseMove(gMouse->mPosition);
}

// Enters the tab: sets the previews (an ambient-only terrain shows its first unit's button), shows the selected one
// and asks the load thread to load the selected terrain
void EEUIFSETerrain::Enter()
{
    GERectangle theSource;
    unsigned long i;
    for (i = 0; i < this->mTerrains.size(); i++) {
        if ((this->mTerrains[i])->mUnitClasses.size() != 0 && (this->mTerrains[i])->mIsAmbientsOnly) {
            if ((this->mTerrains[i])->mUnitClasses.size() > 0 &&
                gUnitClassTable[(this->mTerrains[i])->mUnitClasses[0]])
                this->mTerrainType->SetTexture(gUnitClassTable[(this->mTerrains[i])->mUnitClasses[0]]->mButton->mTexture, i);
        } else
            this->mTerrainType->SetTexture((this->mTerrains[i])->mPreviewTexture, i);
    }
    if (this->mTerrainType->GetCurrentSelection(i) == kErrorOK) {
        GEBitmap* theBitmap = this->mBitmap->GetBitmap();
        if (this->mTerrainType->GetData(i).mTexture != NULL) {
            theBitmap->SetTexture(this->mTerrainType->GetData(i).mTexture, this->mTerrainType->GetScreenIndex());
            unsigned long theHeight = (*(this->mTerrainType->Begin() + i)).mTexture->GetHeight();
            unsigned long theWidth = (*(this->mTerrainType->Begin() + i)).mTexture->GetWidth();
            theSource = GERectangle(0.0f, 0.0f, (float)theWidth, (float)theHeight);
            const GERectangle& theDestination = this->mBitmap->GetRectangle();
            theBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource,
                                                    this->mTerrainType->GetScreenIndex());
            this->mTerrainColorType->FireTriggerEvent(kControlTriggerSelectionChanged);
        }
    }
    EESEUITerrainTypeLB* theList = static_cast<EESEUITerrainTypeLB*>(this->mTerrainType->GetForm()->GetControlById(kSETerrainTerrainType));
    if (theList->GetCurrentSelection(i) == kErrorOK && i < this->mTerrains.size()) {
        EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(this->mTerrainType->GetForm());
        theForm->GetLoadThread()->Post408(reinterpret_cast<void*>(this->mTerrains[i]->mID));
        this->mSelectedTerrain = i;
    } else
        this->mSelectedTerrain = -1;
}

// Selects a terrain, asking the load thread to unload the previous one and load the new one (none if out of range)
void EEUIFSETerrain::SelectTerrain(unsigned long inSelection)
{
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(this->mTerrainColorTypeLabel->GetForm());
    if (this->mSelectedTerrain < this->mTerrains.size())
        theForm->GetLoadThread()->Post409(reinterpret_cast<void*>(this->mTerrains[this->mSelectedTerrain]->mID));
    if (inSelection < this->mTerrains.size()) {
        this->mSelectedTerrain = inSelection;
        theForm->GetLoadThread()->Post408(reinterpret_cast<void*>(this->mTerrains[inSelection]->mID));
    } else
        this->mSelectedTerrain = -1;
}

// Puts the brush highlight on the map point under the mouse inside the game view, else hides it
void EEUIFSETerrain::MouseMove(const GE2DPoint& inLocation)
{
    GE3DPoint thePoint;
    EETile* theTile;
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(this->mTerrainTypeLabel->GetForm());
    if (gViewport.GetViewRectangle().Contains(inLocation)) {
        EEPointUnitOrTile(inLocation, theTile, thePoint, false);
        theForm->GetBrushGroup()->mIsHighlightShown = true;
        theForm->GetBrushGroup()->SetHighlight(thePoint.GetX(), thePoint.GetY(), !this->mMountainsButton->GetIsChecked());
    } else {
        theForm->GetBrushGroup()->mIsHighlightShown = false;
    }
}
