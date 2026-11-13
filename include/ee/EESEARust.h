// The scenario editor's actions whole in Rust (ee_screens editor_whole.rs; port addition, no original address): the
// save, test, exit and quick save actions, the tech lists' move, the rotation, the trigger area's rectangle, the
// blank and random maps, the wall and gate placements (the original's EESEASaveScenario, EESEATestScenario,
// EESEAExitScenario, EESEAQuickSaveScenario, EESEATechTree, EESEARotateUnits, EESEAPaintArea, EESEACreateBlankMap,
// EESEACreateRandomMap, EESEAPlaceWall, EESEAPlaceGate, EESEAPlaceUnit, EESEAMoveUnit). One EEAction holds the Rust action; the kinds that posted
// themselves when made post when made. Requires ee/EEAction.h.
#ifndef EE_EESEARUST_H
#define EE_EESEARUST_H

#include <stdint.h>

// (rust/ee_screens.h declares these too; here so the actions' users need no Rust header)
struct EeScreensSewAction;
extern "C" EeScreensSewAction* ee_screens_sew_new(void* owner, uint32_t kind, void* object, int32_t a, int32_t b,
                                                  int32_t c, int32_t d);
extern "C" void ee_screens_sew_begin(EeScreensSewAction* action);
extern "C" void ee_screens_sew_run(EeScreensSewAction* action);
extern "C" void ee_screens_sew_free(EeScreensSewAction* action);
extern "C" void ee_screens_sew_rotate(EeScreensSewAction* action, float yaw);
extern "C" void ee_screens_sew_area_corner(EeScreensSewAction* action, int32_t x, int32_t y);
extern "C" void ee_screens_sew_area(const EeScreensSewAction* action, int32_t* out);
extern "C" void ee_screens_sew_wall(EeScreensSewAction* action, float x0, float y0, float x1, float y1, int32_t player,
                                    int32_t technology);
extern "C" uint8_t ee_screens_sew_gate(EeScreensSewAction* action, int32_t x, int32_t y, int32_t player,
                                       int32_t technology, int32_t direction);
extern "C" uint8_t ee_screens_sew_place_unit(EeScreensSewAction* action, int32_t player);
extern "C" void ee_screens_sew_place_push(EeScreensSewAction* action, int32_t player, int32_t unit_type, const float* t,
                                         int32_t animation);
extern "C" void ee_screens_sew_pick_up(EeScreensSewAction* action, uint32_t id);
extern "C" void ee_screens_sew_put_down(EeScreensSewAction* action);
extern "C" void ee_screens_sew_cancel(EeScreensSewAction* action);
extern "C" uint8_t ee_screens_sew_moving(const EeScreensSewAction* action);

// the kinds (EE_SCREENS_SEW_*)
enum {
    kSEASave = 1,
    kSEATest = 2,
    kSEAExit = 3,
    kSEAQuickSave = 4,      // object: the editor's EEAQuickSaveLoad
    kSEATechTree = 5,       // first, last, player, the left list
    kSEARotate = 6,
    kSEAPaintArea = 7,      // the first corner
    kSEABlankMap = 8,       // width, height, terrain
    kSEARandomMap = 9,      // size, style
    kSEAPlaceWall = 10,
    kSEAPlaceGate = 11,
    kSEAPlaceUnit = 12,
    kSEAMoveUnit = 13,
};

// The editor's events whole in Rust (ee_screens editor_events.rs): the hide-UI box, the viewport change, the
// toolbar's quick load and quick save, the trigger screens' slider label, check box display and area/object refill
// and the effect editors' own events (the original's EESEEHideUI, EESEEChangeViewport, EESEEQuickLoad,
// EESEEQuickSave, EESEESliderChange, EESEECheckDisplay, EESEEUseObjectArea, EESETSETCAMGModifyObject,
// EESETSETMSDCameraScrollChange, EESETSETSAAAttributeChange, EESEEUseAreaObject, EESEEMoveAreaObject,
// EESEEFaceAreaObject). Requires lle/UIEvent.h for EESEERust.
struct EeScreensSeeEvent;
extern "C" EeScreensSeeEvent* ee_screens_see_new(uint32_t kind, void* object);
extern "C" int32_t ee_screens_see_process(const EeScreensSeeEvent* event, void* form, void* entity);
extern "C" void ee_screens_see_viewport(EeScreensSeeEvent* event, const float* rectangle, uint8_t hidden);
extern "C" void ee_screens_see_free(EeScreensSeeEvent* event);
extern "C" void ee_screens_see_hide_ui_views(EeScreensSeeEvent* event, const float* unchecked, const float* checked);
extern "C" void ee_screens_see_slider(EeScreensSeeEvent* event, void* label, const char* format, float scale, float offset);
extern "C" void ee_screens_see_push_control(EeScreensSeeEvent* event, void* control);
extern "C" void ee_screens_see_object_area(EeScreensSeeEvent* event, void* label, void* combo, void* area);
extern "C" void ee_screens_see_objects(EeScreensSeeEvent* event, void* a, void* b, void* c);
enum {
    kSEEHideUI = 1,
    kSEEChangeViewport = 2,
    kSEEQuickLoad = 3,          // object: the quick save/load action
    kSEEQuickSave = 4,          // object: the quick save/load action
    kSEESliderChange = 5,       // EESEENewSliderChange
    kSEECheckDisplay = 6,       // its controls: EESEEPushControl
    kSEEUseObjectArea = 7,      // EESEENewUseObjectArea
    kSEECAMGModifyObject = 8,   // EESEENewObjects(kind, the military group type, its two lists)
    kSEEMSDCameraScroll = 9,    // EESEENewObjects(kind, the send-dialogue type, the effect's controls)
    kSEESAAAttribute = 10,      // EESEENewObjects(kind, the set-AI-attribute type, the effect's controls)
    kSEEUseAreaObject = 11,     // EESEEUseAreaObject and EESEEMoveAreaObject (one body)
    kSEEFaceAreaObject = 12,
};

class EESEARust : public EEAction {
public:
    EESEARust(uint32_t inKind, void* inObject = 0, int32_t inA = 0, int32_t inB = 0, int32_t inC = 0, int32_t inD = 0)
        : EEAction(false)
    {
        this->mAction = ee_screens_sew_new(this, inKind, inObject, inA, inB, inC, inD);
        ee_screens_sew_begin(this->mAction);
    }
    virtual ~EESEARust() { ee_screens_sew_free(this->mAction); }
    virtual void Virtual4() { ee_screens_sew_run(this->mAction); }
    virtual void Virtual8() {}
    EeScreensSewAction* mAction;
};

class EESEERust : public UIEvent {
public:
    EESEERust(uint32_t inKind, void* inObject = 0) : mEvent(ee_screens_see_new(inKind, inObject)) {}
    virtual ~EESEERust() { ee_screens_see_free(this->mEvent); }
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        return ee_screens_see_process(this->mEvent, inForm, inEntity);
    }
    EeScreensSeeEvent* mEvent;
};

class UIControl;
class UIEntity;
class UITextLabel;
class EESETSEScenarioComboBox;

// EESEESliderChange(label, format, scale, offset): the slider's value * scale + offset printed by the format into the
// label (the format is copied)
inline EESEERust* EESEENewSliderChange(UITextLabel* inLabel, const char* inFormat, float inScale, float inOffset)
{
    EESEERust* theEvent = new EESEERust(kSEESliderChange);
    ee_screens_see_slider(theEvent->mEvent, inLabel, inFormat, inScale, inOffset);
    return theEvent;
}

// EESEECheckDisplay's mControls.push_back: a control shown while the check box is checked, else hidden
inline void EESEEPushControl(EESEERust* inEvent, UIControl* inControl)
{
    ee_screens_see_push_control(inEvent->mEvent, inControl);
}

// EESEEUseObjectArea(label, combo box, area control): the combo box refilled with the areas when the area control
// is the one checked, else with the objects (FillScenarioComboBox), its first line picked
inline EESEERust* EESEENewUseObjectArea(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, UIEntity* inAreaControl)
{
    EESEERust* theEvent = new EESEERust(kSEEUseObjectArea);
    ee_screens_see_object_area(theEvent->mEvent, inLabel, inComboBox, inAreaControl);
    return theEvent;
}

// the effect editors' events: kSEECAMGModifyObject (the EESETSETCreateAIMilitaryGroup, its "to add" and "added"
// lists), kSEEMSDCameraScroll (the EESETSETMediaSendDialog, the EESETSEffectControls), kSEESAAAttribute (the
// EESETSETSetAIAttribute, the EESETSEffectControls); each pointer passed as its own class's
inline EESEERust* EESEENewObjects(uint32_t inKind, void* inA, void* inB, void* inC = 0)
{
    EESEERust* theEvent = new EESEERust(inKind);
    ee_screens_see_objects(theEvent->mEvent, inA, inB, inC);
    return theEvent;
}

#endif
