// Skinned combo box and scroll container: layout and frame painting (file name inferred); the host of the skins'
// layouts (R6 WON round 37: rust/crates/ee_won/src/skinlay.rs lays out MSComboBox, MSScrollContainer, MSListCtrl,
// WONComboBox, WONScrollContainer and WONListCtrl and sizes WONComboBox, MSEtchedFrameGroup and WONSimpleButton)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbySkinFrame.h"
#include "ee/wonapi/WONSkinLayout.h"
namespace WONAPI {

// Returns the combo box's desired size: edit height + 2 (else 22), widest item + 10 + button (else 100) wide
// (skinlay.rs)
void WONComboBox::GetDesiredSize(int &width, int &height)
{
	WONSkinLayout(this,EeWonSkinLayKind::WonComboSize,0,0,0,&width,&height);
}

// Lays out the combo box: the edit fills the width left of the button, the button sits inside or beside the frame
// (skinlay.rs)
void WONComboBox::Virtual08()
{
	ComboBox::Virtual08();
	WONSkinLayout(this,EeWonSkinLayKind::WonCombo);
}

// Paints the combo box and its frame, leaving the button out of the frame when it sits outside it
void WONComboBox::Paint(Graphics& g)
{
	ComboBox::Paint(g);

	// guidraw.rs combo_frame
	EeWonFrame aFrame = WONDrawFrameOf(this->mFrame,this,0,0,0,0);
	bool aDrawFrame = this->mDrawFrame, anInFrame = this->mButtonInFrame;
	bool aVisible = aDrawFrame && !anInFrame ? this->mButton->IsVisible() : true;
	int aButtonWidth = aDrawFrame && !anInFrame && aVisible ? this->mButton->Width() : 0;
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mFrame.mImage.get();
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_combo_frame(aDrawFrame,anInFrame,aVisible,aButtonWidth,&aFrame,o,n); });
	WONDrawRun(g,anOps,aSlots);
}

// Lays out the visible scrollbars inside the frame and gives the rest, below the header, to the scroll area
// (skinlay.rs)
void WONScrollContainer::Virtual08()
{
	Container::Virtual08();
	WONSkinLayout(this,EeWonSkinLayKind::WonScroll);
}

// Paints the container, the corner image between two visible scrollbars (when opaque) and the frame
void WONScrollContainer::Paint(Graphics &g)
{
	Container::Paint(g);

	// the corner image between two visible scrollbars, then the frame (guidraw.rs scroll_container)
	EeWonFrame aFrame = WONDrawFrameOf(this->mFrame,this,0,0,0,0);
	bool aVert = this->GetScrollbar(true)->IsVisible();
	bool aHorz = aVert && this->GetScrollbar(false)->IsVisible();
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mFrame.mImage.get();
	aSlots.mImages[1] = this->mCorner.get();
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_scroll_container(aVert,aHorz,this->mTransparent,this->mCorner.get()!=NULL,
			this->GetScrollbar(true)->Left(),this->GetScrollbar(true)->Top()+this->GetScrollbar(true)->Height(),&aFrame,o,n); });
	WONDrawRun(g,anOps,aSlots);
}
}

// ---- the skins' layouts' host (skinlay.rs EeWonSkinLay): the receiver's parts and fields by the layout's kind, and the
// calls each layout makes
namespace WONAPI {
typedef EeWonSkinLay SL;
typedef EeWonSkinLayKind SK;
typedef EeWonSkinPart SP;
typedef EeWonSkinField SF;
static intptr_t SkinFrameField(const WONFrame& f, intptr_t which)
{
    switch ((SF)which) {
    case SF::FrameLeft: return f.mLeft;
    case SF::FrameTop: return f.mTop;
    case SF::FrameRight: return f.mRight;
    case SF::FrameBottom: return f.mBottom;
    default: return 0;
    }
}
template <class C> static intptr_t SkinScrollPart(C* t, intptr_t which)
{
    switch ((SP)which) {
    case SP::ScrollArea: return (intptr_t)static_cast<Component*>(t->GetScrollArea());
    case SP::VertBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(true));
    case SP::HorzBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(false));
    default: return 0;
    }
}
static intptr_t SkinPartOrField(Component* c, SK k, intptr_t which, bool part)
{
    switch (k) {
    case SK::MsCombo: {
        MSComboBox* t = static_cast<MSComboBox*>(c);
        if (part) return (SP)which == SP::Button ? (intptr_t)static_cast<Component*>(t->mButton.get()) : (intptr_t)static_cast<Component*>(t->mEdit.get());
        return t->mDrawFrame ? 1 : 0;
    }
    case SK::WonCombo: case SK::WonComboSize: {
        WONComboBox* t = static_cast<WONComboBox*>(c);
        if (part) return (SP)which == SP::Button ? (intptr_t)static_cast<Component*>(t->mButton.get())
                       : (SP)which == SP::Edit ? (intptr_t)static_cast<Component*>(t->mEdit.get()) : (intptr_t)static_cast<Component*>(t->mListArea.get());
        return (SF)which == SF::DrawFrame ? t->mDrawFrame : (SF)which == SF::ButtonInFrame ? t->mButtonInFrame : SkinFrameField(t->mFrame, which);
    }
    case SK::MsScroll: case SK::MsList: {
        MSScrollContainer* t = static_cast<MSScrollContainer*>(c);
        if (part) return (SP)which == SP::Header ? (intptr_t)static_cast<Component*>(static_cast<MSListCtrl*>(t)->mHeader.get()) : SkinScrollPart(t, which);
        return (SF)which == SF::DrawFrame ? t->mDrawFrame : (SF)which == SF::Draw3DFrame ? t->mDraw3DFrame : t->mHeaderHeight;
    }
    case SK::WonScroll: case SK::WonList: case SK::WonListTextHeader: case SK::WonListHeader: {
        WONScrollContainer* t = static_cast<WONScrollContainer*>(c);
        if (part) return (SP)which == SP::Header ? (intptr_t)static_cast<Component*>(static_cast<WONListCtrl*>(t)->mHeader.get()) : SkinScrollPart(t, which);
        return (SF)which == SF::HeaderHeight ? t->mHeaderHeight : SkinFrameField(t->mFrame, which);
    }
    case SK::EtchedSize: {
        MSEtchedFrameGroup* t = static_cast<MSEtchedFrameGroup*>(c);
        return !part ? 0 : (SP)which == SP::Label ? (intptr_t)static_cast<Component*>(t->mLabel.get()) : (intptr_t)t->mComponent.get();
    }
    default: return 0;
    }
}
}

using namespace WONAPI;
extern "C" intptr_t ee_won_guicpp_skin_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Component* m = (Component*)inMe;
    Component* x = (Component*)b;
    if (op != EeWonGuiOp::SkinLay) return 0;
    switch ((SL)a) {
    case SL::Part: return SkinPartOrField(m, (SK)b, c, true);
    case SL::Field: return SkinPartOrField(m, (SK)b, c, false);
    case SL::SetHeaderHeight: static_cast<WONScrollContainer*>(m)->mHeaderHeight = (int)b; return 0;
    case SL::Width: return x->Width();
    case SL::Height: return x->Height();
    case SL::Left: return x->Left();
    case SL::Top: return x->Top();
    case SL::Right: return x->Right();
    case SL::Visible: return x->IsVisible() ? 1 : 0;
    case SL::SetPosSize: { const int* r = (const int*)c; x->SetPosSize(r[0], r[1], r[2], r[3]); return 0; }
    case SL::DesiredSize: { int* wh = (int*)c; x->GetDesiredSize(wh[0], wh[1]); return 0; }
    case SL::SetLeftPad: static_cast<InputBox*>(x)->SetLeftPad((int)c); return 0;
    case SL::ListCount: return static_cast<ListArea*>(x)->GetNumItems();
    case SL::ListTotalWidth: return static_cast<ListArea*>(x)->GetTotalWidth();
    case SL::SetColumn: static_cast<WONListCtrl*>(m)->mHeader->SetColumn((int)b, *(const GUIString*)c, (int*)d); return 0;
    case SL::SetColumnItem: static_cast<WONListCtrl*>(m)->mHeader->SetColumnItem((int)b, (Component*)c); return 0;
    case SL::SetColumnWidth: static_cast<MultiListArea*>(static_cast<WONListCtrl*>(m)->GetScrollArea())->SetColumnWidth((int)b, (int)c, false); return 0;
    case SL::Image: {
        Image* i = static_cast<WONSimpleButton*>(m)->mImages[b].get();
        if (i == NULL) return 0;
        ((int*)c)[0] = i->mWidth; ((int*)c)[1] = i->mHeight;
        return 1;
    }
    default: return 0;
    }
}
