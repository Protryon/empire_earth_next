// C:\EEX\Low-Level Engine\User Interface\UITLScrolling.h (RECONSTRUCTION). A word-wrapped text label (label type 1)
// whose lines scroll up pixel by pixel ("credits" style): a start delay, the scroll, an end delay, then trigger
// event 0x3f4. UITLScrolling.cpp (0x1007fd7d..0x100805c4) holds the ctor and the out-of-line members; the inline
// members (0x10067211.., the implicit copy members and ??_E 0x100673fa) are emitted first by UIControlsTable.cpp.
// Vtable 0x1009983c (UITextLabel's 37 slots; overrides CopyData, DrawTextA and CheckScrollBar). Layout proven by the
// implicit copy ctor 0x10067239 (sizeof 0x170). Field names are provisional unless an accessor names them.
// Requires lle/UITextLabel.h, lle/TMPoll.h.
#ifndef LLE_UITLSCROLLING_H
#define LLE_UITLSCROLLING_H

#include <vector>
#include "lle/UITextLabel.h"

class TMPoll;

class LLE_API UITLScrolling : public UITextLabel {
    friend struct UITLScrollingRust;        // UITLScrolling.cpp: the Rust scroll's questions (ee_ui scrolling.rs)
public:
    UITLScrolling(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UITLScrolling();
    virtual long CopyData();
    virtual long DrawTextA();
    unsigned long GetPixelStartingOffset() const { return this->mPixelStartingOffset; }  // 0x10067211
    unsigned long GetStartingLine() const { return this->mStartingLine; }           // 0x10067218
    void PauseScrolling();
    void ResumeScrolling();
    void ScrollText(float inSpeed, long inStartDelay, long inEndDelay, long inStartOffset, long inEndOffset);
    void ScrollText(unsigned long inTime, unsigned long inEndDelay);
    void SetPixelStartingOffset(unsigned long inOffset) { this->mPixelStartingOffset = inOffset; }  // 0x1006721f
    void SetStartingLine(unsigned long inLine) { this->mStartingLine = inLine; }               // 0x1006722c
    void StopScrolling();
protected:
    virtual void CheckScrollBar() {}        // ICF-folded (0x1000c16a): no scroll bar
private:
    std::vector<UWideString> mDrawLines;    // +0x124 the visible lines as of the last CopyData (2 spare)
    GERectangle mDrawRectangle;             // +0x134 the text rectangle as of the last CopyData
    GEText* mDrawText;                      // +0x144
    TMPoll* mScrollPoll;                    // +0x148 one pixel per tick
    TMPoll* mStartPoll;                     // +0x14c delay before the scroll starts
    TMPoll* mEndPoll;                       // +0x150 delay after the last line (then event 0x3f4)
    unsigned long mStartingLine;            // +0x154 first line at or below the top edge
    unsigned long mDrawStartingLine;        // +0x158 (CopyData)
    unsigned long mEndOffset;               // +0x15c pixels the last line scrolls before the end delay
    unsigned long mDrawLineCount;           // +0x160 (CopyData)
    long mPixelStartingOffset;              // +0x164 y of mStartingLine
    unsigned long mDrawPixelOffset;         // +0x168 (CopyData)
    bool mScrolling;                        // +0x16c
    bool mUnknown16D;                       // +0x16d (provisional name; copied, never set here)
};

#endif
