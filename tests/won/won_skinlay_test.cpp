// The skins' layouts (R6 WON round 37): the original MSControlPainters.cpp's, MSControlConstructors.cpp's,
// SkinnedContainers.cpp's, SkinListControls.cpp's, MSEtchedFrameGroup.cpp's and WONSimpleButton.cpp's statements
// (MSComboBox, MSScrollContainer, MSListCtrl, WONComboBox, WONScrollContainer and WONListCtrl Virtual08,
// WONListCtrl's two column headers, the desired sizes of WONComboBox, MSEtchedFrameGroup and WONSimpleButton),
// copied from the C++ before the port with `this` a stand-in receiver, against ee_won's Rust layouts (skinlay.rs)
// through a host that answers the `SkinLay` op as the ported units' host does.
//
// The world: a receiver with its flags, frame and header height, and its parts (the combo's button, edit box and
// list area; the scroll area, the two scrollbars, the header; the etched group's label and contained component; the
// simple button's four images), each part a stand-in component with a place, size, visibility and desired size.
// Every call with an effect (SetPosSize, SetLeftPad, GetDesiredSize, SetColumn, SetColumnItem, SetColumnWidth, the
// header height's store) is traced with its arguments; the reads are not (the stand-ins answer them from the world,
// which the traced calls change). Each of 200,000 sequences builds a random world (parts NULL where the original
// tested for NULL), makes one random call on both and compares the traces and the sizes out. The first 400 are
// rust/crates/ee_won/src/golden_skinlay.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

static std::vector<std::string> sTrace;
static void T(const char* f, ...);
#include <stdarg.h>
static void T(const char* f, ...)
{
    char aLine[200];
    va_list v;
    va_start(v, f);
    vsnprintf(aLine, sizeof(aLine), f, v);
    va_end(v);
    sTrace.push_back(aLine);
}

// ---- the world's stand-ins (the same for the original statements and the Rust's host)
struct Comp {
    int mId, x, y, w, h, dw, dh;
    bool vis;
    int Width() { return w; }
    int Height() { return h; }
    int Left() { return x; }
    int Top() { return y; }
    int Right() { return x + w; }
    bool IsVisible() { return vis; }
    void SetPosSize(int ax, int ay, int aw, int ah) { T("pos %d %d %d %d %d", mId, ax, ay, aw, ah); x = ax; y = ay; w = aw; h = ah; }
    void GetDesiredSize(int& aw, int& ah) { T("desired %d", mId); aw = dw; ah = dh; }
    void SetLeftPad(int n) { T("leftpad %d %d", mId, n); }
    // a list area
    int mCount, mTotal;
    int GetNumItems() { return mCount; }
    int GetTotalWidth() { return mTotal; }
    void SetColumnWidth(int c, int aw, bool fromSize) { T("colwidth %d %d %d %d", mId, c, aw, fromSize ? 1 : 0); }
    // a header
    int mColumnDesired;
    void SetColumn(int c, const void* title, int* desired) { T("column %d %d %d", mId, c, (int)(intptr_t)title); if (desired != NULL) *desired = mColumnDesired; }
    void SetColumnItem(int c, Comp* item) { T("columnitem %d %d %d", mId, c, item ? item->mId : 0); }
};
template <class X> struct P {
    X* p;
    X* get() const { return p; }
    X* operator->() const { return p; }
};
struct Image { unsigned short mWidth, mHeight; };
struct Frame { int mLeft, mTop, mRight, mBottom; };

struct World : Comp {
    bool mDrawFrame, mDraw3DFrame, mButtonInFrame;
    int mHeaderHeight;
    Frame mFrame;
    Comp parts[12];
    P<Comp> mButton, mEdit, mListArea, mHeader, mComponent, mLabel;
    Comp* mArea;
    Comp* mBars[2];
    P<Image> mImages[4];
    Image images[4];
    Comp* GetScrollArea() { return mArea; }
    Comp* GetScrollbar(bool v) { return mBars[v ? 1 : 0]; }
};

// ---- the original statements (`this->` a World)
namespace orig {
#define this t
// MSComboBox::Virtual08 (after ComboBox::Virtual08)
static void MsCombo(World* t)
{
    int topPad=0,bottomPad=0,rightPad=0,leftPad=0;
    if(this->mDrawFrame)
        topPad=bottomPad=rightPad=2;
    int inputHeight=this->Height();
    int buttonHeight=inputHeight-bottomPad-topPad;
    int buttonWidth=this->mButton->Width();
    if(!this->mButton->IsVisible()) buttonWidth=0;
    int inputWidth=this->Width()-buttonWidth-leftPad-rightPad;
    if(this->mButton->IsVisible())
        this->mButton->SetPosSize(inputWidth+leftPad,topPad,buttonWidth,buttonHeight);
    this->mEdit->SetPosSize(leftPad,0,inputWidth,inputHeight);
}
// MSScrollContainer::Virtual08 (after Container::Virtual08)
static void MsScroll(World* t)
{
    if(this->GetScrollArea()==NULL) return;
    bool horzOn=this->GetScrollbar(false)->IsVisible();
    bool vertOn=this->GetScrollbar(true)->IsVisible();
    int xoff=0,yoff=0;
    if(this->mDrawFrame && this->mDraw3DFrame) xoff=yoff=2;
    int scrollbarWidth=this->GetScrollbar(true)->Width();
    int scrollbarHeight=this->GetScrollbar(false)->Height();
    int scrollWidth=this->Width()-xoff*2;
    if(vertOn) scrollWidth-=scrollbarWidth;
    int scrollHeight=this->Height()-yoff*2;
    if(horzOn) scrollHeight-=scrollbarHeight;
    if(vertOn)
        this->GetScrollbar(true)->SetPosSize(this->Width()-scrollbarWidth-xoff,yoff,scrollbarWidth,scrollHeight);
    if(horzOn)
        this->GetScrollbar(false)->SetPosSize(xoff,this->Height()-scrollbarHeight-yoff,scrollWidth,scrollbarHeight);
    this->GetScrollArea()->SetPosSize(xoff,this->mHeaderHeight+yoff,scrollWidth,scrollHeight-this->mHeaderHeight);
}
// MSListCtrl::Virtual08
static void MsList(World* t)
{
    MsScroll(t);
    this->mHeader->SetPosSize(this->GetScrollArea()->Left(),this->GetScrollArea()->Top()-this->mHeaderHeight,
                     this->GetScrollArea()->Width(),this->mHeaderHeight);
}
// WONComboBox::GetDesiredSize
static void WonComboSize(World* t, int &width, int &height)
{
	if(this->mEdit.get()!=NULL)
	{
		this->mEdit->GetDesiredSize(width,height);
		height+=2;
	}
	else
		height = 22;

	if(this->mListArea.get()!=NULL && this->mListArea->GetNumItems()>0)
	{
		width = 10 + this->mListArea->GetTotalWidth();
		if(this->mButton.get()!=NULL)
			width += this->mButton->Width();
	}
	else
		width = 100;
}
// WONComboBox::Virtual08 (after ComboBox::Virtual08)
static void WonCombo(World* t)
{
	int aTopPad = 0, aBottomPad = 0, aRightPad = 0, aLeftPad = 0;
	if(this->mDrawFrame)
	{
		aLeftPad = this->mFrame.mLeft;
		aTopPad = this->mFrame.mTop;
		aBottomPad = this->mFrame.mBottom;
		aRightPad = this->mFrame.mRight;
	}

	int anInputBoxHeight = this->Height(); //mEdit->mHeight;
	int aButtonHeight = anInputBoxHeight-aBottomPad-aTopPad;
	int aButtonWidth = this->mButton->Width();
	if(!this->mButton->IsVisible())
		aButtonWidth = 0;

	int anInputBoxWidth = this->Width()-aButtonWidth-aRightPad;


	this->mEdit->SetLeftPad(aLeftPad + 2);
	this->mEdit->SetPosSize(0,0,anInputBoxWidth,anInputBoxHeight);

	if(!this->mButton->IsVisible())
		return;
	if(this->mButtonInFrame)
		this->mButton->SetPosSize(this->mEdit->Right(),aTopPad,aButtonWidth,aButtonHeight);
	else
		this->mButton->SetPosSize(this->Width()-aButtonWidth,0,aButtonWidth,this->Height());
}
// WONScrollContainer::Virtual08 (after Container::Virtual08)
static void WonScroll(World* t)
{
	if(this->GetScrollArea()==NULL)
		return;

	bool horzOn = this->GetScrollbar(false)->IsVisible();
	bool vertOn = this->GetScrollbar(true)->IsVisible();

	int left = this->mFrame.mLeft, right = this->mFrame.mRight, top = this->mFrame.mTop, bottom = this->mFrame.mBottom;

	int aScrollbarWidth = this->GetScrollbar(true)->Width();
	int aScrollbarHeight = this->GetScrollbar(false)->Height();

	int aScrollWidth = this->Width() - left - right;
	if(vertOn)
		aScrollWidth -= aScrollbarWidth;

	int aScrollHeight = this->Height() - top - bottom;
	if(horzOn)
		aScrollHeight -= aScrollbarHeight;

	if(vertOn)
		this->GetScrollbar(true)->SetPosSize(this->Width()-aScrollbarWidth-right,top,aScrollbarWidth,aScrollHeight);

	if(horzOn)
		this->GetScrollbar(false)->SetPosSize(left,this->Height()-aScrollbarHeight-bottom,aScrollWidth,aScrollbarHeight);

	this->GetScrollArea()->SetPosSize(left,this->mHeaderHeight+top,aScrollWidth,aScrollHeight-this->mHeaderHeight);
}
// WONListCtrl::Virtual08
static void WonList(World* t)
{
	WonScroll(t);

	this->mHeader->SetPosSize(this->GetScrollArea()->Left(),this->GetScrollArea()->Top()-this->mHeaderHeight,this->GetScrollArea()->Width(),this->mHeaderHeight);
}
// WONListCtrl::SetStandarColumnHeader
static void WonListTextHeader(World* t, int theCol, const void* theTitle, int theWidth)
{
	int aDesiredHeight = 0;
	this->mHeader->SetColumn(theCol,theTitle, &aDesiredHeight);

	if(this->mHeaderHeight==0)
	{
		if(aDesiredHeight>0)
			this->mHeaderHeight = aDesiredHeight;
		else
			this->mHeaderHeight = 15;
		T("headerheight %d", this->mHeaderHeight);
	}

	static_cast<Comp*>(this->GetScrollArea())->SetColumnWidth(theCol,theWidth,false);
}
// WONListCtrl::SetColumnHeader
static void WonListHeader(World* t, int theCol, Comp *theHeader, int theWidth)
{
	if(this->mHeaderHeight==0)
	{
		if(theHeader->Height()>0)
			this->mHeaderHeight = theHeader->Height();
		else
			this->mHeaderHeight = 15;
		T("headerheight %d", this->mHeaderHeight);
	}

	this->mHeader->SetColumnItem(theCol,theHeader);
	static_cast<Comp*>(this->GetScrollArea())->SetColumnWidth(theCol,theWidth,false);
}
// MSEtchedFrameGroup::GetDesiredSize
static void EtchedSize(World* t, int& width,int& height) {
    if(this->mComponent.get()!=NULL) {
        width=this->mComponent->Width()+20;
        height=this->mComponent->Height()+this->mLabel->Height()+20;
    } else width=height=20;
}
// WONSimpleButton::GetDesiredSize
static void SimpleButtonSize(World* t, int &width, int &height)
{
	width = height = 0;
	for(int i=0; i<4; i++)
	{
		Image *anImage = this->mImages[i].get();
		if(anImage==NULL)
			continue;
		if(anImage->mWidth>width)
			width = anImage->mWidth;

		if(anImage->mHeight>height)
			height = anImage->mHeight;
	}
}
#undef this
}

// the Rust reads the components' bounds and visibility from compmem.rs's blocks: each read gets one filled from
// the stand-in it is given (every component the Rust names is a Comp)
static EeWonCompMembers sFillBlock;
static EeWonCompMembers* FillBlock(intptr_t c)
{
    memset(&sFillBlock, 0, sizeof(sFillBlock));
    const Comp* x = (const Comp*)c;
    sFillBlock.bounds.x = x->x; sFillBlock.bounds.y = x->y; sFillBlock.bounds.width = x->w; sFillBlock.bounds.height = x->h;
    sFillBlock.bits = x->vis ? 0 : 1;
    return &sFillBlock;
}

// ---- the Rust's host (as the ported units answer `SkinLay`)
typedef EeWonSkinLay L;
static intptr_t Host(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    World* t = (World*)inMe;
    if (op != EeWonGuiOp::SkinLay) { T("bad op %d", (int)op); return 0; }
    Comp* x = (Comp*)b;
    switch ((L)a) {
    case L::Part:
        switch ((EeWonSkinPart)c) {
        case EeWonSkinPart::Button: return (intptr_t)t->mButton.get();
        case EeWonSkinPart::Edit: return (intptr_t)t->mEdit.get();
        case EeWonSkinPart::ListArea: return (intptr_t)t->mListArea.get();
        case EeWonSkinPart::ScrollArea: return (intptr_t)t->GetScrollArea();
        case EeWonSkinPart::VertBar: return (intptr_t)t->GetScrollbar(true);
        case EeWonSkinPart::HorzBar: return (intptr_t)t->GetScrollbar(false);
        case EeWonSkinPart::Header: return (intptr_t)t->mHeader.get();
        case EeWonSkinPart::Label: return (intptr_t)t->mLabel.get();
        case EeWonSkinPart::Contained: return (intptr_t)t->mComponent.get();
        default: T("bad part %d", (int)c); return 0;
        }
    case L::Field:
        switch ((EeWonSkinField)c) {
        case EeWonSkinField::DrawFrame: return t->mDrawFrame;
        case EeWonSkinField::Draw3DFrame: return t->mDraw3DFrame;
        case EeWonSkinField::ButtonInFrame: return t->mButtonInFrame;
        case EeWonSkinField::HeaderHeight: return t->mHeaderHeight;
        case EeWonSkinField::FrameLeft: return t->mFrame.mLeft;
        case EeWonSkinField::FrameTop: return t->mFrame.mTop;
        case EeWonSkinField::FrameRight: return t->mFrame.mRight;
        case EeWonSkinField::FrameBottom: return t->mFrame.mBottom;
        default: T("bad field %d", (int)c); return 0;
        }
    case L::SetHeaderHeight: t->mHeaderHeight = (int)b; T("headerheight %d", t->mHeaderHeight); return 0;
    case L::SetPosSize: { const int* r = (const int*)c; x->SetPosSize(r[0], r[1], r[2], r[3]); return 0; }
    case L::DesiredSize: { int* wh = (int*)c; x->GetDesiredSize(wh[0], wh[1]); return 0; }
    case L::SetLeftPad: x->SetLeftPad((int)c); return 0;
    case L::ListCount: return x->GetNumItems();
    case L::ListTotalWidth: return x->GetTotalWidth();
    case L::SetColumn: t->mHeader->SetColumn((int)b, (const void*)c, (int*)d); return 0;
    case L::SetColumnItem: t->mHeader->SetColumnItem((int)b, (Comp*)c); return 0;
    case L::SetColumnWidth: t->GetScrollArea()->SetColumnWidth((int)b, (int)c, false); return 0;
    case L::Image: {
        int* wh = (int*)c;
        Image* i = t->mImages[b].get();
        if (i == NULL) return 0;
        wh[0] = i->mWidth; wh[1] = i->mHeight;
        return 1;
    }
    default: T("bad lay %d", (int)a); return 0;
    }
}

// ---- a random world, built twice
static int Num()
{
    switch (R(12)) {
    case 0: return 0;
    case 1: return RI(-30, -1);
    case 2: return RI(0x7ffffff0, 0x7fffffff);
    default: return RI(-5, 70);
    }
}
static void MakeWorld(World& w, unsigned seed, int kind)
{
    unsigned keep = sSeed;
    sSeed = seed;
    memset(&w, 0, sizeof(w));
    w.mId = 1; w.x = Num(); w.y = Num(); w.w = Num(); w.h = Num(); w.vis = true;
    w.mDrawFrame = R(2); w.mDraw3DFrame = R(2); w.mButtonInFrame = R(2);
    w.mHeaderHeight = R(2) ? 0 : Num();
    w.mFrame.mLeft = RI(-2, 8); w.mFrame.mTop = RI(-2, 8); w.mFrame.mRight = RI(-2, 8); w.mFrame.mBottom = RI(-2, 8);
    for (int i = 0; i < 12; i++) {
        Comp& c = w.parts[i];
        c.mId = 10 + i; c.x = Num(); c.y = Num(); c.w = Num(); c.h = Num(); c.vis = R(3) != 0;
        c.dw = Num(); c.dh = Num(); c.mCount = R(3) ? RI(0, 4) : RI(-2, 0); c.mTotal = Num();
        c.mColumnDesired = R(3) ? Num() : 0;
    }
    // the original tested these for NULL: WONComboBox's desired size (edit box, list area, button), the scroll
    // area, the etched group's component; the others it dereferenced
    bool nullable = kind == (int)EeWonSkinLayKind::WonComboSize;
    w.mButton.p = nullable && R(4) == 0 ? NULL : &w.parts[0];
    w.mEdit.p = nullable && R(4) == 0 ? NULL : &w.parts[1];
    w.mListArea.p = nullable && R(4) == 0 ? NULL : &w.parts[2];
    bool scroll = kind == (int)EeWonSkinLayKind::MsScroll || kind == (int)EeWonSkinLayKind::WonScroll;
    w.mArea = scroll && R(6) == 0 ? NULL : &w.parts[3];
    w.mBars[1] = &w.parts[4];
    w.mBars[0] = R(8) == 0 ? &w.parts[4] : &w.parts[5];   // now and then one bar for both (the reads see the moves)
    w.mHeader.p = &w.parts[6];
    w.mLabel.p = &w.parts[7];
    w.mComponent.p = R(4) == 0 ? NULL : &w.parts[8];
    for (int i = 0; i < 4; i++) {
        w.images[i].mWidth = (unsigned short)(R(5) == 0 ? 0xffff - R(3) : R(80));
        w.images[i].mHeight = (unsigned short)(R(5) == 0 ? 0xffff - R(3) : R(80));
        w.mImages[i].p = R(3) == 0 ? NULL : &w.images[i];
    }
    sSeed = keep;
}

// the world as one line (the golden's; the Rust tests rebuild it): the receiver, the twelve parts, which parts the
// receiver holds (-1: NULL), the images (-1: NULL)
static std::string WorldText(World& w)
{
    char aBuf[200];
    std::string s;
    snprintf(aBuf, sizeof(aBuf), "%d %d %d %d %d %d %d %d %d %d %d %d", w.x, w.y, w.w, w.h, w.mDrawFrame ? 1 : 0,
             w.mDraw3DFrame ? 1 : 0, w.mButtonInFrame ? 1 : 0, w.mHeaderHeight, w.mFrame.mLeft, w.mFrame.mTop,
             w.mFrame.mRight, w.mFrame.mBottom);
    s += aBuf;
    for (int i = 0; i < 12; i++) {
        Comp& c = w.parts[i];
        snprintf(aBuf, sizeof(aBuf), " / %d %d %d %d %d %d %d %d %d %d", c.x, c.y, c.w, c.h, c.vis ? 1 : 0, c.dw, c.dh,
                 c.mCount, c.mTotal, c.mColumnDesired);
        s += aBuf;
    }
    Comp* held[9] = {w.mButton.p, w.mEdit.p, w.mListArea.p, w.mArea, w.mBars[1], w.mBars[0], w.mHeader.p, w.mLabel.p,
                     w.mComponent.p};
    s += " /";
    for (int i = 0; i < 9; i++) {
        snprintf(aBuf, sizeof(aBuf), " %d", held[i] ? (int)(held[i] - w.parts) : -1);
        s += aBuf;
    }
    s += " /";
    for (int i = 0; i < 4; i++) {
        snprintf(aBuf, sizeof(aBuf), " %d %d", w.mImages[i].p ? (int)w.images[i].mWidth : -1, (int)w.images[i].mHeight);
        s += aBuf;
    }
    return s;
}

static std::string Joined()
{
    std::string s;
    for (size_t i = 0; i < sTrace.size(); i++) { if (i) s += "; "; s += sTrace[i]; }
    sTrace.clear();
    return s;
}

int main(int argc, char** argv)
{
    ee_won_comp_members_resolver(FillBlock);
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    const int kSequences = 200000;
    static const char* kNames[] = {"", "mscombo", "msscroll", "mslist", "woncombosize", "woncombo", "wonscroll",
                                   "wonlist", "wonlisttextheader", "wonlistheader", "etchedsize", "simplebuttonsize"};
    for (int seq = 0; seq < kSequences; seq++) {
        sSeed = 104729u * (unsigned)seq + 3u;
        int kind = 1 + (int)R(11);
        unsigned worldSeed = Rand() * 65536u + Rand();
        static World a, b;
        MakeWorld(a, worldSeed, kind);
        MakeWorld(b, worldSeed, kind);
        int col = RI(-1, 6), width = Num(), inW = Num(), inH = Num();
        int itemIdx = 9 + (int)R(3);
        const void* title = (const void*)(intptr_t)(0x1000 + R(16));
        int oaW = inW, oaH = inH, obW = inW, obH = inH;
        std::string aWorld = aPrint ? WorldText(a) : std::string();
        switch ((EeWonSkinLayKind)kind) {
        case EeWonSkinLayKind::MsCombo: orig::MsCombo(&a); break;
        case EeWonSkinLayKind::MsScroll: orig::MsScroll(&a); break;
        case EeWonSkinLayKind::MsList: orig::MsList(&a); break;
        case EeWonSkinLayKind::WonComboSize: orig::WonComboSize(&a, oaW, oaH); break;
        case EeWonSkinLayKind::WonCombo: orig::WonCombo(&a); break;
        case EeWonSkinLayKind::WonScroll: orig::WonScroll(&a); break;
        case EeWonSkinLayKind::WonList: orig::WonList(&a); break;
        case EeWonSkinLayKind::WonListTextHeader: orig::WonListTextHeader(&a, col, title, width); break;
        case EeWonSkinLayKind::WonListHeader: orig::WonListHeader(&a, col, &a.parts[itemIdx], width); break;
        case EeWonSkinLayKind::EtchedSize: orig::EtchedSize(&a, oaW, oaH); break;
        case EeWonSkinLayKind::SimpleButtonSize: orig::SimpleButtonSize(&a, oaW, oaH); break;
        }
        std::string ta = Joined();
        intptr_t arg = kind == (int)EeWonSkinLayKind::WonListTextHeader ? (intptr_t)title
                     : kind == (int)EeWonSkinLayKind::WonListHeader ? (intptr_t)&b.parts[itemIdx] : 0;
        ee_won_skin_layout((EeWonSkinLayKind)kind, &b, Host, col, arg, width, &obW, &obH);
        std::string tb = Joined();
        char aOut[80];
        snprintf(aOut, sizeof(aOut), " | %d %d", oaW, oaH); ta += aOut;
        snprintf(aOut, sizeof(aOut), " | %d %d", obW, obH); tb += aOut;
        if (ta != tb && ++aFailures < 20) printf("FAIL %d %s\n orig %s\n rust %s\n", seq, kNames[kind], ta.c_str(), tb.c_str());
        if (aPrint && seq < 400)
            printf("%d %d %d %d %d %d %d %d # %s # %s\n", seq, kind, col, (int)(intptr_t)title, width, itemIdx, inW, inH,
                   aWorld.c_str(), ta.c_str());
    }
    if (aPrint) return 0;
    printf("won_skinlay_test: %d sequences, %d failures\n", kSequences, aFailures);
    return aFailures ? 1 : 0;
}
