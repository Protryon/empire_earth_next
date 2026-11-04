// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFScenarioEditor). The scenario editor's main form, first part: 0x19cc39..0x19e2d2 (the form's own events'
// handlers, the ctor, the event ??_Gs it emits). The unit continues with the parts once kept as EEUIEGrabScroll.cpp
// (0x19e2d2) and EEUIEOpenCivBuilder.cpp (0x19eba9): EEUIFScenarioEditor.h places the unit at ~0x19cc39..0x1a0315
// (one file). After EESEUnit.cpp. Unit flags: the exe module set.
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
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
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
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIFScenarioEditorEvents.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMCondition.h"
#include <stdio.h>
#include "ee/EEUIMiniMap.h"
#include "ee/EEUIEWheelZoom.h"
#include "ee/EEUIEHotKeyZoom.h"
#include "ee/EEUIECameraEvents.h"
#include "ee/EEUIEGrabScroll.h"
#include "ee/EEUIEMainGameEvents.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EESEEHideUI.h"
#include "ee/EEStringIDs.h"
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
extern unsigned long gTexturePixelDepth;    // 0x5308b4
extern GEFont* gU531930;                    // 0x531930 (Empire Earth.cpp's fonts)
extern GEFont* gU531934;                    // 0x531934
extern GEFont* gU531938;                    // 0x531938

// FUNCTION: EXE 0x59cc39
// Refreshes the unit tab's unit types and the players tab's tech lists when another player is picked
long EEUIENewPlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(inForm);
    theEditor->GetUnitScreen()->ReselectUnitType();
    static_cast<EESEPlayerScreen*>(theEditor->mTabGroups[kEditorTabPlayers])->FillTechLists();
    return kErrorOK;
}

// FUNCTION: EXE 0x59cc5e
// Turns the camera back to the editor's home rotation (45 degree yaw, 35 degree downward pitch) (Home key)
long EEUIEReturnToHomeRotation::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    GETransformation theTransformation;
    theTransformation.MakeIdentity();
    gWorld.mCameraControl.SetViewTurn(theTransformation, false);
    theTransformation.SetOrientationYPR(0.7853982f, -0.61086524f, 0.0f);
    gWorld.mCameraControl.SetOffsetTurn(theTransformation, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x59ccc0
// Saves the scenario triggers to ScenarioTriggers.scn and writes a ScenarioTriggers.txt report of every trigger,
// the unused effects, conditions, objects and areas and slow looping triggers (Ctrl+S); returns the open result
long EEUIESaveScenarioTriggersOnly::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theFile;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    UString thePath(260);
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFile.SetFile("ScenarioTriggers.scn");
    theFile.Create();
    long theResult = theFile.Open(kFileAccessWrite);
    if (theResult == kErrorOK)
        theManager->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
    theFile.Close();
    thePath = gStartupVolume + gStartupDirectory + "ScenarioTriggers.txt";
    if (theResult == kErrorOK) {
        FILE* theText = fopen(thePath, "w");
        if (theText) {
            unsigned long i;
            for (i = 0; i < theManager->mTriggers.size(); i++) {
                EESMTrigger* theTrigger = theManager->GetTrigger(i);
                fprintf(theText, "%s\n", (const char*)theTrigger->mName);
                if (theTrigger->mText.GetLength() > 0)
                    fprintf(theText, "Description:\n\"%s\"\n", (const char*)theTrigger->mText);
                if (!theTrigger->mIsOn)
                    fprintf(theText, "\tTrigger starts OFF\n");
                if (theTrigger->mConditionDuration)
                    fprintf(theText, "\tConditions True for %d\n", theTrigger->mConditionDuration);
                if (theTrigger->mIsLooping)
                    fprintf(theText, "\tLooping\n");
                fprintf(theText, "\tIf ");
                for (unsigned long j = 0; j < theTrigger->GetConditionCount(); j++) {
                    EESMTConditionWrapper* theWrapper = theTrigger->GetCondition(j);
                    if (theWrapper->mIsNot)
                        fprintf(theText, " NOT");
                    EESMCondition* theCondition = theManager->GetCondition(theWrapper->mValue);
                    if (theCondition)
                        fprintf(theText, " (%s) ", (const char*)theCondition->mName);
                    else
                        fprintf(theText, " (BAD CONDITION) ");
                    if (theWrapper->mOperator == kSMTriggerOperatorAnd)
                        fprintf(theText, " AND");
                    else if (theWrapper->mOperator == kSMTriggerOperatorOr)
                        fprintf(theText, " OR");
                }
                fprintf(theText, "\n\tThen ");
                for (j = 0; j < theTrigger->mEffects.size(); j++) {
                    EESMTEffectWrapper* theWrapper = theTrigger->GetEffect(j);
                    if (theWrapper->mDelay)
                        fprintf(theText, " (Delay %d sec) ", theWrapper->mDelay);
                    EESMEffect* theEffect = theManager->GetEffect(theWrapper->mEffectIndex);
                    if (theEffect)
                        fprintf(theText, " (%s) ", (const char*)theEffect->mName);
                    else
                        fprintf(theText, " (BAD EFFECT) ");
                    if (theWrapper->mOperator == kSMTriggerOperatorAnd)
                        fprintf(theText, " AND");
                    else if (theWrapper->mOperator == kSMTriggerOperatorOr)
                        fprintf(theText, " OR");
                }
                fprintf(theText, "\n\n");
            }

            for (i = 1; i < theManager->mEffects.size(); i++) {
                bool theUnused = true;
                EESMEffect* theEffect = theManager->GetEffect(i);
                if (theEffect->mType == kSMEffectNone)
                    theUnused = false;
                for (unsigned long j = 0; theUnused && j < theManager->GetNumTriggers(); j++) {
                    EESMTrigger* theTrigger = theManager->GetTrigger(j);
                    for (unsigned long k = 0; theUnused && k < theTrigger->mEffects.size(); k++) {
                        if (theTrigger->GetEffect(k)->mEffectIndex == i)
                            theUnused = false;
                    }
                }
                if (theUnused)
                    fprintf(theText, "Effect: (%s) is not used in any trigger\n", (const char*)theManager->GetEffect(i)->mName);
            }

            for (i = 1; i < theManager->GetNumConditions(); i++) {
                bool theUnused = true;
                EESMCondition* theCondition = theManager->GetCondition(i);
                if (theCondition->mType < kSMConditionPlayerAttribute)
                    theUnused = false;
                for (unsigned long j = 0; theUnused && j < theManager->mTriggers.size(); j++) {
                    EESMTrigger* theTrigger = theManager->GetTrigger(j);
                    for (unsigned long k = 0; theUnused && k < theTrigger->mConditions.size(); k++) {
                        if (theTrigger->GetCondition(k)->mValue == i)
                            theUnused = false;
                    }
                }
                if (theUnused)
                    fprintf(theText, "Condition: (%s) is not used in any trigger\n", (const char*)theManager->GetCondition(i)->mName);
            }

            for (i = 1; i < theManager->mObjects.size(); i++) {
                bool theUnused = true;
                for (unsigned long j = 1; theUnused && j < theManager->mEffects.size(); j++) {
                    EESMEffect* theEffect = theManager->GetEffect(j);
                    for (unsigned long k = 0; theUnused && k < theEffect->mObjects.size(); k++) {
                        if (theEffect->mObjects[k] == i)
                            theUnused = false;
                    }
                    if (theUnused && (theEffect->mTargetObject == i || theEffect->mPlaceObject == i ||
                        theEffect->mTextObject == i || theEffect->mObject == i ||
                        theEffect->mTaskObject == i || theEffect->mLocationObject == i))
                        theUnused = false;
                }
                for (j = 1; theUnused && j < theManager->mConditions.size(); j++) {
                    EESMCondition* theCondition = theManager->GetCondition(j);
                    if (theCondition->mType != kSMConditionObject || theCondition->mObjectIndex != i)
                        continue;
                    for (unsigned long k = 0; theUnused && k < theManager->mTriggers.size(); k++) {
                        EESMTrigger* theTrigger = theManager->GetTrigger(k);
                        for (unsigned long n = 0; theUnused && n < theTrigger->mConditions.size(); n++) {
                            if (theTrigger->GetCondition(n)->mValue == j)
                                theUnused = false;
                        }
                    }
                }
                for (j = 1; theUnused && j < theManager->mObjects.size(); j++) {
                    EESMObject* theObject = theManager->GetScenarioObject(j);
                    if (theObject->mParent == i || theObject->mNearObject == i)
                        theUnused = false;
                }
                if (theUnused)
                    fprintf(theText, "Object: (%s) is not used in any condition, effect or other object\n", (const char*)theManager->GetScenarioObject(i)->mName);
            }

            for (i = 2; i < theManager->mAreas.size(); i++) {
                bool theUnused = true;
                for (unsigned long j = 1; theUnused && j < theManager->GetNumEffects(); j++) {
                    EESMEffect* theEffect = theManager->GetEffect(j);
                    if (theEffect->mTargetArea == i || theEffect->mPlaceArea == i ||
                        theEffect->mTaskArea == i || theEffect->mLocationArea == i)
                        theUnused = false;
                }
                for (j = 1; theUnused && j < theManager->GetNumObjects(); j++) {
                    if (theManager->GetScenarioObject(j)->mArea == i)
                        theUnused = false;
                }
                if (theUnused)
                    fprintf(theText, "Area: (%s) is not used in any condition, effect or Object\n", (const char*)theManager->GetArea(i)->mName);
            }

            for (i = 0; i < theManager->mTriggers.size(); i++) {
                EESMTrigger* theTrigger = theManager->GetTrigger(i);
                if (!theTrigger->mIsLooping)
                    continue;
                for (unsigned long j = 0; j < theTrigger->mEffects.size(); j++) {
                    EESMEffect* theEffect = theManager->GetEffect(theTrigger->GetEffect(j)->mEffectIndex);
                    // A missing effect is listed as BAD EFFECT above; skip it here instead of dereferencing NULL.
                    if (theEffect == NULL)
                        continue;
                    if ((theEffect->mType == kSMEffectObject && (theEffect->mObjectAction == kSMObjectActionClassName || theEffect->mObjectAction == kSMObjectActionClassAttribute)) ||
                        (theEffect->mType == kSMEffectPlayer && theEffect->mPlayerAction == kSMPlayerActionModifyTechTree))
                        fprintf(theText, "WARNING: Potential performance issue with looping Trigger (%s) calling effect (%s) which repeatedly sets the objects name, class attribute or player tech\n", (const char*)theTrigger->mName, (const char*)theEffect->mName);
                }
            }
            fclose(theText);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x59d582
// Loads the scenario triggers back from ScenarioTriggers.scn in the startup directory (Ctrl+L); always returns OK
long EEUIELoadScenarioTriggersOnly::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFile.SetFile("ScenarioTriggers.scn");
    if (theFile.Open(kFileAccessRead) == kErrorOK)
        gWorld.mScenarioManager->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
    theFile.Close();
    return kErrorOK;
}

// The mini map's size and the screen height: plain (non-folded) .rdata floats in the original (0x444f88, 0x44507c),
// read by the ctor only (provisional names)
static const float kSEMiniMapSize = 169.84f;
static const float kSEScreenHeight = 768.0f;

// FUNCTION: EXE 0x59d615
// Creates the editor form (id 1000, 15 control groups): button texture, edge scroll, mini map, player combo box,
// labels, tab groups, tool bar, hide-UI check box, camera and trigger save/load and debug hot keys; map tab on
EEUIFScenarioEditor::EEUIFScenarioEditor(UIManager* inManager)
    : UIForm(1000, 0xf, inManager, -1, true)
{
    UWideString theText;
    UIHotKey theHotKey;
    GETextureDescription theDescription;
    GEText* theGEText;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    unsigned long theIndex = 0;
    this->SetRectangle(GERectangle(0.0f, 0.0f, 1023.0f, 767.0f));
    this->mButtonTexture = gRasterizer->GetTextureManager(this->GetManager()->GetScreen())->LoadTexture("textures\\SE Buttons",
                                                                                  theDescription);
    this->mButtonTexture->Reference();
    this->mEdgeScroll = new EEAScroll;
    this->mPoll = new TMPoll(500, 0, false);
    this->mPoll->Start();
    this->mIsEntityGroupEnabled = false;
    this->mB2B1 = false;
    this->mCurrentEntityGroup = 0;
    this->mB2B2 = false;
    this->mLoadThread = NULL;
    this->CreateBackground();

    // the mini map (169.84 square, in the bottom right corner)
    this->mMiniMap = new EEUIMiniMap(GERectangle(GE2DPoint(1024.0f - kSEMiniMapSize, kSEScreenHeight - kSEMiniMapSize), kSEMiniMapSize,
                                        kSEMiniMapSize), this,
                            theIndex, 8, 3);
    theIndex++;
    this->AddControl(this->mMiniMap, 0);
    this->mMiniMap->SetDisplayMode(kDisplayModeHidden);

    // the player combo box
    this->mPlayerComboBox = new UIComboBox(0x149, 1, std::vector<UWideString>(), gU531938, GERectangle(GE2DPoint(0.0f, 0.0f), 140.0f,
                           20.0f), 400, this, theIndex, 0, 2);
    this->mPlayerComboBox->SetTextBoxBgOpacity(0.4f);
    theGEText = this->mPlayerComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mPlayerComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mPlayerComboBox->SetBgOpacity(1.0f);
    this->AddControl(this->mPlayerComboBox, 0);
    this->mPlayerComboBox->SetDisplayMode(kDisplayModeHidden);
    theIndex++;
    this->mNewPlayerEvent = new EEUIENewPlayer;
    this->mPlayerComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, this->mNewPlayerEvent);

    // the top text label
    this->mLoadTextLabel = new UITextLabel(UWideString(gU4E5B98), gU531930, 9,
                            GERectangle(GE2DPoint(500.0f, 0.0f), 300.0f, (float)gU531930->GetHeight()), this, 0,
                            theIndex, 0);
    this->mLoadTextLabel->SetAlignment(kTextAlignRight);
    this->mLoadTextLabel->SetBgOpacity(0.4f);
    theGEText = this->mLoadTextLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->AddControl(this->mLoadTextLabel, 0);
    theIndex++;
    this->mLoadTextLabel->SetDisplayMode(kDisplayModeHidden);
    this->mLoadTextLabel->SetAlwaysDirty(true);
    this->mLoadTextLabel->SetFitRectToText(true);

    // the loading text label
    this->mHelpLabel = new UITextLabel(UWideString(gU4E5B98), gU531934, 0x10, GERectangle(GE2DPoint(0.0f, 0.0f), 400.0f, 400.0f),
                            this, 1, theIndex, 0);
    this->mHelpLabel->SetAlignment(kTextAlignLeft);
    this->mHelpLabel->SetBgOpacity(1.0f);
    this->mHelpLabel->SetBgColor(GEColor());
    this->SetUseToolTips(true);
    this->SetHelpTextTimeDelay(600);
    theGEText = this->mHelpLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.0f, 0.0f, 0.0f));
    theGEText->SetIsTransparent(true);
    this->AddControl(this->mHelpLabel, 0);
    theIndex++;
    this->mHelpLabel->SetDisplayMode(kDisplayModeHidden);
    this->mHelpLabel->SetAlwaysDirty(true);
    this->mHelpLabel->SetFitRectToText(true);

    this->CreatePanels(theIndex);
    this->CreateTabButtons(theIndex);
    this->CreateToolbarButtons(theIndex);

    // the hide-UI check box
    this->mHideUICheckBox = new UICheckBox(kScenarioEditorHideUICheckBox, UWideString(gU4E5B98), gU531934,
                           GERectangle(GE2DPoint(849.0f, 718.0f), 20.0f, 20.0f), this, theIndex, 0, false);
    this->mHideUICheckBox->SetStringTableID(IDS_HIDE_UI);
    this->mHideUICheckBox->SetChecked(true);
    EESEEHideUI* theHideUI = new EESEEHideUI;
    this->mHideUICheckBox->SetTriggerEvent(kControlTriggerChecked, theHideUI);
    this->mHideUICheckBox->SetTriggerEvent(kControlTriggerUnchecked, theHideUI);
    theHideUI->Dereference();
    this->AddControl(this->mHideUICheckBox, 0);
    theIndex++;
    this->mHideUICheckBox->SetDisplayMode(kDisplayModeHidden);
    this->GetEntityGroup(0)->Enable();

    // the camera
    this->mWheelZoomEvent = new EEUIEWheelZoom;
    this->SetMouseWheelMoveEvent(this->mWheelZoomEvent);
    this->mZoomInEvent = new EEUIEHotKeyZoom(0);
    theHotKey.SetKey(kKeyRBracket);
    theHotKey.SetPressedEvent(this->mZoomInEvent);
    this->AddHotKey(theHotKey, 0);
    this->mZoomOutEvent = new EEUIEHotKeyZoom(1);
    theHotKey.SetKey(kKeyLBracket);
    theHotKey.SetPressedEvent(this->mZoomOutEvent);
    this->AddHotKey(theHotKey, 0);
    this->mDisableMouseMoveEvent = new EEUIEDisableMouseMove;
    this->mEnableMouseMoveEvent = new EEUIEEnableMouseMove;
    this->mRevolveCameraEvent = new EEUIERevolveCamera;
    this->mRotateCameraEvent = new EEUIERotateCamera;
    theHotKey.SetKey(kKeyGrave);
    theHotKey.SetJustPressedEvent(this->mDisableMouseMoveEvent);
    theHotKey.SetPressedEvent(this->mRotateCameraEvent);
    theHotKey.SetJustUnpressedEvent(this->mEnableMouseMoveEvent);
    this->AddHotKey(theHotKey, 0);
    EEUIEReturnToHomeRotation* theHome = new EEUIEReturnToHomeRotation;
    theHotKey.SetKey(kKeyHome);
    theHotKey.SetJustPressedEvent(theHome);
    theHotKey.SetPressedEvent(NULL);
    theHotKey.SetJustUnpressedEvent(NULL);
    this->AddHotKey(theHotKey, 0);
    theHome->Dereference();

    // the trigger-only save and load, the grab scroll, the debug hot keys
    this->AddHotKey(kKeyS, kKeyControl, kKeyNone, kKeyNone, NULL, NULL, new EEUIESaveScenarioTriggersOnly, NULL, 0, false);
    this->AddHotKey(kKeyL, kKeyControl, kKeyNone, kKeyNone, NULL, NULL, new EEUIELoadScenarioTriggersOnly, NULL, 0, false);
    this->mGrabScroll = new EEUIEGrabScroll;
    this->SetMouseMiddleButtonPressedEvent(this->mGrabScroll);
    this->SetMouseMiddleButtonUnpressedEvent(this->mGrabScroll);
    this->AddHotKey(kKeyTab, kKeyNone, kKeyNone, kKeyNone, new EEUIEMGToggleShowHiddenUnits, NULL, NULL, NULL, 0, false);
    this->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, NULL, NULL, new EEUIEMGDumpLoadedMedia(false, false), NULL, 1, false);
    this->AddHotKey(kKeyD, kKeyControl, kKeyShift, kKeyNone, NULL, NULL, new EEUIEMGDumpLoadedMedia(true, false), NULL, 1, false);
    this->mIsSwitching = false;
    this->mIsSuspended = false;
    this->mIsUIHidden = false;
    this->mNumCargoUnits = 0;
    this->mExitAfterSave = false;
    this->mTabButtons[kEditorTabMap]->Check();
    this->mC1A0.SetPointsAndMax(gWorldTable.GetLongValue(76));
    gResource->GetString(IDS_SE_MAP_NONE, theText);
    this->SetMusicFileName(theText);
}

// FUNCTION: EXE 0x59e17a
// Destroys and frees the mouse wheel zoom event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e1a5
// Destroys and frees the dump-loaded-media hot key event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e1d0
// Destroys and frees the show-hidden-units toggle event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e1fb
// Destroys and frees the zoom hot key event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e226
// Destroys and frees the disable-mouse-move event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e251
// Destroys and frees the enable-mouse-move event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e27c
// Destroys and frees the revolve-camera event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e2a7
// Destroys and frees the rotate-camera event (scalar deleting destructor, emitted with its vtable)

// ---- 0x19e2d2..0x19eba9 (formerly EEUIEGrabScroll.cpp; one TU with the form) ----
// The grab scroll, the form's events' deleting destructors and the form's own members (once kept as
// C:\EEX\Empire Earth\Scenario Editor\EEUIEGrabScroll.cpp; file name inferred: no __FILE__).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/ESChecksum.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVertexArray.h"
#include "lle/GEOrientation.h"
#include "lle/GERectangle.h"
#include "lle/GERhombus.h"
#include "lle/GETransformation.h"
#include "lle/GEVector.h"
#include "lle/ISEntity.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocolConfiguration.h"
#include "lle/TSCriticalSection.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"
#include "lle/UMFastMath.h"
#include "lle/URandom.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "ee/EEUIEGrabScroll.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUnit.h"
#include "ee/EESEUnit.h"
#include "lle/TMPoll.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GELine.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EEAction.h"
#include "lle/GEBitmap.h"
#include "lle/UIButton.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "lle/UITextLabel.h"
#include "lle/UIFullBackground.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEUndo.h"
#include "ee/EEUIFSERedo.h"
#include "ee/EESEEditorTab.h"
#include "ee/EEMouse.h"

extern GERectangle gEditorViewRectangle;     // 0x530900 (the editor's game view rectangle; EESETS* units test the mouse in it)
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (EEGhostUnit.cpp: the ghost at the mouse)

// FUNCTION: EXE 0x59e2d2
// Creates the middle-button grab-scroll event, not grabbing and with no saved mouse-move event
EEUIEGrabScroll::EEUIEGrabScroll()
{
    this->mSavedMouseMoveEvent = NULL;
    this->mIsGrabbing = false;
}

// SYNTHETIC: EXE 0x59e2ed
// (the scalar deleting destructor, emitted with the vtable; slot EEUIEGrabScroll+0x0)

// FUNCTION: EXE 0x59e309
// Destroys the grab-scroll event, releasing the mouse-move event it saved
EEUIEGrabScroll::~EEUIEGrabScroll()
{
    if (this->mSavedMouseMoveEvent != NULL) {
        this->mSavedMouseMoveEvent->Dereference();
    }
}

// FUNCTION: EXE 0x59e32f
// Destroys and frees the new-player event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e35a
// Destroys and frees the home-rotation event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e385
// Destroys and frees the trigger-only save event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59e3b0
// Destroys and frees the trigger-only load event (scalar deleting destructor, emitted with its vtable)

// SYNTHETIC: EXE 0x59e3db
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFScenarioEditor+0x0)

// FUNCTION: EXE 0x59e3f7
// Destroys the editor form, releasing its events, textures and edge scroll and deleting its tabs and controls
EEUIFScenarioEditor::~EEUIFScenarioEditor()
{
    this->mWheelZoomEvent->Dereference();
    this->mRevolveCameraEvent->Dereference();
    this->mRotateCameraEvent->Dereference();
    this->mEnableMouseMoveEvent->Dereference();
    this->mDisableMouseMoveEvent->Dereference();
    this->mEdgeScroll->Dereference();
    this->mNewPlayerEvent->Dereference();
    this->mPanelBackground->Dereference();
    for (long i = 0; i < 7; i++) {
        delete this->mTabButtons[i];
        delete this->mTabGroups[i];
    }
    this->mButtonTexture->Dereference();
    delete[] this->mTabButtons;
    delete this->mMiniMap;
    delete this->mPoll;
    delete this->mUndoButton;
    delete this->mRedoButton;
    delete this->mBrushGroup;
    delete this->mFullBackground;
    delete this->mLoadTextLabel;
    delete this->mLoadButton;
    delete this->mTestButton;
    delete this->mSaveButton;
    delete this->mExitButton;
    delete this->mQuickSaveButton;
    delete this->mQuickLoadButton;
    delete this->mCivBuilderButton;
    delete this->mHideUICheckBox;
    delete this->mPlayerComboBox;
    delete this->mHelpLabel;
    this->mZoomInEvent->Dereference();
    this->mZoomOutEvent->Dereference();
    this->mGrabScroll->Dereference();
}

// FUNCTION: EXE 0x59e606
// Adds a unit to the editor selection, showing the cargo panel for a placed carrier and the status of a lone unit
void EEUIFScenarioEditor::SelectUnit(EEUnit* inUnit)
{
    inUnit->Reference();
    this->mSelectedUnits.push_back(inUnit);
    inUnit->mIsEditorSelected = true;
    if (inUnit->mTileNode != NULL &&
        (inUnit->mComplexUnit->mCargoRoom > 0 || inUnit->mComplexUnit->GetNumCargoUnits() > 0)) {
        this->mNumCargoUnits++;
        this->GetUnitScreen()->ShowCargo(true);
    }
    if (this->mSelectedUnits.size() == 1)
        this->GetUnitScreen()->ShowUnitStatus(inUnit);
}

#include "lle/UIFullBackground.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEGamePreferences.h"
#include "GEWindow.h"
extern RTSRenderObject* gU56534C;

// FUNCTION: EXE 0x59e68a
// Closes the editor form, restoring the game camera mode, the game render list and the full-window mouse bounds
void EEUIFScenarioEditor::Close()
{
    gFreeCamera = false;
    ::gWorld.mCameraControl.SetMode(gGamePreferences.mSettings.mZoomStyle);
    ::gWorld.mCameraControl.SetChanged();
    this->mFullBackground->UnloadBackground();
    gGraphics->PostClearRenderObjects();
    gGraphics->PostPushRenderObject(gU56534C);
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    gB529065 = false;
    UIForm::Close();
}


// FUNCTION: EXE 0x59e72c
// Prepares the frame: dirties the game view (unless full view) and mini map, moves the placement ghost,
// updates the current tab group and the edge scroll (unless full view or a control holds the mouse)
long EEUIFScenarioEditor::CopyData()
{
    GERectangle theRectangle = gEditorViewRectangle;
    if (!sEditorTabs[this->mCurrentTab].mFullView) {
        theRectangle.SetRight(theRectangle.GetRight() - 1.0f);
        theRectangle.SetBottom(theRectangle.GetBottom() - 1.0f);
        this->GetManager()->AddDirtyRectangle(theRectangle, this);
    }
    this->mMiniMap->SetDirty(true);
    this->mWasUIHidden = this->mIsUIHidden;
    EEPreviewPlacement(gMouse->mPosition);
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Update();
    if (!sEditorTabs[this->mCurrentTab].mFullView
        && (!this->GetIsMouseCaptured() || sEditorTabs[this->mCurrentTab].mGroup == 1 || sEditorTabs[this->mCurrentTab].mGroup == 2))
        this->mEdgeScroll->Update(gMouse->mPosition);
    return UIForm::CopyData();
}

#include "ee/EESEEditorTab.h"
#include "ee/EEAction.h"
#include "lle/GEBitmap.h"
#include "lle/UIButton.h"
#include "lle/UResourceFile.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEUndo.h"
#include "ee/EEUIFSERedo.h"
#include "lle/GEText.h"
#include "lle/GEViewport.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/UIFullBackground.h"
#include "ee/EmpireEarth.h"
extern GEFont* gU531930;

// FUNCTION: EXE 0x59e80f
// Loads the bottom panel texture (scenpart) and the full-screen tab background (Scenfull)
// NEAR: EE_LOST_DEBUG(5) at the start makes this bind_only, but LostDebug.h would have to be included before the
// unit's later headers (that reorders its .rdata$r RTTI COMDATs), so it is not applied. The texture manager goes
// through a local: the original loads it before the name temporary.
void EEUIFScenarioEditor::CreateBackground()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = false;
    theDescription.mCreateMIPMaps = true;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    GEScreen* theScreen = NULL;
    for (unsigned long i = 0; i < theRasterizer->GetScreenCount(); i++) {
        if (theRasterizer->GetIsScreenActivated(i)) {
            theScreen = theRasterizer->GetScreen(i);
            break;
        }
    }
    GETextureManager* theManager = theScreen->GetTextureManager();
    GETexture* theTexture = theManager->LoadTexture("textures\\scenpart", theDescription);
    theTexture->SetFrame(0);
    theTexture->SetStageAlphaMode(kStageOpTexture, -1);
    theTexture->SetStageColorMode(kStageOpTexture, -1);
    theTexture->SetStageMinFilterMode(0, -1);
    theTexture->SetStageMagFilterMode(0, -1);
    theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    theTexture->SetStageMipMapBias(-1.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    this->mPanelBackground = new GEBitmap;
    this->mPanelBackground->SetTexture(theTexture, gScreenIndex);
    this->mR1F0 = GERectangle(0.0f, 606.0f, 1023.0f, 767.0f);
    this->mFullBackground = new UIFullBackground("textures\\Scenfull", 4, 3, this, 1024, 768);
}

// SYNTHETIC: EXE 0x59e9de
// (the scalar deleting destructor, emitted with the vtable; slot UIFullBackground+0x0)

// FUNCTION: EXE 0x59e9fb
// Creates the brush group, the undo and redo buttons and the seven tab groups
void EEUIFScenarioEditor::CreatePanels(unsigned long& ioIndex)
{
    UWideString theText;
    GEText* theGEText;
    GE2DPoint theLocation(5.0f, 616.0f);
    this->mBrushGroup = new EEUIFSEBrushGroup(ioIndex, theLocation, this, gU531930);
    theLocation.SetXY(5.0f, 718.0f);
    this->mUndoButton = new EEUIFSEUndo(0, 0, 0, 0, theLocation, 65, 25, this, ioIndex);
    ioIndex++;
    gResource->GetString(IDS_UNDO, theText);
    this->mUndoButton->SetText(theText, gU531930);
    theGEText = this->mUndoButton->GetGEText();
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theGEText->SetIsTransparent(true);
    theLocation.SetXY(75.0f, 718.0f);
    this->mRedoButton = new EEUIFSERedo(1, 0, 0, 0, theLocation, 65, 25, this, ioIndex);
    ioIndex++;
    gResource->GetString(IDS_REDO, theText);
    this->mRedoButton->SetText(theText, gU531930);
    theGEText = this->mRedoButton->GetGEText();
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theGEText->SetIsTransparent(true);
    this->mTabGroups.resize(7, static_cast<EEUIFSEMainGroup*>(0));
    for (unsigned long i = 0; i < 7; i++) {
        long theGroup = sEditorTabs[i].mGroup;
        this->mTabGroups[theGroup] = this->CreateTabGroup(theGroup, ioIndex);
    }
}

// ---- 0x19eba9..0x1a0315 (formerly the head of EEUIEOpenCivBuilder.cpp) ----
// The tab and tool bar buttons, the form's remaining members and the quick load/save events (once kept as
// C:\EEX\Empire Earth\Scenario Editor\EEUIEOpenCivBuilder.cpp, EESEEQuickLoad.cpp and EESEEQuickSave.cpp;
// file names inferred: no __FILE__).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/ESChecksum.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVertexArray.h"
#include "lle/GEOrientation.h"
#include "lle/GERectangle.h"
#include "lle/GERhombus.h"
#include "lle/GETransformation.h"
#include "lle/GEVector.h"
#include "lle/ISEntity.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocolConfiguration.h"
#include "lle/TSCriticalSection.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"
#include "lle/UMFastMath.h"
#include "lle/URandom.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GEFont.h"
#include "lle/GEBitmap.h"
#include "lle/GEText.h"
#include "lle/UIButton.h"
#include "lle/UIRadioButton.h"
#include "ee/EmpireEarth.h"
#include "lle/UResourceFile.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSERBBrush.h"
#include "ee/EESEEQuickLoad.h"
#include "ee/EESEEQuickSave.h"
#include <algorithm>
#include "lle/UITextLabel.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUnit.h"
#include "ee/EESEUnit.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEAction.h"
#include "ee/EEUserInterface.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

#include "lle/UIEntityGroup.h"
#include "lle/UISlider.h"
#include "ee/EEUIFSEElevation.h"
#include "lle/GELine.h"
#include "ee/EEUIMiniMap.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"

// The render-list API and object (defined in EERenderObject.cpp)
void EEPostGraphicsAll();
extern RTSRenderObject* gU56534C;

// EEUIEOpenCivBuilder (RTTI 0x445100): the original news 0xc bytes and its ctor initializes only the UIEvent base

// The seven editor tabs, original VA 0x8c46d0, stride 0x20 (provisional names).
// 0x19eba9 reads order/help/normal+pressed cells/flag while constructing the buttons;
// 0x19ef5a dispatches group numbers; 0x19f615 reads full-view and brush flags.
// Fundamental fields keep the original static payload (no point-constructor init chain).
#include "ee/EESEEditorTab.h"
EESEEditorTab sEditorTabs[7] = {
    {0, 0, 0x213a,   0.0f, 50.0f,  32.0f, 50.0f,  1, false, false},
    {1, 1, 0x213b, 128.0f, 50.0f, 160.0f, 50.0f,  2, false, true},
    {2, 2, 0x213c,  64.0f, 50.0f,  96.0f, 50.0f,  3, false, true},
    {3, 3, 0x213d,   0.0f, 82.0f,  32.0f, 82.0f,  6, false, false},
    {4, 4, 0x213e, 192.0f, 50.0f, 224.0f, 50.0f, 14, false, false},
    {5, 5, 0x1b61,  64.0f, 82.0f,  96.0f, 82.0f,  4, true,  false},
    {6, 6, 0x2143, 128.0f, 82.0f, 160.0f, 82.0f,  7, true,  false}
};

// The tool bar buttons (0x4c47b0, right after sEditorTabs; CreateToolbarButtons walks the six of them): the control id, the
// 32x32 destination, the normal and pressed source corners in mButtonTexture, the help string (provisional names)
struct EESEToolbarButton {
    unsigned long mID;
    unsigned long mX;
    unsigned long mY;
    unsigned long mNormalX;
    unsigned long mNormalY;
    unsigned long mPressedX;
    unsigned long mPressedY;
    unsigned long mHelp;
};
EESEToolbarButton sToolbarButtons[6] = {
    {0xc, 0x287, 0x2e0, 0x80, 0x72, 0xa0, 0x72, 0x2140},
    {0xd, 0x2aa, 0x2e0, 0x00, 0xc4, 0x20, 0xc4, 0x2141},
    {0xb, 0x2cd, 0x2e0, 0x40, 0x72, 0x60, 0x72, 0x1b59},
    {0xa, 0x2f0, 0x2e0, 0x00, 0x72, 0x20, 0x72, 0x1b58},
    {0xe, 0x313, 0x2e0, 0x40, 0xc4, 0x60, 0xc4, 0x2142},
    {0xf, 0x336, 0x2e0, 0x80, 0xc4, 0xa0, 0xc4, 0x213f}
};

#include "ee/EESEEditorEvents.h"

#include "lle/UIFullBackground.h"

#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "ee/EERenderObject.h"
#include "ee/EEROTerrain.h"

#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "ee/EESEPlayerScreen.h"
extern RTSRenderObject* gU56535C; // the terrain render object (an EEROTerrain newed in Empire Earth.cpp)

#include "lle/GELine.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEMiniMap.h"
#include "lle/TMPoll.h"
#include "ee/EEAScroll.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMusicManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESEAExitScenario.h"
#include "lle/GEText.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EEUIFSETerrain.h"
#include "lle/UICheckBox.h"
#include "ee/EESETSBaseObject.h"
extern GEFont* gU531930;                    // 0x531930 (Empire Earth.cpp's fonts)
extern GEFont* gU531938;                    // 0x531938
#include "ee/EESETriggerScreen.h"
#include "ee/EESEInstructions.h"
#include "ee/EESEAQuickSaveLoad.h"
#include "LostDebug.h"
extern unsigned long gTexturePixelDepth; // 0x5308b4 (Empire Earth.cpp)
extern bool gSightUpdatesEnabled;
extern GEViewport gViewport;

// FUNCTION: EXE 0x59eba9
// Creates the seven tab buttons from sEditorTabs along the bottom and the civilization builder button after them
void EEUIFScenarioEditor::CreateTabButtons(unsigned long& ioIndex)
{
    GERectangle theDestination, theSource;
    float theX;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    for (unsigned long i = 0; i < theRasterizer->GetScreenCount(); i++) {
        if (theRasterizer->GetIsScreenActivated(i)) {
            theRasterizer->GetScreen(i);
            break;
        }
    }
    this->mTabButtons = new EEUIFSEMainButton*[7];
    for (i = 0; i < 7; i++) {
        const EESEEditorTab& theTab = sEditorTabs[i];
        this->mTabButtons[i] = new EEUIFSEMainButton(220 + i, theTab.mFlag, this->mButtonTexture,
            GE2DPoint(theTab.mNormalX, theTab.mNormalY), GE2DPoint(theTab.mPressedX, theTab.mPressedY),
            GE2DPoint((float)theTab.mOrder * 44.0f, 735.0f), this, i, ioIndex, i == 0);
        this->mTabButtons[i]->SetStringTableID(theTab.mHelp);
        ioIndex++;
        this->AddControl(this->mTabButtons[i], 0);
    }
    this->mCurrentEntityGroup = sEditorTabs[0].mFlag;
    this->mCurrentTab = this->mTabButtons[kEditorTabMap]->mTab;
    GEBitmap* theNormal = new GEBitmap;
    GEBitmap* thePressed = new GEBitmap;
    theNormal->SetTexture(this->mButtonTexture, this->GetManager()->GetScreen());
    thePressed->SetTexture(this->mButtonTexture, this->GetManager()->GetScreen());
    theSource = GERectangle(192.0f, 116.0f, 224.0f, 148.0f);
    theX = this->mTabButtons[6]->GetRectangle().GetRight() + 12.0f;
    theDestination.SetTop(735.0f);
    theDestination.SetLeft(theX);
    theDestination.SetBottom(735.0f + 32.0f - 1.0f);
    theDestination.SetRight(theX + 32.0f - 1.0f);
    theNormal->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetManager()->GetScreen());
    theSource = GERectangle(224.0f, 116.0f, 256.0f, 148.0f);
    thePressed->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, this->GetManager()->GetScreen());
    this->mCivBuilderButton = new UIButton(17, theNormal, NULL, thePressed, theDestination, this, ioIndex, 0);
    theNormal->Dereference();
    thePressed->Dereference();
    this->mCivBuilderButton->SetStringTableID(IDS_CIVILIZATION_BUILDER_TOOL);
    UIEvent* theEvent = new EEUIEOpenCivBuilder;
    this->mCivBuilderButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    this->AddControl(this->mCivBuilderButton, 0);
    ioIndex++;
}

// FUNCTION: EXE 0x59ef15
// Opens the civilization builder on the scenario's custom civilization
long EEUIEOpenCivBuilder::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return gUserInterface->OpenCivilizationBuilder(&static_cast<EEUIFScenarioEditor*>(inForm)->mC1A0, true);
}

// FUNCTION: EXE 0x59ef2f
// Destroys and frees the open-civilization-builder event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59ef5a
// Creates the panel group of a tab (map, elevation, terrain, units, triggers, players, instructions); NULL otherwise
EEUIFSEMainGroup* EEUIFScenarioEditor::CreateTabGroup(long inGroup, unsigned long& ioIndex)
{
    switch (inGroup) {
    case kEditorTabMap:
        return new EEUIFSEMap(ioIndex, this, gU531930);
    case kEditorTabElevation:
        return new EEUIFSEElevation(this->mEdgeScroll, this->mBrushGroup, ioIndex, this, gU531930);
    case kEditorTabTerrain:
        return new EEUIFSETerrain(this->mEdgeScroll, this->mBrushGroup, ioIndex, this, gU531930);
    case kEditorTabUnits:
        return new EESEUnit(ioIndex, this, gU531938, this->mPlayerComboBox);
    case kEditorTabTriggers:
        return new EESETriggerScreen(ioIndex, this, gU531938);
    case kEditorTabPlayers:
        return new EESEPlayerScreen(ioIndex, this, gU531938, this->mPlayerComboBox);
    case kEditorTabInstructions:
        return new EESEInstructions(ioIndex, this, gU531938);
    }
    return NULL;
}

// FUNCTION: EXE 0x59f0b1
// Creates the six tool bar buttons from sToolbarButtons (quick load, quick save, save, load, test, exit) and their
// events; the two quick buttons share one quick save/load action
void EEUIFScenarioEditor::CreateToolbarButtons(unsigned long& ioIndex)
{
    GERectangle theDestination;
    GERectangle theSource;
    unsigned long theScreen = this->GetManager()->GetScreen();
    EESEAQuickSaveLoad* theQuickSaveLoad = new EESEAQuickSaveLoad(gScenarioExtension);
    for (long i = 0; i < 6; i++) {
        const EESEToolbarButton& theButton = sToolbarButtons[i];
        GEBitmap* theNormal = new GEBitmap;
        GEBitmap* thePressed = new GEBitmap;
        theNormal->SetTexture(this->mButtonTexture, theScreen);
        thePressed->SetTexture(this->mButtonTexture, theScreen);
        theDestination.SetLeft((float)theButton.mX);
        theDestination.SetRight((float)(theButton.mX + 31));
        theDestination.SetTop((float)theButton.mY);
        theDestination.SetBottom((float)(theButton.mY + 31));
        theSource.SetLeft((float)theButton.mNormalX);
        theSource.SetRight((float)(theButton.mNormalX + 32));
        theSource.SetTop((float)theButton.mNormalY);
        theSource.SetBottom((float)(theButton.mNormalY + 32));
        theNormal->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, theScreen);
        theSource.SetLeft((float)theButton.mPressedX);
        theSource.SetRight((float)(theButton.mPressedX + 32));
        theSource.SetTop((float)theButton.mPressedY);
        theSource.SetBottom((float)(theButton.mPressedY + 32));
        thePressed->ComputeStretchedCoordsAndScale(theDestination, theDestination, theSource, theScreen);
        UIButton* theControl = new UIButton(theButton.mID, theNormal, NULL, thePressed,
                                            GERectangle(GE2DPoint(theDestination.GetLeft(), theDestination.GetTop()),
                                                        32.0f, 32.0f),
                                            this, ioIndex, 0);
        theControl->SetStringTableID(theButton.mHelp);
        ioIndex++;
        this->AddControl(theControl, 0);
        UIEvent* theEvent;
        switch (theButton.mID) {
        case kToolbarButtonLoad:
            theEvent = new EESEELoadScenario;
            this->mLoadButton = theControl;
            break;
        case kToolbarButtonSave:
            theEvent = new EESEESaveScenario;
            this->mSaveButton = theControl;
            break;
        case kToolbarButtonQuickLoad:
            this->mQuickLoadButton = theControl;
            theEvent = new EESEEQuickLoad(theQuickSaveLoad);
            break;
        case kToolbarButtonQuickSave:
            theEvent = new EESEEQuickSave(theQuickSaveLoad);
            this->mQuickSaveButton = theControl;
            break;
        case kToolbarButtonTest:
            theEvent = new EESEETestScenario;
            this->mTestButton = theControl;
            break;
        case kToolbarButtonExit:
            theEvent = new EESEEExitScenario;
            this->mExitButton = theControl;
            break;
        default:
            theEvent = NULL;
            break;
        }
        theControl->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        theNormal->Dereference();
        thePressed->Dereference();
    }
    theQuickSaveLoad->Dereference();
}

// FUNCTION: EXE 0x59f4df
// Destroys and frees the quick save/load action (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x59f501
// Draws the full-view background or the bottom panel, then the current tab group; returns the full view's result
long EEUIFScenarioEditor::DrawBackground(const GERectangle& inRectangle)
{
    GERectangle theClip;
    long theResult = 0;
    if (sEditorTabs[this->mCurrentTab].mFullView) {
        theResult = this->mFullBackground->DrawBackground(inRectangle);
    } else if (inRectangle.And(this->mR1F0, theClip)) {
        GERectangle theSource(0.0f, 0.0f, 256.0f, 256.0f);
        UIForm::DrawBackground(this->mPanelBackground, theClip, theSource, this->mR1F0);
    }
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Draw(inRectangle);
    return theResult;
}

// FUNCTION: EXE 0x59f5b3
// Draws the form's polygons (no editor additions)
long EEUIFScenarioEditor::DrawPolygons()
{
    return UIForm::DrawPolygons();
}

// FUNCTION: EXE 0x59f5b9
// Leaves the current tab: hides the brush highlight, leaves its group (unless switching), disables its controls
void EEUIFScenarioEditor::LeaveTab()
{
    this->mBrushGroup->mIsHighlightShown = false;
    if (!this->mIsSwitching)
        this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Leave();
    if (this->mIsEntityGroupEnabled) {
        this->mIsEntityGroupEnabled = false;
        this->GetEntityGroup(this->mCurrentEntityGroup)->Disable();
    }
}

// FUNCTION: EXE 0x59f615
// Switches to a tab: enters or leaves full view as needed, enables its controls, enters and loads its group
void EEUIFScenarioEditor::SelectTab(unsigned char inFlag, long inTab)
{
    if (sEditorTabs[this->mCurrentTab].mFullView && !sEditorTabs[inTab].mFullView)
        this->LeaveFullView();
    else if (!sEditorTabs[this->mCurrentTab].mFullView && sEditorTabs[inTab].mFullView)
        this->EnterFullView();
    this->LeaveTab();
    this->mCurrentTab = inTab;
    this->GetEntityGroup(inFlag)->Enable();
    this->mIsEntityGroupEnabled = true;
    this->mCurrentEntityGroup = inFlag;
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Enter();
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Load();
    if (sEditorTabs[this->mCurrentTab].mBrush)
        this->mBrushGroup->mIsHighlightShown = true;
}

// FUNCTION: EXE 0x59f6e9
// Leaves the editor through the load thread, to test the scenario or to quit, back on the map tab; suspends the form
void EEUIFScenarioEditor::ExitEditor(bool inTestScenario)
{
    this->mTabButtons[kEditorTabMap]->CheckDirect();
    this->mIsSwitching = true;
    if (inTestScenario) {
        gWorld.RebuildActiveUnits();
        if (gUIManager->mInScenarioEditor == true)
            this->mLoadThread->Post404();
        else
            this->mLoadThread->Post405();
    } else
        this->mLoadThread->Post401();
    this->mLoadThread = NULL;
    this->mIsSwitching = false;
    this->mIsSuspended = true;
}

// FUNCTION: EXE 0x59f757
// Resumes the editor after a load and reloads the current tab group
void EEUIFScenarioEditor::ResumeCurrentGroup()
{
    this->mIsSuspended = false;
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Load();
}

// FUNCTION: EXE 0x59f77b
// Suspends the editor and passes an event to the load thread, which posts it to the user interface
void EEUIFScenarioEditor::PostLoadEvent(UReferenceCounter* inObject)
{
    this->mIsSuspended = true;
    this->mLoadThread->Post40B(inObject);
}

// FUNCTION: EXE 0x59f794
// Opens the editor: editor camera, backgrounds, paused music and ambient sound, editor game view and mini map,
// enters the current tab; posts the exit action if a save asked to exit afterwards
void EEUIFScenarioEditor::Open()
{
    GETransformation theTransformation;
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    this->mIsSwitching = true;
    this->mIsSuspended = false;
    gWorld.mCameraControl.SetMode(2);
    gWorld.RefreshUnits();
    gSightUpdatesEnabled = false;
    theTransformation.SetOrientationYPR(0.7853981852531433f, -0.6108652353286743f, 0.0f);
    gWorld.mCameraControl.SetOffsetTurn(theTransformation, false);
    gWorld.mCameraControl.SetChanged();
    if (this->mLoadThread == NULL) this->mLoadThread = new EESELoadThread(this, 0);
    this->mFullBackground->LoadBackground(theDescription);
    if (sEditorTabs[this->mCurrentTab].mFullView) this->EnterFullView();
    else this->LeaveFullView();
    gGraphics->PostFadeIn(0);
    this->GetPlayerScreen()->Refresh();
    gMusicManager->Pause();
    gAmbientSoundManager.StopSounds();
    gAmbientSoundManager.Pause();
    this->mEdgeScroll->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f,
                                (float)gGameWindow->GetHeight() - 1.0f);
    gFreeCamera = true;
    gB529065 = true;
    this->mR1E0 = gViewport.GetViewRectangle();
    EESetGameViewRectangle(GERectangle(0.0f, 0.0f, 1024.0f, 606.0f));
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL) gMiniMap->RefreshView();
    EEUIMiniMap::Update(gMiniMap);
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Enter();
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Load();
    gWorld.mScenarioManager->mIsMovingCamera = false;
    gMiniMap->Redraw();
    UIForm::Open();
    this->mHelpLabel->SetDisplayMode(kDisplayModeHidden);
    this->SetHelpTextLabel(this->mHelpLabel);
    this->mIsSwitching = false;
    if (this->mExitAfterSave != true)
        return;
    EESEAExitScenario* theAction = new EESEAExitScenario;
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    this->mExitAfterSave = false;
}

// FUNCTION: EXE 0x59fa3b
// Suspends the editor and passes a loaded object to the load thread to show
void EEUIFScenarioEditor::PostShowLoadedObject(UReferenceCounter* inObject)
{
    this->mIsSuspended = true;
    this->mLoadThread->Post40C(inObject);
}

// FUNCTION: EXE 0x59fa54
// Deselects every selected unit and hides the cargo panel and the unit status
void EEUIFScenarioEditor::ClearSelection()
{
    while (this->mSelectedUnits.size() != 0) {
        EEUnit* theUnit = this->mSelectedUnits.back();
        theUnit->mIsEditorSelected = false;
        theUnit->Dereference();
        this->mSelectedUnits.pop_back();
    }
    this->mNumCargoUnits = 0;
    this->GetUnitScreen()->ShowCargo(false);
    this->GetUnitScreen()->ShowUnitStatus(NULL);
}

// FUNCTION: EXE 0x59fab9
// Removes the first unit from the editor selection, updating the cargo panel and the unit status
void EEUIFScenarioEditor::DeselectFirstUnit()
{
    if (this->mSelectedUnits.size() == 0)
        return;
    EEUnit* theUnit = this->mSelectedUnits.front();
    theUnit->mIsEditorSelected = false;
    this->mSelectedUnits.erase(this->mSelectedUnits.begin());
    if (theUnit->mTileNode != NULL &&
        (theUnit->mComplexUnit->mCargoRoom > 0 || theUnit->mComplexUnit->GetNumCargoUnits() > 0)) {
        this->mNumCargoUnits--;
        if (this->mNumCargoUnits == 0)
            this->GetUnitScreen()->ShowCargo(false);
    }
    theUnit->Dereference();
    this->GetUnitScreen()->ShowUnitStatus(this->GetFirstSelectedUnit());
}


// FUNCTION: EXE 0x59fb84
// Removes a unit from the editor selection, updating the cargo panel, and the unit status if it was first
void EEUIFScenarioEditor::DeselectUnit(EEUnit* inUnit)
{
    bool theFirst = false;
    std::vector<EEUnit*>::iterator theFound;
    theFound = std::find(this->mSelectedUnits.begin(), this->mSelectedUnits.end(), inUnit);
    if (theFound == this->mSelectedUnits.end())
        return;
    EEUnit* theUnit = *theFound;
    theUnit->mIsEditorSelected = false;
    if (theUnit == this->mSelectedUnits[0])
        theFirst = true;
    if (theUnit->GetTileNode() != NULL &&
        (theUnit->GetComplexUnit()->mCargoRoom > 0 || theUnit->GetComplexUnit()->GetNumCargoUnits() > 0)) {
        this->mNumCargoUnits--;
        if (this->mNumCargoUnits == 0)
            this->GetUnitScreen()->ShowCargo(false);
    }
    this->mSelectedUnits.erase(theFound);
    theUnit->Dereference();
    if (theFirst == true)
        this->GetUnitScreen()->ShowUnitStatus(this->GetFirstSelectedUnit());
}


// FUNCTION: EXE 0x59fc97
// Makes the current tab group store its settings
void EEUIFScenarioEditor::RefreshCurrentGroup()
{
    this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Store();
}

// FUNCTION: EXE 0x59fcb4
// Sets the editor's game view rectangle and disables the current tab group while the UI is hidden
void EEUIFScenarioEditor::SetGameView(const GERectangle& inRectangle, bool inIsUIHidden)
{
    EESetGameViewRectangle(inRectangle);
    this->mIsUIHidden = inIsUIHidden;
    if (inIsUIHidden == true)
        this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Disable();
    else
        this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->Enable();
}

// FUNCTION: EXE 0x59fd09
// Shows the given loading text in the top label, or hides the label for an empty text
void EEUIFScenarioEditor::SetLoadText(const UWideString& inText)
{
    if (inText.GetLength() > 0) {
        this->mLoadTextLabel->SetDisplayMode(kDisplayModeShown);
        this->mLoadTextLabel->SetText(inText);
    } else
        this->mLoadTextLabel->SetDisplayMode(kDisplayModeHidden);
}


// FUNCTION: EXE 0x59fd43
// Enters full view: resets the game render list to its base object, hides the mini map and redraws all controls
void EEUIFScenarioEditor::EnterFullView()
{
    gGraphics->PostClearRenderObjects();
    gGraphics->PostPushRenderObject(gU56534C);
    this->mMiniMap->SetDisplayMode(kDisplayModeHidden);
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
}

// FUNCTION: EXE 0x59fd91
// Leaves full view: restores the full game render list, shows the mini map and redraws all controls
void EEUIFScenarioEditor::LeaveFullView()
{
    EEPostGraphicsAll();
    this->mMiniMap->SetDisplayMode(kDisplayModeShown);
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
}

// FUNCTION: EXE 0x59fdc8
// Handles the editor keys (; quick save, / quick load, T / Shift+T grid mode, 1-8 and Alt+1-8 pick a player),
// else the tab group's and the form's keys; nothing while suspended
long EEUIFScenarioEditor::UpdateKeyboard()
{
    FSFileSpec theUnusedFile;
    UString theUnusedString;
    bool theHandled = false;
    if (this->mIsSuspended)
        return 0;
    if (!this->GetIsKeyboardCaptured()) {
        ISEntity* theKey;
        gISKeyboard->GetEntity(kKeySemicolon, theKey);
        if (theKey->JustPressed()) {
            UIControl* theControl = this->GetControlById(13);
            if (theControl != NULL) theControl->FireTriggerEvent(kControlTriggerButtonUp);
            theHandled = true;
        }
        gISKeyboard->GetEntity(kKeySlash, theKey);
        if (theKey->JustPressed()) {
            UIControl* theControl = this->GetControlById(12);
            if (theControl != NULL) theControl->FireTriggerEvent(kControlTriggerButtonUp);
            theHandled = true;
        }
        gISKeyboard->GetEntity(kKeyT, theKey);
        if (theKey->JustPressed()) {
            gISKeyboard->GetEntity(kKeyShift, theKey);
            if (theKey->GetValue() != 0)
                static_cast<EEROTerrain*>(gU56535C)->StepGridMode(true);
            else
                static_cast<EEROTerrain*>(gU56535C)->StepGridMode(false);
            theHandled = true;
        }
        if (this->mPlayerComboBox->GetDisplayMode() == kDisplayModeShown) {
            EESEPlayerScreen* thePlayers = static_cast<EESEPlayerScreen*>(this->mTabGroups[kEditorTabPlayers]);
            ISEntity* theModifier;
            unsigned long thePlayer = 0;
            gISKeyboard->GetEntity(kKeyAlt, theModifier);
            gISKeyboard->GetEntity(kKey1, theKey);
            if (theKey->JustPressed()) thePlayer = 1;
            gISKeyboard->GetEntity(kKey2, theKey);
            if (theKey->JustPressed()) thePlayer = 2;
            gISKeyboard->GetEntity(kKey3, theKey);
            if (theKey->JustPressed()) thePlayer = 3;
            gISKeyboard->GetEntity(kKey4, theKey);
            if (theKey->JustPressed()) thePlayer = 4;
            gISKeyboard->GetEntity(kKey5, theKey);
            if (theKey->JustPressed()) thePlayer = 5;
            gISKeyboard->GetEntity(kKey6, theKey);
            if (theKey->JustPressed()) thePlayer = 6;
            gISKeyboard->GetEntity(kKey7, theKey);
            if (theKey->JustPressed()) thePlayer = 7;
            gISKeyboard->GetEntity(kKey8, theKey);
            if (theKey->JustPressed()) thePlayer = 8;
            if (thePlayer > 0) {
                if (theModifier->GetValue() != 0) thePlayer += 8;
                if (thePlayer <= thePlayers->GetPlayerCount())
                    this->mPlayerComboBox->SetCurrentSelection(thePlayer - 1);
                theHandled = true;
            }
        }
    }
    if (!theHandled) {
        if (!this->GetIsKeyboardCaptured())
            this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->UpdateKeys();
        return UIForm::UpdateKeyboard();
    }
    return 0;
}

// FUNCTION: EXE 0x5a0083
// Passes the mouse to the current tab group unless a control captured it; does nothing while suspended
long EEUIFScenarioEditor::UpdateMouse()
{
    long theResult = 0;
    if (!this->mIsSuspended) {
        if (!this->GetIsMouseCaptured())
            this->mTabGroups[sEditorTabs[this->mCurrentTab].mGroup]->MouseMove(this->GetMouseLocation());
        theResult = UIForm::UpdateMouse();
    }
    return theResult;
}

// FUNCTION: EXE 0x5a00c6
// Creates the quick save/load action (EEAQuickSaveLoad's inline ctor, out of line after its first user
// CreateToolbarButtons)

// FUNCTION: EXE 0x5a0102
// Destroys the quick save/load action (EEAQuickSaveLoad's inline dtor, out of line: EESEAQuickSaveLoad's ??_G
// 0x19f4df and its own ??_G call it)

// FUNCTION: EXE 0x5a013e
// Destroys and frees the base quick save/load action (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5a015a
// Creates the hide-UI check box event (EESEEHideUI ctor)

// FUNCTION: EXE 0x5a0196
// Destroys and frees the hide-UI check box event (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5a01c1
// Creates the quick load button event, referencing the shared quick save/load action
EESEEQuickLoad::EESEEQuickLoad(UReferenceCounter* inOwner) : mOwner(inOwner)
{
    inOwner->Reference();
}

// SYNTHETIC: EXE 0x5a01e1
// (the scalar deleting destructor, emitted with the vtable; slot EESEEQuickLoad+0x0, stores EESEEQuickLoad vtable)
// the dtor is inline (expanded into ??_G: Dereference the owner, no out-of-line ??1 in the exe)
inline EESEEQuickLoad::~EESEEQuickLoad()
{
    this->mOwner->Dereference();
}

// FUNCTION: EXE 0x5a0215
// Creates the quick save button event, referencing the shared quick save/load action
EESEEQuickSave::EESEEQuickSave(EEAQuickSaveLoad* inOwner) : mOwner(inOwner)
{
    inOwner->Reference();
}

// SYNTHETIC: EXE 0x5a0235
// (the scalar deleting destructor, emitted with the vtable; slot EESEEQuickSave+0x0, stores EESEEQuickSave vtable)
// likewise inline (expanded into ??_G)
inline EESEEQuickSave::~EESEEQuickSave()
{
    this->mOwner->Dereference();
}

// FUNCTION: EXE 0x5a0269
// Destroys and frees the load scenario button event
// (scalar deleting destructor, emitted with its vtable: CreateToolbarButtons news it)

// FUNCTION: EXE 0x5a0294
// Destroys and frees the save scenario button event
// (scalar deleting destructor, emitted with its vtable: CreateToolbarButtons news it)

// FUNCTION: EXE 0x5a02bf
// Destroys and frees the test scenario button event
// (scalar deleting destructor, emitted with its vtable: CreateToolbarButtons news it)

// FUNCTION: EXE 0x5a02ea
// Destroys and frees the exit editor button event
// (scalar deleting destructor, emitted with its vtable: CreateToolbarButtons news it)
