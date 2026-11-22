// The scenario editor's actions whole in Rust (ee_screens editor_whole.rs; port addition, no original address): the
// kinds of EESEARust (ee/EEARust.h: one EEARust kind whose record holds the Rust action): the save, test, exit and
// quick save actions, the tech lists' move, the rotation, the trigger area's rectangle, the blank and random maps,
// the wall and gate placements (the original's EESEASaveScenario, EESEATestScenario, EESEAExitScenario,
// EESEAQuickSaveScenario, EESEATechTree, EESEARotateUnits, EESEAPaintArea, EESEACreateBlankMap, EESEACreateRandomMap,
// EESEAPlaceWall, EESEAPlaceGate, EESEAPlaceUnit, EESEAMoveUnit, EESEADeleteUnit). Requires what ee/EEARust.h does.
#ifndef EE_EESEARUST_H
#define EE_EESEARUST_H

#include <stdint.h>

#include "ee/EEARust.h"

class UIForm;

// The editor hosts' operands (this lane's statement hosts: p their handles, f their floats): the made object's out
// slot p0, the form p1, the rectangle f0..f3 (left, top, right, bottom: Rust computes a GERectangle(point, width,
// height)'s right and bottom as its constructor does), the shell arguments at p (int64_t: their count, then them)
inline void*& EESEOutSlot(void* const* p) { return *static_cast<void**>(p[0]); }
inline UIForm* EESEForm(void* const* p) { return static_cast<UIForm*>(p[1]); }
inline GERectangle EESERect(const float* f) { return GERectangle(f[0], f[1], f[2], f[3]); }
inline std::vector<int64_t> EESEArgs(const void* p) { return std::vector<int64_t>((const int64_t*)p + 1, (const int64_t*)p + 1 + *(const int64_t*)p); }

// The editor's events whole in Rust (ee_screens editor_events.rs): the hide-UI box, the viewport change, the
// toolbar's quick load and quick save, the trigger screens' slider label, check box display and area/object refill
// and the effect editors' own events (the original's EESEEHideUI, EESEEChangeViewport, EESEEQuickLoad,
// EESEEQuickSave, EESEESliderChange, EESEECheckDisplay, EESEEUseObjectArea, EESETSETCAMGModifyObject,
// EESETSETMSDCameraScrollChange, EESETSETSAAAttributeChange, EESEEUseAreaObject, EESEEMoveAreaObject,
// EESEEFaceAreaObject), and the editor tabs' (EEUIFSEEMapType, EEUIFSEESortUnitList, EESEEChangeStartEpoch,
// EESEEGameUnitLimit). Requires lle/UIEvent.h for EESEERust.

class EESEERust : public UIEvent {
public:
    EESEERust(uint32_t inKind, void* inObject = 0) : mEvent(ee_screens_see_new(inKind, inObject)) {}
    explicit EESEERust(EeScreensSeeEvent* inEvent) : mEvent(inEvent) {}   // an event Rust made with its members
    virtual ~EESEERust() { ee_screens_see_free(this->mEvent); }
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        return ee_screens_see_process(this->mEvent, inForm, inEntity);
    }
    EeScreensSeeEvent* mEvent;
};

// EESEESliderChange(label, format, scale, offset): the slider's value * scale + offset printed by the format into the
// label (the format is copied)
inline EESEERust* EESEENewSliderChange(void* inLabel, const char* inFormat, float inScale, float inOffset)
{
    return new EESEERust(ee_screens_see_new_slider(EE_SCREENS_SEE_SLIDER_CHANGE, inLabel, inFormat, inScale, inOffset));
}

// EESEECheckDisplay's mControls.push_back: a control shown while the check box is checked, else hidden
inline void EESEEPushControl(EESEERust* inEvent, void* inControl)
{
    ee_screens_see_push_control(inEvent->mEvent, inControl);
}

// EESEEUseObjectArea(label, combo box, area control): the combo box refilled with the areas when the area control
// is the one checked, else with the objects (FillScenarioComboBox), its first line picked
inline EESEERust* EESEENewUseObjectArea(void* inLabel, void* inComboBox, void* inAreaControl)
{
    return new EESEERust(ee_screens_see_new_object_area(inLabel, inComboBox, inAreaControl));
}

// EESEEGameUnitLimit(label, modifier, offset): the unit limit per player into the game page's label 0x162, then
// UIESliderChange's "%g" of the slider's value * modifier + offset into the label (no value maps to a string)
inline EESEERust* EESEENewUnitLimit(void* inLabel, float inModifier, long inOffset)
{
    return new EESEERust(ee_screens_see_new_slider(EE_SCREENS_SEE_UNIT_LIMIT, inLabel, "%g", inModifier, (float)inOffset));
}

// the effect editors' events: EE_SCREENS_SEE_CAMG_MODIFY_OBJECT, EE_SCREENS_SEE_MSD_CAMERA_SCROLL, EE_SCREENS_SEE_SAA_ATTRIBUTE (the effect types,
// EeScreensSeEffects of ee_screens editor_effects.rs, and the effect screen's EESETSEffectControls)
inline EESEERust* EESEENewObjects(uint32_t inKind, void* inA, void* inB, void* inC = 0)
{
    return new EESEERust(ee_screens_see_new_objects(inKind, inA, inB, inC));
}

#endif
