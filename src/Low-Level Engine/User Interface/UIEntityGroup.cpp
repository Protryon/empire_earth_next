// C:\EEX\Low-Level Engine\User Interface\UIEntityGroup.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006e304..0x1006e5a8: the U2D key init chains, then Disable, Enable and RemoveEntity, and the
// copy<UIEntity**> COMDAT (from vector::erase) last. The inline members are emitted by UIBackground.cpp.
// UIEPolledEvent.cpp starts at 0x1006e5a8.
#include <windows.h>

#include "rust/ee_ui.h"

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
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

// The Rust loops' questions and calls on the group's entities (ee_ui entity_group.rs)
extern "C" unsigned long ee_uicpp_group(void* inGroup, int inCall, unsigned long inIndex, bool inArg)
{
    UIEntityGroup* theGroup = static_cast<UIEntityGroup*>(inGroup);
    if (inCall == 0)
        return theGroup->GetEntityCount();
    UIEntity* theEntity = theGroup->GetEntity(inIndex);
    UIControl* theControl = static_cast<UIControl*>(theEntity);
    switch (inCall) {
    case 1:
        return theEntity->GetType() == kControlTypeButton;
    case 2:
        return theControl->GetDisplayMode() == kDisplayModeShown;
    case 3:
        return theControl->GetForm() == theControl->GetForm()->GetManager()->GetCurrentForm();
    case 4:
        theControl->GetForm()->GetManager()->AddDirtyRectangle(theControl->GetRectangle(), theControl->GetForm());
        return 0;
    case 5:
        theControl->HideControl();
        return 0;
    case 6:
        theControl->SetDirty(true);
        return 0;
    case 7:
        return theControl->GetForm()->GetTabbingOn();
    case 8:
        return theControl->GetForm()->GetTabStop() == theControl->GetIndex();
    case 9:
        theControl->GetForm()->Tab();
        return 0;
    default:
        (static_cast<UIHotKey*>(theEntity))->SetEnabled(inArg);
        return 0;
    }
}

// FUNCTION: LLE 0x1006e424
// Disables the group: hides its shown buttons on the current form (tabbing past them) and disables its hotkeys
// (the loop is Rust: ee_ui entity_group.rs)
void UIEntityGroup::Disable()
{
    this->mDisableCount = 1;
    ee_ui_group_disable(this);
}

// FUNCTION: LLE 0x1006e4df
// Enables the group: redraws its buttons and enables its hotkeys
void UIEntityGroup::Enable()
{
    this->mDisableCount = 0;
    ee_ui_group_enable(this);
}

// FUNCTION: LLE 0x1006e520
// Removes an entity from the group; returns not-found if it is not a member
long UIEntityGroup::RemoveEntity(UIEntity* inEntity)
{
    long theResult = kErrorNotFound;
    std::vector<UIEntity*>::iterator theIterator = std::find(this->mEntities.begin(), this->mEntities.end(), inEntity);
    if (theIterator != this->mEntities.end()) {
        theResult = kErrorOK;
        this->mEntities.erase(theIterator);
    }
    return theResult;
}

// Template COMDAT (vector<UIEntity*>::erase)
// FUNCTION: LLE 0x1006e584

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1006e304
// SYNTHETIC: LLE 0x1006e30e
// SYNTHETIC: LLE 0x1006e31e
// SYNTHETIC: LLE 0x1006e32a
// SYNTHETIC: LLE 0x1006e334
// SYNTHETIC: LLE 0x1006e33e
// SYNTHETIC: LLE 0x1006e34e
// SYNTHETIC: LLE 0x1006e35a
// SYNTHETIC: LLE 0x1006e364
// SYNTHETIC: LLE 0x1006e36e
// SYNTHETIC: LLE 0x1006e37e
// SYNTHETIC: LLE 0x1006e38a
// SYNTHETIC: LLE 0x1006e394
// SYNTHETIC: LLE 0x1006e39e
// SYNTHETIC: LLE 0x1006e3ae
// SYNTHETIC: LLE 0x1006e3ba
// SYNTHETIC: LLE 0x1006e3c4
// SYNTHETIC: LLE 0x1006e3ce
// SYNTHETIC: LLE 0x1006e3de
// SYNTHETIC: LLE 0x1006e3ea
// SYNTHETIC: LLE 0x1006e3f4
// SYNTHETIC: LLE 0x1006e3fe
// SYNTHETIC: LLE 0x1006e40e
// SYNTHETIC: LLE 0x1006e41a
