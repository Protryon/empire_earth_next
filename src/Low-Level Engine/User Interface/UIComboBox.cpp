// C:\EEX\Low-Level Engine\User Interface\UIComboBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100602e4..0x10064ed6. The first includer of UIListBox.h, UIEditBox.h, UILBString.h and UIComboBox.h, so it
// starts with their inline blocks in class-definition order (UWSNoCaseAlphaSort::operator(), UIEditBox's inline
// members + implicit copy members + ??_E, UIListBox<UWideString>'s copy members + ??_E, UILBString's, then
// UIComboBox's), then the U2D key init chains (0x100613ef), the ctor (0x1006150f) and the out-of-line members in
// alphabetical order, the vector<UWideString> copy members, every member of UIListBox<UWideString> in declaration
// order (0x100629a3..), the vector<UWideString> COMDATs, the InsertLine member template (0x1006452c) and the
// lower_bound/sort chains it and Sort() instantiate. UIControl.cpp starts at 0x10064ed6.
//
// UIComboBox is whole in Rust (rust/crates/ee_ui/src/comboobj.rs, R6 ui round 15; its logic is combo.rs): the
// members forward to the Rust object, which calls out through UIComboBox::Rust::Op (UIControl's members, the focus
// highlight, the parts by address, and the combo box's questions and actions, EeUiComboQuery / EeUiComboAct). The
// constructor makes the parts here.
#include <windows.h>
#include <mbstring.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "rust/ui/comboobj.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UIEditBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "UIObjHost.h"

// A nested struct: a member of UIComboBox, so it has the class's friendships (UIControl's, UITextLabel's, UIListBox's)
struct UIComboBox::Rust {
    static EeUiRect R(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
    static GERectangle R(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
    static UIControl* Part(UIComboBox* c, intptr_t inPart) { return c->PartOf((int)inPart); }
    static UILBString* List(UIComboBox* c) { return c->GetList(); }
    static UITextLabel* Label(UIComboBox* c) { return reinterpret_cast<UITextLabel*>(c->PartOf(4)); }
    static UIEditBox* Edit(UIComboBox* c) { return reinterpret_cast<UIEditBox*>(c->PartOf(3)); }
    static intptr_t Query(UIComboBox* c, intptr_t q, intptr_t arg, float* f)
    {
        switch ((EeUiComboQuery)q) {
        case EeUiComboQuery::RectOf: UIObjHost::PutRect(f, Part(c, arg)->GetRectangle()); return 0;
        case EeUiComboQuery::TextBoxBorder: return Part(c, 0)->GetBorderThickness();
        case EeUiComboQuery::LineCount: return List(c)->GetLineCount();
        case EeUiComboQuery::Line: return (intptr_t)(const char*)List(c)->GetData((unsigned long)arg);
        case EeUiComboQuery::LineHeight: return List(c)->GetLineHeight();
        case EeUiComboQuery::ListBorder: return List(c)->GetBorderThickness();
        case EeUiComboQuery::ListScrollBarRect: {
            const UIScrollBar* theBar = List(c)->GetScrollBar();
            if (!theBar)
                return 0;
            UIObjHost::PutRect(f, theBar->GetRectangle());
            return 1;
        }
        case EeUiComboQuery::ListScrollBarCaptured: return List(c)->GetScrollBarCaptured();
        case EeUiComboQuery::ListSelection: {
            unsigned long theSelection = 0;
            long theResult = List(c)->GetCurrentSelection(theSelection);
            memcpy(&f[0], &theSelection, 4);
            return theResult;
        }
        case EeUiComboQuery::WindowHeight: return c->GetRasterizer()->GetWindow(c->GetScreenIndex())->GetHeight();
        case EeUiComboQuery::EditText: return (intptr_t)Edit(c)->GetText(0);
        case EeUiComboQuery::TabbingOn: return c->GetForm()->GetTabbingOn();
        case EeUiComboQuery::TabStop: return c->GetForm()->GetTabStop();
        case EeUiComboQuery::HasMouseCapture: return c->GetHasMouseCapture();
        case EeUiComboQuery::MouseLocation:
            f[0] = c->GetMouseLocation().GetX();
            f[1] = c->GetMouseLocation().GetY();
            return 0;
        case EeUiComboQuery::Key:
            switch (arg) {
            case 0: return UIControl::DownKeyPressed();
            case 1: return UIControl::UpKeyPressed();
            case 2: return UIControl::HomeKeyPressed();
            case 3: return UIControl::EndKeyPressed();
            default: return UIControl::sEnterKey->GetValue() != 0;
            }
        }
        return 0;
    }
    static intptr_t Act(UIComboBox* c, intptr_t inAct, intptr_t b, intptr_t cc, intptr_t d, float* f)
    {
        switch ((EeUiComboAct)inAct) {
        case EeUiComboAct::ListChangeSelection: List(c)->ChangeSelectionIndex((unsigned long)b, (cc & 1) != 0, (cc & 2) != 0); break;
        case EeUiComboAct::ListInsertLine: List(c)->InsertLine(UWideString(reinterpret_cast<const char*>(b)), (unsigned long)cc); break;
        case EeUiComboAct::ListDeleteLine: List(c)->DeleteLine((unsigned long)b); break;
        case EeUiComboAct::ListDeleteAll: List(c)->DeleteAllEntries(); break;
        case EeUiComboAct::ListChangeWidth: List(c)->ChangeWidth((unsigned long)b); break;
        case EeUiComboAct::ShowLine:
            if (Label(c))
                Label(c)->SetText(List(c)->GetData((unsigned long)b), true);
            else
                Edit(c)->SetText(List(c)->GetData((unsigned long)b));
            break;
        case EeUiComboAct::ClearText:
            if (Label(c))
                Label(c)->SetText("", true);
            else
                Edit(c)->SetText("");
            break;
        case EeUiComboAct::DeleteText:
            if (Label(c))
                Label(c)->DeleteLine(0);
            else
                Edit(c)->DeleteText();
            break;
        case EeUiComboAct::PartMouseDown: return Part(c, b)->MouseLButtonDown();
        case EeUiComboAct::PartMouseUp: return Part(c, b)->MouseLButtonUp();
        case EeUiComboAct::PartMouseMove: return Part(c, b)->MouseMove(GE2DPoint(f[0], f[1]));
        case EeUiComboAct::ListWheel: return List(c)->MouseWheelTurned((long)b);
        case EeUiComboAct::PartUpdateKeyboard: return Part(c, b)->UpdateKeyboard();
        case EeUiComboAct::PartTranslatedKey: return Part(c, b)->UpdateTranslatedKey(reinterpret_cast<const char*>(cc), (short)d);
        case EeUiComboAct::PartTabbed:
            if (cc)
                Part(c, b)->TabbedTo();
            else
                Part(c, b)->TabbedFrom();
            break;
        case EeUiComboAct::PartSetRectangle: Part(c, b)->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::PartSetLocation: Part(c, b)->SetLocation(GE2DPoint(f[0], f[1])); break;
        case EeUiComboAct::PartCopyData: return Part(c, b)->CopyData();
        case EeUiComboAct::CopyListColours: {
            GEText* theText = Label(c) ? Label(c)->GetGEText() : Edit(c)->GetGEText();
            GEText* theListText = List(c)->GetGEText();
            theListText->SetFgColor(theText->GetFgColor());
            theListText->SetBgColor(theText->GetBgColor());
            theListText->SetIsTransparent(theText->GetIsTransparent());
            break;
        }
        case EeUiComboAct::BaseCopyData: return c->UIControl::CopyData();
        case EeUiComboAct::SetPopup: c->GetForm()->SetPopup(b ? c : NULL); break;
        case EeUiComboAct::SetDirty: c->SetDirty(true); break;
        case EeUiComboAct::SetRectangle: c->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::BaseSetRectangle: c->UIControl::SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::BaseSetLocation: c->UIControl::SetLocation(GE2DPoint(f[0], f[1])); break;
        case EeUiComboAct::SetAlwaysDirty: c->SetAlwaysDirty(b != 0); break;
        case EeUiComboAct::Fire: c->FireTriggerEvent((unsigned long)b); break;
        case EeUiComboAct::CaptureMouse: c->CaptureMouse(); break;
        case EeUiComboAct::CaptureKeyboard: c->CaptureKeyboard(); break;
        case EeUiComboAct::ReleaseMouse: c->ReleaseCapturedMouse(); break;
        case EeUiComboAct::ReleaseKeyboard: c->ReleaseCapturedKeyboard(); break;
        case EeUiComboAct::OpenList: c->OpenListBox(); break;
        case EeUiComboAct::CloseList: c->CloseListBox(); break;
        case EeUiComboAct::AddString: return c->AddString();
        case EeUiComboAct::ChangeSelection: c->ChangeSelection(); break;
        case EeUiComboAct::ProcessArrow:
            if (b)
                c->ProcessDown();
            else
                c->ProcessUp();
            break;
        }
        return 0;
    }
    static intptr_t Op(void* inCombo, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t cc, intptr_t d, float* f)
    {
        UIComboBox* c = static_cast<UIComboBox*>(inCombo);
        GERhombus* theRhombus = reinterpret_cast<GERhombus*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(c, inOp, a, f, &r) || UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::ComboQuery: return Query(c, a >> 24, a & 0xffffff, f);
        case EeUiOp::ComboAct: return Act(c, a, b, cc, d, f);
        case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus();
        case EeUiOp::RhombusDelete: delete theRhombus; break;
        case EeUiOp::RhombusSetRect: theRhombus->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::RhombusSetColor: theRhombus->SetColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::RhombusDraw: return c->GetRasterizer()->Draw(theRhombus, c->GetScreenIndex());
        case EeUiOp::DrawCountDecrement: c->mDrawCount--; break;
        default: break;
        }
        return 0;
    }
};

// Creates a combo box: a text label (type 1) or edit box, a drop button and a hidden list box showing the first entry
UIComboBox::UIComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                       GEFont* inFont, const GERectangle& inRectangle, unsigned long inMaxListBoxHeight,
                       UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                       unsigned long inBorderThickness)
    : UIControl(kControlTypeComboBox, inId, inRectangle, inForm, inIndex, 0)
{
    GERectangle theRectangle;
    UWideString theEmpty("");
    GEColor theColor;
    UITextLabel* theLabel = NULL;
    UIEditBox* theEdit = NULL;
    UIControl* theTextBox;
    theRectangle = Rust::R(ee_ui_combo_drop_rect(Rust::R(this->GetRectangle()), Rust::R(inRectangle)));
    GEBitmap* theNormal = inForm->GetManager()->CreateBitmapForScaledControl(
        inForm->GetManager()->GetSBBitmapDownNormal(), theRectangle);
    GEBitmap* thePressed = inForm->GetManager()->CreateBitmapForScaledControl(
        inForm->GetManager()->GetSBBitmapDownPressed(), theRectangle);
    UIButton* theDrop = new UIButton(this->GetId(), theNormal, NULL, thePressed, theRectangle, this->GetForm(), this->GetIndex(), 0);
    theNormal->Dereference();
    thePressed->Dereference();
    GERectangle theTextRectangle = Rust::R(ee_ui_combo_text_rect(Rust::R(this->GetRectangle()), Rust::R(inRectangle), theDrop->GetWidth()));
    if (inTextBoxType == 1) {
        theLabel = new UITextLabel(theEmpty, inFont, this->GetId(), theTextRectangle, this->GetForm(), 0, this->GetIndex(),
                                   inBorderThickness);
        theTextBox = theLabel;
    } else {
        theEdit = new UIEditBox(this->GetId(), inFont, inMaxChars, false, theTextRectangle, this->GetForm(),
                                this->GetIndex(), inBorderThickness, false, false);
        theTextBox = theEdit;
    }
    UILBString* theList = new UILBString(
        this->GetId(), inData, inFont, true,
        Rust::R(ee_ui_combo_list_rect(Rust::R(this->GetRectangle()), theTextBox->GetRectangle().GetBottom(), inMaxListBoxHeight)),
        this->GetForm(), this->GetIndex(), inBorderThickness);
    // the focus highlight takes the list's selection colour; the first line is selected when there is one
    theList->GetSelectedBackground()->GetColorPerVertex(theColor, theColor, theColor, theColor);
    float theRgb[3] = {theColor.GetRed(), theColor.GetGreen(), theColor.GetBlue()};
    this->mRust = ee_ui_combo_new(this, UIComboBox::Rust::Op, theLabel, theDrop, theList, theEdit, inTextBoxType,
                                  inMaxListBoxHeight, theList->GetLineCount() > 0 ? 0 : (unsigned long)-1, theRgb);
    theList->ChangeSelectionIndex(0, true, false);
    theList->SetBgOpacity(1.0f);
    theList->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
    theList->SetScrollBarBgOpacity(1.0f);
    theList->SetScrollBarBgColor(GEColor(0.0f, 0.0f, 0.0f));
    if (theList->GetLineCount() > 0) {
        if (theLabel)
            theLabel->SetText(theList->GetData(0), true);
        else
            theEdit->SetText(theList->GetData(0));
    }
    UIControl::SetRectangle(GERectangle(theTextBox->GetRectangle().GetTopLeft(), theDrop->GetRectangle().GetBottomRight()));
}

// Destroys the combo box and its text box, list box and drop button
UIComboBox::~UIComboBox() { ee_ui_combo_free(this->mRust); }

// Appends a line to the list, reopening an open list so it resizes
void UIComboBox::AppendLine(UWideString& inLine)
{
    this->SetDirty(true);
    this->GetList()->AppendLine(inLine);
    this->Call(EeUiComboCall::Reopen);
}

// Appends several lines to the list, reopening an open list so it resizes
void UIComboBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    this->SetDirty(true);
    this->GetList()->AppendStringArray(inLines);
    this->Call(EeUiComboCall::Reopen);
}
