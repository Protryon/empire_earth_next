// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEBrushGroup.h (RECONSTRUCTION; class and header names inferred). The
// editor's brush size radio group: not polymorphic (no RTTI); ctor 0x1a0315 (a rectangle zeroed, the title
// label +0x24, five EEUIFSERBBrush buttons +0x10..+0x20 with brush sizes 0..4). Each button stores its size
// into +0x28 when checked. Only what matched code uses.
#ifndef EE_EEUIFSEBRUSHGROUP_H
#define EE_EEUIFSEBRUSHGROUP_H

class EEUIFSERBBrush;
class UITextLabel;
class GEFont;
class GE2DPoint;
class UIForm;

class EEUIFSEBrushGroup {
public:
    EEUIFSEBrushGroup(unsigned long& ioIndex, const GE2DPoint& inPosition, UIForm* inForm, GEFont* inFont);   // 0x1a0315
    ~EEUIFSEBrushGroup();                   // 0x1a074a, five owned buttons plus title
    // 0x1a0778: the brush outline at tile (inX, inY) (EEUIFSEElevation::MouseMove) (provisional name)
    void SetHighlight(float inX, float inY, bool inIsCentered);
    void SetIsHighlightShown(bool inValue) { this->mIsHighlightShown = inValue; }
    GERectangle mRectangle;                 // +0x00
    EEUIFSERBBrush* mButtons[5];            // +0x10
    UITextLabel* mTitle;                    // +0x24
    unsigned long mBrushSize;                        // +0x28 (EEUIFSERBBrush::Check)
    bool mIsHighlightShown;                 // +0x2c
    bool mIsCentered;                       // +0x2d, half-tile outline flag; overall size remains0x30
};

#endif
