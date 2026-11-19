// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEElevation.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFSEElevation). The editor's elevation tab: 0x1a098e..0x1a1397 (the ctor, EEUIEElevationTool's ??_G, the tab's
// ??_G, dtor and slots). After the form's unit (0x1a0315..: EEUIFSEBrushGroup), before EEUIFSEESElevation.cpp
// (0x1a1397). Unit flags: the exe module set.
//
// The tab whole in Rust (ee_screens editor_elevation.rs): its group editor_groups_ffi.rs holds it (the class
// EEUIFSEElevation is gone). This unit
// is the tab's host (ee_screenscpp_seel): the controls only the tab makes and the world it reads.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"


#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIShell.h"
#include "ee/EEStringIDs.h"
#include "rust/game/screens.h"
extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls (EESERBUnitTypes.cpp)

// ---- the tab's host (editor_elevation.rs: the ops from 900)

static_assert(kSEElevationPaint == 0xa1 && kSEElevationElevationSlider == 0x9d && kSEElevationStrengthSlider == 0xa2 &&
                  kSEElevationRaiseButton == 0x96 && kSEElevationLevelButton == 0xa0 &&
                  kSEElevationStrengthLabel == 0x9f && IDS_PAINT_HILLS == 30465 && IDS_SE_MAP_TERRAIN == 20018,
              "editor_elevation.rs's ids");

extern "C" int32_t ee_screenscpp_seel(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    void** theOut = static_cast<void**>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEEL_OP_PAINT: {
        void* const* thePair = static_cast<void* const*>(p[2]);
        UIControl* t = new EEUIShellControl(EE_SCREENS_SK_PAINT_ELEVATION, {(int64_t)thePair[0], (int64_t)thePair[1]},
                                            kSEElevationPaint, GERectangle(GE2DPoint(0.0f, 0.0f), 1024.0f, 606.0f),
                                            theForm, (unsigned long)i[0], 0);
        theForm->AddControl(t, 2);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEEL_OP_ELEVATION_SLIDER: {
        UISlider* t = new EEUIShellSlider(EE_SCREENS_SK_ELEVATION_SLIDER, {}, kSEElevationElevationSlider,
                                          GERectangle(GE2DPoint(f[0], f[1]), 20.0f, (float)(unsigned long)f[2]), 1,
                                          theForm, (unsigned long)i[0], 2);
        t->SetRange(0, 0x40);
        t->SetCurrentRangeValue(t->GetMaxRange() - 0x18);
        t->SetPageSize(1);
        t->SetBgColor(GEColor(0.2f, 0.2f, 0.2f));
        t->SetBgOpacity(0.4f);
        *theOut = t;
        return (int32_t)(t->GetMaxRange() - t->GetCurrentRangeValue() - 0x18);
    }
    case EE_SCREENS_SEEL_OP_STRENGTH_SLIDER: {
        UISlider* t = new UISlider(kSEElevationStrengthSlider, GERectangle(GE2DPoint(f[0], f[1]), 20.0f, f[2]), 1,
                                   theForm, (unsigned long)i[0], 2);
        t->SetRange(0, 0x10);
        t->SetCurrentRangeValue(t->GetMaxRange() - 6);
        t->SetPageSize(1);
        t->SetBgColor(GEColor(0.2f, 0.2f, 0.2f));
        t->SetBgOpacity(0.4f);
        UIEvent* theEvent = new UIESliderChange(static_cast<UITextLabel*>(p[2]), -4.0f, 0x28);
        t->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
        theEvent->Dereference();
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEEL_OP_FONT_HEIGHT: return (int32_t)gSeplScreenFont->GetHeight();
    case EE_SCREENS_SEEL_OP_ADD:
        if (i[0])
            static_cast<UIControl*>(p[0])->SetDisplayMode(kDisplayModeHidden);
        else
            theForm->AddControl(static_cast<UIControl*>(p[0]), 2);
        return 0;
    case EE_SCREENS_SEEL_OP_MOUSE: {
        UIForm* theEditor = static_cast<UIForm*>(p[0]);
        f[0] = gMouse->mPosition.GetX();
        f[1] = gMouse->mPosition.GetY();
        return theEditor == theEditor->GetManager()->GetCurrentForm();
    }
    case EE_SCREENS_SEEL_OP_HIGHLIGHT: {
        EETile* theTile;
        long theX;
        long theY;
        EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]));
        GE2DPoint theLocation(f[0], f[1]);
        if (!gViewport.GetViewRectangle().Contains(theLocation))
            theEditor->GetBrushGroup()->mIsHighlightShown = false;
        else if (i[0] == EE_SCREENS_SEEL_HIGHLIGHT_POINT) {   // the terrain tab's: the map point, centred by i1
            GE3DPoint thePoint;
            EEPointUnitOrTile(theLocation, theTile, thePoint, false);
            theEditor->GetBrushGroup()->mIsHighlightShown = true;
            theEditor->GetBrushGroup()->SetHighlight(thePoint.GetX(), thePoint.GetY(), i[1] != 0);
        } else {
            EEPointUnitOrTileXY(theLocation, theTile, theX, theY, false);
            theEditor->GetBrushGroup()->mIsHighlightShown = true;
            theEditor->GetBrushGroup()->SetHighlight((float)theX, (float)theY, false);
        }
        return 0;
    }
    }
    return 0;
}
