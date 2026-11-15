// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEElevation.h (RECONSTRUCTION; file name inferred).
// The editor's elevation tab (RTTI EEUIFSEElevation > EEUIFSEMainGroup; vtable 0x4451e4; new 0x34 by the form's
// 0x19ef5a; ctor 0x1a098e) and its tool radio buttons' event (RTTI EEUIEElevationTool > UIEvent; vtable 0x4451d0;
// ProcessEvent 0x1a085f before the tab's unit, ??_G 0x1a1232 after the ctor). Member names are provisional. Requires
// ee/EEUIFSEMainGroup.h.
#ifndef EE_EEUIFSEELEVATION_H
#define EE_EEUIFSEELEVATION_H

class EEAScroll;
class EEUIFSEBrushGroup;
class EEUIFScenarioEditor;
class EEUIFSEESElevation;
class EEUIFSEPaintElevation;
class GEFont;
class UIRadioButton;
class UISlider;
class UITextLabel;

struct EeScreensSeElevation;

// whole in Rust (ee_screens editor_elevation.rs): the class holds its object
class EEUIFSEElevation : public EEUIFSEMainGroup {
public:
    EEUIFSEElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long& ioIndex,
                     EEUIFScenarioEditor* inForm, GEFont* inFont);   // 0x1a098e
    virtual ~EEUIFSEElevation();            // +0x00 0x1a1279 (??_G 0x1a125d)
    virtual void Update();                  // +0x04 0x1a12fb (the mouse over the view while the form is current)
    virtual void MouseMove(const GE2DPoint& inLocation);   // +0x24 0x1a1329 (the brush outline at the tile)

    EeScreensSeElevation* mRust;            // the tab (editor_elevation.rs): its controls (ids 0x96 .. 0xa2)
};

#endif
