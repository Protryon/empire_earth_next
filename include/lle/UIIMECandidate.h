// C:\EEX\Low-Level Engine\User Interface\UIIMECandidate.h (RECONSTRUCTION). The IME candidate window the UIManager
// draws over the forms: a black box with a bevelled border, one "%2d: " numbered line per candidate of the current
// page and a blue highlight on the selected one. UIIMECandidate.cpp (0x100774b9..0x10077f75) holds the out-of-line
// members. Vtable 0x100999bc (??_E, CopyData, DrawPolygons, DrawTextA, MouseLButtonDown, MouseLButtonUp; the two
// mouse handlers are inline `return keseOK`, ICF-folded at 0x10076d3e). sizeof 0x90 (??_E stride 0x1007069d).
// The implicit copy ctor/operator= (0x1007054f/0x100705ec) and ??_E are emitted by UIForm.cpp, its first includer.
// Requires lle/GERhombus.h, lle/GERectangle.h, <vector>.
#ifndef LLE_UIIMECANDIDATE_H
#define LLE_UIIMECANDIDATE_H

#include <vector>

class GEFont;
class GEText;
class UIManager;


// The IME candidate list handed down from the window procedure (laid out like the Win32 CANDIDATELIST: style at +4,
// count at +8; UIManager::ChangeIMECandidateWindow 0x1007a074). Field names are provisional.
struct UIIMECandidateList {
    unsigned long mSize;                    // +0x00
    unsigned long mStyle;                   // +0x04
    unsigned long mCount;                   // +0x08
    unsigned long mSelection;               // +0x0c
    unsigned long mPageStart;               // +0x10
    unsigned long mPageSize;                // +0x14
    unsigned long mOffset[1];               // +0x18 byte offsets of the (multi-byte) strings from the list start
};

class LLE_API UIIMECandidate {
    friend struct UIIMECandidateRust;       // UIIMECandidate.cpp: the Rust list's walk (ee_ui ime.rs)
public:
    UIIMECandidate(UIManager* inManager, float inHeightRatio, float inWidthRatio, GEFont* inFont);
    virtual ~UIIMECandidate();
    virtual long CopyData();                // slot 0x04
    virtual long DrawPolygons();
    virtual long DrawTextA();
    virtual long MouseLButtonDown(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseLButtonUp(const GE2DPoint& inLocation) { return kErrorOK; }
    void CloseCandidateWindow();
    const GERectangle& GetIMECandidateRectangle() const { return this->mCandidateRectangle; }
    const std::vector<UWideString>& GetIMECandidateStrings() const { return this->mCandidateStrings; }
    GEText* GetIMECandidateTextObject() const { return this->mText; }
    const GERectangle& GetIMECopyCandidateRectangle() const { return this->mCopyCandidateRectangle; }
    void OpenCandidateWindow(const GERectangle& inScreenRectangle, const GERectangle& inControlRectangle);
    void SetCandidateList(const UIIMECandidateList& inList, float inLeft);
private:
    std::vector<UWideString> mCandidateStrings;     // +0x04 the lines of the current page
    std::vector<UWideString> mCopyCandidateStrings; // +0x14 as of the last CopyData (drawn)
    GERectangle mCandidateRectangle;        // +0x24
    GERectangle mCopyCandidateRectangle;    // +0x34
    GERhombus mBackgroundRhombus;           // +0x44 black
    GERhombus mSelectionRhombus;            // +0x5c blue, behind the selected line
    UIManager* mManager;                    // +0x74
    GEText* mText;                          // +0x78 measures the lines (transparent)
    GEText* mCopyText;                      // +0x7c draws the lines
    float mHeightRatio;                     // +0x80 window height as a fraction of the screen's (0.2)
    float mWidthRatio;                      // +0x84 window width as a fraction of the screen's (0.35)
    long mSelection;                        // +0x88 selected line within the page
    long mCopySelection;                    // +0x8c as of the last CopyData; negative: none
};

#endif
