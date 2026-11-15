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
#include "lle/GEText.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EEUIFSETerrain.h"
#include "lle/UICheckBox.h"
#include "ee/EESETSBaseObject.h"
extern GEFont* gU531930;                    // 0x531930 (Empire Earth.cpp's fonts)
extern GEFont* gU531938;
#include "ee/EESETriggerScreen.h"
#include "ee/EESEInstructions.h"
#include "ee/EESEAQuickSaveLoad.h"
#include "rust/ee_screens.h"

extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls (EESERBUnitTypes.cpp)

// Builds the brush size group (title and tiny to huge radio buttons, tiny checked) on the terrain and elevation tabs
// (editor_brush.rs)
EEUIFSEBrushGroup::EEUIFSEBrushGroup(unsigned long& ioIndex, const GE2DPoint& inLocation,
                                   UIForm* inForm, GEFont* inFont)
{
    this->mIsHighlightShown = false;
    GEFont* theScreenFont = gSeplScreenFont;
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    size_t theControls[6];
    ee_screens_sebr_build(this, inForm, inLocation.GetX(), inLocation.GetY(), &theIndex, theControls);
    gSeplScreenFont = theScreenFont;
    ioIndex = theIndex;
    this->mTitle = static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(theControls[0]));
    for (unsigned long i = 0; i < 5; i++)
        this->mButtons[i] = static_cast<EEUIFSERBBrush*>(reinterpret_cast<UIControl*>(theControls[i + 1]));
    this->mBrushSize = this->mButtons[0]->mBrushSize;
    this->mIsCentered = false;
}

// Destroys the brush group, deleting its five buttons and title
EEUIFSEBrushGroup::~EEUIFSEBrushGroup()
{
    for (unsigned long i = 0; i < 5; i++)
        delete this->mButtons[i];
    delete this->mTitle;
}

// Sets the highlighted brush square around a map position, one tile larger to the top left when centered
// (editor_brush.rs)
void EEUIFSEBrushGroup::SetHighlight(float inX, float inY, bool inIsCentered)
{
    float r[4];
    ee_screens_sebr_highlight(inX, inY, (uint32_t)this->mBrushSize, inIsCentered, r);
    this->mRectangle.SetLeft(r[0]);
    this->mRectangle.SetTop(r[1]);
    this->mRectangle.SetRight(r[2]);
    this->mRectangle.SetBottom(r[3]);
    this->mIsCentered = inIsCentered;
}
