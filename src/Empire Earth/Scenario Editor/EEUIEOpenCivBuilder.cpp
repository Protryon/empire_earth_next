// C:\EEX\Empire Earth\Scenario Editor\EEUIEOpenCivBuilder.cpp (file name inferred: no __FILE__). 0x19eba9..0x1a098e,
// with EESEEQuickLoad.cpp and EESEEQuickSave.cpp folded in; the functions before 0x1a0315 now live in
// EEUIFScenarioEditor.cpp. Unit flags: the exe module set.
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

// The seven editor tabs, original VA 0x8c46d0, stride 0x20 (provisional names).
// 0x19eba9 reads order/help/normal+pressed cells/flag while constructing the buttons;
// 0x19ef5a dispatches group numbers; 0x19f615 reads full-view and brush flags.
// Fundamental fields keep the original static payload (no point-constructor init chain).
#include "ee/EESEEditorTab.h"
// EESEEditorTab sEditorTabs[7] = {   // (one array at 0x4c46d0, defined in EEUIFScenarioEditor.cpp)
//     {0, 0, 0x213a,   0.0f, 50.0f,  32.0f, 50.0f,  1, false, false},
//     {1, 1, 0x213b, 128.0f, 50.0f, 160.0f, 50.0f,  2, false, true},
//     {2, 2, 0x213c,  64.0f, 50.0f,  96.0f, 50.0f,  3, false, true},
//     {3, 3, 0x213d,   0.0f, 82.0f,  32.0f, 82.0f,  6, false, false},
//     {4, 4, 0x213e, 192.0f, 50.0f, 224.0f, 50.0f, 14, false, false},
//     {5, 5, 0x1b61,  64.0f, 82.0f,  96.0f, 82.0f,  4, true,  false},
//     {6, 6, 0x2143, 128.0f, 82.0f, 160.0f, 82.0f,  7, true,  false}
// };

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
// EESEToolbarButton sToolbarButtons[6] = {   // (one array at 0x4c47b0, defined in EEUIFScenarioEditor.cpp)
//     {0xc, 0x287, 0x2e0, 0x80, 0x72, 0xa0, 0x72, 0x2140},
//     {0xd, 0x2aa, 0x2e0, 0x00, 0xc4, 0x20, 0xc4, 0x2141},
//     {0xb, 0x2cd, 0x2e0, 0x40, 0x72, 0x60, 0x72, 0x1b59},
//     {0xa, 0x2f0, 0x2e0, 0x00, 0x72, 0x20, 0x72, 0x1b58},
//     {0xe, 0x313, 0x2e0, 0x40, 0xc4, 0x60, 0xc4, 0x2142},
//     {0xf, 0x336, 0x2e0, 0x80, 0xc4, 0xa0, 0xc4, 0x213f}
// };


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
#include "ee/EEStringIDs.h"
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
extern unsigned long& gTexturePixelDepth; // 0x5308b4 (Empire Earth.cpp)
extern bool gSightUpdatesEnabled;
extern GEViewport gViewport;

// FUNCTION: EXE 0x5a0315
// Builds the brush size group (title and tiny to huge radio buttons, tiny checked) on the terrain and elevation tabs
EEUIFSEBrushGroup::EEUIFSEBrushGroup(unsigned long& ioIndex, const GE2DPoint& inLocation,
                                   UIForm* inForm, GEFont* inFont)
{
    GE2DPoint theLocation = inLocation;
    UWideString theText;
    GEText* theGEText;
    this->mIsHighlightShown = false;
    gResource->GetString(IDS_BRUSH_SIZE, theText);
    this->mTitle = new UITextLabel(theText, inFont, 7,
        GERectangle(theLocation, 100.0f, (float)inFont->GetHeight()), inForm, 0, ioIndex, 0);
    ioIndex++;
    theGEText = this->mTitle->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mTitle->SetAlignment(kTextAlignCenter);
    this->mTitle->SetBgOpacity(0.4f);
    inForm->AddControl(this->mTitle, 2);
    inForm->AddControl(this->mTitle, 3);
    theLocation.SetY(this->mTitle->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_TINY, theText);
    this->mButtons[0] = new EEUIFSERBBrush(this, 0, 2, theText, inFont, theLocation, inForm, ioIndex, true);
    ioIndex++;
    theGEText = this->mButtons[0]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theLocation.SetY(this->mButtons[0]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_SMALL, theText);
    this->mButtons[1] = new EEUIFSERBBrush(this, 1, 3, theText, inFont, theLocation, inForm, ioIndex, false);
    ioIndex++;
    theGEText = this->mButtons[1]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mButtons[1], 2);
    inForm->AddControl(this->mButtons[1], 3);
    theLocation.SetY(this->mButtons[1]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_MEDIUM, theText);
    this->mButtons[2] = new EEUIFSERBBrush(this, 2, 4, theText, inFont, theLocation, inForm, ioIndex, false);
    ioIndex++;
    theGEText = this->mButtons[2]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mButtons[2], 2);
    inForm->AddControl(this->mButtons[2], 3);
    theLocation.SetY(this->mButtons[2]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_LARGE, theText);
    this->mButtons[3] = new EEUIFSERBBrush(this, 3, 5, theText, inFont, theLocation, inForm, ioIndex, false);
    ioIndex++;
    theGEText = this->mButtons[3]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mButtons[3], 2);
    inForm->AddControl(this->mButtons[3], 3);
    theLocation.SetY(this->mButtons[3]->GetRectangle().GetBottom() + 1.0f);
    gResource->GetString(IDS_SE_HUGE, theText);
    this->mButtons[4] = new EEUIFSERBBrush(this, 4, 6, theText, inFont, theLocation, inForm, ioIndex, false);
    ioIndex++;
    theGEText = this->mButtons[4]->GetTextLabel()->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inForm->AddControl(this->mButtons[4], 2);
    inForm->AddControl(this->mButtons[4], 3);
    inForm->AddControl(this->mButtons[0], 2);
    inForm->AddControl(this->mButtons[0], 3);
    this->mBrushSize = this->mButtons[0]->mBrushSize;
    this->mIsCentered = false;
}

// FUNCTION: EXE 0x5a074a
// Destroys the brush group, deleting its five buttons and title
EEUIFSEBrushGroup::~EEUIFSEBrushGroup()
{
    for (unsigned long i = 0; i < 5; i++)
        delete this->mButtons[i];
    delete this->mTitle;
}

// FUNCTION: EXE 0x5a0778
// Sets the highlighted brush square around a map position, one tile larger to the top left when centered
void EEUIFSEBrushGroup::SetHighlight(float inX, float inY, bool inIsCentered)
{
    if (!inIsCentered) {
        this->mRectangle.SetLeft((float)(long)(inX - this->mBrushSize));
        this->mRectangle.SetTop((float)(long)(inY - this->mBrushSize));
        this->mRectangle.SetRight((float)(long)(inX + this->mBrushSize));
        this->mRectangle.SetBottom((float)(long)(inY + this->mBrushSize));
        this->mIsCentered = false;
    } else {
        inX += 0.5f;
        inY += 0.5f;
        this->mRectangle.SetLeft((float)(long)(inX - this->mBrushSize - 1.0f));
        this->mRectangle.SetTop((float)(long)(inY - this->mBrushSize - 1.0f));
        this->mRectangle.SetRight((float)(long)(inX + this->mBrushSize));
        this->mRectangle.SetBottom((float)(long)(inY + this->mBrushSize));
        this->mIsCentered = true;
    }
}

