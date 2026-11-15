// BackgroundGrabber as a whole Rust object (R6 WON round 37): the original BackgroundGrabber.cpp's statements, copied
// from the C++ before the port with stand-ins for the component, its window, the Graphics, the native and raw images
// and the debug window, against ee_won's Rust grabber (grabobj.rs) through a host that makes the same calls on the
// same stand-ins as the ported unit's host (BackgroundGrabber.cpp ee_won_guicpp_grab_call).
//
// The world: a component (size, its point on the root, flags DebugBG and Translucent), its window (images it makes,
// numbered; its offscreen image; its screen corner), a Graphics (clip rectangle, blending or not) and the debug window.
// Every call on them is traced (images by number); a raw image is the background image's pixels made from its number
// and a generation, and its draw traces the hash of its pixels after the tint. Each of 20,000 sequences makes a grabber
// on each side and runs 40 random calls (Paint, PrePaint, Invalidate, Clear, SetTranslucentColor with colours of every
// sign and levels -1..9, SizeChanged, NeedDrawBG) between random changes of the world; after each the traces and the
// state the root container reads (ee_won_grab_read against the original's members) are compared. The first 300
// sequences are rust/crates/ee_won/src/golden_grabobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

typedef uint32_t DWORD;
static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

static std::vector<std::string> sTrace;
static void T(const char* f, ...)
{
    char aLine[200];
    va_list v;
    va_start(v, f);
    vsnprintf(aLine, sizeof(aLine), f, v);
    va_end(v);
    sTrace.push_back(aLine);
}

// ---- the stand-ins (one world per side)
enum { ComponentFlag_Translucent = 0x200, ComponentFlag_DebugBG = 0x20000 };
struct WONRectangle {
    int x, y, width, height;
    WONRectangle(int ax = 0, int ay = 0, int w = 0, int h = 0) : x(ax), y(ay), width(w), height(h) {}
};
template <class X> struct SmartPtr {
    X* p;
    SmartPtr(X* a = NULL) : p(a) {}
    X* get() const { return p; }
    X* operator->() const { return p; }
};
struct RawImage32 {
    int mWidth, mHeight;
    DWORD* mImageData;
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
};
static uint64_t Fnv(const DWORD* p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n * 4; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
struct NativeImage;
struct ImgGraphics {
    NativeImage* mOwner;
    void DrawImage(NativeImage* i, int x, int y);
};
struct NativeImage {
    int mId, mWidth, mHeight, mGeneration;
    ImgGraphics mGraphics;
    std::vector<RawImage32*> mRaws;
    unsigned short GetWidth() { return (unsigned short)mWidth; }
    unsigned short GetHeight() { return (unsigned short)mHeight; }
    ImgGraphics& GetGraphics() { mGraphics.mOwner = this; return mGraphics; }
    SmartPtr<RawImage32> GetRaw()
    {
        RawImage32* r = new RawImage32;
        r->mWidth = mWidth; r->mHeight = mHeight;
        r->mImageData = new DWORD[(size_t)(mWidth * mHeight > 0 ? mWidth * mHeight : 1)];
        unsigned s = (unsigned)mId * 2654435761u + (unsigned)mGeneration * 40503u;
        for (int i = 0; i < mWidth * mHeight; i++) { s = s * 1103515245u + 12345u; r->mImageData[i] = s ^ (s << 13); }
        T("getraw %d %d %d", mId, mWidth, mHeight);
        mRaws.push_back(r);
        return r;
    }
};
void ImgGraphics::DrawImage(NativeImage* i, int x, int y)
{
    T("bgdraw %d from %d at %d %d", mOwner->mId, i ? i->mId : 0, x, y);
    mOwner->mGeneration++;
}
struct Graphics {
    WONRectangle mClipRect;
    bool mBlend;
    void DrawImage(NativeImage* i, int x, int y) { T("draw %d at %d %d", i ? i->mId : 0, x, y); }
    void DrawImage(RawImage32* r, int x, int y)
    {
        T("drawraw %d %d at %d %d hash %016llx", r->mWidth, r->mHeight, x, y,
          (unsigned long long)Fnv(r->mImageData, (size_t)(r->mWidth * r->mHeight)));
    }
    bool BlendSupported() { return mBlend; }
    void SetColor(DWORD c) { T("setcolor %x", (unsigned)c); }
    void FillRectBlend(int x, int y, int w, int h, int a) { T("blend %d %d %d %d %d", x, y, w, h, a); }
};
struct Component;
struct DebugRoot {
    void RemoveAllChildren() { T("debug removeall"); }
    void Invalidate() { T("debug invalidate"); }
};
struct ImageComponent {
    NativeImage* mImage;
    ImageComponent(NativeImage* i) : mImage(i) {}
};
struct Window;
struct DebugWindow {
    SmartPtr<DebugRoot> mRoot;
    DebugRoot mRootObj;
    DebugWindow() : mRoot(&mRootObj) {}
    void SetParent(Window* w) { T("debug parent %d", w ? 1 : 0); }
    void SetStyle(int s) { T("debug style %x", s); }
    void Create(const WONRectangle& r) { T("debug create %d %d %d %d", r.x, r.y, r.width, r.height); }
    void AddComponent(ImageComponent* c) { T("debug add %d", c->mImage ? c->mImage->mId : 0); delete c; }
    void SetPosSize(int x, int y, int w, int h) { T("debug pos %d %d %d %d", x, y, w, h); }
};
struct World;
struct Window {
    World* mWorld;
    SmartPtr<NativeImage> CreateImage(int w, int h);
    NativeImage* GetOffscreenImage();
    void GetRect(WONRectangle& r, bool);
};
struct Component {
    World* mWorld;
    int Width();
    int Height();
    void LocalToRoot(int& x, int& y);
    void RootToChild(int& x, int& y);
    bool ComponentFlagSet(int f);
    Window* GetRoot();
};
struct World {
    int w, h, rootX, rootY, flags, screenX, screenY, nextImage;
    Window window;
    Component comp;
    NativeImage offscreen;
    std::vector<NativeImage*> images;
    std::vector<DebugWindow*> debugs;
    Graphics g;
};
SmartPtr<NativeImage> Window::CreateImage(int w, int h)
{
    NativeImage* i = new NativeImage;
    i->mId = ++mWorld->nextImage; i->mWidth = w; i->mHeight = h; i->mGeneration = 0;
    T("createimage %d %d %d", i->mId, w, h);
    mWorld->images.push_back(i);
    return i;
}
NativeImage* Window::GetOffscreenImage() { T("offscreen"); return &mWorld->offscreen; }
void Window::GetRect(WONRectangle& r, bool) { r = WONRectangle(mWorld->screenX, mWorld->screenY, 640, 480); }
int Component::Width() { return mWorld->w; }
int Component::Height() { return mWorld->h; }
void Component::LocalToRoot(int& x, int& y) { x += mWorld->rootX; y += mWorld->rootY; }
void Component::RootToChild(int& x, int& y) { x -= mWorld->rootX; y -= mWorld->rootY; }
bool Component::ComponentFlagSet(int f) { return (mWorld->flags & f) != 0; }
Window* Component::GetRoot() { return &mWorld->window; }
static World* sCreatingWorld;
static SmartPtr<DebugWindow> CreateNativeWindow()
{
    DebugWindow* d = new DebugWindow;
    sCreatingWorld->debugs.push_back(d);
    T("debug new");
    return d;
}

// ---- the original statements (BackgroundGrabber.cpp before the port; `Graphics&` the stand-in)
namespace orig {
struct BackgroundGrabber {
    SmartPtr<NativeImage> mBGImage;
    int mImageX, mImageY;
    WONRectangle mClipRect;
    bool mNeedPutBG, mHaveBG;
    int mTranslucentColor, mTranslucentLevel;
    SmartPtr<DebugWindow> mDebugWindow;
    unsigned long long mImageOrder;
    static unsigned long long mImageOrderCounter;
    BackgroundGrabber();
    void Clear();
    void PrePaint(Component*, Graphics&);
    void SetTranslucentColor(int, int);
    void Paint(Component*, Graphics&);
    void Invalidate(Component*);
    void SizeChanged(Component*);
    bool NeedDrawBG() const { return this->mHaveBG && this->mNeedPutBG; }
};
unsigned long long BackgroundGrabber::mImageOrderCounter = 0;

BackgroundGrabber::BackgroundGrabber()
{
    this->mImageX = this->mImageY = 0;
    this->mNeedPutBG = false;
    this->mHaveBG = false;
    this->mTranslucentColor = 0;
    this->mTranslucentLevel = 4;
}

void BackgroundGrabber::Clear()
{
    this->mHaveBG = this->mNeedPutBG = false;
}

void BackgroundGrabber::PrePaint(Component* theComponent, Graphics& g)
{
    if(!this->mHaveBG || !this->mNeedPutBG)
        return;
    int x = 0, y = 0;
    theComponent->LocalToRoot(x,y);
    g.DrawImage(this->mBGImage.get(),this->mImageX-x,this->mImageY-y);
    this->mHaveBG = false;
    this->mNeedPutBG = false;
}

void BackgroundGrabber::SetTranslucentColor(int theColor,int theLevel)
{
    this->mTranslucentLevel = theLevel;
    this->mTranslucentColor = theColor;
}

#define COLOR_1_8(c) (((c)>>3)&0xff1f1f1f)
#define COLOR_2_8(c) (((c)>>2)&0xff3f3f3f)
#define COLOR_3_8(c) (COLOR_1_8(c)+COLOR_2_8(c))
#define COLOR_4_8(c) (((c)>>1)&0xff7f7f7f)
#define COLOR_5_8(c) (COLOR_4_8(c)+COLOR_1_8(c))
#define COLOR_6_8(c) (COLOR_4_8(c)+COLOR_2_8(c))
#define COLOR_7_8(c) (COLOR_4_8(c)+COLOR_3_8(c))

static int GetColor8th(int theColor, int theNumerator)
{
	switch(theNumerator)
	{
		case 1: return COLOR_1_8(theColor);
		case 2: return COLOR_2_8(theColor);
		case 3: return COLOR_3_8(theColor);
		case 4: return COLOR_4_8(theColor);
		case 5: return COLOR_5_8(theColor);
		case 6: return COLOR_6_8(theColor);
		case 7: return COLOR_7_8(theColor);
	}

	return 0;
}

void BackgroundGrabber::Paint(Component *theComponent, Graphics &g)
{
	if(this->mHaveBG)
		return;
	this->mClipRect = g.mClipRect;
	this->mImageOrder = BackgroundGrabber::mImageOrderCounter++;
	int aWidth = theComponent->Width();
	int aHeight = theComponent->Height();
	if(this->mBGImage.get()==NULL || aWidth!=this->mBGImage->GetWidth() || aHeight!=this->mBGImage->GetHeight())
	{
		Window *aWindow = theComponent->GetRoot();
		this->mBGImage = aWindow->CreateImage(aWidth, aHeight);

		if(theComponent->ComponentFlagSet(ComponentFlag_DebugBG) && this->mDebugWindow.get()==NULL)
		{
			this->mDebugWindow = CreateNativeWindow();
			this->mDebugWindow->SetParent(theComponent->GetRoot());
			this->mDebugWindow->SetStyle(0x40);
			WONRectangle aRect(0,0,this->mBGImage->GetWidth(),this->mBGImage->GetHeight());
			this->mDebugWindow->Create(aRect);
		}

		if(this->mDebugWindow.get()!=NULL)
		{
			this->mDebugWindow->mRoot.get()->RemoveAllChildren();
			this->mDebugWindow->AddComponent(new ImageComponent(this->mBGImage.get()));
			int x=0,y=0;
			theComponent->LocalToRoot(x,y);
			WONRectangle aScreenRect;
			theComponent->GetRoot()->GetRect(aScreenRect,false);
			this->mDebugWindow->SetPosSize(x+aScreenRect.x,y+aScreenRect.y,-1,-1);
		}

	}

	NativeImage *anImage = theComponent->GetRoot()->GetOffscreenImage();
	int x = 0, y = 0;
	theComponent->RootToChild(x,y);
	this->mBGImage->GetGraphics().DrawImage(anImage, x, y);
	this->mImageX = 0;//theComponent->Left();
	this->mImageY = 0;//theComponent->Top();
	theComponent->LocalToRoot(this->mImageX,this->mImageY);

	if(this->mDebugWindow.get()!=NULL)
		this->mDebugWindow->mRoot.get()->Invalidate();

	this->mHaveBG = true;

	if(theComponent->ComponentFlagSet(ComponentFlag_Translucent))
	{

		int aColor = GetColor8th(this->mTranslucentColor,8-this->mTranslucentLevel);
		if(g.BlendSupported())
		{
			g.SetColor(this->mTranslucentColor);
			g.FillRectBlend(0,0,theComponent->Width(),theComponent->Height(),(8-this->mTranslucentLevel)*255/8);
			return;
		}

		SmartPtr<RawImage32> aRawImage = this->mBGImage->GetRaw();
		int aNumPixels = aRawImage->GetWidth()*aRawImage->GetHeight();
		DWORD *aPixels =aRawImage->mImageData;
		DWORD *anEndPixels = aPixels+aNumPixels;
		switch(this->mTranslucentLevel)
		{
		case 1:
			do
			{
				*aPixels = COLOR_1_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 2:
			do
			{
				*aPixels = COLOR_2_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 3:
			do
			{
				*aPixels = COLOR_3_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 4:
			do
			{
				*aPixels = COLOR_4_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 5:
			do
			{
				*aPixels = COLOR_5_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 6:
			do
			{
				*aPixels = COLOR_6_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		case 7:
			do
			{
				*aPixels = COLOR_7_8(*aPixels) + aColor;
			} while(++aPixels!=anEndPixels);
			break;

		}
		g.DrawImage(aRawImage.get(),0,0);
	}
}

void BackgroundGrabber::Invalidate(Component *theComponent)
{
	if(this->mHaveBG)
		this->mNeedPutBG = true;
}

void BackgroundGrabber::SizeChanged(Component* theComponent)
{
}
}

// ---- the ported grabber: its C++ members and its host (BackgroundGrabber.cpp's, over the stand-ins)
struct PortGrabber {
    SmartPtr<NativeImage> mBGImage;
    SmartPtr<DebugWindow> mDebugWindow;
    EeWonGrabber* mRust;
};
typedef EeWonGrab Gb;
static intptr_t PortHost(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    PortGrabber* t = (PortGrabber*)inMe;
    Component* comp = (Component*)b;
    Graphics* g = (Graphics*)b;
    if (op != EeWonGuiOp::Grab) { T("bad op %d", (int)op); return 0; }
    switch ((Gb)a) {
    case Gb::LocalToRoot: comp->LocalToRoot(((int*)c)[0], ((int*)c)[1]); return 0;
    case Gb::DrawImage: g->DrawImage(t->mBGImage.get(), (int)c, (int)d); return 0;
    case Gb::Clip: {
        int* r = (int*)c;
        r[0] = g->mClipRect.x; r[1] = g->mClipRect.y; r[2] = g->mClipRect.width; r[3] = g->mClipRect.height;
        return 0;
    }
    case Gb::CompSize: ((int*)c)[0] = comp->Width(); ((int*)c)[1] = comp->Height(); return 0;
    case Gb::ImageSize:
        if (t->mBGImage.get() == NULL)
            return 0;
        ((int*)c)[0] = t->mBGImage->GetWidth(); ((int*)c)[1] = t->mBGImage->GetHeight();
        return 1;
    case Gb::NewImage: t->mBGImage = comp->GetRoot()->CreateImage((int)c, (int)d); return 0;
    case Gb::FlagSet: return comp->ComponentFlagSet((int)c) ? 1 : 0;
    case Gb::DebugExists: return t->mDebugWindow.get() != NULL ? 1 : 0;
    case Gb::DebugCreate: {
        t->mDebugWindow = CreateNativeWindow();
        t->mDebugWindow->SetParent(comp->GetRoot());
        t->mDebugWindow->SetStyle(0x40);
        WONRectangle aRect(0, 0, t->mBGImage->GetWidth(), t->mBGImage->GetHeight());
        t->mDebugWindow->Create(aRect);
        return 0;
    }
    case Gb::DebugShow: {
        t->mDebugWindow->mRoot.get()->RemoveAllChildren();
        t->mDebugWindow->AddComponent(new ImageComponent(t->mBGImage.get()));
        int x = 0, y = 0;
        comp->LocalToRoot(x, y);
        WONRectangle aScreenRect;
        comp->GetRoot()->GetRect(aScreenRect, false);
        t->mDebugWindow->SetPosSize(x + aScreenRect.x, y + aScreenRect.y, -1, -1);
        return 0;
    }
    case Gb::DebugInvalidate: t->mDebugWindow->mRoot.get()->Invalidate(); return 0;
    case Gb::Capture: {
        NativeImage* anImage = comp->GetRoot()->GetOffscreenImage();
        int x = 0, y = 0;
        comp->RootToChild(x, y);
        t->mBGImage->GetGraphics().DrawImage(anImage, x, y);
        return 0;
    }
    case Gb::BlendSupported: return g->BlendSupported() ? 1 : 0;
    case Gb::Blend: {
        const int* p = (const int*)d;
        g->SetColor((int)c);
        g->FillRectBlend(0, 0, p[0], p[1], p[2]);
        return 0;
    }
    case Gb::Tint: {
        SmartPtr<RawImage32> aRawImage = t->mBGImage->GetRaw();
        ee_won_grab_tint(aRawImage->mImageData, (size_t)aRawImage->GetWidth() * aRawImage->GetHeight(), (int)c, (int)d);
        g->DrawImage(aRawImage.get(), 0, 0);
        return 0;
    }
    default: T("bad grab %d", (int)a); return 0;
    }
}

static void Clean(World& w)
{
    for (size_t i = 0; i < w.images.size(); i++) {
        for (size_t k = 0; k < w.images[i]->mRaws.size(); k++) { delete[] w.images[i]->mRaws[k]->mImageData; delete w.images[i]->mRaws[k]; }
        delete w.images[i];
    }
    for (size_t i = 0; i < w.debugs.size(); i++) delete w.debugs[i];
    w.images.clear();
    w.debugs.clear();
}
static std::string Joined()
{
    std::string s;
    for (size_t i = 0; i < sTrace.size(); i++) { if (i) s += "; "; s += sTrace[i]; }
    sTrace.clear();
    return s;
}
static std::string State(int need, int order, int x, int y, int cx, int cy, int cw, int ch)
{
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "need %d order %d at %d %d clip %d %d %d %d", need, order, x, y, cx, cy, cw, ch);
    return aLine;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    long long aChecks = 0;
    const int kSequences = 20000;
    static World A, B;
    for (int seq = 0; seq < kSequences; seq++) {
        sSeed = 6151u * (unsigned)seq + 29u;
        bool aShow = aPrint && seq < 300;
        A.window.mWorld = &A; A.comp.mWorld = &A; B.window.mWorld = &B; B.comp.mWorld = &B;
        A.nextImage = B.nextImage = 0;
        A.offscreen.mId = B.offscreen.mId = 999;
        orig::BackgroundGrabber* oa = new orig::BackgroundGrabber;
        oa->mImageOrder = 0;   // the original left it unset until the first capture (nothing read it before)
        PortGrabber* pb = new PortGrabber;
        pb->mRust = ee_won_grab_new(pb, PortHost);
        if (aShow) printf("S %d\n", seq);
        for (int step = 0; step < 40; step++) {
            // the world changes now and then (the same on both sides)
            if (step == 0 || R(3) == 0) {
                int w = R(6) == 0 ? 0 : RI(1, 12), h = R(6) == 0 ? 0 : RI(1, 12);
                if (R(2)) { A.w = B.w = w; A.h = B.h = h; }
                A.rootX = B.rootX = RI(-20, 60); A.rootY = B.rootY = RI(-20, 60);
                A.flags = B.flags = (R(3) == 0 ? ComponentFlag_DebugBG : 0) | (R(2) ? ComponentFlag_Translucent : 0) | (int)R(4);
                A.screenX = B.screenX = RI(-5, 900); A.screenY = B.screenY = RI(-5, 700);
                A.g.mClipRect = B.g.mClipRect = WONRectangle(RI(-5, 30), RI(-5, 30), RI(-5, 40), RI(-5, 40));
                A.g.mBlend = B.g.mBlend = R(2) != 0;
                char aLine[200];
                snprintf(aLine, sizeof(aLine), "W %d %d %d %d %x %d %d %d %d %d %d %d", A.w, A.h, A.rootX, A.rootY, A.flags,
                         A.screenX, A.screenY, A.g.mClipRect.x, A.g.mClipRect.y, A.g.mClipRect.width, A.g.mClipRect.height,
                         A.g.mBlend ? 1 : 0);
                if (aShow) printf("%s\n", aLine);
            }
            unsigned which = R(9);
            int ca = 0, cb = 0;
            const char* name = "";
            if (which == 0 || which == 1) {
                // the original's tint never ended over 0 pixels: a translucent capture without blending needs pixels
                bool hang = (A.flags & ComponentFlag_Translucent) && !A.g.mBlend && A.w * A.h == 0;
                if (hang) { step--; continue; }
                name = "paint";
                sCreatingWorld = &A; oa->Paint(&A.comp, A.g);
                std::string ta = Joined();
                sCreatingWorld = &B; ee_won_grab_call(pb->mRust, EeWonGrabCall::Paint, (intptr_t)&B.comp, (intptr_t)&B.g);
                std::string tb = Joined();
                aChecks++;
                if (ta != tb && ++aFailures < 20) printf("FAIL %d.%d paint\n orig %s\n rust %s\n", seq, step, ta.c_str(), tb.c_str());
                if (aShow) printf("C paint | %s\n", ta.c_str());
            } else {
                std::string ta, tb;
                switch (which) {
                case 2: name = "prepaint"; oa->PrePaint(&A.comp, A.g); ta = Joined();
                    ee_won_grab_call(pb->mRust, EeWonGrabCall::PrePaint, (intptr_t)&B.comp, (intptr_t)&B.g); tb = Joined(); break;
                case 3: name = "invalidate"; oa->Invalidate(&A.comp); ta = Joined();
                    ee_won_grab_call(pb->mRust, EeWonGrabCall::Invalidate, (intptr_t)&B.comp, 0); tb = Joined(); break;
                case 4: name = "clear"; oa->Clear(); ta = Joined();
                    ee_won_grab_call(pb->mRust, EeWonGrabCall::Clear, 0, 0); tb = Joined(); break;
                case 5: {
                    name = "translucent";
                    ca = R(4) == 0 ? (int)(R(0x8000) | (R(0x8000) << 15) | (R(4) << 30)) : (int)(R(0x1000000));
                    cb = RI(-1, 9);
                    oa->SetTranslucentColor(ca, cb); ta = Joined();
                    ee_won_grab_call(pb->mRust, EeWonGrabCall::SetTranslucentColor, ca, cb); tb = Joined();
                    break;
                }
                case 6: name = "sizechanged"; oa->SizeChanged(&A.comp); ta = Joined();
                    ee_won_grab_call(pb->mRust, EeWonGrabCall::SizeChanged, (intptr_t)&B.comp, 0); tb = Joined(); break;
                default: {
                    name = "needdraw";
                    char x[20];
                    snprintf(x, sizeof(x), "%d", oa->NeedDrawBG() ? 1 : 0); ta = x;
                    snprintf(x, sizeof(x), "%d", (int)ee_won_grab_call(pb->mRust, EeWonGrabCall::NeedDrawBG, 0, 0)); tb = x;
                    break;
                }
                }
                aChecks++;
                if (ta != tb && ++aFailures < 20) printf("FAIL %d.%d %s\n orig %s\n rust %s\n", seq, step, name, ta.c_str(), tb.c_str());
                if (aShow) printf("C %s %d %d | %s\n", name, ca, cb, ta.c_str());
            }
            // the state the root container reads
            EeWonRootGrab q;
            memset(&q, 0, sizeof(q));
            ee_won_grab_read(pb->mRust, &q);
            std::string sa = State(oa->NeedDrawBG() ? 1 : 0, (int)oa->mImageOrder, oa->mImageX, oa->mImageY, oa->mClipRect.x,
                                   oa->mClipRect.y, oa->mClipRect.width, oa->mClipRect.height);
            std::string sb = State(q.need, q.order, q.x, q.y, q.clip.x, q.clip.y, q.clip.w, q.clip.h);
            aChecks++;
            if (sa != sb && ++aFailures < 20) printf("FAIL %d.%d %s state\n orig %s\n rust %s\n", seq, step, name, sa.c_str(), sb.c_str());
            if (aShow) printf("R %s\n", sa.c_str());
        }
        ee_won_grab_free(pb->mRust);
        delete pb;
        delete oa;
        Clean(A);
        Clean(B);
    }
    if (aPrint) return 0;
    printf("won_grabobj_test: %lld checks, %d failures\n", aChecks, aFailures);
    return aFailures ? 1 : 0;
}
