// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEMap.cpp (__FILE__ 0x4c5148, the ctor's ESVERIFYs). The editor's map tab:
// 0x1a18f1..0x1a3f10 (UIBMap::ButtonUp, UIRBMap's Check/HideControls, EEUIFSEEMapType::ProcessEvent, the tab's ctor,
// the helper classes' ??_G, the tab's ??_G/dtor/members, then the vector push_back and EETerrainTable::GetNumTables
// COMDATs). After EEUIFSEMainButton.cpp (0x1a147f), before EEUIFSEPaint.cpp (0x1a3f10). Unit flags: the exe module
// set.
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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEESElevation.h"
#include "ee/EEUIFSEElevation.h"
#include "lle/URandom.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EESEACreateBlankMap.h"
#include "ee/EESEACreateRandomMap.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EEStringIDs.h"
#include "ee/EERules.h"
// the map sizes offered by the width/height combo boxes (0x4c4f40, 25 entries) (provisional name)
static long sSEMapSizes[25] = {15, 25, 50, 60, 70, 75, 80, 90, 100, 125, 130, 135, 140, 145, 150, 155, 160, 165, 175,
                               200, 225, 250, 300, 350, 400};

// FUNCTION: EXE 0x5a18f1
// Releases the Create Map button and creates the map from the tab's settings
void UIBMap::ButtonUp()
{
    UIButton::ButtonUp();
    this->mMap->CreateMap();
}

// FUNCTION: EXE 0x5a1907
// Selects a map mode (blank, random or seeded) and shows only that mode's controls; blank re-runs the
// elevation file selection handler
void UIRBMap::Check()
{
    UIRadioButton::Check();
    this->HideControls();
    switch (this->mId) {
    case 0x6f:
        this->GetForm()->GetControlById(kSEMapMapStyleLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapMapStyle)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapSeedLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapRandomMapSize)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapRandomMapSizeLabel)->SetDisplayMode(kDisplayModeShown);
        break;
    case 0x6e:
        this->GetForm()->GetControlById(kSEMapDefaultTerrainLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapDefaultTerrain)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapWidth)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapHeight)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapWidthLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapHeightLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapElevationFile)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapElevationFileLabel)->SetDisplayMode(kDisplayModeShown);
        {
            UIForm* theForm = this->GetForm();
            theForm->GetControlById(kSEMapElevationFile)->GetTriggerEvent(kControlTriggerSelectionChanged)->ProcessEvent(NULL, theForm->GetControlById(kSEMapElevationFile));
        }
        break;
    case 0x6d:
        this->GetForm()->GetControlById(kSEMapMapStyleLabel)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapMapStyle)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapSeedEditBox)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapRandomMapSize)->SetDisplayMode(kDisplayModeShown);
        this->GetForm()->GetControlById(kSEMapRandomMapSizeLabel)->SetDisplayMode(kDisplayModeShown);
        break;
    }
}

// FUNCTION: EXE 0x5a1a6b
// Hides every mode-specific control of the map tab
void UIRBMap::HideControls()
{
    this->mForm->GetControlById(kSEMapDefaultTerrainLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapDefaultTerrain)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapMapStyleLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapMapStyle)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapSeedLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapSeedEditBox)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWidth)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapHeight)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWidthLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapHeightLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapRandomMapSize)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapRandomMapSizeLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapElevationFile)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapElevationFileLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWaterCutoffLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWaterDepthLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapLandCutoffLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapLandHeightLabel)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapLandCutoff)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapLandHeight)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWaterDepth)->SetDisplayMode(kDisplayModeHidden);
    this->mForm->GetControlById(kSEMapWaterCutoff)->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x5a1bc6
// Shows the elevation controls when an elevation file is selected (selection above 0), else hides them
long EEUIFSEEMapType::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theSelection;
    static_cast<UIComboBox*>(inEntity)->GetCurrentSelection(theSelection);
    if (theSelection > 0)
        this->mMap->ShowElevationControls(true);
    else
        this->mMap->ShowElevationControls(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5a1be6
// Builds the map tab: the editor terrains, the size, terrain, style and seed controls, the mode radio buttons,
// the Create Map button and the elevation file controls (the seed edit box starts with the text "HE0")
EEUIFSEMap::EEUIFSEMap(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    UWideString theText;
    GE2DPoint thePosition(0.0f, 0.0f);
    GEText theMeasure(inFont);
    GEText* theGEText;
    FSFileSpec theSpec;
    std::vector<UWideString> theStrings;
    char theBuffer[256];
    unsigned long i;
    unsigned long theCount;
    unsigned long theWidth;
    EETerrainTable* theTable;

    for (i = 0; i < EETerrainTable::GetNumTables(); i++) {
        theTable = EETerrainTable::Get(i);
        if (theTable != NULL && theTable->mIsInEditor)
            this->mTerrains.push_back(EETerrainTable::Get(i));
    }

    gResource->GetString(IDS_MAP_WIDTH, theText);
    thePosition.SetXY(130.0f, 616.0f);
    this->mWidthLabel = new UITextLabel(theText, inFont, kSEMapWidthLabel, GERectangle(thePosition, 75.0f, (float)inFont->GetHeight()),
                                  inForm, 0, ioIndex, 0);
    theGEText = this->mWidthLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mWidthLabel, 1);
    this->mWidthLabel->SetBgOpacity(0.4f);
    this->mWidthLabel->SetAlignment(kTextAlignCenter);
    ioIndex++;

    gResource->GetString(IDS_SE_MAP_MAP_HEIGHT, theText);
    thePosition.SetXY((float)this->mWidthLabel->GetWidth() + 135.0f, 616.0f);
    this->mHeightLabel = new UITextLabel(theText, inFont, kSEMapHeightLabel, GERectangle(thePosition, 75.0f, (float)inFont->GetHeight()),
                                   inForm, 0, ioIndex, 0);
    this->mHeightLabel->SetBgOpacity(0.4f);
    this->mHeightLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mHeightLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mHeightLabel, 1);
    ioIndex++;

    theStrings.resize(25);
    for (i = 0; i < 25; i++) {
        sprintf(theBuffer, "%ld", sSEMapSizes[i]);
        theStrings[i] = theBuffer;
    }

    thePosition.SetX(this->mWidthLabel->GetRectangle().GetLeft()); thePosition.SetY(this->mWidthLabel->GetRectangle().GetBottom() + 5.0f);
    this->mWidth = new UIComboBox(kSEMapWidth, 1, theStrings, inFont, GERectangle(thePosition, 75.0f, 20.0f), 0x180, inForm, ioIndex,
                            0xff, 0);
    this->mWidth->SetTextBoxBgOpacity(0.4f);
    ioIndex++;
    thePosition.SetXY(this->mHeightLabel->GetRectangle().GetLeft(), this->mHeightLabel->GetRectangle().GetBottom() + 5.0f);
    this->mHeight = new UIComboBox(kSEMapHeight, 1, theStrings, inFont, GERectangle(thePosition, 75.0f, 20.0f), 0x180, inForm,
                             ioIndex, 0xff, 0);
    this->mHeight->SetListBoxAlwaysDirty(true);
    this->mHeight->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mWidth->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mWidth->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mWidth->SetListBoxAlwaysDirty(true);
    theGEText = this->mHeight->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mHeight->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mWidth, 1);
    inForm->AddControl(this->mHeight, 1);
    ioIndex++;

    gResource->GetString(IDS_DEFAULT_TERRAIN, theText);
    thePosition.SetX(this->mWidth->GetRectangle().GetLeft()); thePosition.SetY(this->mWidth->GetTextLabel()->GetRectangle().GetBottom() + 5.0f);
    theWidth = (unsigned long)(this->mHeight->GetRectangle().GetRight() - this->mWidth->GetRectangle().GetLeft());
    this->mDefaultTerrainLabel = new UITextLabel(theText, inFont, kSEMapDefaultTerrainLabel,
                                           GERectangle(thePosition, (float)theWidth, (float)inFont->GetHeight()),
                                           inForm, 0, ioIndex, 0);
    theGEText = this->mDefaultTerrainLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mDefaultTerrainLabel->SetAlignment(kTextAlignCenter);
    this->mDefaultTerrainLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mDefaultTerrainLabel, 1);
    ioIndex++;

    gResource->GetString(IDS_SE_MAP_MAP_TYPE, theText);
    theMeasure.SetText(theText);
    thePosition.SetXY(this->mWidthLabel->GetRectangle().GetLeft(),
                      this->mWidth->GetTextLabel()->GetRectangle().GetBottom() + 5.0f);
    this->mMapStyleLabel = new UITextLabel(theText, inFont, kSEMapMapStyleLabel,
                                     GERectangle(thePosition, (float)theWidth, (float)inFont->GetHeight()), inForm, 0,
                                     ioIndex, 0);
    this->mMapStyleLabel->SetBgOpacity(0.4f);
    this->mMapStyleLabel->SetAlignment(kTextAlignCenter);
    theGEText = this->mMapStyleLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mMapStyleLabel, 1);
    ioIndex++;

    theCount = EERMGetNumberOfGenerators();
    theStrings.resize(theCount);
    for (i = 0; i < theCount; i++)
        theStrings[i] = EERMGetGeneratorDisplayName(i);

    thePosition.SetX(this->mWidthLabel->GetRectangle().GetLeft()); thePosition.SetY(this->mMapStyleLabel->GetRectangle().GetBottom() + 5.0f);
    this->mMapStyle = new UIComboBox(kSEMapMapStyle, 1, theStrings, inFont,
                               GERectangle(thePosition,
                                           this->mHeightLabel->GetRectangle().GetRight() -
                                               this->mWidthLabel->GetRectangle().GetLeft(),
                                           20.0f),
                               0x180, inForm, ioIndex, 0xff, 0);
    ESVERIFY_AT(this->mMapStyle, 507);
    this->mMapStyle->SetListBoxAlwaysDirty(true);
    this->mMapStyle->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mMapStyle->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mMapStyle->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mMapStyle, 1);
    ioIndex++;

    theCount = this->mTerrains.size();
    theStrings.resize(theCount);
    for (i = 0; i < theCount; i++) {
        gResource->GetString(this->mTerrains[i]->mNameID, theText);
        theStrings[i] = theText;
    }

    thePosition.SetXY(this->mDefaultTerrainLabel->GetRectangle().GetLeft(),
                      this->mDefaultTerrainLabel->GetRectangle().GetBottom() + 5.0f);
    this->mDefaultTerrain = new UIComboBox(kSEMapDefaultTerrain, 1, theStrings, inFont,
                                     GERectangle(thePosition,
                                                 this->mHeightLabel->GetRectangle().GetRight() -
                                                     this->mWidthLabel->GetRectangle().GetLeft(),
                                                 20.0f),
                                     0x180, inForm, ioIndex, 0xff, 0);
    ESVERIFY_AT(this->mDefaultTerrain, 557);
    this->mDefaultTerrain->SetListBoxAlwaysDirty(true);
    this->mDefaultTerrain->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mDefaultTerrain->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mDefaultTerrain->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mDefaultTerrain, 1);
    ioIndex++;

    thePosition.SetXY(0.0f, 0.0f);
    this->mSeedEditBox = new UIEditBox(kSEMapSeedEditBox, inFont, 6, true,
                                 GERectangle(thePosition, 50.0f, (float)inFont->GetHeight() + 4.0f), inForm, ioIndex,
                                 2, false, false);
    theGEText = this->mSeedEditBox->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mSeedEditBox->SetBgOpacity(1.0f);
    this->mSeedEditBox->SetText(UWideString("HE0"));
    inForm->AddControl(this->mSeedEditBox, 1);
    ioIndex++;

    thePosition.SetXY(0.0f, 0.0f);
    theText = UWideString("0");
    this->mSeedLabel = new UITextLabel(theText, inFont, kSEMapSeedLabel,
                                 GERectangle(thePosition, 50.0f, (float)inFont->GetHeight() + 4.0f), inForm, 0,
                                 ioIndex, 2);
    this->mSeedLabel->SetBgOpacity(0.4f);
    theGEText = this->mSeedLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    inForm->AddControl(this->mSeedLabel, 1);
    ioIndex++;

    theWidth = (unsigned long)inForm->GetManager()->GetRadioOn()->GetHeight();
    if (theWidth < (unsigned long)inFont->GetHeight())
        theWidth = inFont->GetHeight();

    gResource->GetString(IDS_BLANK, theText);
    thePosition.SetXY(25.0f, 616.0f);
    this->mMap[0] = new UIRBMap(0x6e, theText, inFont, thePosition, 100.0f, theWidth, inForm, ioIndex, true);
    ESVERIFY_AT(this->mMap[0], 644);
    this->mMap[0]->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mMap[0]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mMap[0], 1);
    ioIndex++;

    gResource->GetString(IDS_RANDOM, theText);
    thePosition.SetY(this->mMap[0]->GetRectangle().GetBottom() + 1.0f);
    this->mMap[1] = new UIRBMap(0x6f, theText, inFont, thePosition, 100.0f, theWidth, inForm, ioIndex, false);
    ESVERIFY_AT(this->mMap[1], 667);
    this->mMap[1]->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mMap[1]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mMap[1], 1);
    ioIndex++;

    gResource->GetString(IDS_SEED, theText);
    thePosition.SetY(this->mMap[1]->GetRectangle().GetBottom() + 1.0f);
    this->mMap[2] = new UIRBMap(0x6d, theText, inFont, thePosition, 100.0f, theWidth, inForm, ioIndex, false);
    ESVERIFY_AT(this->mMap[2], 690);
    this->mMap[2]->GetTextLabel()->SetBgOpacity(0.4f);
    theGEText = this->mMap[2]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mMap[2], 1);
    ioIndex++;

    this->mCreateMap = new UIBMap(0x65, thePosition, 260.0f, inFont->GetHeight() + 10.0f, inForm, ioIndex, this);
    ESVERIFY_AT(this->mCreateMap, 712);
    this->mCreateMap->SetBgOpacity(1.0f);
    inForm->AddControl(this->mCreateMap, 1);
    ioIndex++;

    thePosition.SetX(this->mMap[2]->GetTextLabel()->GetRectangle().GetLeft()); thePosition.SetY(this->mMap[2]->GetRectangle().GetBottom() + 5.0f);
    this->mSeedEditBox->SetLocation(thePosition);
    this->mSeedLabel->SetLocation(thePosition);
    thePosition.SetXY(this->mMap[0]->GetRectangle().GetLeft(), this->mSeedEditBox->GetRectangle().GetBottom() + 5.0f);
    this->mCreateMap->SetLocation(thePosition);
    gResource->GetString(IDS_CREATE_MAP, theText);
    theMeasure.SetText(theText);
    this->mCreateMap->SetText(theText, inFont);
    theGEText = this->mCreateMap->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));

    gResource->GetString(IDS_INGAME_MAP_SIZE, theText);
    thePosition.SetX(this->mWidthLabel->GetRectangle().GetLeft()); thePosition.SetY(this->mWidthLabel->GetRectangle().GetTop());
    this->mRandomMapSizeLabel = new UITextLabel(theText, inFont, kSEMapRandomMapSizeLabel,
                                          GERectangle(thePosition,
                                                      (float)(unsigned long)(this->mHeightLabel->GetRectangle().GetRight() -
                                                                             this->mWidthLabel->GetRectangle().GetLeft()),
                                                      (float)inFont->GetHeight()),
                                          inForm, 0, ioIndex, 0);
    ESVERIFY_AT(this->mRandomMapSizeLabel, 765);
    theGEText = this->mRandomMapSizeLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mRandomMapSizeLabel->SetAlignment(kTextAlignCenter);
    this->mRandomMapSizeLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mRandomMapSizeLabel, 1);
    ioIndex++;

    theStrings.resize(EEGetRules().mGameOptions.mMapSizes.size());   // gameoptions.yaml map_sizes
    for (i = 0; i < theStrings.size(); i++)
        gResource->GetString(EEGetRules().mGameOptions.mMapSizes[i].mNameID, theStrings[i]);

    thePosition.SetXY(this->mWidth->GetRectangle().GetLeft(), this->mWidth->GetRectangle().GetTop());
    this->mRandomMapSize = new UIComboBox(kSEMapRandomMapSize, 1, theStrings, inFont,
                                    GERectangle(thePosition,
                                                (float)(unsigned long)(this->mHeightLabel->GetRectangle().GetRight() -
                                                                       this->mWidthLabel->GetRectangle().GetLeft()),
                                                20.0f),
                                    0x180, inForm, ioIndex, 0xff, 0);
    ESVERIFY_AT(this->mRandomMapSize, 786);
    this->mRandomMapSize->SetListBoxAlwaysDirty(true);
    this->mRandomMapSize->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mRandomMapSize->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mRandomMapSize->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mRandomMapSize, 1);
    ioIndex++;

    theSpec.SetVolume(gStartupVolume);
    theSpec.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\");
    FSPath* thePath = new FSPath;
    thePath->AddDirectory(theSpec, true);

    gResource->GetString(IDS_ELEVATION_FILE, theText);
    thePosition.SetX(this->mHeightLabel->GetRectangle().GetRight() + 15.0f); thePosition.SetY(this->mHeightLabel->GetRectangle().GetTop());
    this->mElevationFileLabel = new UITextLabel(theText, inFont, kSEMapElevationFileLabel,
                                          GERectangle(thePosition, 200.0f, (float)inFont->GetHeight()), inForm, 0,
                                          ioIndex, 0);
    ESVERIFY_AT(this->mElevationFileLabel, 812);
    theGEText = this->mElevationFileLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mElevationFileLabel->SetAlignment(kTextAlignCenter);
    this->mElevationFileLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mElevationFileLabel, 1);
    ioIndex++;

    thePosition.SetY(this->mElevationFileLabel->GetRectangle().GetBottom() + 5.0f);
    this->mElevationFile = new EEUIFileComboBox(thePath, UString("*") + ".dat", kSEMapElevationFile, 1, std::vector<UWideString>(), inFont,
                                          thePosition, 200, 400, inForm, ioIndex, 0, 2, false);
    theGEText = this->mElevationFile->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mElevationFile->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mElevationFile->SetTextBoxBgOpacity(0.4f);
    UIEvent* theEvent = new EEUIFSEEMapType(this);
    this->mElevationFile->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mElevationFile->SetStringTableID(IDS_ELEVATION_FILE_TO_USE_WITH_BLANK);
    inForm->AddControl(this->mElevationFile, 1);
    ioIndex++;
    thePath->Dereference();

    gResource->GetString(IDS_WATER_DEPTH, theText);
    thePosition.SetXY(this->mElevationFileLabel->GetRectangle().GetLeft(),
                      this->mElevationFileLabel->GetRectangle().GetBottom() + 30.0f);
    this->mWaterDepthLabel = new UITextLabel(theText, inFont, kSEMapWaterDepthLabel, GERectangle(thePosition, 150.0f, 15.0f), inForm, 0,
                                       ioIndex, 0);
    ESVERIFY_AT(this->mWaterDepthLabel, 867);
    theGEText = this->mWaterDepthLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mWaterDepthLabel->SetAlignment(kTextAlignCenter);
    this->mWaterDepthLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mWaterDepthLabel, 1);
    ioIndex++;

    thePosition.SetX(this->mWaterDepthLabel->GetRectangle().GetRight() + 1.0f);
    this->mWaterDepth = new UIEditBox(kSEMapWaterDepth, inFont, 6, true, GERectangle(thePosition, 49.0f, 15.0f), inForm, ioIndex, 2,
                                false, false);
    theGEText = this->mWaterDepth->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mWaterDepth->SetBgOpacity(1.0f);
    this->mWaterDepth->SetText(UWideString("0"));
    this->mWaterDepth->SetStringTableID(IDS_THIS_IS_MAXIMUM_DEPTH_THAT_WATER);
    inForm->AddControl(this->mWaterDepth, 1);
    ioIndex++;

    gResource->GetString(IDS_WATER_CUTOFF, theText);
    thePosition.SetXY(this->mElevationFileLabel->GetRectangle().GetLeft(),
                      this->mElevationFileLabel->GetRectangle().GetBottom() + 45.0f);
    this->mWaterCutoffLabel = new UITextLabel(theText, inFont, kSEMapWaterCutoffLabel, GERectangle(thePosition, 150.0f, 15.0f), inForm, 0,
                                        ioIndex, 0);
    ESVERIFY_AT(this->mWaterCutoffLabel, 911);
    theGEText = this->mWaterCutoffLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mWaterCutoffLabel->SetAlignment(kTextAlignCenter);
    this->mWaterCutoffLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mWaterCutoffLabel, 1);
    ioIndex++;

    thePosition.SetX(this->mWaterCutoffLabel->GetRectangle().GetRight() + 1.0f);
    this->mWaterCutoff = new UIEditBox(kSEMapWaterCutoff, inFont, 6, true, GERectangle(thePosition, 49.0f, 15.0f), inForm, ioIndex, 2,
                                 false, false);
    theGEText = this->mWaterCutoff->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mWaterCutoff->SetBgOpacity(1.0f);
    this->mWaterCutoff->SetText(UWideString("0"));
    this->mWaterCutoff->SetStringTableID(IDS_THIS_IS_DEPTH_IN_METERS);
    inForm->AddControl(this->mWaterCutoff, 1);
    ioIndex++;

    gResource->GetString(IDS_LAND_HEIGHT, theText);
    thePosition.SetXY(this->mElevationFileLabel->GetRectangle().GetLeft(),
                      this->mElevationFileLabel->GetRectangle().GetBottom() + 60.0f);
    this->mLandHeightLabel = new UITextLabel(theText, inFont, kSEMapLandHeightLabel, GERectangle(thePosition, 150.0f, 15.0f), inForm, 0,
                                       ioIndex, 0);
    ESVERIFY_AT(this->mLandHeightLabel, 955);
    theGEText = this->mLandHeightLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mLandHeightLabel->SetAlignment(kTextAlignCenter);
    this->mLandHeightLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mLandHeightLabel, 1);
    ioIndex++;

    thePosition.SetX(this->mLandHeightLabel->GetRectangle().GetRight() + 1.0f);
    this->mLandHeight = new UIEditBox(kSEMapLandHeight, inFont, 6, true, GERectangle(thePosition, 49.0f, 15.0f), inForm, ioIndex, 2,
                                false, false);
    theGEText = this->mLandHeight->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mLandHeight->SetBgOpacity(1.0f);
    this->mLandHeight->SetText(UWideString("0"));
    this->mLandHeight->SetStringTableID(IDS_THIS_IS_MAXIMUM_HEIGHT_THAT_LAND);
    inForm->AddControl(this->mLandHeight, 1);
    ioIndex++;

    gResource->GetString(IDS_LAND_CUTOFF, theText);
    thePosition.SetX(this->mElevationFileLabel->GetRectangle().GetLeft());
    thePosition.SetY(this->mElevationFileLabel->GetRectangle().GetBottom() + 75.0f);
    this->mLandCutoffLabel = new UITextLabel(theText, inFont, kSEMapLandCutoffLabel, GERectangle(thePosition, 150.0f, 15.0f), inForm, 0,
                                       ioIndex, 0);
    ESVERIFY_AT(this->mLandCutoffLabel, 999);
    theGEText = this->mLandCutoffLabel->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mLandCutoffLabel->SetAlignment(kTextAlignCenter);
    this->mLandCutoffLabel->SetBgOpacity(0.4f);
    inForm->AddControl(this->mLandCutoffLabel, 1);
    ioIndex++;

    thePosition.SetX(this->mLandCutoffLabel->GetRectangle().GetRight() + 1.0f);
    this->mLandCutoff = new UIEditBox(kSEMapLandCutoff, inFont, 6, true, GERectangle(thePosition, 49.0f, 15.0f), inForm, ioIndex, 2,
                                false, false);
    theGEText = this->mLandCutoff->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mLandCutoff->SetBgOpacity(1.0f);
    this->mLandCutoff->SetText(UWideString("0"));
    this->mLandCutoff->SetStringTableID(IDS_THIS_IS_HEIGHT_IN_METERS);
    inForm->AddControl(this->mLandCutoff, 1);
    ioIndex++;

    this->mMapStyle->SetDisplayMode(kDisplayModeHidden);
    this->mMapStyleLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSeedLabel->SetDisplayMode(kDisplayModeHidden);
    this->mSeedEditBox->SetDisplayMode(kDisplayModeHidden);
    this->mRandomMapSize->SetDisplayMode(kDisplayModeHidden);
    this->mRandomMapSizeLabel->SetDisplayMode(kDisplayModeHidden);
    this->ShowElevationControls(false);
}

// FUNCTION: EXE 0x5a3a9a
// Destroys the Create Map button and frees it if asked (UIBMap's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5a3abd
// Destroys and frees a map mode radio button (UIRBMap's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5a3ae0
// Destroys and frees the elevation file event (EEUIFSEEMapType's scalar deleting destructor, with its vtable)

// SYNTHETIC: EXE 0x5a3b0b
// (EEUIFSEMap's scalar deleting destructor)

// FUNCTION: EXE 0x5a3b27
// Destroys the map tab, deleting all of its controls
EEUIFSEMap::~EEUIFSEMap()
{
    delete this->mWidthLabel;
    delete this->mHeightLabel;
    delete this->mWidth;
    delete this->mHeight;
    delete this->mDefaultTerrainLabel;
    delete this->mMapStyleLabel;
    delete this->mMap[0];
    delete this->mMap[1];
    delete this->mMap[2];
    delete this->mSeedLabel;
    delete this->mSeedEditBox;
    delete this->mCreateMap;
    delete this->mDefaultTerrain;
    delete this->mMapStyle;
    delete this->mRandomMapSize;
    delete this->mRandomMapSizeLabel;
    delete this->mElevationFileLabel;
    delete this->mLandHeight;
    delete this->mLandHeightLabel;
    delete this->mWaterCutoff;
    delete this->mWaterCutoffLabel;
    delete this->mLandCutoff;
    delete this->mLandCutoffLabel;
    delete this->mWaterDepth;
    delete this->mWaterDepthLabel;
    delete this->mElevationFile;
}

// FUNCTION: EXE 0x5a3c7d
// Creates a blank map of the chosen size and terrain (then draws a new random seed), or a random map of the chosen
// size and style with a fresh seed (shown in the seed label) or the seed typed in the edit box
void EEUIFSEMap::CreateMap()
{
    URandomIntegers theRandom;
    unsigned long theTerrain;
    unsigned long theSize;
    unsigned long theStyle;

    if (this->mMap[0]->GetIsChecked()) {
        long theWidth = atoi(this->mWidth->GetTextLabel()->GetText(0));
        long theHeight = atoi(this->mHeight->GetTextLabel()->GetText(0));
        this->mDefaultTerrain->GetListBox()->GetCurrentSelection(theTerrain);
        long theTerrainID = (this->mTerrains[theTerrain])->mID;
        EEAction* theAction = new EESEACreateBlankMap(theWidth, theHeight, theTerrainID);
        theAction->Dereference();
        theRandom.SetSeed(TMGetTime());
        gUserInterface->mRandomSeed = theRandom.Generate(0, 1000000);
    } else {
        this->mRandomMapSize->GetCurrentSelection(theSize);
        if (this->mMap[1]->GetIsChecked()) {
            theRandom.SetSeed(TMGetTime());
            gUserInterface->mRandomSeed = theRandom.Generate(0, 1000000);
            this->mSeedLabel->SetText((long)gUserInterface->mRandomSeed);
        } else
            gUserInterface->mRandomSeed = atoi(this->mSeedEditBox->GetText(0));
        this->mMapStyle->GetCurrentSelection(theStyle);
        EEAction* theAction = new EESEACreateRandomMap(theSize, theStyle);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x5a3dec
// In blank mode shows or hides the water/land elevation controls (the height box does the opposite); else hides all
void EEUIFSEMap::ShowElevationControls(bool inShow)
{
    if (this->mMap[0]->GetIsChecked() == true) {
        this->mWaterCutoff->SetDisplayMode(inShow == true);
        this->mWaterCutoffLabel->SetDisplayMode(inShow == true);
        this->mWaterDepth->SetDisplayMode(inShow == true);
        this->mWaterDepthLabel->SetDisplayMode(inShow == true);
        this->mLandCutoff->SetDisplayMode(inShow == true);
        this->mLandCutoffLabel->SetDisplayMode(inShow == true);
        this->mLandHeight->SetDisplayMode(inShow == true);
        this->mLandHeightLabel->SetDisplayMode(inShow == true);
        this->mHeight->SetDisplayMode(inShow != true);
        this->mHeightLabel->SetDisplayMode(inShow != true);
    } else {
        this->mWaterCutoff->SetDisplayMode(kDisplayModeHidden);
        this->mWaterCutoffLabel->SetDisplayMode(kDisplayModeHidden);
        this->mWaterDepth->SetDisplayMode(kDisplayModeHidden);
        this->mWaterDepthLabel->SetDisplayMode(kDisplayModeHidden);
        this->mLandCutoff->SetDisplayMode(kDisplayModeHidden);
        this->mLandCutoffLabel->SetDisplayMode(kDisplayModeHidden);
        this->mLandHeight->SetDisplayMode(kDisplayModeHidden);
        this->mLandHeightLabel->SetDisplayMode(kDisplayModeHidden);
        this->mHeight->SetDisplayMode(kDisplayModeHidden);
        this->mHeightLabel->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x5a3ebf
// Refreshes the elevation file list when the tab is entered
void EEUIFSEMap::Enter()
{
    if (this->mElevationFile != NULL)
        this->mElevationFile->CreateEntries();
}

// FUNCTION: EXE 0x5a3ecc
// Appends a terrain table to the tab's terrain list (vector<EETerrainTable*>::push_back; first user the ctor)

// FUNCTION: EXE 0x5a3ef8
// Returns the number of terrain tables (EETerrainTable::GetNumTables, inline COMDAT; first user the ctor)
