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
    kSEADeleteUnit = 14,    // a the unit's id (posted only when it exists), or -1: the player b's selection (c: all)
};

// The editor's events whole in Rust (ee_screens editor_events.rs): the hide-UI box, the viewport change, the
// toolbar's quick load and quick save, the trigger screens' slider label, check box display and area/object refill
// and the effect editors' own events (the original's EESEEHideUI, EESEEChangeViewport, EESEEQuickLoad,
// EESEEQuickSave, EESEESliderChange, EESEECheckDisplay, EESEEUseObjectArea, EESETSETCAMGModifyObject,
// EESETSETMSDCameraScrollChange, EESETSETSAAAttributeChange, EESEEUseAreaObject, EESEEMoveAreaObject,
// EESEEFaceAreaObject), and the editor tabs' (EEUIFSEEMapType, EEUIFSEESortUnitList, EESEEChangeStartEpoch,
// EESEEGameUnitLimit). Requires lle/UIEvent.h for EESEERust.
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
    kSEEMapType = 13,           // object: the map tab (EEUIFSEMap)
    kSEESortUnitList = 14,
    kSEEStartEpoch = 15,        // object: the row's end epoch combo box
    kSEEUnitLimit = 16,         // EESEENewUnitLimit
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

// EESEEGameUnitLimit(label, modifier, offset): the unit limit per player into the game page's label 0x162, then
// UIESliderChange's "%g" of the slider's value * modifier + offset into the label (no value maps to a string)
inline EESEERust* EESEENewUnitLimit(UITextLabel* inLabel, float inModifier, long inOffset)
{
    EESEERust* theEvent = new EESEERust(kSEEUnitLimit);
    ee_screens_see_slider(theEvent->mEvent, inLabel, "%g", inModifier, (float)inOffset);
    return theEvent;
}

// the effect editors' events: kSEECAMGModifyObject, kSEEMSDCameraScroll, kSEESAAAttribute (the effect types,
// EeScreensSeEffects of ee_screens editor_effects.rs, and the effect screen's EESETSEffectControls)
inline EESEERust* EESEENewObjects(uint32_t inKind, void* inA, void* inB, void* inC = 0)
{
    EESEERust* theEvent = new EESEERust(inKind);
    ee_screens_see_objects(theEvent->mEvent, inA, inB, inC);
    return theEvent;
}

#endif
