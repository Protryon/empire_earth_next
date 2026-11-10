// C:\EEX\Low-Level Engine\User Interface\UITriStateCheckBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100805c4..0x10080c07: the header-inline members (dtor, GetType, ButtonUp), the implicit copy ctor/operator= and
// ??_E (this unit is the first includer of UITriStateCheckBox.h), the U2D key init chains (0x100806d4), then the ctor
// and the out-of-line members (alphabetical, the protected CycleCheck last).
#include <windows.h>
#include <stddef.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UITriStateCheckBox.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

// The Rust logic's view of this box (rust/crates/ee_ui/src/button_variants.rs, docs/RUST_PLAN.md R6): the record
// from mTextLabel on, the rectangle and border it reads, and a SetText's label context. A friend of the class.
struct UITriStateCheckBoxRust {
    // a SetText's or SetLocation's label: where the box keeps it, the text and the font
    struct Label {
        UITextLabel** mLabel;
        const UWideString* mText;
        GEFont* mFont;
    };
    EeUiTriStateRefs mRefs;
    UITriStateCheckBoxRust(UITriStateCheckBox* inBox, void* inContext)
    {
        static_assert(offsetof(UITriStateCheckBox, mCheckState) - offsetof(UITriStateCheckBox, mTextLabel) ==
                          offsetof(EeUiTriStateFields, check_state),
                      "ee_ui/src/button_variants.rs TriStateFields");
        static_assert(offsetof(UITriStateCheckBox, mHasText) - offsetof(UITriStateCheckBox, mTextLabel) ==
                          offsetof(EeUiTriStateFields, has_text),
                      "ee_ui/src/button_variants.rs TriStateFields");
        static_assert(sizeof(((UITriStateCheckBox*)0)->mBorderThickness) == sizeof(unsigned long), "ee_ui button_variants_ffi.rs");
        this->mRefs.button = inBox;
        this->mRefs.fields = reinterpret_cast<EeUiTriStateFields*>(&inBox->mTextLabel);
        this->mRefs.rect = reinterpret_cast<const EeUiRect*>(&inBox->mRectangle);
        this->mRefs.border = &inBox->mBorderThickness;
        this->mRefs.ctx = inContext;
    }
    operator const EeUiTriStateRefs*() const { return &this->mRefs; }
    static UITriStateCheckBox* Box(void* inBox) { return static_cast<UITriStateCheckBox*>(inBox); }
    static UITextLabel*& TextLabel(void* inBox) { return Box(inBox)->mTextLabel; }
    static EeUiRect Rect(const GERectangle& inRectangle)
    {
        return EeUiRect{inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
    static EeUiPoint Point(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
    static GE2DPoint Point(const EeUiPoint& inPoint) { return GE2DPoint(inPoint.x, inPoint.y); }
    // the constructor's UIButton rectangle: a box at most 19 pixels square
    static GERectangle BoxRect(const GERectangle& inRectangle)
    {
        EeUiRect theRectangle = Rect(inRectangle);
        return Rect(ee_ui_tristate_rect(&theRectangle));
    }
};

extern "C" EeUiRect ee_uicpp_tristate_label_rect(void* inBox)
{
    return UITriStateCheckBoxRust::Rect(UITriStateCheckBoxRust::TextLabel(inBox)->GetRectangle());
}
extern "C" void ee_uicpp_tristate_label_set_rect(void* inBox, const EeUiRect* inRectangle)
{
    UITriStateCheckBoxRust::TextLabel(inBox)->SetRectangle(UITriStateCheckBoxRust::Rect(*inRectangle));
}
extern "C" void ee_uicpp_tristate_label_set_location(void* inBox, float inX, float inY)
{
    UITriStateCheckBoxRust::TextLabel(inBox)->SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_tristate_label_set_text(void* inBox, void* inLabel)
{
    UITriStateCheckBoxRust::Label* theLabel = static_cast<UITriStateCheckBoxRust::Label*>(inLabel);
    (*theLabel->mLabel)->SetText(*theLabel->mText);   // the public SetText(text, true)
}
extern "C" void ee_uicpp_tristate_label_create(void* inBox, void* inLabel, const EeUiRect* inRectangle)
{
    UITriStateCheckBoxRust::Label* theLabel = static_cast<UITriStateCheckBoxRust::Label*>(inLabel);
    UITriStateCheckBox* theBox = UITriStateCheckBoxRust::Box(inBox);
    *theLabel->mLabel = new UITextLabel(*theLabel->mText, theLabel->mFont, theBox->GetId(), UITriStateCheckBoxRust::Rect(*inRectangle),
                                        theBox->GetForm(), 0, theBox->GetIndex(), 0);
}
extern "C" void ee_uicpp_tristate_label_delete(void* inBox)
{
    delete UITriStateCheckBoxRust::TextLabel(inBox);   // the Rust side clears the pointer
}
extern "C" void ee_uicpp_tristate_base_set_rectangle(void* inBox, const EeUiRect* inRectangle)
{
    UITriStateCheckBoxRust::Box(inBox)->UIControl::SetRectangle(UITriStateCheckBoxRust::Rect(*inRectangle));
}
extern "C" void ee_uicpp_tristate_base_set_location(void* inBox, float inX, float inY)
{
    UITriStateCheckBoxRust::Box(inBox)->UIControl::SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_tristate_set_dirty(void* inBox)
{
    UITriStateCheckBoxRust::Box(inBox)->SetDirty(true);
}
extern "C" void ee_uicpp_tristate_set_check_bitmaps(void* inBox, unsigned long inState)
{
    UITriStateCheckBox* theBox = UITriStateCheckBoxRust::Box(inBox);
    UIManager* theManager = theBox->GetForm()->GetManager();
    switch (inState) {
    case kCheckStateOff:
        theBox->SetNormalBitmap(theManager->GetCheckOff());
        theBox->SetPressedBitmap(theManager->GetCheckDownOff());
        break;
    case kCheckStateOn:
        theBox->SetNormalBitmap(theManager->GetCheckOn());
        theBox->SetPressedBitmap(theManager->GetCheckDownOn());
        break;
    default:
        theBox->SetNormalBitmap(theManager->GetCheckNot());
        theBox->SetPressedBitmap(theManager->GetCheckDownNot());
        break;
    }
}
extern "C" void ee_uicpp_tristate_fire(void* inBox, unsigned long inEvent)
{
    UITriStateCheckBoxRust::Box(inBox)->FireTriggerEvent(inEvent);
}
extern "C" long ee_uicpp_tristate_copy_data(void* inBox, int inWhich)
{
    if (inWhich == 0)
        return UITriStateCheckBoxRust::Box(inBox)->UIButton::CopyData();
    return UITriStateCheckBoxRust::TextLabel(inBox)->CopyData();
}

#define EE_UI_TRI_REFS(label) UITriStateCheckBoxRust(this, label)

// FUNCTION: LLE 0x100807f4
// Creates a three-state check box (off, on, not) at most 19 pixels square with an optional text label beside it
UITriStateCheckBox::UITriStateCheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont,
                                       const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                                       unsigned long inBorderThickness)
    : UIButton(inId, inForm->GetManager()->GetCheckOff(), NULL, inForm->GetManager()->GetCheckDownOff(),
               UITriStateCheckBoxRust::BoxRect(inRectangle), inForm, inIndex, inBorderThickness)
{
    this->mCheckState = 0;
    this->mTextLabel = NULL;
    this->mHasText = false;
    EeUiPoint theOffset;
    if (ee_ui_tristate_bitmap_offset(inRectangle.GetHeight(), inBorderThickness, &theOffset))
        this->SetBitmapDrawOffset(UITriStateCheckBoxRust::Point(theOffset));
    this->SetText(inText, inFont);
}

// FUNCTION: LLE 0x10080916
// Sets the check box width, shrinking the text label to fit beside the box
void UITriStateCheckBox::ChangeWidth(unsigned long inWidth)
{
    ee_ui_tristate_change_width(EE_UI_TRI_REFS(NULL), inWidth);
}

// FUNCTION: LLE 0x100809ab
// Copies the draw state of the box and its label; deletes a label whose text was cleared
long UITriStateCheckBox::CopyData()
{
    return ee_ui_tristate_copy_data(EE_UI_TRI_REFS(NULL));
}

// FUNCTION: LLE 0x100809ec
// Draws the box's polygons, then the label's
long UITriStateCheckBox::DrawPolygons()
{
    long theResult = UIButton::DrawPolygons();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawPolygons();
    return theResult;
}

// FUNCTION: LLE 0x10080a05
// Draws the control text, then the label's text
long UITriStateCheckBox::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x10080a1d
// Sets the check state, shows its bitmaps and fires the checked trigger (an unknown state only stores the value)
void UITriStateCheckBox::SetCheckState(unsigned long inState)
{
    ee_ui_tristate_set_check_state(EE_UI_TRI_REFS(NULL), inState);
}

// FUNCTION: LLE 0x10080ac1
// Moves the check box and its text label
void UITriStateCheckBox::SetLocation(const GE2DPoint& inLocation)
{
    UITriStateCheckBoxRust::Label theLabel = {&this->mTextLabel, NULL, NULL};
    ee_ui_tristate_set_location(EE_UI_TRI_REFS(&theLabel), UITriStateCheckBoxRust::Point(inLocation));
}

// FUNCTION: LLE 0x10080b1c
// Sets the label text, creating the label on first use; empty text hides the label
void UITriStateCheckBox::SetText(const UWideString& inText, GEFont* inFont)
{
    UITriStateCheckBoxRust::Label theLabel = {&this->mTextLabel, &inText, inFont};
    ee_ui_tristate_set_text(EE_UI_TRI_REFS(&theLabel), inText);
}

// FUNCTION: LLE 0x10080be6
// Advances the check state off -> on -> not -> off
void UITriStateCheckBox::CycleCheck()
{
    ee_ui_tristate_cycle(EE_UI_TRI_REFS(NULL));
}

// Header-inline members first emitted here (bodies in lle/UITriStateCheckBox.h)
// FUNCTION: LLE 0x100805c4
// FUNCTION: LLE 0x100805e6
// FUNCTION: LLE 0x100805ea

// Compiler-generated: the implicit copy ctor/operator=, ??_E and the U2D key init chains (names in
// analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100805fb
// SYNTHETIC: LLE 0x1008063a
// SYNTHETIC: LLE 0x10080673
// SYNTHETIC: LLE 0x100806d4
// SYNTHETIC: LLE 0x100806de
// SYNTHETIC: LLE 0x100806ee
// SYNTHETIC: LLE 0x100806fa
// SYNTHETIC: LLE 0x10080704
// SYNTHETIC: LLE 0x1008070e
// SYNTHETIC: LLE 0x1008071e
// SYNTHETIC: LLE 0x1008072a
// SYNTHETIC: LLE 0x10080734
// SYNTHETIC: LLE 0x1008073e
// SYNTHETIC: LLE 0x1008074e
// SYNTHETIC: LLE 0x1008075a
// SYNTHETIC: LLE 0x10080764
// SYNTHETIC: LLE 0x1008076e
// SYNTHETIC: LLE 0x1008077e
// SYNTHETIC: LLE 0x1008078a
// SYNTHETIC: LLE 0x10080794
// SYNTHETIC: LLE 0x1008079e
// SYNTHETIC: LLE 0x100807ae
// SYNTHETIC: LLE 0x100807ba
// SYNTHETIC: LLE 0x100807c4
// SYNTHETIC: LLE 0x100807ce
// SYNTHETIC: LLE 0x100807de
// SYNTHETIC: LLE 0x100807ea
