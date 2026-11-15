// C:\EEX\Empire Earth\Scenario Editor\EEUIFSERBBrush.cpp (file name inferred: no __FILE__). 0x1a4c00..0x1a4cbb: ctor,
// Check, ??_G. After EEUIFSEPaintElevation.cpp, before EEUIFSERedo.cpp. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIRadioButton.h"
#include "lle/UIForm.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSERBBrush.h"

// FUNCTION: EXE 0x5a4c00
// Creates a brush size radio button for the brush group
EEUIFSERBBrush::EEUIFSERBBrush(EEUIFSEBrushGroup* inGroup, long inBrushSize, unsigned long inId,
                               const UWideString& inText, GEFont* inFont, const GE2DPoint& inPosition, UIForm* inForm,
                               unsigned long inIndex, bool inIsChecked)
    : UIRadioButton(inId, inText, inFont, GERectangle(inPosition, 100.0f, 20.0f), inForm, inIndex, 0, false,
                    inIsChecked, false),
      mGroup(inGroup), mBrushSize(inBrushSize)
{
    this->SetBgOpacity(0.4f);
}

// FUNCTION: EXE 0x5a4c83
// Sets the brush group's brush size to this button's size, then checks the button
void EEUIFSERBBrush::Check()
{
    this->mGroup->mBrushSize = this->mBrushSize;
    UIRadioButton::Check();
}

// SYNTHETIC: EXE 0x5a4c98
// (the scalar deleting destructor, emitted with the vtable)
