// C:\EEX\Empire Earth\UI Events\EEUIEGameSettingsEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEGSE).
// The game settings form's events (form 5: audio, interface and video tabs): 0x1d6b94..0x1d99ff. The unit starts with
// the init chain of sMusicFileName (the FormationChange unit ends at 0x1d6b94) and ends with the template COMDATs of
// the video-mode sort and the never-inlined EEGamePreferences::GetSettings COMDAT (0x1d99fb); the generic camera
// events that follow (EEUIEGrabScroll 0x1d99ff, ...) are another class family. The ProcessEvents are in alphabetical
// class order, then the two AoC additions (GraphicsQuality, AdvancedGraphicsSettingsChanged); the file-static helpers
// follow in address order (forward-declared). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>

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
#include "lle/PIMModule.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
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
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/UISlider.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "lle/UIESliderChange.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMouse.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEDisplayDevices.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEGameSettingsEvents.h"
#include "ee/EEStringIDs.h"
#include "platform/PlatWindow.h"
#include "rust/ee_rust.h"   // ee_screens_*: the presets, the video tab's plan, the value tables (ee_screens settings.rs)
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

// port: the display and audio settings' engine side
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);   // lle/SEMixer.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();
static UWideString sMusicFileName;          // 0x562f78 (init chain 0x1d6b94): the form's music while it is open
static bool sInGame;                        // 0x562f8c: opened from the game (InGameOpen), not the main screen

// the game view rectangle the settings reset (0x4463c0..: separate .rdata words, so named constants)
static const float kGameViewLeft = 0.0f;
static const float kGameViewRight = 1600.0f;
static const float kGameViewTop = 35.0f;
static const float kGameViewBottom = 1085.0f;
static const float kMinimumMouseSensitivity = 0.01f;
// the graphics quality presets (0..4; the original's radio groups 0x4463d4, 0x4463e8 and the check boxes 82..88, 110,
// 105) are Rust's (ee_screens settings.rs): GetGraphicsQuality, SetGraphicsQuality and ShowGraphics forward to it.
// The check boxes in the order the presets list them (EeScreensGraphics::checks), the detail radios high to low.
static const unsigned long kQualityChecks[9] = {kGameSettingsHiResFilterCheckbox, kGameSettingsHiResLightingCheckbox,
    kGameSettingsHiResTexturesCheckbox, kGameSettingsVsyncCheckbox, kGameSettingsShadowsCheckbox,
    kGameSettingsCloudsCheckbox, kGameSettingsBattlefieldAmbientsCheckBox, kGameSettingsWeatherEffectsCheckbox,
    kGameSettingsFullscreenAntialiasCheckbox};
static const unsigned long kModelRadios[3] = {kGameSettingsModelDetailRadioHigh, kGameSettingsModelDetailRadioMed,
    kGameSettingsModelDetailRadioLow};
static const unsigned long kEffectRadios[3] = {kGameSettingsEffectsDetailHigh, kGameSettingsEffectsDetailMed,
    kGameSettingsEffectsDetailLow};
// the quality radio buttons 0 (lowest) .. 4 (highest), then Custom
static const unsigned long kQualityRadios[6] = {kGameSettingsGQRadio1Lowest, kGameSettingsGQRadio2,
    kGameSettingsGQRadio3Middle, kGameSettingsGQRadio4, kGameSettingsGQRadio5Highest, kGameSettingsGQRadio6Custom};

// the modes of a screen, largest first (the sort's predicate; its operator() is the COMDAT 0x1d9554)
struct EEVideoModeGreater {
    bool operator()(const GEVideoMode& inA, const GEVideoMode& inB) const
    {
        bool theResult;
        if (inA.mWidth > inB.mWidth)
            theResult = true;
        else if (inA.mWidth < inB.mWidth)
            theResult = false;
        else if (inA.mHeight > inB.mHeight)
            theResult = true;
        else if (inA.mHeight < inB.mHeight)
            theResult = false;
        else if (inA.mBitCount > inB.mBitCount)
            theResult = true;
        else if (inA.mBitCount < inB.mBitCount)
            theResult = false;
        else
            theResult = inA.mRefreshRate > inB.mRefreshRate;
        return theResult;
    }
};

static void MusicOff();
static void MusicOn();
static void FillModes(unsigned long inRasterizer, unsigned long inScreen, std::vector<GEVideoMode>& outModes);
static unsigned long GetGraphicsQuality(UIForm* inForm);
static void SetGraphicsQuality(UIForm* inForm, unsigned long inQuality);
static long ApplySettings(UIForm* inForm, unsigned long& outModeIndex);
static void OnRestoreDefault(UIForm* inForm);
static void OpenSettingsForm(UIForm* inForm);
static void ShowVideo(UIForm* inForm, EEGameSettings inSettings);
static void ShowAudio(UIForm* inForm, EEGameSettings inSettings);
static void ShowInterface(UIForm* inForm, EEGameSettings inSettings);
static void ShowScreens(UIForm* inForm, EEGameSettings inSettings);
static void ShowResolutions(UIForm* inForm, EEGameSettings inSettings);
static void ShowRasterizers(UIForm* inForm, EEGameSettings inSettings);
static void ShowGraphics(UIForm* inForm, EEGameSettings inSettings);

// ---- port: the modern display and audio settings (EEGameSettings mUIScale..mAudioDevice; config.yaml)
// Each combo box line is a value (the tables are Rust's: EE_SCREENS_TABLE_*); the selection is the line whose value
// is nearest the setting.
static std::vector<std::string> sOutputDevices;
static bool sPortDisplayChanged;   // ApplySettings -> OutOfGameOk   // the output device combo's lines after "System Default"

static void PortSelect(UIForm* inForm, unsigned long inIndex, uint32_t inTable, long inValue)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inForm->GetControl(inIndex));
    if (!theBox)
        return;
    theBox->SetCurrentSelection(ee_screens_settings_nearest(inTable, inValue));
}

static void PortRead(UIForm* inForm, unsigned long inIndex, uint32_t inTable, long& outValue)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inForm->GetControl(inIndex));
    unsigned long theSelection;
    if (theBox && theBox->GetCurrentSelection(theSelection) == kErrorOK)
        ee_screens_settings_value(inTable, theSelection, &outValue);
}

static unsigned long GetGraphicsQuality(UIForm* inForm);
static void SetGraphicsQuality(UIForm* inForm, unsigned long inQuality);
static bool sPortQualityGuard;   // the quality combo box and the detail controls set each other

// The quality combo box shows the preset the detail controls match (5: Custom)
static void PortShowQuality(UIForm* inForm)
{
    UIComboBox* theBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsQualityComboBox));
    if (!theBox || sPortQualityGuard)
        return;
    // the controls' events are queued: only a different selection is set, or the two would set each other forever
    unsigned long theQuality = GetGraphicsQuality(inForm), theSelection;
    if (theBox->GetCurrentSelection(theSelection) == kErrorOK && theSelection == theQuality)
        return;
    sPortQualityGuard = true;
    theBox->SetCurrentSelection(theQuality);
    sPortQualityGuard = false;
}

// The quality combo box's selection: a preset sets the detail controls (Custom leaves them)
class EEUIEGSEPortQuality : public UIEvent {
public:
    virtual ~EEUIEGSEPortQuality() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        UIComboBox* theBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsQualityComboBox));
        unsigned long theQuality;
        if (sPortQualityGuard || !theBox || theBox->GetCurrentSelection(theQuality) != kErrorOK || theQuality >= 5 ||
            GetGraphicsQuality(inForm) == theQuality)   // already that preset (queued events: no ping-pong)
            return kErrorOK;
        sPortQualityGuard = true;
        SetGraphicsQuality(inForm, theQuality);
        sPortQualityGuard = false;
        return kErrorOK;
    }
};

static void PortShowVideo(UIForm* inForm, const EEGameSettings& inSettings)
{
    // the controls these replace (the video card, the 3D acceleration, the resolution list, the "Hi-Res" and v-sync
    // check boxes; the quality preset still sets the check boxes underneath)
    static const unsigned long kReplaced[] = {kGameSettingsVideoCardLabel, kGameSettingsVideoCardComboBox,
        kGameSettings3DRenderingSysLabel, kGameSettingsRenderingSysListBox, kGameSettingsScreenResLabel,
        kGameSettingsScreenResListBox, kGameSettingsCantChangeResInGameWarning, kGameSettingsHiResFilterCheckbox,
        kGameSettingsHiResLightingCheckbox, kGameSettingsHiResTexturesCheckbox, kGameSettingsVsyncCheckbox};
    for (size_t i = 0; i < sizeof(kReplaced) / sizeof(kReplaced[0]); i++)
        if (inForm->GetControl(kReplaced[i]))
            inForm->GetControl(kReplaced[i])->SetDisplayMode(kDisplayModeHidden);
    // the graphics quality's radio buttons, slider and labels (the quality combo box drives them, hidden)
    for (unsigned long i = 14; i <= 22; i++)
        if (i != 18 && i != 19 && inForm->GetControl(i))
            inForm->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
    for (unsigned long i = kGameSettingsGQRadio1Lowest; i <= kGameSettingsGQRadio6Custom + 1; i++)
        if (inForm->GetControl(i))
            inForm->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
    PortShowQuality(inForm);
    PortSelect(inForm, kGameSettingsWindowModeComboBox, EE_SCREENS_TABLE_WINDOW_MODE, inSettings.mWindowMode);
    PortSelect(inForm, kGameSettingsUIScaleComboBox, EE_SCREENS_TABLE_UI_SCALE, inSettings.mUIScale);
    PortSelect(inForm, kGameSettingsRenderResolutionComboBox, EE_SCREENS_TABLE_RENDER_RESOLUTION, inSettings.mRenderResolution);
    PortSelect(inForm, kGameSettingsFrameRateComboBox, EE_SCREENS_TABLE_FRAME_RATE, inSettings.mFrameRateLimit);
    PortSelect(inForm, kGameSettingsTextureFilteringComboBox, EE_SCREENS_TABLE_TEXTURE_FILTERING, inSettings.mTextureFiltering);
    PortSelect(inForm, kGameSettingsBrightnessComboBox, EE_SCREENS_TABLE_BRIGHTNESS, inSettings.mBrightness);
}

static void PortShowAudio(UIForm* inForm, const EEGameSettings& inSettings)
{
    if (UISlider* theMaster = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMasterVolumeSlider)))
        theMaster->SetCurrentRangeValue(inSettings.mMasterVolume);
    if (UICheckBox* theMute = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsMuteInBackgroundCheckBox)))
        theMute->SetChecked(inSettings.mMuteInBackground);
    UIComboBox* theDevices = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsOutputDeviceComboBox));
    if (!theDevices)
        return;
    if (sOutputDevices.empty()) {   // once per run: the devices there are, after "System Default"
        char theNames[16][128];
        unsigned long theCount = SPIMGetOutputDevices(theNames, 16);
        for (unsigned long i = 0; i < theCount; i++) {
            sOutputDevices.push_back(theNames[i]);
            theDevices->InsertLine(UWideString(theNames[i]), i + 1);
        }
    }
    unsigned long theSelection = 0;
    for (size_t i = 0; i < sOutputDevices.size(); i++)
        if (sOutputDevices[i] == inSettings.mAudioDevice)
            theSelection = (unsigned long)i + 1;
    theDevices->SetCurrentSelection(theSelection);
}

static void PortShowInterface(UIForm* inForm, const EEGameSettings& inSettings)
{
    // the network adapter section (the label, the selection label, the combo box): not a choice any more
    for (unsigned long i = 107; i <= kGameSettingsAdaptorSeletionComboBox; i++)
        if (inForm->GetControl(i))
            inForm->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
    PortSelect(inForm, kGameSettingsMaximumZoomOutComboBox, EE_SCREENS_TABLE_ZOOM_OUT, inSettings.mMaximumZoomOut);
}

// Reads the controls into the settings; -> whether the window mode, the UI scale or the render resolution changed
static bool PortApply(UIForm* inForm, EEGameSettings& ioSettings)
{
    long theWindowMode = ioSettings.mWindowMode, theUIScale = ioSettings.mUIScale;
    long theRenderResolution = ioSettings.mRenderResolution;
    PortRead(inForm, kGameSettingsWindowModeComboBox, EE_SCREENS_TABLE_WINDOW_MODE, ioSettings.mWindowMode);
    PortRead(inForm, kGameSettingsUIScaleComboBox, EE_SCREENS_TABLE_UI_SCALE, ioSettings.mUIScale);
    PortRead(inForm, kGameSettingsRenderResolutionComboBox, EE_SCREENS_TABLE_RENDER_RESOLUTION, ioSettings.mRenderResolution);
    PortRead(inForm, kGameSettingsFrameRateComboBox, EE_SCREENS_TABLE_FRAME_RATE, ioSettings.mFrameRateLimit);
    PortRead(inForm, kGameSettingsTextureFilteringComboBox, EE_SCREENS_TABLE_TEXTURE_FILTERING, ioSettings.mTextureFiltering);
    PortRead(inForm, kGameSettingsBrightnessComboBox, EE_SCREENS_TABLE_BRIGHTNESS, ioSettings.mBrightness);
    PortRead(inForm, kGameSettingsMaximumZoomOutComboBox, EE_SCREENS_TABLE_ZOOM_OUT, ioSettings.mMaximumZoomOut);
    if (UISlider* theMaster = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMasterVolumeSlider)))
        ioSettings.mMasterVolume = theMaster->GetCurrentRangeValue();
    if (UICheckBox* theMute = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsMuteInBackgroundCheckBox)))
        ioSettings.mMuteInBackground = theMute->GetIsChecked();
    UIComboBox* theDevices = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsOutputDeviceComboBox));
    unsigned long theDevice;
    if (theDevices && theDevices->GetCurrentSelection(theDevice) == kErrorOK)
        snprintf(ioSettings.mAudioDevice, sizeof(ioSettings.mAudioDevice), "%s",
                 theDevice > 0 && theDevice <= sOutputDevices.size() ? sOutputDevices[theDevice - 1].c_str() : "");
    ioSettings.mFullScreen = ioSettings.mWindowMode == 0;
    ioSettings.mWaitForVSync = ioSettings.mFrameRateLimit == 0;
    // the game's size follows the UI scale (32 bits: the bit depth is no setting any more)
    GXGameSize((unsigned long)ioSettings.mUIScale, ioSettings.mWindowMode != 0,
               reinterpret_cast<unsigned long*>(&ioSettings.mGameWindowWidth),
               reinterpret_cast<unsigned long*>(&ioSettings.mGameWindowHeight));
    ioSettings.mGameBitDepth = 32;
    ioSettings.mTextureBitDepth = 32;
    EEApplyPortSettings(ioSettings);
    return theWindowMode != ioSettings.mWindowMode || theUIScale != ioSettings.mUIScale ||
           theRenderResolution != ioSettings.mRenderResolution;
}

// Puts a new window mode into effect (the window's state; the caller resizes the game)
static void PortApplyWindowMode(const EEGameSettings& inSettings)
{
    PlatWindow* theWindow = gGameWindow ? gGameWindow->GetOSRepresentation() : NULL;
    if (!theWindow)
        return;
    if (inSettings.mWindowMode == 0) {
        PlatWindowSetFixedSize(theWindow, 0);
        PlatWindowSetFullscreen(theWindow, 1);
    } else {
        int32_t theWidth = 0, theHeight = 0;
        PlatWindowSetFullscreen(theWindow, 0);
        PlatWindowSetFixedSize(theWindow, 0);
        if (PlatWindowGetDisplayLogicalSize(NULL, &theWidth, &theHeight) && theWidth > 0)
            PlatWindowSetSize(theWindow, theWidth * 4 / 5, theHeight * 4 / 5);
        PlatWindowSetFixedSize(theWindow, 1);
    }
}

// FUNCTION: EXE 0x5d6bc0
// Shows the audio tab of the settings form
long EEUIEGSEAudioTab::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetEntityGroup(1)->Disable();
    inForm->GetEntityGroup(3)->Disable();
    inForm->GetEntityGroup(2)->Enable();
    inForm->GetBackgroundByID(0x8c)->SetVisibility(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6c0f
// Undoes the live music, sound and mouse changes from the saved preferences, closes the form and restores its music
long EEUIEGSECancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    float theVolume = gGamePreferences.mSettings.mMusicVolume;
    gConductor->GetConductor().SetVolume(theVolume);
    gUIManager->SetMusicVolume(theVolume);
    gSoundManager->SetMasterVolume(gGamePreferences.mSettings.mSoundVolume);
    gConductor->GetConductor().SetReverb(gGamePreferences.mSettings.mReverbEnabled);
    if (gGamePreferences.mSettings.mMusicEnabled)
        MusicOn();
    else
        MusicOff();
    gMouse->SetSensitivity(gGamePreferences.mSettings.mMouseSensitivity);
    UIManager::SetMouseLeftHanded(!gGamePreferences.mSettings.mRightHandMouseOrientation);
    gUserInterface->CloseForm(NULL);
    inForm->SetMusicFileName(sMusicFileName);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6cca
// Applies and saves the settings in game; closes the form, or asks for a display change when the rasterizer, screen,
// mode or anti-aliasing changed; returns the save result
long EEUIEGSEInGameOk::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    std::vector<GEVideoMode> theModes;
    UIComboBox* theScreenBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsVideoCardComboBox));
    UILBString* theRasterizerList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    GEScreen* theScreen = gRasterizer->GetScreen(gGamePreferences.mSettings.mScreenID);
    unsigned long theOldRasterizer = 0;
    unsigned long theIndex = 0;
    bool theOldB5DD = gGamePreferences.mSettings.mAntiAliasing;
    for (theIndex = 0; theIndex < GERasterizer::GetRasterizerCount(); theIndex++) {
        if (GERasterizer::GetRasterizer(theIndex) == gRasterizer) {
            theOldRasterizer = theIndex;
            break;
        }
    }
    unsigned long theOldScreen = gGamePreferences.mSettings.mScreenID;
    unsigned long theModeIndex;
    long theResult = ApplySettings(inForm, theModeIndex);
    unsigned long theRasterizer;
    if (theRasterizerList->GetCurrentSelection(theRasterizer) == kErrorNotSupported)
        theRasterizer = 0;
    unsigned long theScreenIndex;
    if (theScreenBox->GetCurrentSelection(theScreenIndex) != kErrorOK)
        theScreenIndex = 0;
    FillModes(theRasterizer, theScreenIndex, theModes);
    GEVideoMode theOldMode = theScreen->GetCurrentMode();
    GEVideoMode theNewMode = theModes[theModeIndex];
    if (theOldRasterizer == theRasterizer && theOldScreen == theScreenIndex
        && theOldMode.mBitCount == theNewMode.mBitCount && theOldMode.mHeight == theNewMode.mHeight
        && theOldMode.mWidth == theNewMode.mWidth && theOldB5DD == gGamePreferences.mSettings.mAntiAliasing)
        gUserInterface->CloseForm(NULL);
    else
        gUserInterface->PostDisplayChanged(false);
    inForm->SetMusicFileName(sMusicFileName);
    return theResult;
}

// FUNCTION: EXE 0x5d6e34
// Opens the settings form in game: silences its music, pauses (host only), hides the display choices behind
// the can't-change-resolution warning
long EEUIEGSEInGameOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    sMusicFileName = inForm->GetMusicFileName();
    inForm->SetMusicFileName("");
    if (gCommunications->IsServer() == true)
        gWorld.PauseGame();
    OpenSettingsForm(inForm);
    UIEvent* theEvent = new EEUIEGSEInGameOk;
    inForm->GetControl(kGameSettingsOKButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->GetControl(kGameSettingsVideoCardComboBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsVideoCardLabel)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsRenderingSysListBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettings3DRenderingSysLabel)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsScreenResListBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsScreenResLabel)->SetDisplayMode(kDisplayModeHidden);
    // port: in a game the display combo boxes stay hidden, as the resolution list was (no warning: nothing it
    // refers to is shown); the frame rate, filtering, brightness and textures still change
    inForm->GetControl(kGameSettingsWindowModeComboBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsUIScaleComboBox)->SetDisplayMode(kDisplayModeHidden);
    inForm->GetControl(kGameSettingsRenderResolutionComboBox)->SetDisplayMode(kDisplayModeHidden);
    sInGame = true;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6f51
// Destroys the in-game OK event and frees it when asked (scalar deleting destructor)
// (EEUIEGSEInGameOk's ??_G, after its first user)

// FUNCTION: EXE 0x5d6f7c
// Shows the interface tab of the settings form
long EEUIEGSEInterfaceTab::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetEntityGroup(1)->Disable();
    inForm->GetEntityGroup(2)->Disable();
    inForm->GetEntityGroup(3)->Enable();
    inForm->GetBackgroundByID(0x8c)->SetVisibility(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6fcb
// Applies the music on/off and music quality (reverb) radio choices at once
long EEUIEGSEMusicQualityRadio::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIRadioButton* theReverbOn = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualHighRadio));
    UIRadioButton* theReverbOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualLowRadio));
    UIRadioButton* theMusicOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicOffRadioButton));
    if (theMusicOff->GetIsChecked())
        MusicOff();
    else
        MusicOn();
    if (theReverbOn->GetIsChecked())
        gConductor->GetConductor().SetReverb(1);
    else if (theReverbOff->GetIsChecked())
        gConductor->GetConductor().SetReverb(0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d7020
// Sets the music volume from its slider (also the UI's music volume while music plays); returns the conductor's result
long EEUIEGSEMusicVolumeSlider::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theValue = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMusicVolumeSlider))->GetCurrentRangeValue();
    float theVolume = theValue * 0.01f;
    long theResult = gConductor->GetConductor().SetVolume(theVolume);
    if (gConductor->GetConductor().GetConductorState() == 1)
        gUIManager->SetMusicVolume(theVolume);
    return theResult;
}

// FUNCTION: EXE 0x5d7082
// Applies and saves the settings from the main menu and closes the form; returns the save result
long EEUIEGSEOutOfGameOk::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theModeIndex;
    long theResult = ApplySettings(inForm, theModeIndex);
    inForm->GetManager()->Pop();
    if (sPortDisplayChanged) {   // port: the window mode, the UI scale or the render resolution at once
        PortApplyWindowMode(gGamePreferences.GetSettings());
        gUserInterface->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false);
    }
    return theResult;
}

// FUNCTION: EXE 0x5d70ac
// Opens the settings form from the main menu, remembering its music
long EEUIEGSEOutOfGameOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    sMusicFileName = inForm->GetMusicFileName();
    OpenSettingsForm(inForm);
    UIEvent* theEvent = new EEUIEGSEOutOfGameOk;
    inForm->GetControl(kGameSettingsOKButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    sInGame = false;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d7119
// Destroys the out-of-game OK event and frees it when asked (scalar deleting destructor)
// (EEUIEGSEOutOfGameOk's ??_G, after its first user)

// FUNCTION: EXE 0x5d7144
// Refills the screen list when another rendering system is selected
long EEUIEGSERenderingSystemListBox::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    static UWideString sLastRasterizer;
    UILBString* theList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    unsigned long theSelection;
    theList->GetCurrentSelection(theSelection);
    theName = theList->GetData(theSelection);
    if (sLastRasterizer != theName)
        ShowScreens(inForm, gGamePreferences.GetSettings());
    sLastRasterizer = theName;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d7202
// Destroys the remembered rendering system name at exit
// (sLastRasterizer's atexit dtor)

// FUNCTION: EXE 0x5d720d
// Copies the game settings
// (EEGameSettings' implicit copy ctor, after its first user)

// FUNCTION: EXE 0x5d7689
// Asks whether to restore the default settings, showing them if the player agrees
long EEUIEGSERestoreDefault::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    gResource->GetString(IDS_ARE_YOU_SURE_YOU_WANT_TO_RESTORE, theQuestion);
    gUIManager->DisplayQuestion(theQuestion, true, inForm, OnRestoreDefault, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d76d7
// Sets the master sound volume from its slider
long EEUIEGSESoundVolumeSlider::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theValue = static_cast<UISlider*>(inEntity)->GetCurrentRangeValue();
    return gSoundManager->SetMasterVolume(theValue * 0.01f);
}

// FUNCTION: EXE 0x5d7709
// Plays one of four sample sounds at random at the current volume
long EEUIEGSETestSoundVolume::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theSounds[4] = { 0x34, 0x47, 0xa0, 0x17a };
    EESoundObject* theSound = EESoundObject::Get(theSounds[rand() % 4]);
    if (theSound) {
        theSound->LoadMedia();
        gSoundManager->Play(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d777b
// Refills the resolution list when another video card is selected
long EEUIEGSEVideoCardComboBox::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    static UWideString sLastScreen;
    UIComboBox* theBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsVideoCardComboBox));
    unsigned long theSelection;
    theBox->GetCurrentSelection(theSelection);
    theName = theBox->GetListBox()->GetData(theSelection);
    if (sLastScreen != theName)
        ShowResolutions(inForm, gGamePreferences.GetSettings());
    sLastScreen = theName;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d7836
// Destroys the remembered video card name at exit
// (sLastScreen's atexit dtor)

// FUNCTION: EXE 0x5d7841
// Shows the video tab of the settings form
long EEUIEGSEVideoTab::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormGameSettings);
    theForm->GetEntityGroup(2)->Disable();
    theForm->GetEntityGroup(3)->Disable();
    theForm->GetEntityGroup(1)->Enable();
    theForm->GetBackgroundByID(0x8c)->SetVisibility(true);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d789c
// Applies the graphics quality preset of the clicked quality radio button
long EEUIEGSEGraphicsQuality::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (!inEntity)
        return kErrorOK;
    unsigned long theQuality;
    unsigned long theID = static_cast<UIControl*>(inEntity)->GetId();
    if (theID == inForm->GetControl(kGameSettingsGQRadio1Lowest)->GetId())
        theQuality = 0;
    else if (theID == inForm->GetControl(kGameSettingsGQRadio2)->GetId())
        theQuality = 1;
    else if (theID == inForm->GetControl(kGameSettingsGQRadio3Middle)->GetId())
        theQuality = 2;
    else if (theID == inForm->GetControl(kGameSettingsGQRadio4)->GetId())
        theQuality = 3;
    else if (theID == inForm->GetControl(kGameSettingsGQRadio5Highest)->GetId())
        theQuality = 4;
    else
        return kErrorOK;
    SetGraphicsQuality(inForm, theQuality);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d791d
// Checks the quality radio that matches the advanced graphics choices, or shows and checks Custom
long EEUIEGSEAdvancedGraphicsSettingsChanged::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIRadioButton* theCustom = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsGQRadio6Custom));
    unsigned long theRadio = ee_screens_quality_radio(GetGraphicsQuality(inForm));
    theCustom->SetDisplayMode(kDisplayModeHidden);
    UIRadioButton* theButton = static_cast<UIRadioButton*>(inForm->GetControl(kQualityRadios[theRadio]));
    if (theRadio == EE_SCREENS_QUALITY_CUSTOM)
        theCustom->SetDisplayMode(kDisplayModeShown);
    if (!theButton->GetIsChecked())
        theButton->Check();
    PortShowQuality(inForm);   // port: the quality combo box
    return kErrorOK;
}

// FUNCTION: EXE 0x5d79ee
// Turns the music off
static void MusicOff()
{
    gConductor->GetConductor().SetConductorState(0);
    gUIManager->SetMusicVolume(0.0f);
    gMusicManager->Stop();
}

// FUNCTION: EXE 0x5d7a19
// Turns the music on, starting the game music when the form was opened in game
static void MusicOn()
{
    gConductor->GetConductor().SetConductorState(1);
    if (sInGame == true)
        gMusicManager->Start();
    gUIManager->SetMusicVolume(gConductor->GetConductor().GetVolume());
}

// FUNCTION: EXE 0x5d7a54
// Collects the video modes of a rasterizer's screen, sorted largest first
static void FillModes(unsigned long inRasterizer, unsigned long inScreen, std::vector<GEVideoMode>& outModes)
{
    bool theFound = false;
    for (std::vector<EEDisplayRasterizer>::iterator theRasterizer = gDisplayRasterizers.begin();
         theRasterizer != gDisplayRasterizers.end() && !theFound; theRasterizer++) {
        if (theRasterizer->mIndex != inRasterizer)
            continue;
        for (std::vector<EEDisplayScreen>::iterator theScreen = theRasterizer->mScreens.begin();
             theScreen != theRasterizer->mScreens.end() && !theFound; theScreen++) {
            if (theScreen->mIndex != inScreen)
                continue;
            for (std::vector<EEDisplayMode>::iterator theMode = theScreen->mModes.begin();
                 theMode != theScreen->mModes.end(); theMode++)
                outModes.push_back(theMode->mMode);
            theFound = true;
        }
    }
    std::sort(outModes.begin(), outModes.end(), EEVideoModeGreater());
}

// FUNCTION: EXE 0x5d7af6
// Returns the graphics quality preset (0..4) the advanced check boxes and detail radios match, else 5 (custom)
static unsigned long GetGraphicsQuality(UIForm* inForm)
{
    EeScreensGraphics theControls;
    for (int i = 0; i < 9; i++)
        theControls.checks[i] = static_cast<UICheckBox*>(inForm->GetControl(kQualityChecks[i]))->GetIsChecked();
    for (int i = 0; i < 3; i++) {
        theControls.model[i] = static_cast<UIRadioButton*>(inForm->GetControl(kModelRadios[i]))->GetIsChecked();
        theControls.effect[i] = static_cast<UIRadioButton*>(inForm->GetControl(kEffectRadios[i]))->GetIsChecked();
    }
    return ee_screens_graphics_quality(&theControls);
}

// Checks a detail radio of a group (kModelRadios, kEffectRadios) by its level: 0 low, 1 medium, 2 high
static void CheckDetail(UIForm* inForm, const unsigned long (&inRadios)[3], long inLevel)
{
    if (inLevel >= 0 && inLevel <= 2)
        static_cast<UIRadioButton*>(inForm->GetControl(inRadios[2 - inLevel]))->Check();
}

// FUNCTION: EXE 0x5d7cd0
// Sets the advanced check boxes and detail radios to a graphics quality preset (0..4)
static void SetGraphicsQuality(UIForm* inForm, unsigned long inQuality)
{
    EeScreensPreset thePreset;
    if (!ee_screens_quality_preset(inQuality, &thePreset))
        return;
    for (int i = 0; i < 9; i++)
        static_cast<UICheckBox*>(inForm->GetControl(kQualityChecks[i]))->SetChecked(thePreset.checks[i]);
    CheckDetail(inForm, kModelRadios, thePreset.model);
    CheckDetail(inForm, kEffectRadios, thePreset.effect);
}

// FUNCTION: EXE 0x5d7e9b
// Stores the form's choices (rasterizer, mode, graphics, sound, mouse, autosave) in the preferences and saves them;
// always turns full-screen anti-aliasing off (its check box is never read); returns the save result
static long ApplySettings(UIForm* inForm, unsigned long& outModeIndex)
{
    UString theName;
    GERectangle theViewRectangle;
    std::vector<GEVideoMode> theModes;
    UILBString* theResolutionList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsScreenResListBox));
    UICheckBox* theCheck82 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsHiResFilterCheckbox));
    UIRadioButton* theRadio75 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsModelDetailRadioHigh));
    UIRadioButton* theRadio76 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsModelDetailRadioMed));
    UIRadioButton* theRadio77 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsModelDetailRadioLow));
    UIRadioButton* theRadio79 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsEffectsDetailHigh));
    UIRadioButton* theRadio80 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsEffectsDetailMed));
    UICheckBox* theCheck83 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsHiResLightingCheckbox));
    UIRadioButton* theRadio81 = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsEffectsDetailLow));
    UIComboBox* theScreenBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsVideoCardComboBox));
    UIComboBox* theAdapterBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsAdaptorSeletionComboBox));
    UICheckBox* theCheck84 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsHiResTexturesCheckbox));
    UIRadioButton* theReverbOn = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualHighRadio));
    UICheckBox* theCheck86 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsShadowsCheckbox));
    UIRadioButton* theReverbOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualLowRadio));
    UICheckBox* theCheck87 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsCloudsCheckbox));
    UIRadioButton* theMusicOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicOffRadioButton));
    UICheckBox* theCheck110 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsBattlefieldAmbientsCheckBox));
    UIRadioButton* theRightHanded = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsRightHandMouseRadio));
    UICheckBox* theCheck88 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsWeatherEffectsCheckbox));
    UISlider* theVoiceVolume = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMaxNumSoundsSlider));
    UICheckBox* theAmbientSounds = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsAmbientSoundsCheckBox));
    UISlider* theMouseSensitivity = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMouseSensitivitySlider));
    UICheckBox* theCheck13 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsShowBloodCheckBox));
    UISlider* theSideScrollSpeed = static_cast<UISlider*>(inForm->GetControl(kGameSettingsSideScrollSpeedSlider));
    UICheckBox* theWaitForVSync = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsVsyncCheckbox));
    UISlider* theGrabScrollSpeed = static_cast<UISlider*>(inForm->GetControl(kGameSettingsGrabScrollSpeedSlider));
    UILBString* theRasterizerList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    UISlider* theAutoSaveTime = static_cast<UISlider*>(inForm->GetControl(kGameSettingsAutosaveFreqSlider));
    UISlider* theAutoSaveCount = static_cast<UISlider*>(inForm->GetControl(kGameSettingsAutosavesToKeepSlider));
    unsigned long theRasterizer = 0;
    gGamePreferences.GetSettings().mAntiAliasing = false;
    if (theRasterizerList->GetCurrentSelection(theRasterizer) == kErrorNotSupported)
        theRasterizer = 0;
    unsigned long theScreen;
    if (theScreenBox->GetCurrentSelection(theScreen) != kErrorOK)
        theScreen = 0;
    unsigned long theAdapter;
    theAdapterBox->GetCurrentSelection(theAdapter);
    gGamePreferences.GetSettings().mNetworkAdapter = theAdapter;
    if (gGamePreferences.GetSettings().mRasterizerName != GERasterizer::GetRasterizer(theRasterizer)->GetName()
        || gGamePreferences.GetSettings().mScreenID != theScreen) {
        gUserInterface->PostChangeRasterizer(theRasterizer, theScreen);
        theName = GERasterizer::GetRasterizer(theRasterizer)->GetName();
        gGamePreferences.GetSettings().mRasterizerName = theName;
        gGamePreferences.GetSettings().mScreenID = theScreen;
    }
    // port: the game's size is the UI scale's (PortApply), not the resolution list's (hidden); the mode index is
    // the matching mode's (InGameOk compares it with the current mode)
    sPortDisplayChanged = PortApply(inForm, gGamePreferences.GetSettings());
    FillModes(theRasterizer, theScreen, theModes);
    outModeIndex = 0;
    for (unsigned long i = 0; i < theModes.size(); i++)
        if (theModes[i].mWidth == gGamePreferences.GetSettings().mGameWindowWidth &&
            theModes[i].mHeight == gGamePreferences.GetSettings().mGameWindowHeight &&
            theModes[i].mBitCount == gGamePreferences.GetSettings().mGameBitDepth)
            outModeIndex = i;
    (void)theResolutionList;
    theViewRectangle.SetBottom(kGameViewBottom);
    theViewRectangle.SetLeft(kGameViewLeft);
    theViewRectangle.SetRight(kGameViewRight);
    theViewRectangle.SetTop(kGameViewTop);
    EESetGameViewRectangle(theViewRectangle);
    gGamePreferences.GetSettings().mShowBlood = theCheck13->GetIsChecked();
    gGamePreferences.GetSettings().mZoomEnabled = true;
    gGamePreferences.GetSettings().mTrilinearFilter = theCheck82->GetIsChecked();
    if (theCheck82->GetIsChecked())
        gTextureManager->UpdateMIPFilterMode(kMipFilterLinear);
    else
        gTextureManager->UpdateMIPFilterMode(kMipFilterPoint);
    gGamePreferences.GetSettings().mUnitLighting = theCheck83->GetIsChecked();
    if (theCheck84->GetIsChecked()) {
        gGamePreferences.GetSettings().mMIPLevelsToDrop = 0;
        gGamePreferences.GetSettings().mTerrainMIPLevelsToDrop = 0;
    } else {
        gGamePreferences.GetSettings().mMIPLevelsToDrop = 1;
        gGamePreferences.GetSettings().mTerrainMIPLevelsToDrop = 1;
    }
    // port: the texture resolution combo box (Original, 2x, 4x); world textures loaded from now on use it
    UIComboBox* theTextureResolution = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsTextureResolutionComboBox));
    unsigned long theTextureSelection;
    if (theTextureResolution && theTextureResolution->GetCurrentSelection(theTextureSelection) == kErrorOK
        && theTextureSelection < 3) {
        gGamePreferences.GetSettings().mTextureResolution = theTextureSelection == 0 ? 0 : theTextureSelection == 1 ? 2 : 4;
        EESetTextureResolution(gGamePreferences.GetSettings().mTextureResolution);
    }
    gGamePreferences.GetSettings().mDisplayShadows = theCheck86->GetIsChecked();
    gGamePreferences.GetSettings().mDisplayClouds = theCheck87->GetIsChecked();
    gGamePreferences.GetSettings().mDisplayBattlefieldAmbients = theCheck110->GetIsChecked();
    gGamePreferences.GetSettings().mDisplayWeatherEffects = theCheck88->GetIsChecked();
    if (theRadio77->GetIsChecked())
        gGamePreferences.GetSettings().mModelDetail = 0;
    else if (theRadio76->GetIsChecked())
        gGamePreferences.GetSettings().mModelDetail = 1;
    else if (theRadio75->GetIsChecked())
        gGamePreferences.GetSettings().mModelDetail = 2;
    if (theRadio81->GetIsChecked())
        gGamePreferences.GetSettings().mEffectDetail = 0;
    else if (theRadio80->GetIsChecked())
        gGamePreferences.GetSettings().mEffectDetail = 1;
    else if (theRadio79->GetIsChecked())
        gGamePreferences.GetSettings().mEffectDetail = 2;
    gGamePreferences.GetSettings().mAmbientSounds = theAmbientSounds->GetIsChecked();
    gAmbientSoundManager.SetEnabled(theAmbientSounds->GetIsChecked());
    gGamePreferences.GetSettings().mSoundVolume = gSoundManager->GetMasterVolume();
    float theVoiceFactor = gGamePreferences.GetSettings().mMaxNumberOfSounds = theVoiceVolume->GetCurrentRangeValue() * 0.01f;
    unsigned long theChannels = (unsigned long)(gSoundManager->GetMaxChannels() * theVoiceFactor);
    if (theChannels < 8)
        theChannels = 8;
    gSoundManager->SetMaxChannels(theChannels);
    gGamePreferences.GetSettings().mMusicEnabled = theMusicOff->GetIsChecked() == false;
    if (theReverbOn->GetIsChecked())
        gGamePreferences.GetSettings().mReverbEnabled = true;
    else if (theReverbOff->GetIsChecked())
        gGamePreferences.GetSettings().mReverbEnabled = false;
    gGamePreferences.GetSettings().mMusicVolume = gConductor->GetConductor().GetVolume();
    float theSensitivity = theMouseSensitivity->GetCurrentRangeValue() * 0.01f;
    if (theSensitivity < kMinimumMouseSensitivity)
        theSensitivity = kMinimumMouseSensitivity;
    gMouse->SetSensitivity(theSensitivity);
    gGamePreferences.GetSettings().mMouseSensitivity = theSensitivity;
    gGamePreferences.GetSettings().mSideScrollSpeed = theSideScrollSpeed->GetCurrentRangeValue() * 0.01f;
    gGamePreferences.GetSettings().mGrabScrollSpeed = theGrabScrollSpeed->GetCurrentRangeValue() * 0.01f;
    bool theRight = theRightHanded->GetIsChecked();
    UIManager::SetMouseLeftHanded(theRight == false);
    gGamePreferences.GetSettings().mRightHandMouseOrientation = theRight;
    gGamePreferences.GetSettings().mAutoSaveInMilliseconds = theAutoSaveTime->GetCurrentRangeValue() * 60000;
    gGamePreferences.GetSettings().mNumberOfAutoSaves = theAutoSaveCount->GetCurrentRangeValue();
    (void)theWaitForVSync;   // port: the frame rate combo box decides v-sync (PortApply)
    long theResult = gGamePreferences.Save();
    return theResult;
}

// FUNCTION: EXE 0x5d83da
// Shows the default settings on every tab
static void OnRestoreDefault(UIForm* inForm)
{
    EEGameSettings theSettings(EEGamePreferences::sDefaultSettings);
    ShowAudio(inForm, theSettings);
    ShowInterface(inForm, theSettings);
    ShowVideo(inForm, theSettings);
}

// FUNCTION: EXE 0x5d8450
// Prepares the settings form: autosave slider labels, every tab filled from the preferences, the video tab shown
static void OpenSettingsForm(UIForm* inForm)
{
    UWideString theString;
    inForm->GetEntityGroup(1)->Disable();
    inForm->GetEntityGroup(2)->Disable();
    inForm->GetEntityGroup(3)->Disable();
    UIESliderChange* theEvent = new UIESliderChange(static_cast<UITextLabel*>(inForm->GetControl(kGameSettingsAutosaveFreqNumberLabel)), 1.0f, 0);
    inForm->GetControl(kGameSettingsAutosaveFreqSlider)->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    gResource->GetString(IDS_LOBBY_OFF, theString);
    theEvent->AddString(0.0f, theString);
    theEvent->Dereference();
    theEvent = new UIESliderChange(static_cast<UITextLabel*>(inForm->GetControl(kGameSettingsAutosavesToKeepNumberLabel)), 1.0f, 0);
    inForm->GetControl(kGameSettingsAutosavesToKeepSlider)->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
    theEvent->Dereference();
    // port: the quality combo box's event, and the display combo boxes (an in-game open hides them again after this)
    UIEvent* theQualityEvent = new EEUIEGSEPortQuality;
    inForm->GetControl(kGameSettingsQualityComboBox)->SetTriggerEvent(kControlTriggerSelectionChanged, theQualityEvent);
    theQualityEvent->Dereference();
    inForm->GetControl(kGameSettingsWindowModeComboBox)->SetDisplayMode(kDisplayModeShown);
    inForm->GetControl(kGameSettingsUIScaleComboBox)->SetDisplayMode(kDisplayModeShown);
    inForm->GetControl(kGameSettingsRenderResolutionComboBox)->SetDisplayMode(kDisplayModeShown);
    ShowAudio(inForm, gGamePreferences.GetSettings());
    ShowInterface(inForm, gGamePreferences.GetSettings());
    ShowVideo(inForm, gGamePreferences.GetSettings());
    UIEvent* theVideoTab = new EEUIEGSEVideoTab;
    srand(TMGetTime());
    theVideoTab->ProcessEvent(inForm, NULL);
    theVideoTab->Dereference();
}

// FUNCTION: EXE 0x5d865a
// Destroys the video tab event and frees it when asked (scalar deleting destructor)
// (EEUIEGSEVideoTab's ??_G, after its first user)

// FUNCTION: EXE 0x5d8685
// Fills the video tab: rendering systems, screens, resolutions, the blood check box (shown outside Korea) and graphics
static void ShowVideo(UIForm* inForm, EEGameSettings inSettings)
{
    UICheckBox* theCheck13 = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsShowBloodCheckBox));
    ShowRasterizers(inForm, inSettings);
    ShowScreens(inForm, inSettings);
    ShowResolutions(inForm, inSettings);
    theCheck13->SetChecked(inSettings.mShowBlood);
    theCheck13->SetDisplayMode(gGamePreferences.IsNotKorean() ? 1 : 0);
    ShowGraphics(inForm, inSettings);
    PortShowVideo(inForm, inSettings);   // port
}

// FUNCTION: EXE 0x5d8728
// Fills the audio tab; applies the music and sound volumes only when they equal the defaults (restoring defaults)
static void ShowAudio(UIForm* inForm, EEGameSettings inSettings)
{
    UICheckBox* theAmbientSounds = static_cast<UICheckBox*>(inForm->GetControl(kGameSettingsAmbientSoundsCheckBox));
    UISlider* theVoiceVolume = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMaxNumSoundsSlider));
    UIRadioButton* theReverbOn = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualHighRadio));
    UISlider* theMusicVolume = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMusicVolumeSlider));
    UISlider* theSoundVolume = static_cast<UISlider*>(inForm->GetControl(kGameSettingsSoundVolumeSlider));
    UIRadioButton* theReverbOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicQualLowRadio));
    UIRadioButton* theMusicOff = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsMusicOffRadioButton));
    if (inSettings.mMusicVolume == EEGamePreferences::sDefaultSettings.mMusicVolume) {
        gConductor->GetConductor().SetVolume(inSettings.mMusicVolume);
        gUIManager->SetMusicVolume(inSettings.mMusicVolume);
    }
    theMusicVolume->SetCurrentRangeValue((long)(gConductor->GetConductor().GetVolume() * 100.0f));
    if (inSettings.mSoundVolume == EEGamePreferences::sDefaultSettings.mSoundVolume)
        gSoundManager->SetMasterVolume(inSettings.mSoundVolume);
    theSoundVolume->SetCurrentRangeValue((long)(gSoundManager->GetMasterVolume() * 100.0f));
    theVoiceVolume->SetCurrentRangeValue((long)(inSettings.mMaxNumberOfSounds * 100.0f));
    if (inSettings.mReverbEnabled)
        theReverbOn->Check();
    else
        theReverbOff->Check();
    if (!inSettings.mMusicEnabled)
        theMusicOff->Check();
    theAmbientSounds->SetChecked(inSettings.mAmbientSounds);
    PortShowAudio(inForm, inSettings);   // port
}

// FUNCTION: EXE 0x5d8861
// Fills the interface tab: mouse sensitivity and hand, scroll speeds, network adapters (Default first) and autosave
static void ShowInterface(UIForm* inForm, EEGameSettings inSettings)
{
    UIRadioButton* theRightHanded = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsRightHandMouseRadio));
    UIRadioButton* theLeftHanded = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsLeftHandMouseRadio));
    UISlider* theSideScrollSpeed = static_cast<UISlider*>(inForm->GetControl(kGameSettingsSideScrollSpeedSlider));
    UISlider* theGrabScrollSpeed = static_cast<UISlider*>(inForm->GetControl(kGameSettingsGrabScrollSpeedSlider));
    UISlider* theAutoSaveTime = static_cast<UISlider*>(inForm->GetControl(kGameSettingsAutosaveFreqSlider));
    UISlider* theMouseSensitivity = static_cast<UISlider*>(inForm->GetControl(kGameSettingsMouseSensitivitySlider));
    UISlider* theAutoSaveCount = static_cast<UISlider*>(inForm->GetControl(kGameSettingsAutosavesToKeepSlider));
    UIComboBox* theAdapterBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsAdaptorSeletionComboBox));
    NEProtocol* theProtocol = NULL;
    std::vector<UWideString> theAdapters;
    UWideString theText;
    theMouseSensitivity->SetCurrentRangeValue((long)(inSettings.mMouseSensitivity * 100.0f));
    gMouse->SetSensitivity(inSettings.mMouseSensitivity);
    theSideScrollSpeed->SetCurrentRangeValue((long)(inSettings.mSideScrollSpeed * 100.0f));
    theGrabScrollSpeed->SetCurrentRangeValue((long)(inSettings.mGrabScrollSpeed * 100.0f));
    NEProtocol::GetProtocol(0, theProtocol);
    unsigned long theCount = theProtocol->EnumerateNetworkAdapters(theAdapters);
    unsigned long theAdapter = inSettings.mNetworkAdapter;
    theAdapterBox->DeleteAllEntries();
    gResource->GetString(IDS_CIV_POWER_DEFAULT, theText);
    theAdapterBox->AppendLine(theText);
    for (unsigned long i = 0; i < theCount; i++)
        theAdapterBox->AppendLine(theAdapters[i]);
    if (theAdapter > theCount)
        theAdapter = 0;
    theAdapterBox->SetCurrentSelection(theAdapter);
    bool theRight = inSettings.mRightHandMouseOrientation;
    UIManager::SetMouseLeftHanded(theRight == false);
    if (theRight)
        theRightHanded->Check();
    else
        theLeftHanded->Check();
    theAutoSaveTime->SetCurrentRangeValue(inSettings.mAutoSaveInMilliseconds / 60000);
    theAutoSaveCount->SetCurrentRangeValue(inSettings.mNumberOfAutoSaves);
    static_cast<UITextLabel*>(inForm->GetControl(kGameSettingsAutosaveFreqNumberLabel))->SetText((long)(inSettings.mAutoSaveInMilliseconds / 60000));
    static_cast<UITextLabel*>(inForm->GetControl(kGameSettingsAutosavesToKeepNumberLabel))->SetText(inSettings.mNumberOfAutoSaves);
    PortShowInterface(inForm, inSettings);   // port
}

// FUNCTION: EXE 0x5d8a51
// Lists the selected rendering system's screens, selecting the settings' screen
static void ShowScreens(UIForm* inForm, EEGameSettings inSettings)
{
    UIComboBox* theScreenBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsVideoCardComboBox));
    UILBString* theRasterizerList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    unsigned long theIndex = 0;
    unsigned long theRasterizer = 0;
    theScreenBox->ReleaseCapturedMouse();
    theScreenBox->DeleteAllEntries();
    theRasterizerList->GetCurrentSelection(theRasterizer);
    unsigned long theScreenID = inSettings.mScreenID;
    for (std::vector<EEDisplayScreen>::iterator theScreen = gDisplayRasterizers[theRasterizer].mScreens.begin();
         theScreen != gDisplayRasterizers[theRasterizer].mScreens.end(); theScreen++) {
        theScreenBox->AppendLine(theScreen->mName);
        if (theScreenID == theIndex)
            theScreenBox->SetCurrentSelection(theIndex);
        theIndex++;
    }
}

// FUNCTION: EXE 0x5d8ae7
// Lists the selected screen's video modes, selecting the settings' mode, else the default mode, else the first
static void ShowResolutions(UIForm* inForm, EEGameSettings inSettings)
{
    UWideString theText;
    std::vector<GEVideoMode> theModes;
    UILBString* theRasterizerList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    UIComboBox* theScreenBox = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsVideoCardComboBox));
    UILBString* theResolutionList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsScreenResListBox));
    EEGameSettings theDefaults(EEGamePreferences::sDefaultSettings);
    unsigned long theSelection = 0;
    bool theFound = false;
    unsigned long theRasterizer;
    theRasterizerList->GetCurrentSelection(theRasterizer);
    GERasterizer* theRasterizerObject = GERasterizer::GetRasterizer(theRasterizer);
    unsigned long theScreen;
    theScreenBox->GetCurrentSelection(theScreen);
    GEScreen* theGEScreen = theRasterizerObject->GetScreen(theScreen);
    unsigned long theIndex = 0;
    FillModes(theRasterizer, theScreen, theModes);
    GEVideoMode theCurrentMode;
    theCurrentMode.mBitCount = inSettings.mGameBitDepth;
    theCurrentMode.mHeight = inSettings.mGameWindowHeight;
    theCurrentMode.mWidth = inSettings.mGameWindowWidth;
    theResolutionList->DeleteAllEntries();
    for (std::vector<GEVideoMode>::iterator it = theModes.begin(); it != theModes.end(); it++) {
        GEVideoMode theMode = *it;
        sprintf(theText, "%lu x %lu x %lu Bit", theMode.mWidth, theMode.mHeight, theMode.mBitCount);
        theResolutionList->AppendLine(theText);
        if (theMode.mBitCount == theCurrentMode.mBitCount && theMode.mHeight == theCurrentMode.mHeight
            && theMode.mWidth == theCurrentMode.mWidth) {
            theSelection = theIndex;
            theFound = true;
        }
        if (!theFound && theMode.mBitCount == theDefaults.mGameBitDepth
            && theMode.mHeight == theDefaults.mGameWindowHeight && theMode.mWidth == theDefaults.mGameWindowWidth) {
            theSelection = theIndex;
            theFound = true;
        }
        theIndex++;
    }
    theResolutionList->ChangeSelectionIndex(theSelection, false, true);
}

// FUNCTION: EXE 0x5d8c79
// Lists the rendering systems, selecting the settings' one
static void ShowRasterizers(UIForm* inForm, EEGameSettings inSettings)
{
    UWideString theName;
    UILBString* theList = static_cast<UILBString*>(inForm->GetControl(kGameSettingsRenderingSysListBox));
    std::vector<EEDisplayRasterizer>::iterator it = gDisplayRasterizers.begin();
    unsigned long theIndex = 0;
    theList->DeleteAllEntries();
    theName = inSettings.mRasterizerName;
    for (; it != gDisplayRasterizers.end(); it++) {
        theList->AppendLine(it->mName);
        if (it->mName == theName)
            theList->ChangeSelectionIndex(theIndex, false, true);
        theIndex++;
    }
}

// FUNCTION: EXE 0x5d8d07
// Fills the graphics check boxes and detail radios from the settings and checks the matching quality preset (or Custom)
static void ShowGraphics(UIForm* inForm, EEGameSettings inSettings)
{
    EeScreensGraphicsSettings theSettings;
    theSettings.trilinear_filter = inSettings.mTrilinearFilter;
    theSettings.unit_lighting = inSettings.mUnitLighting;
    theSettings.shadows = inSettings.mDisplayShadows;
    theSettings.clouds = inSettings.mDisplayClouds;
    theSettings.battlefield_ambients = inSettings.mDisplayBattlefieldAmbients;
    theSettings.weather_effects = inSettings.mDisplayWeatherEffects;
    theSettings.anti_aliasing = inSettings.mAntiAliasing;
    theSettings.wait_for_vsync = inSettings.mWaitForVSync;
    theSettings.mip_levels_to_drop = inSettings.mMIPLevelsToDrop;
    theSettings.terrain_mip_levels_to_drop = inSettings.mTerrainMIPLevelsToDrop;
    theSettings.texture_resolution = inSettings.mTextureResolution;
    theSettings.model_detail = inSettings.mModelDetail;
    theSettings.effect_detail = inSettings.mEffectDetail;
    EeScreensGraphicsShow theShow;
    ee_screens_graphics_show(&theSettings, &theShow);
    // the original's order: 82, 83, 84, the texture resolution, 86, 87, 110, 88, 105, the detail radios, 85 (v-sync)
    for (int i = 0; i < 3; i++)
        static_cast<UICheckBox*>(inForm->GetControl(kQualityChecks[i]))->SetChecked(theShow.checks[i]);
    UIComboBox* theTextureResolution = static_cast<UIComboBox*>(inForm->GetControl(kGameSettingsTextureResolutionComboBox));
    if (theTextureResolution)   // port
        theTextureResolution->SetCurrentSelection(theShow.texture_line);
    for (int i = 4; i < 9; i++)
        static_cast<UICheckBox*>(inForm->GetControl(kQualityChecks[i]))->SetChecked(theShow.checks[i]);
    CheckDetail(inForm, kModelRadios, theShow.model);
    CheckDetail(inForm, kEffectRadios, theShow.effect);
    static_cast<UICheckBox*>(inForm->GetControl(kQualityChecks[3]))->SetChecked(theShow.checks[3]);
    unsigned long theRadio = ee_screens_quality_radio(GetGraphicsQuality(inForm));
    UIRadioButton* theCustom = static_cast<UIRadioButton*>(inForm->GetControl(kGameSettingsGQRadio6Custom));
    theCustom->SetDisplayMode(kDisplayModeHidden);
    static_cast<UIRadioButton*>(inForm->GetControl(kQualityRadios[theRadio]))->Check();
    if (theRadio == EE_SCREENS_QUALITY_CUSTOM)
        theCustom->SetDisplayMode(kDisplayModeShown);
}

// SYNTHETIC: EXE 0x5d6b94
// SYNTHETIC: EXE 0x5d6b9e
// SYNTHETIC: EXE 0x5d6ba9
// SYNTHETIC: EXE 0x5d6bb5

// the template COMDATs of the video-mode vector and its sort (first-reference order; the vector's begin/end/_Buy/
// _Tidy/_Ucopy/fill/_Construct/operator+/operator- are ICF-folded into other units' twins)
// TEMPLATE: EXE 0x5d8f29
// TEMPLATE: EXE 0x5d8f55
// TEMPLATE: EXE 0x5d8faf
// TEMPLATE: EXE 0x5d9277
// TEMPLATE: EXE 0x5d9332
// TEMPLATE: EXE 0x5d9361
// TEMPLATE: EXE 0x5d938a
// TEMPLATE: EXE 0x5d9463
// TEMPLATE: EXE 0x5d94a0
// TEMPLATE: EXE 0x5d9554
// TEMPLATE: EXE 0x5d9594
// TEMPLATE: EXE 0x5d959a
// TEMPLATE: EXE 0x5d95d7
// TEMPLATE: EXE 0x5d9616
// TEMPLATE: EXE 0x5d9734
// TEMPLATE: EXE 0x5d9769
// TEMPLATE: EXE 0x5d97d0
// TEMPLATE: EXE 0x5d97f9
// TEMPLATE: EXE 0x5d981e
// TEMPLATE: EXE 0x5d98bf
// TEMPLATE: EXE 0x5d9920
// TEMPLATE: EXE 0x5d994d
// TEMPLATE: EXE 0x5d99b4

// FUNCTION: EXE 0x5d99fb
// Returns the game preferences' current settings
// (EEGamePreferences::GetSettings, the inline accessor no caller expands; the ICF representative of its twins)
