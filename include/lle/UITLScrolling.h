// C:\EEX\Low-Level Engine\User Interface\UITLScrolling.h (RECONSTRUCTION). A word-wrapped text label (label type 1)
// whose lines scroll up pixel by pixel ("credits" style): a start delay, the scroll, an end delay, then trigger
// event 0x3f4. Vtable 0x1009983c (UITextLabel's 37 slots; overrode CopyData, DrawTextA and CheckScrollBar). The
// original's sizeof was 0x170 (the implicit copy ctor 0x10067239; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): its state (the draw lines and
// rectangle, the draw text, the three polls, the offsets) is the Rust label's scrolling part, made by the
// constructor's init call; the label's CopyData, DrawTextA and CheckScrollBar (forwarders) run the scrolling
// label's own in Rust. UITLScrolling.cpp holds the ctor and the forwarders. Requires lle/UITextLabel.h.
#ifndef LLE_UITLSCROLLING_H
#define LLE_UITLSCROLLING_H

#include "lle/UITextLabel.h"

class LLE_API UITLScrolling : public UITextLabel {
public:
    UITLScrolling(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    UITLScrolling(const UITLScrolling&) = delete;
    UITLScrolling& operator=(const UITLScrolling&) = delete;
    unsigned long GetPixelStartingOffset() const;
    unsigned long GetStartingLine() const;
    void PauseScrolling();
    void ResumeScrolling();
    void ScrollText(float inSpeed, long inStartDelay, long inEndDelay, long inStartOffset, long inEndOffset);
    void ScrollText(unsigned long inTime, unsigned long inEndDelay);
    void SetPixelStartingOffset(unsigned long inOffset);
    void SetStartingLine(unsigned long inLine);
    void StopScrolling();
};

#endif
