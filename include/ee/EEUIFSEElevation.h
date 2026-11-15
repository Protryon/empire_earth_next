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

class EEUIFSEElevation : public EEUIFSEMainGroup {
public:
    EEUIFSEElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long& ioIndex,
                     EEUIFScenarioEditor* inForm, GEFont* inFont);   // 0x1a098e
    virtual ~EEUIFSEElevation();            // +0x00 0x1a1279 (??_G 0x1a125d)
    virtual void Update();                  // +0x04 0x1a12fb (the mouse over the view while the form is current)
    virtual void MouseMove(const GE2DPoint& inLocation);   // +0x24 0x1a1329 (the brush outline at the tile)

    UIRadioButton* mRaiseButton;            // +0x04 (0x96)
    UIRadioButton* mLowerButton;            // +0x08 (0x97)
    UIRadioButton* mLevelButton;            // +0x0c (0xa0)
    EEUIFSEESElevation* mElevationSlider;   // +0x10 (0x9d)
    long mL14;                              // +0x14
    UISlider* mStrengthSlider;              // +0x18 (0xa2)
    EEUIFSEPaintElevation* mPaint;          // +0x1c (new 0xe4, ctor 0x1a4470; id 0xa1)
    UITextLabel* mStrengthLabel;            // +0x20 (0x9f)
    UITextLabel* mElevationTitle;           // +0x24 (0x98)
    UITextLabel* mStrengthTitle;            // +0x28 (0x99)
    long mL2C;                              // +0x2c
    long mL30;                              // +0x30
};

#endif
