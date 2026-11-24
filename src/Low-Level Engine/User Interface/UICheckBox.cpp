// C:\EEX\Low-Level Engine\User Interface\UICheckBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005ef4b..0x100602e4: the header-inline members (and implicit copy members, ??_E) of the classes this unit is the
// first to include, in include order: UISlider, UIScrollButton, UIScrollBar, UITextLabel, UICheckBox; then the U2D
// key init chains (0x1005fb21, the rasterizer header comes next), UIRadioButton's inline members (0x1005fc41); then
// its own code (0x1005fdef: the ctor, then the members) and the vector<UWideString*>::_Tidy COMDAT.
//
// UICheckBox is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record is the Rust button's
// and every method below forwards to it; the label is a part the Rust button holds (UIButton.cpp's host makes it).
// The constructor's UIButton part (the box rectangle, the manager's check bitmaps) stays here.
#include <windows.h>
#include <stddef.h>

#include "rust/ui/buttonobj.h"
#include "rust/ui/checkobj.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/UIRadioButton.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButtonRust.h"





