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
#include "ee/EESEAPaintTerrain.h"
#include "ee/EESEAPaintMoutains.h"
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
        return _mbsicmp(reinterpret_cast<const unsigned char*>((const char*)theFirst),
                        reinterpret_cast<const unsigned char*>((const char*)theSecond)) < 0;
    }
};

// FUNCTION: EXE 0x5aaad4
// Draws each color set line as a row of colored swatches clipped to the list box, then the list box itself
long EESEUITerrainColorLB::DrawPolygons()
{
    GERectangle theRectangle;
    GERectangle theLineRectangle;
    GERhombus theRhombus;
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        theLineRectangle.SetTop(this->mCopyTextRectangle.GetTop() + (float)(this->mLineHeight * i));
        theLineRectangle.SetBottom(theLineRectangle.GetTop() + (float)this->mLineHeight - 1.0f);
        unsigned long theWidth = (unsigned long)(this->mCopyTextRectangle.GetWidth() / (float)(this->mCopyLineArray[i]).size());
        unsigned long theXOffset;
        unsigned long j;
        for (j = 0, theXOffset = 0; j < (this->mCopyLineArray[i]).size(); j++, theXOffset += theWidth) {
            theLineRectangle.SetLeft(this->mCopyTextRectangle.GetLeft() + (float)theXOffset);
            theLineRectangle.SetRight(theLineRectangle.GetLeft() + (float)theWidth - 1.0f);
            if (theLineRectangle.And(this->mCopyTextRectangle, theRectangle) != true)
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

// FUNCTION: EXE 0x5aacf2
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
    if (this->mCopiedHasSelection == true) {
        this->mCopiedHasFocus = false;
        this->mIsSelectionFramed = true;
        this->mCopiedFocusedRectColor = this->mFocusedRectColor;
    } else
        this->mIsSelectionFramed = false;
    for (unsigned long j = 0; j < this->mCopyLineArray.size(); j++) {
        if (this->mCopyLineArray[j].mTexture != NULL)
            this->mCopyLineArray[j].mTexture->Reference();
    }
    return theResult;
}

// FUNCTION: EXE 0x5aade1
// Draws the list box, each line's 16x16 terrain preview and a frame around the selected line
long EESEUITerrainTypeLB::DrawPolygons()
{
    GELine theLine(GE2DPoint(), GE2DPoint(), this->mCopiedFocusedRectColor, this->mCopiedFocusedRectColor, 1.0f);
    GERectangle theSource;
    GERectangle theDestination;
    long theResult = UIListBox<EESEUITTData>::DrawPolygons();
    for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
        GETexture* theTexture = (this->mCopyLineArray[i]).mTexture;
        if (theTexture == NULL)
            continue;
        this->mBitmap->SetTexture(theTexture, this->GetScreenIndex());
        theSource = GERectangle(0.0f, 0.0f, (float)theTexture->GetWidth(), (float)theTexture->GetHeight());
        theDestination.SetLeft(this->mCopyTextRectangle.GetLeft());
        theDestination.SetTop(this->mCopyTextRectangle.GetTop() + (float)(this->mLineHeight * i));
        theDestination.SetRight(theDestination.GetLeft() + 16.0f - 1.0f);
        theDestination.SetBottom(theDestination.GetTop() + 16.0f - 1.0f);
        if (!theDestination.And(this->mCopyTextRectangle))
            continue;
        this->mBitmap->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetScreenIndex());
        this->mBitmap->SetLocation(theDestination.GetTopLeft());
        theResult = this->GetRasterizer()->Draw(this->mBitmap, this->GetScreenIndex());
    }
    if (this->mIsSelectionFramed == true) {
        theDestination = this->mCopiedSelectedBackground->GetRectangle();
        if (theDestination.And(this->mCopyTextRectangle) == true) {
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

// FUNCTION: EXE 0x5ab15f
// Sets the preview texture of a terrain line, releasing the old one and referencing the new one
void EESEUITerrainTypeLB::SetTexture(GETexture* inTexture, unsigned long inLine)
{
    if (this->mData[inLine].mTexture != NULL)
        this->mData[inLine].mTexture->Dereference();
    this->mData[inLine].mTexture = inTexture;
    if (inTexture != NULL)
        this->mData[inLine].mTexture->Reference();
}

// FUNCTION: EXE 0x5ab1a2
// Draws the visible terrain names to the right of their previews; returns the last draw result
long EESEUITerrainTypeLB::DrawTextA()
{
    GERectangle theRectangle;
    unsigned long theYOffset = 0;
    unsigned long theHeight = this->mLineHeight;
    unsigned long i = 0;
    this->DrawScrollBarText();
    this->mDrawCount--;
    theRectangle = this->mCopyTextRectangle;
    theRectangle.SetRight(theRectangle.GetRight() + 1.0f);
    theRectangle.SetLeft(theRectangle.GetLeft() + 16.0f);
    long theResult = 0;
    for (; i < this->mDrawLineCount; i++) {
        theRectangle.SetTop(this->mCopyTextRectangle.GetTop() + (float)theYOffset);
        theRectangle.SetBottom(theRectangle.GetTop() + (float)theHeight);
        this->mCopyText->SetLocation(GE2DPoint(theRectangle.GetLeft(), theRectangle.GetTop()));
        this->mCopyText->SetText(this->mCopyLineArray[i].mName);
        theResult = this->mForm->DrawClippedText(theRectangle, this->mCopyText, this);
        theYOffset += theHeight;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ab2ab
// Checks the paint type button and shows (terrain) or hides (bulldozer) the terrain type, color and preview controls
void EEUIFSETRBPaintType::Check()
{
    UIRadioButton::Check();
    switch (this->GetId()) {
    case kSETerrainTerrainButton:
        this->mForm->GetControlById(kSETerrainTerrainTypeLabel)->SetDisplayMode(kDisplayModeShown);
        this->mForm->GetControlById(kSETerrainTerrainType)->SetDisplayMode(kDisplayModeShown);
        this->mForm->GetControlById(kSETerrainTerrainColorTypeLabel)->SetDisplayMode(kDisplayModeShown);
        this->mForm->GetControlById(kSETerrainTerrainColorType)->SetDisplayMode(kDisplayModeShown);
        this->mForm->GetControlById(kSETerrainBitmap)->SetDisplayMode(kDisplayModeShown);
        break;
    case kSETerrainMountainsButton:
        this->mForm->GetControlById(kSETerrainTerrainTypeLabel)->SetDisplayMode(kDisplayModeHidden);
        this->mForm->GetControlById(kSETerrainTerrainType)->SetDisplayMode(kDisplayModeHidden);
        this->mForm->GetControlById(kSETerrainTerrainColorTypeLabel)->SetDisplayMode(kDisplayModeHidden);
        this->mForm->GetControlById(kSETerrainTerrainColorType)->SetDisplayMode(kDisplayModeHidden);
        this->mForm->GetControlById(kSETerrainBitmap)->SetDisplayMode(kDisplayModeHidden);
        break;
    }
}

// FUNCTION: EXE 0x5ab344
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
            return new EESEAPaintTerrain(this->mTerrain->mTerrains[theType]->mID, theColorID);
        }
    } else {
        this->mIsCentered = false;
        return new EESEAPaintMoutains;
    }
    return NULL;
}

// FUNCTION: EXE 0x5ab460
// Creates a bulldozer paint action (EESEAPaintMoutains' inline ctor, out of line: its first user 0x1ab344 refused it)

// FUNCTION: EXE 0x5ab48a
// Destroys and frees the bulldozer paint action (scalar deleting destructor)

// FUNCTION: EXE 0x5ab4bc
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

// FUNCTION: EXE 0x5ab6d7
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

// FUNCTION: EXE 0x5ab87c
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

// FUNCTION: EXE 0x5ac31d
// Copies the color set list for drawing (EESEUITerrainColorLB::CopyData, inline, emitted with the vtable)

// FUNCTION: EXE 0x5ac32e
// Destroys and frees the terrain color set list (scalar deleting destructor)

// FUNCTION: EXE 0x5ac350
// Creates the terrain type list box (inline ctor, out of line: EEUIFSETerrain's ctor refused it)

// FUNCTION: EXE 0x5ac3f1
// Destroys and frees the terrain type list box (scalar deleting destructor)

// FUNCTION: EXE 0x5ac40d
// Destroys the terrain type list box (inline dtor, out of line)

// FUNCTION: EXE 0x5ac48b
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

// SYNTHETIC: EXE 0x5ac516
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFSETRBPaintType+0x0, stores EEUIFSETRBPaintType vtable)

// SYNTHETIC: EXE 0x5ac539
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFSETerrainPaint+0x0, stores EEUIFSETerrainPaint vtable)

// FUNCTION: EXE 0x5ac55b
// Destroys and frees the terrain color change event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5ac586
// Destroys and frees the terrain type change event (scalar deleting destructor, emitted with its vtable)

// SYNTHETIC: EXE 0x5ac5b1
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFSETerrain+0x0)

// SYNTHETIC: EXE 0x5ac5cd
// (the scalar deleting destructor, emitted with the vtable; slot UIBitmap+0x0)

// FUNCTION: EXE 0x5ac5ea
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

// FUNCTION: EXE 0x5ac675
// Leaves the tab: asks the load thread to unload the selected terrain and drops every preview texture
void EEUIFSETerrain::Leave()
{
    EESEUITerrainTypeLB* theList = static_cast<EESEUITerrainTypeLB*>(this->mTerrainType->GetForm()->GetControlById(kSETerrainTerrainType));
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) == 0 && theSelection < this->mTerrains.size()) {
        EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mTerrainType->GetForm());
        theEditor->mLoadThread->Post409(reinterpret_cast<void*>(this->mTerrains[theSelection]->mID));
    }
    for (theSelection = 0; theSelection < this->mTerrains.size(); theSelection++)
        this->mTerrainType->SetTexture(NULL, theSelection);
}

// FUNCTION: EXE 0x5ac6f9
// Moves the brush highlight to the mouse while the editor is the current form
void EEUIFSETerrain::Update()
{
    UIForm* theForm = this->mTerrainType->GetForm();
    if (theForm == theForm->GetManager()->GetCurrentForm())
        this->MouseMove(gMouse->mPosition);
}

// FUNCTION: EXE 0x5ac727
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
        theForm->mLoadThread->Post408(reinterpret_cast<void*>(this->mTerrains[i]->mID));
        this->mSelectedTerrain = i;
    } else
        this->mSelectedTerrain = -1;
}

// FUNCTION: EXE 0x5aca00
// Selects a terrain, asking the load thread to unload the previous one and load the new one (none if out of range)
void EEUIFSETerrain::SelectTerrain(unsigned long inSelection)
{
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(this->mTerrainColorTypeLabel->GetForm());
    if (this->mSelectedTerrain < this->mTerrains.size())
        theForm->mLoadThread->Post409(reinterpret_cast<void*>(this->mTerrains[this->mSelectedTerrain]->mID));
    if (inSelection < this->mTerrains.size()) {
        this->mSelectedTerrain = inSelection;
        theForm->mLoadThread->Post408(reinterpret_cast<void*>(this->mTerrains[inSelection]->mID));
    } else
        this->mSelectedTerrain = -1;
}

// FUNCTION: EXE 0x5aca71
// Puts the brush highlight on the map point under the mouse inside the game view, else hides it
void EEUIFSETerrain::MouseMove(const GE2DPoint& inLocation)
{
    GE3DPoint thePoint;
    EETile* theTile;
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(this->mTerrainTypeLabel->GetForm());
    if (gViewport.GetViewRectangle().Contains(inLocation)) {
        EEPointUnitOrTile(inLocation, theTile, thePoint, false);
        theForm->mBrushGroup->mIsHighlightShown = true;
        theForm->mBrushGroup->SetHighlight(thePoint.GetX(), thePoint.GetY(), !this->mMountainsButton->GetIsChecked());
    } else {
        theForm->mBrushGroup->mIsHighlightShown = false;
    }
}

// FUNCTION: EXE 0x5acafa
// Appends a terrain type to the tab's color type list (vector<EETerrainTypeTable*>::push_back)

// FUNCTION: EXE 0x5acb26
// Creates a color set list box (UIListBox<vector<GEColor> > ctor)

// FUNCTION: EXE 0x5acda0
// Destroys a color set list box (UIListBox<vector<GEColor> > dtor)

// FUNCTION: EXE 0x5ace34
// Appends a color set line and updates the scroll bar (UIListBox<vector<GEColor> >::AppendLine)

// FUNCTION: EXE 0x5ace5a
// Appends several color set lines (UIListBox<vector<GEColor> >::AppendStringArray)

// FUNCTION: EXE 0x5acea6
// Copies the color set lines and layout for drawing (UIListBox<vector<GEColor> >::CopyData)

// FUNCTION: EXE 0x5ad0fd
// Removes every color set line (UIListBox<vector<GEColor> >::DeleteAllEntries)

// FUNCTION: EXE 0x5ad12e
// Removes one color set line (UIListBox<vector<GEColor> >::DeleteLine)

// FUNCTION: EXE 0x5ad1bc
// Gets the selected color set line; fails when empty or unselected (UIListBox<vector<GEColor> >::GetCurrentSelection)

// FUNCTION: EXE 0x5ad1f2
// Moves the list box (UIListBox<vector<GEColor> >::SetLocation, ICF-shared by the editor and WON list boxes)

// FUNCTION: EXE 0x5ad327
// Handles the list box keys (UIListBox<vector<GEColor> >::UpdateKeyboard, ICF-shared by the list boxes)

// FUNCTION: EXE 0x5ad428
// Returns the number of color set lines (UIListBox<vector<GEColor> >::GetNumLines)

// FUNCTION: EXE 0x5ad441
// Handles a left click on the list box (UIListBox<vector<GEColor> >::MouseLButtonDown, ICF-shared)

// FUNCTION: EXE 0x5ad4dc
// Handles a double click on the list box (UIListBox<vector<GEColor> >::MouseLButtonDoublePressed, ICF-shared)

// FUNCTION: EXE 0x5ad5a8
// Handles mouse moves over the list box (UIListBox<vector<GEColor> >::MouseMove, ICF-shared)

// FUNCTION: EXE 0x5ad67e
// Returns 0 (EEUIFSETerrainPaint::MouseRButtonDown: one ICF-folded body shared by many empty virtuals)

// FUNCTION: EXE 0x5ad681
// Handles the End key in the list box (UIListBox<vector<GEColor> >::ProcessEnd, ICF-shared)

// FUNCTION: EXE 0x5ad6d7
// Creates a terrain type list box (UIListBox<EESEUITTData> ctor)

// FUNCTION: EXE 0x5ad951
// Destroys a terrain type list box (UIListBox<EESEUITTData> dtor)

// FUNCTION: EXE 0x5ad9e5
// Appends a terrain line and updates the scroll bar (UIListBox<EESEUITTData>::AppendLine)

// FUNCTION: EXE 0x5ada0b
// Appends several terrain lines (UIListBox<EESEUITTData>::AppendStringArray)

// FUNCTION: EXE 0x5ada60
// Copies the terrain lines and layout for drawing (UIListBox<EESEUITTData>::CopyData)

// FUNCTION: EXE 0x5adcd9
// Removes every terrain line (UIListBox<EESEUITTData>::DeleteAllEntries)

// FUNCTION: EXE 0x5add0a
// Removes one terrain line (UIListBox<EESEUITTData>::DeleteLine)

// FUNCTION: EXE 0x5add98
// Gets the selected terrain line; fails when empty or unselected (UIListBox<EESEUITTData>::GetCurrentSelection)

// FUNCTION: EXE 0x5addd5
// Scrolls the list box by a wheel turn; returns 9 without a scroll bar (UIListBox<vector<GEColor> >::MouseWheelTurned)

// FUNCTION: EXE 0x5addf3
// Returns the number of terrain lines (UIListBox<EESEUITTData>::GetNumLines)

// FUNCTION: EXE 0x5ade0f
// Handles the Down key in the list box (UIListBox<vector<GEColor> >::ProcessDown, ICF-shared)

// FUNCTION: EXE 0x5ade79
// Takes the focus when tabbed to (UIListBox<vector<GEColor> >::TabbedTo, ICF-shared)

// FUNCTION: EXE 0x5ade81
// Resizes the terrain line array, filling with empty lines (vector<EESEUITTData>::resize)

// SYNTHETIC: EXE 0x5adeaa
// (the scalar deleting destructor, emitted with the vtable; slot UIListBox<std::vector<GEColor,std::allocator<GEColor> > >+0x0)

// SYNTHETIC: EXE 0x5adec6
// (the scalar deleting destructor, emitted with the vtable; slot UIListBox<EESEUITTData>+0x0)

// TEMPLATE: EXE 0x5adee2
// ?CheckScrollBar@?$UIListBox@V?$vector@VGEColor@@V?$allocator@VGEColor@@@std@@@std@@@@IAEXXZ (ICF-shared with the other UIListBox instantiations)

// FUNCTION: EXE 0x5ae0a7
// Assigns a color set array (vector<vector<GEColor> >::operator=)

// FUNCTION: EXE 0x5ae1f9
// Appends a color set (vector<vector<GEColor> >::push_back)

// FUNCTION: EXE 0x5ae225
// Erases color sets (vector<vector<GEColor> >::erase)

// FUNCTION: EXE 0x5ae298
// Empties a color set array (vector<vector<GEColor> >::clear)

// FUNCTION: EXE 0x5ae2d3
// Assigns a terrain line array (vector<EESEUITTData>::operator=)

// FUNCTION: EXE 0x5ae45c
// Copies a terrain line: name, preview texture and color set (EESEUITTData::operator=)

// FUNCTION: EXE 0x5ae47e
// Resizes the terrain line array, filling with a given line (vector<EESEUITTData>::resize)

// FUNCTION: EXE 0x5ae53b
// Returns a terrain line by index (vector<EESEUITTData>::operator[], ICF-shared with vector<EEWorldTableEntry>)

// FUNCTION: EXE 0x5ae54a
// Appends a terrain line (vector<EESEUITTData>::push_back)

// FUNCTION: EXE 0x5ae576
// Erases terrain lines (vector<EESEUITTData>::erase)

// FUNCTION: EXE 0x5ae5ea
// Empties the terrain line array (vector<EESEUITTData>::clear)

// FUNCTION: EXE 0x5ae625
// Destroys the terrain lines and frees their storage (vector<EESEUITTData>::_Tidy)

// FUNCTION: EXE 0x5ae65e
// Inserts a color set (vector<vector<GEColor> >::insert)

// FUNCTION: EXE 0x5ae6ae
// Inserts a terrain line (vector<EESEUITTData>::insert)

// FUNCTION: EXE 0x5ae705
// Inserts copies of a terrain line (vector<EESEUITTData>::insert)

// FUNCTION: EXE 0x5ae9ec
// Erases terrain lines (vector<EESEUITTData>::erase)

// FUNCTION: EXE 0x5aea56
// Fills raw storage with copies of a terrain line (vector<EESEUITTData>::_Ufill)

// FUNCTION: EXE 0x5aea7d
// Sorts the editor's terrains by localized name (std::sort with EESETerrainNameLess)

// FUNCTION: EXE 0x5aeb3a
// Copies a range of color sets (std::copy for vector<GEColor>)

// FUNCTION: EXE 0x5aeb63
// Copies terrain lines into raw storage (vector<EESEUITTData>::_Ucopy)

// FUNCTION: EXE 0x5aeb8b
// Copies a range of terrain lines (std::copy for EESEUITTData)

// FUNCTION: EXE 0x5aebc4
// Fills a range with a terrain line (std::fill for EESEUITTData)

// FUNCTION: EXE 0x5aebf5
// Copies a range of terrain lines backwards (std::copy_backward for EESEUITTData)

// FUNCTION: EXE 0x5aec2e
// Copies a range of terrain lines (std::copy for EESEUITTData)

// FUNCTION: EXE 0x5aec64
// Constructs a terrain line copy in place (std::_Construct for EESEUITTData)

// SYNTHETIC: EXE 0x5aec8a
// (the scalar deleting destructor, emitted with the vtable; name ??_GEEPGSPlayerReport@@QAEPAXI@Z, slot UWideString+0x0)

// FUNCTION: EXE 0x5aeca7
// Insertion-sorts a terrain range by name (std::_Insertion_sort, EESETerrainNameLess)

// FUNCTION: EXE 0x5aecd0
// Sorts a terrain range by name, falling back to a heap sort (std::_Sort, EESETerrainNameLess)

// FUNCTION: EXE 0x5aeda3
// Insertion-sorts a terrain range without bounds checks (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x5aedd4
// Insertion-sorts a terrain range by name (std::_Insertion_sort_1, EESETerrainNameLess)

// FUNCTION: EXE 0x5aee71
// Returns whether the first terrain's localized name sorts before the second's, ignoring case

// FUNCTION: EXE 0x5aeede
// Advances a pointer iterator by one element (std::_Ptrit::operator++, ICF-shared)

// FUNCTION: EXE 0x5aeee4
// Builds a heap of terrains by name (std::make_heap, EESETerrainNameLess)

// FUNCTION: EXE 0x5aef1e
// Sorts a terrain heap by name (std::sort_heap, EESETerrainNameLess)

// FUNCTION: EXE 0x5aef5f
// Partitions a terrain range around a median name (std::_Unguarded_partition)

// FUNCTION: EXE 0x5af07f
// Inserts a terrain into its sorted place without bounds checks (std::_Unguarded_insert)

// FUNCTION: EXE 0x5af0b2
// Builds a heap of terrains by name (std::_Make_heap)

// FUNCTION: EXE 0x5af0f6
// Moves the first terrain of a heap to its end (std::pop_heap)

// FUNCTION: EXE 0x5af11f
// Sifts a terrain down the heap (std::_Adjust_heap)

// FUNCTION: EXE 0x5af18d
// Pops the top of a terrain heap (std::_Pop_heap_0)

// FUNCTION: EXE 0x5af1e0
// Sifts a terrain up the heap (std::_Push_heap)

// FUNCTION: EXE 0x5af22f
// Pops the top of a terrain heap (std::_Pop_heap)

// SYNTHETIC: EXE 0x5af263
// (the scalar deleting destructor, emitted with the vtable; )

// SYNTHETIC: EXE 0x5af28c
// (the scalar deleting destructor, emitted with the vtable; slot UIScrollBar+0x0)
