// C:\EEX\Empire Earth\UI Events\EEDSUIEvents.cpp (RECONSTRUCTION; file name inferred: the header is
// ee/EEDSUIEvents.h, no __FILE__). Range 0x2544e9..0x2547e0: the debug sound panel's events. Evidence for the extent:
// 0x2544e9 and 0x25451d are file-static helpers of 0x2545c0 (cdecl, not called from the unit before), which the ten
// EEDSUIAdjust*Sound::ProcessEvent slots 0x2546ae..0x2547b2 call; 0x2547cf (EEDSUIHideDebugSoundPanel, ICF-shared
// with EEUIEWNSCancel) closes it; EEUIBColor.cpp starts at 0x2547e0. Before 0x2544e9 lie the template COMDATs of the
// unit utilities TU (EEFOFindNewTargetNearOldTarget.cpp: _Rotate 0x254379.. for its stable_sort).
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
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEDSUIEvents.h"

// The panel's value labels by sound kind (.rdata 0x4c6310; provisional name)
static const unsigned long kDSUIValueLabels[11] = {
    0x25c, 0x25e, 0x245, 0x248, 0x24b, 0x24e, 0x251, 0x254, 0x257, 0x25a, 0x25d
};

// FUNCTION: EXE 0x6544e9
// Reads a slider's value as a volume percentage (the slider runs from loud to quiet); false if the entity is no slider
static bool EEDSUIGetSliderValue(UIEntity* inEntity, long& outValue)
{
    bool theResult = false;
    UISlider* theSlider = dynamic_cast<UISlider*>(inEntity);
    if (theSlider != NULL) {
        outValue = 100 - theSlider->GetCurrentRangeValue();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x65451d
// Returns a unit sound by kind (1..6: its class's acknowledge/creation/death/selection sounds, 7..10: its model's)
// Kinds 7..10 read the first and last entries of the model's two 14-sound arrays (+0x28c/+0x2c4, as the original)
// NEAR: ours builds an ebp frame and lays out the jump-table arms in another order; the loaded members are the same
static bool EEDSUIGetSound(EEComplexUnit* inUnit, long inKind, EESoundObject*& outSound)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EEModel* theModel = theClass->mModel;
    bool theResult = true;
    switch (inKind) {
    case 1:
        outSound = theClass->mAcknowledgeSounds[0];
        break;
    case 2:
        outSound = theClass->mAcknowledgeSounds[1];
        break;
    case 3:
        outSound = theClass->mCreationSound;
        break;
    case 4:
        outSound = theClass->mDeathSound;
        break;
    case 5:
        outSound = theClass->mSelectionSounds[0];
        break;
    case 6:
        outSound = theClass->mSelectionSounds[1];
        break;
    case 7:
        outSound = theModel->mSounds1[0];
        break;
    case 8:
        outSound = theModel->mSounds2[0];
        break;
    case 9:
        outSound = theModel->mSounds1[13];
        break;
    case 10:
        outSound = theModel->mSounds2[13];
        break;
    default:
        theResult = false;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x6545c0
// Sets the volume of the first selected unit's sound of one kind from the debug slider and shows the value in its label
static bool EEDSUIAdjustSound(UIEntity* inEntity, long inKind)
{
    UWideString theText;
    bool theResult = false;
    EEComplexUnit* theUnit = NULL;
    EESoundObject* theSound = NULL;
    long theValue = 0;
    EEComplexUnit* theComplex;
    if (!EEGetFirstSelectedUnit(theUnit) || (theComplex = theUnit->mComplexUnit) == NULL || !EEDSUIGetSliderValue(inEntity, theValue)
        || !EEDSUIGetSound(theComplex, inKind, theSound) || theSound == NULL)
        return theResult;
    theSound->SetVolume(theValue * 0.01f);
    UIForm* theForm = gUIManager->GetFormByID(kFormDebugSound);
    UIControl* theControl = theForm->GetControlById(kDSUIValueLabels[inKind]);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(theControl);
    if (theLabel != NULL) {
        sprintf(theText, "%ld", theValue);
        theLabel->SetText(theText);
    }
    theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x6546ae
// Sets the selected unit's first acknowledge sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustCommandSoundOne::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 1))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x6546cb
// Sets the selected unit's second acknowledge sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustCommandSoundTwo::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 2))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x6546e8
// Sets the selected unit's creation sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustCreatedSound::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 3))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x654704
// Sets the selected unit's death sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustDyingSound::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 4))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x654721
// Sets the selected unit's first selection sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustSelectedSoundOne::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 5))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x65473e
// Sets the selected unit's second selection sound volume from the debug slider; 0 on success, else 3
long EEDSUIAdjustSelectedSoundTwo::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 6))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x65475b
// Sets the volume of the selected unit's model sound 7 (first entry of its first sound array) from the debug slider
long EEDSUIAdjustAttackSoundOne::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 7))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x654778
// Sets the volume of the selected unit's model sound 8 (first entry of its second sound array) from the debug slider
long EEDSUIAdjustAttackSoundTwo::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 8))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x654795
// Sets the volume of the selected unit's model sound 9 (last entry of its first sound array) from the debug slider
long EEDSUIAdjustWalkSoundOne::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 9))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x6547b2
// Sets the volume of the selected unit's model sound 10 (last entry of its second sound array) from the debug slider
long EEDSUIAdjustWalkSoundTwo::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 3;
    if (EEDSUIAdjustSound(inEntity, 10))
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x6547cf
// Closes the debug sound panel by popping the top form
long EEDSUIHideDebugSoundPanel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUIManager->Pop();
    return kErrorOK;
}
