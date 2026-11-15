// C:\EEX\Empire Earth\Scenario Editor\EESETSArea.h (RECONSTRUCTION; file name inferred).
// The trigger screen's area sub-screen (RTTI EESETSArea > EESETSBaseObject; vtable 0x44380c, sizeof 0x38, ctor
// 0x17ccf3, ??_G 0x17d07f, dtor 0x17d09b; EESETSArea.cpp 0x17ccf3..0x17daa8). It edits the scenario manager's area
// mL04 + 2 (EEScenarioManager::GetArea; areas 0/1 are the defaults): a rectangle painted with the mouse
// (EESEAPaintArea) or an anchor tile, chosen by the mode combo box (EESEEAreaCheck). The screen is whole in Rust
// (ee_screens editor_area.rs): the class holds its object and forwards. Requires ee/EESETSBaseObject.h, lle/GERectangle.h, lle/GE2DPoint.h.
#ifndef EE_EESETSAREA_H
#define EE_EESETSAREA_H

struct EeScreensSeArea;
class GEFont;
class UIButton;
class UIComboBox;
class UIForm;
class UITextLabel;

class EESETSArea : public EESETSBaseObject {
public:
    EESETSArea(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x17ccf3
    virtual ~EESETSArea();                  // +0x00 0x17d09b
    virtual void Sort();                    // +0x04 0x17d0e0 (sorts the areas)
    virtual void Update();                  // +0x08 0x17d0eb (drags the paint action's corner to the mouse)
    virtual void MoveDown();                // +0x0c 0x17d1d5 (MoveAreaDown)
    virtual void Delete();                  // +0x10 0x17d1f9 (DeleteArea; no return value in the original)
    virtual void Deactivate();              // +0x14 0x17d21d (hides the paint area; no return value)
    virtual void Duplicate();               // +0x18 0x17d22e (DuplicateArea)
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x17d3e6
    virtual void Activate();                // +0x20 0x17d240
    virtual void Store();                   // +0x24 0x17d272 (writes the controls into the area)
    virtual void Hide();                    // +0x28 0x17d3f8 (hides the controls)
    virtual void Load();                    // +0x2c 0x17d41b (shows the area)
    virtual void MoveUp();                  // +0x30 0x17d1e7 (MoveAreaUp)
    virtual void New();                     // +0x34 0x17d5ba (NewArea)
    virtual void Show();                    // +0x38 0x17d667 (shows the controls)
    virtual void MouseMove(const GE2DPoint& inLocation);    // +0x40 0x17d6a9 (the mouse on the map)

    // 0x17d3aa: the area's centre (the rectangle's when +0x34 and +0x35 are set, the point +0x1c when +0x36 is);
    // false if there is none (EESEECenterCameraOnArea)
    bool GetCenter(GE2DPoint& outCenter);
    void SetAreaMode(bool inRectangle);     // 0x17d5c5 (stores +0x34; EESEEAreaCheck passes selection == 0)
    // the screen's state (the editor gate's areastate): the painted rectangle, the anchor point, the flags 1 rectangle
    // mode, 2 a rectangle, 4 a point, 8 a rectangle being painted
    void GetState(GERectangle& outRectangle, GE2DPoint& outPoint, unsigned long& outFlags) const;

    // the slot inWhat (EE_SCREENS_SEAR_*) with the base's selection
    void Call(uint32_t inWhat, int32_t inArgument = 0);

    EeScreensSeArea* mRust;                 // the screen (editor_area.rs): its controls, rectangle, point, paint action
};

#endif
