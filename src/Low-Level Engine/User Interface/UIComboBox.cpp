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

#include "rust/ee_ui.h"

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

// FUNCTION: LLE 0x1006150f
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

// FUNCTION: LLE 0x10061978
// Destroys the combo box and its text box, list box and drop button
UIComboBox::~UIComboBox() { ee_ui_combo_free(this->mRust); }

// FUNCTION: LLE 0x10061a86
// Appends a line to the list, reopening an open list so it resizes
void UIComboBox::AppendLine(UWideString& inLine)
{
    this->SetDirty(true);
    this->GetList()->AppendLine(inLine);
    this->Call(EeUiComboCall::Reopen);
}

// FUNCTION: LLE 0x10061ac3
// Appends several lines to the list, reopening an open list so it resizes
void UIComboBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    this->SetDirty(true);
    this->GetList()->AppendStringArray(inLines);
    this->Call(EeUiComboCall::Reopen);
}

// Header-inline members first emitted here, in class-definition order (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100602e4
// SYNTHETIC: LLE 0x10060308
// SYNTHETIC: LLE 0x10060362
// SYNTHETIC: LLE 0x1006037a
// SYNTHETIC: LLE 0x10060381
// SYNTHETIC: LLE 0x10060388
// SYNTHETIC: LLE 0x1006038f
// SYNTHETIC: LLE 0x10060396
// SYNTHETIC: LLE 0x1006039d
// SYNTHETIC: LLE 0x100603b9
// SYNTHETIC: LLE 0x100603d4
// SYNTHETIC: LLE 0x100603db
// SYNTHETIC: LLE 0x100603f1
// SYNTHETIC: LLE 0x1006040f
// SYNTHETIC: LLE 0x1006042c
// SYNTHETIC: LLE 0x1006044b
// SYNTHETIC: LLE 0x10060468
// SYNTHETIC: LLE 0x100604a7
// SYNTHETIC: LLE 0x100604af
// SYNTHETIC: LLE 0x10060799
// SYNTHETIC: LLE 0x10060b30
// SYNTHETIC: LLE 0x10060b91
// SYNTHETIC: LLE 0x10060cf9
// SYNTHETIC: LLE 0x10060f03
// SYNTHETIC: LLE 0x10060f64
// SYNTHETIC: LLE 0x10060fd7
// SYNTHETIC: LLE 0x1006100b
// SYNTHETIC: LLE 0x10061028
// SYNTHETIC: LLE 0x1006105b
// SYNTHETIC: LLE 0x10061088
// SYNTHETIC: LLE 0x100610e9
// SYNTHETIC: LLE 0x1006110b
// SYNTHETIC: LLE 0x10061112
// SYNTHETIC: LLE 0x10061130
// SYNTHETIC: LLE 0x10061137
// SYNTHETIC: LLE 0x1006113e
// SYNTHETIC: LLE 0x1006114b
// SYNTHETIC: LLE 0x10061152
// SYNTHETIC: LLE 0x10061159
// SYNTHETIC: LLE 0x10061166
// SYNTHETIC: LLE 0x10061178
// SYNTHETIC: LLE 0x1006119a
// SYNTHETIC: LLE 0x100611c5
// SYNTHETIC: LLE 0x100611d7
// SYNTHETIC: LLE 0x100611e9
// SYNTHETIC: LLE 0x100611ff
// SYNTHETIC: LLE 0x100612cb
// SYNTHETIC: LLE 0x1006138e

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100613ef
// SYNTHETIC: LLE 0x100613f9
// SYNTHETIC: LLE 0x10061409
// SYNTHETIC: LLE 0x10061415
// SYNTHETIC: LLE 0x1006141f
// SYNTHETIC: LLE 0x10061429
// SYNTHETIC: LLE 0x10061439
// SYNTHETIC: LLE 0x10061445
// SYNTHETIC: LLE 0x1006144f
// SYNTHETIC: LLE 0x10061459
// SYNTHETIC: LLE 0x10061469
// SYNTHETIC: LLE 0x10061475
// SYNTHETIC: LLE 0x1006147f
// SYNTHETIC: LLE 0x10061489
// SYNTHETIC: LLE 0x10061499
// SYNTHETIC: LLE 0x100614a5
// SYNTHETIC: LLE 0x100614af
// SYNTHETIC: LLE 0x100614b9
// SYNTHETIC: LLE 0x100614c9
// SYNTHETIC: LLE 0x100614d5
// SYNTHETIC: LLE 0x100614df
// SYNTHETIC: LLE 0x100614e9
// SYNTHETIC: LLE 0x100614f9
// SYNTHETIC: LLE 0x10061505

// The vector<UWideString> copy members, UIListBox<UWideString>'s members in declaration order, the vector
// COMDATs, the InsertLine member template and the lower_bound/sort chains
// TEMPLATE: LLE 0x100627d1
// TEMPLATE: LLE 0x1006281b
// TEMPLATE: LLE 0x1006298d
// TEMPLATE: LLE 0x100629a3
// TEMPLATE: LLE 0x10062c18
// TEMPLATE: LLE 0x10062ca9
// TEMPLATE: LLE 0x10062ccf
// TEMPLATE: LLE 0x10062d24
// TEMPLATE: LLE 0x10062dda
// TEMPLATE: LLE 0x10062e18
// TEMPLATE: LLE 0x10062e20
// TEMPLATE: LLE 0x10063079
// TEMPLATE: LLE 0x100630aa
// TEMPLATE: LLE 0x10063138
// TEMPLATE: LLE 0x10063348
// TEMPLATE: LLE 0x1006335a
// TEMPLATE: LLE 0x10063397
// TEMPLATE: LLE 0x100633b1
// TEMPLATE: LLE 0x100633b8
// TEMPLATE: LLE 0x100633d4
// TEMPLATE: LLE 0x100633db
// TEMPLATE: LLE 0x100633e2
// TEMPLATE: LLE 0x100633e9
// TEMPLATE: LLE 0x100633fc
// TEMPLATE: LLE 0x10063403
// TEMPLATE: LLE 0x1006340a
// TEMPLATE: LLE 0x10063464
// TEMPLATE: LLE 0x1006347a
// TEMPLATE: LLE 0x10063489
// TEMPLATE: LLE 0x10063498
// TEMPLATE: LLE 0x100634b5
// TEMPLATE: LLE 0x100634de
// TEMPLATE: LLE 0x100634e9
// TEMPLATE: LLE 0x100634f6
// TEMPLATE: LLE 0x10063513
// TEMPLATE: LLE 0x10063645
// TEMPLATE: LLE 0x1006366d
// TEMPLATE: LLE 0x100636a0
// TEMPLATE: LLE 0x100636e2
// TEMPLATE: LLE 0x100637d9
// TEMPLATE: LLE 0x1006388e
// TEMPLATE: LLE 0x10063899
// TEMPLATE: LLE 0x100638af
// TEMPLATE: LLE 0x100638b6
// TEMPLATE: LLE 0x100638bd
// TEMPLATE: LLE 0x10063951
// TEMPLATE: LLE 0x10063a48
// TEMPLATE: LLE 0x10063b0b
// TEMPLATE: LLE 0x10063bdf
// TEMPLATE: LLE 0x10063c48
// TEMPLATE: LLE 0x10063c9d
// TEMPLATE: LLE 0x10063cdb
// TEMPLATE: LLE 0x10063d38
// TEMPLATE: LLE 0x10063d60
// TEMPLATE: LLE 0x10063f1e
// TEMPLATE: LLE 0x10063f3a
// TEMPLATE: LLE 0x10063f66
// TEMPLATE: LLE 0x10063fda
// TEMPLATE: LLE 0x10064015
// TEMPLATE: LLE 0x1006404e
// TEMPLATE: LLE 0x10064063
// TEMPLATE: LLE 0x1006411f
// TEMPLATE: LLE 0x10064179
// TEMPLATE: LLE 0x100641e4
// TEMPLATE: LLE 0x100644be
// TEMPLATE: LLE 0x100644e9
// TEMPLATE: LLE 0x10064500
// TEMPLATE: LLE 0x1006452c
// TEMPLATE: LLE 0x100645fb
// TEMPLATE: LLE 0x100646b6
// TEMPLATE: LLE 0x100646df
// TEMPLATE: LLE 0x10064703
// TEMPLATE: LLE 0x10064720
// TEMPLATE: LLE 0x1006474c
// TEMPLATE: LLE 0x1006475e
// TEMPLATE: LLE 0x10064796
// TEMPLATE: LLE 0x100647bf
// TEMPLATE: LLE 0x10064898
// TEMPLATE: LLE 0x100648d7
// TEMPLATE: LLE 0x1006491b
// TEMPLATE: LLE 0x100649e2
// TEMPLATE: LLE 0x100649e8
// TEMPLATE: LLE 0x10064a25
// TEMPLATE: LLE 0x10064a64
// TEMPLATE: LLE 0x10064b93
// TEMPLATE: LLE 0x10064be0
// TEMPLATE: LLE 0x10064c0b
// TEMPLATE: LLE 0x10064c68
// TEMPLATE: LLE 0x10064c91
// TEMPLATE: LLE 0x10064cb6
// TEMPLATE: LLE 0x10064ccb
// TEMPLATE: LLE 0x10064d69
// TEMPLATE: LLE 0x10064dd7
// TEMPLATE: LLE 0x10064e09
// TEMPLATE: LLE 0x10064e79
// TEMPLATE: LLE 0x10064ec5
