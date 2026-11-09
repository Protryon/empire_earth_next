// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.h (RECONSTRUCTION; file name inferred). The in-game
// improvement button (RTTI EEUIBUpgrade > UIButton; vtable 0x449d54; sizeof 0x130; EEUIFMGAttribute derives from
// it). EEUIBUpgrade.cpp is 0x256845..0x257223. Names of the new virtuals and members are provisional (the
// out-of-line members sit in alphabetical order, which fixes where the new virtuals' names sort). Requires
// lle/UIButton.h.
#ifndef EE_EEUIBUPGRADE_H
#define EE_EEUIBUPGRADE_H

class EEUnit;
class UITextLabel;

class EEUIBUpgrade : public UIButton {
public:
    EEUIBUpgrade(unsigned long inId, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight,
                 UIForm* inForm, unsigned long inIndex, bool inScenarioEditor, unsigned long inBorderThickness);
    virtual long CopyData();
    virtual long DrawTextA();
    virtual bool GetHelpString(UWideString& outString) const;
    virtual void SetDirty(bool inDirty);
    void UpdateCost(const UWideString& inText);     // 0x257169
protected:
    virtual void ButtonDown();
    virtual void ButtonUp();
    // the in-class overrides follow the ctor with the ??_G (0x256943, 0x256949)
    virtual long MouseLButtonDown() { return UIButton::MouseLButtonDown(); }
    virtual long MouseLButtonUp() { return UIButton::MouseLButtonUp(); }
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual void MouseOver();
    virtual long MouseRButtonDown();
    virtual long MouseRButtonUp();
public:
    virtual ~EEUIBUpgrade() {}
    // +0x94: the unit the improvement applies to (the selected group's first unit, or the scenario editor's first
    // selected unit) and whether it can be raised; outReason gets the player's reason why not
    virtual bool GetCanUpgrade(EEUnit*& outUnit, UWideString* outReason);
    // +0x98: whether the button is greyed out (the improvement can no longer be raised)
    virtual bool GetDisabled();

    UWideString mHelpText;                  // +0x104
    void SetAttribute(long inAttribute) { this->mAttribute = inAttribute; }  // by value: schedules the store (Fill 0x66b593)
    long mAttribute;                        // +0x118 the improvement (0x10: help string only, 0xc: no cost)
    UITextLabel* mCostLabel;                // +0x11c
    unsigned long mLastMessageTime;         // +0x120 TMGetTime of the last "cannot" chat line
    long mLevel;                            // +0x124 the level last shown
    long mNumQueued;                        // +0x128
    bool mScenarioEditor;                   // +0x12c the button is on the scenario editor's form
    bool mDisabled;                         // +0x12d GetDisabled() as of the last UpdateCost
};

#endif
