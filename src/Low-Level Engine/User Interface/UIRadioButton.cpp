// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007badf..0x1007c38e: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members (and the implicit copy members, ??_E) are emitted first by UICheckBox.cpp (0x1005fc41..).
// The next unit (0x1007c38e..0x1007c901) is UIScalableButton.cpp (not exported; RTTI .?AVUIScalableButton@@).
//
// UIRadioButton is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record is the Rust
// button's and every method below forwards to it, the radio walk over the form's siblings included (UIButton.cpp's
// host answers RadioOf and calls a sibling's virtual UnCheck). The constructor's UIButton part stays here.
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
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIRadioButton.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButtonRust.h"



