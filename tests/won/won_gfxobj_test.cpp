// WONGUI's Graphics (R6 WONGUI B round 1): the original's Graphics.cpp statements (copied below as OrigGraphics, HEAD
// 6d0c36ca) against the port (WONLobbyGUI.h's Graphics, Graphics.cpp over ee_won's gfxobj.rs), both under the same
// traced hooks, images and fonts, over random call sequences: the trace of hook, image and font calls and the state
// (translation, colour, clip rectangle) compared after each call. The base hooks (DrawRectHook, DrawDottedRectHook,
// FillRectHook) are not overridden, so theirs run. `--print FILE` writes the first sequences as
// rust/crates/ee_won_gui/src/golden_gfxobj.txt (cargo replays it, gfxobj_tests.rs); without it the golden is compared
// too. It also checks WindowManager::InitColorScheme's statements against wmobj.rs's table. The exit code is the
// number of failures.
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/wmobj.h"

using namespace WONAPI;

wchar_t gEmptyWideString[2];
static std::vector<std::string> sTrace;
static void T(const char* f, ...)
{
    char b[256];
    va_list v;
    va_start(v, f);
    vsnprintf(b, sizeof(b), f, v);
    va_end(v);
    sTrace.push_back(b);
}

// ---- stand-ins: the images and fonts the calls reach (their out-of-line members, here)
static int IdOf(const void* p);
Image::~Image() {}
void Image::Draw(DisplayContext*, int x, int y, int l, int t, int w, int h) { T("part %d %d %d %d %d %d %d", IdOf(this), x, y, l, t, w, h); }
void Image::DrawEntire(DisplayContext*, int x, int y) { T("entire %d %d %d", IdOf(this), x, y); }
void Image::SetSize(int, int) {}
SmartPtr<Image> Image::Duplicate() { return NULL; }
SmartPtr<NativeImage> Image::GetNative(DisplayContext*) { return NULL; }
Font::Font(DisplayContext*, const FontDescriptor& d) : mDescriptor(d) {}
Font::~Font() {}
void Font::PrepareContext(DisplayContext*) { T("prepare %d", this->mDescriptor.mSize); }
void Font::DrawString(DisplayContext*, const GUIString& s, unsigned short o, unsigned short n, int x, int y)
{ T("string %d %d %d %d %d %d", this->mDescriptor.mSize, IdOf(&s), o, n, x, y); }
int Font::GetStringWidth(const GUIString&, unsigned short, unsigned short) { return 0; }
int Font::GetCharacterWidth(int) { return 0; }
int Font::GetHeight() { return 0; }
int Font::GetAscent() { return 0; }
int Font::GetDescent() { return 0; }

// ---- the original statements (Graphics.cpp at HEAD 6d0c36ca)
class OrigGraphics {
public:
    virtual void ClearClipRectHook() {}
    virtual void SetClipRectHook(int, int, int, int) {}
    virtual void SetPixelHook(int, int, DWORD) {}
    virtual void SetColorHook() {}
    virtual void DrawLineHook(int, int, int, int) {}
    virtual void DrawRectHook(int, int, int, int);
    virtual void DrawDottedRectHook(int, int, int, int);
    virtual void FillRectHook(int, int, int, int);
    virtual void DrawEllipseHook(int, int, int, int) {}
    virtual void FillEllipseHook(int, int, int, int) {}
    virtual void DrawArcHook(int, int, int, int, int, int) {}
    virtual void FillRectBlendHook(int, int, int, int, int) {}
    virtual ~OrigGraphics() {}
    SmartPtr<DisplayContext> mDisplayContext;
    int mTransX, mTransY;
    SmartPtr<Font> mFont;
    DWORD mColor;
    typedef std::list<WONRectangle> ClipStack;
    ClipStack mClipStack;
    WONRectangle mClipRect;
    ColorScheme* mColorScheme;
    OrigGraphics() { this->mTransX = this->mTransY = 0; this->mColorScheme = NULL; }
void SetColor(DWORD theColor)
{
	if(theColor>=0x80000000 && theColor<0x80000000+17 && this->mColorScheme)
	{
		DWORD aColor = this->mColorScheme->GetStandardColor((int)(theColor-0x80000000));
		if(aColor!=this->mColor)
		{
			this->mColor = aColor;
			this->SetColorHook();
		}
	}
	else if(this->mColor!=theColor)
	{
		this->mColor = theColor;
		this->SetColorHook();
	}
}
DWORD GetStandardColor(int theColorRef)
{
	if(this->mColorScheme)
		return this->mColorScheme->GetStandardColor(theColorRef);
	else
		return 0;
}
void SetSysColor(int theColorRef)
{
	if(this->mColorScheme)
		this->SetColor(this->mColorScheme->GetStandardColor(theColorRef));
}
void Translate(int dx, int dy)
{
	this->mTransX += dx;
	this->mTransY += dy;
}
void DoTranslate(int &x, int &y)
{
	x+=this->mTransX;
	y+=this->mTransY;
}
void SetPixel(int x, int y, DWORD color)
{
    this->DoTranslate(x,y);
    this->SetPixelHook(x,y,color);
}
void DrawImage(Image *theImage, int x, int y)
{
	this->DoTranslate(x,y);
	theImage->DrawEntire(this->mDisplayContext.get(),x,y);
}
void DrawImage(Image *theImage, int x, int y, int left, int top, int width, int height)
{
	this->DoTranslate(x,y);
	theImage->Draw(this->mDisplayContext.get(), x, y, left, top, width, height);
}
void DrawLine(int x1, int y1, int x2, int y2)
{
	this->DoTranslate(x1,y1);
	this->DoTranslate(x2,y2);
	this->DrawLineHook(x1,y1,x2,y2);
}
void DrawRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawRectHook(x,y,width,height);
}
void FillRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->FillRectHook(x,y,width,height);
}
void FillRectBlend(int x, int y, int width, int height, int theBlendAmount)
{
	this->DoTranslate(x,y);
	this->FillRectBlendHook(x,y,width,height,theBlendAmount);
}
void DrawDottedRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawDottedRectHook(x,y,width,height);
}
void DrawEllipse(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawEllipseHook(x,y,width,height);
}
void FillEllipse(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->FillEllipseHook(x,y,width,height);
}
void DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle)
{
	this->DoTranslate(x,y);
	this->DrawArcHook(x,y,width,height,startAngle,arcAngle);
}
void SetFont(Font *theFont)
{
	if(theFont!=this->mFont.get())
	{
		this->mFont = theFont;
		this->mFont->PrepareContext(this->mDisplayContext.get());
	}
}
void DrawString(const GUIString &theStr, int x, int y)
{
	this->DoTranslate(x,y);
	this->mFont->DrawString(this->mDisplayContext.get(),theStr,0,theStr.length(),x, y);
}
void DrawString(const GUIString &theStr, unsigned short theOffset, unsigned short theLength, int x, int y)
{
	this->DoTranslate(x,y);
	this->mFont->DrawString(this->mDisplayContext.get(),theStr,theOffset,theLength,x, y);
}
void PerformClip()
{
	this->ClearClipRectHook();
	this->mClipRect.x = this->mClipRect.y = -10000000;
	this->mClipRect.width = this->mClipRect.height = 20000001;
	if(this->mClipStack.empty())
		return;

	WONRectangle aRect = this->mClipStack.front();
	ClipStack::iterator anItr = this->mClipStack.begin();
	++anItr;

	while(anItr!=this->mClipStack.end())
	{
		aRect.DoIntersect(*anItr);
		++anItr;
	}

	this->SetClipRectHook(aRect.x, aRect.y, aRect.width, aRect.height);
	this->mClipRect = aRect;
}
void PushClipRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	WONRectangle aRect(x , y, width, height);
	this->mClipStack.push_back(aRect);
	this->PerformClip();
}
void PopClipRect()
{
	if(!this->mClipStack.empty())
		this->mClipStack.pop_back();

	this->PerformClip();

}
};
void OrigGraphics::DrawRectHook(int x, int y, int width, int height)
{
	this->DrawLineHook(x,y,x+width,y);
	this->DrawLineHook(x+width,y,x+width,y+height);
	this->DrawLineHook(x+width,y+height,x,y+height);
	this->DrawLineHook(x,y+height,x,y);
}
void OrigGraphics::DrawDottedRectHook(int x, int y, int width, int height)
{
	int i;
	for(i=0; i<width; i+=2)
	{
		this->SetPixelHook(x+i,y,this->mColor);
		this->SetPixelHook(x+i,y+height-1,this->mColor);
	}

	for(i=0; i<height; i+=2)
	{
		this->SetPixelHook(x,y+i,this->mColor);
		this->SetPixelHook(x+width-1,y+i,this->mColor);
	}
}
void OrigGraphics::FillRectHook(int x, int y, int width, int height)
{
	for(int i=0; i<height; i++)
		this->DrawLineHook(x,y+i,x+width,y+i);
}

// ---- the same traced hooks on both (the base rectangle hooks not overridden)
#define TRACED_HOOKS \
    void ClearClipRectHook() { T("clearclip"); } \
    void SetClipRectHook(int x, int y, int w, int h) { T("setclip %d %d %d %d", x, y, w, h); } \
    void SetPixelHook(int x, int y, DWORD c) { T("pixel %d %d %x", x, y, (unsigned)c); } \
    void SetColorHook() { T("color %x", (unsigned)this->mColor); } \
    void DrawLineHook(int a, int b, int c, int d) { T("line %d %d %d %d", a, b, c, d); } \
    void DrawEllipseHook(int x, int y, int w, int h) { T("ellipse %d %d %d %d", x, y, w, h); } \
    void FillEllipseHook(int x, int y, int w, int h) { T("fillellipse %d %d %d %d", x, y, w, h); } \
    void DrawArcHook(int x, int y, int w, int h, int s, int a) { T("arc %d %d %d %d %d %d", x, y, w, h, s, a); } \
    void FillRectBlendHook(int x, int y, int w, int h, int n) { T("blend %d %d %d %d %d", x, y, w, h, n); }
struct TOrig : OrigGraphics { TRACED_HOOKS };
struct TPort : Graphics { TRACED_HOOKS };

// ---- the world
static unsigned sSeed = 1;
static int R(int n) { sSeed = sSeed * 1103515245u + 12345u; return n <= 0 ? 0 : (int)((sSeed >> 8) % (unsigned)n); }
static int RI(int lo, int hi) { return lo + R(hi - lo + 1); }
struct TImage : Image { TImage() { this->mWidth = 8; this->mHeight = 8; } };
static TImage sImages[3];
static GUIString sTexts[2] = {GUIString("ab"), GUIString("hello")};
static ColorScheme sSchemes[2];
static int IdOf(const void* p)
{
    for (int i = 0; i < 3; i++) if (p == &sImages[i]) return i;
    for (int i = 0; i < 2; i++) if (p == &sTexts[i]) return 10 + i;
    return -1;
}
// the schemes' colours (gfxobj_tests.rs has the same rule): colour 3 refers to itself, colour 5 to colour 0
static DWORD SchemeColor(int k, int i) { return i == 3 ? 0x80000003u : i == 5 ? 0x80000000u : (DWORD)(0x10101 * i + 0x1000 * k); }
static DWORD RColor()
{
    switch (R(4)) {
    case 0: return 0x80000000u + R(19); // a scheme reference (17 and 18: none)
    case 1: return (DWORD)R(3);
    default: return (DWORD)(R(0x1000000));
    }
}

int main(int argc, char** argv)
{
    const char* aPrint = argc > 2 && strcmp(argv[1], "--print") == 0 ? argv[2] : NULL;
    for (int k = 0; k < 2; k++) for (int i = 0; i < 17; i++) sSchemes[k].mColors[i] = SchemeColor(k, i);
    for (int i = 0; i < 3; i++) sImages[i].AddRef();
    SmartPtr<Font> aFonts[2] = {new Font(NULL, FontDescriptor("A", 0, 7)), new Font(NULL, FontDescriptor("B", 0, 9))};
    const int kSeqs = 20000, kSteps = 40, kGolden = 300;
    static const char* const kNames[] = {"setcolor", "stdcolor", "syscolor", "translate", "pixel", "image", "imagepart",
        "line", "rect", "fill", "blend", "dotted", "ellipse", "fillellipse", "arc", "string", "stringpart", "perform",
        "push", "pop", "scheme", "font"};
    std::vector<std::string> aGolden;
    int aFailures = 0;
    long long aChecks = 0;
    // WindowManager::InitColorScheme's statements (WindowManager.cpp at HEAD 6d0c36ca, on a scheme) against wmobj.rs's
    // table
    {
        ColorScheme aScheme;
        ColorScheme* mColorScheme = &aScheme;
	mColorScheme->SetStandardColor(StandardColor_3DDarkShadow,0x000000);
	mColorScheme->SetStandardColor(StandardColor_3DFace,0xD0D0D0);
	mColorScheme->SetStandardColor(StandardColor_3DHilight,0xFFFFFF);
	mColorScheme->SetStandardColor(StandardColor_3DShadow,0x848484);
	mColorScheme->SetStandardColor(StandardColor_Scrollbar,0xDEDEDE);
	mColorScheme->SetStandardColor(StandardColor_ButtonText,0x000000);
	mColorScheme->SetStandardColor(StandardColor_GrayText,0x848484);
	mColorScheme->SetStandardColor(StandardColor_Hilight,0x08246B);
	mColorScheme->SetStandardColor(StandardColor_HilightText,0xFFFFFF);
	mColorScheme->SetStandardColor(StandardColor_ToolTipBack,0xFFFFDD);
	mColorScheme->SetStandardColor(StandardColor_ToolTipText,0x000000);
	mColorScheme->SetStandardColor(StandardColor_MenuBack,0xD0D0D0);
	mColorScheme->SetStandardColor(StandardColor_MenuText,0);
	mColorScheme->SetStandardColor(StandardColor_Back,0xFFFFFF);
	mColorScheme->SetStandardColor(StandardColor_Text,0x000000);
	mColorScheme->SetStandardColor(StandardColor_Link,0x0000FF);
	mColorScheme->SetStandardColor(StandardColor_LinkDown,0xFF0000);
        uint32_t aTable[StandardColor_Max];
        ee_won_wm_standard_colors(aTable);
        for (int i = 0; i < StandardColor_Max; i++)
            if (aScheme.mColors[i] != aTable[i] && aFailures++ < 20)
                printf("FAIL standard colour %d: %x / %x\n", i, (unsigned)aScheme.mColors[i], (unsigned)aTable[i]);
    }
    for (int q = 0; q < kSeqs; q++) {
        sSeed = 0x2545f491u ^ (unsigned)(q * 2654435761u);
        TOrig* o = new TOrig;
        TPort* p = new TPort;
        o->mColor = p->mColor = 0;
        for (int k = 0; k < kSteps; k++) {
            int c = R(22);
            if (c == 18 && R(3) == 0) c = 19; // more pops
            int a[7] = {RI(-30, 60), RI(-30, 60), RI(-5, 40), RI(-5, 40), RI(-5, 40), RI(-5, 40), 0};
            a[6] = (int)RColor();
            std::vector<std::string> to, tp;
            DWORD ro = 0, rp = 0;
            for (int side = 0; side < 2; side++) {
                sTrace.clear();
                DWORD& r = side == 0 ? ro : rp;
                int x0 = a[0], x1 = a[1], x2 = a[2], x3 = a[3], x4 = a[4], x5 = a[5];
                if (side == 0) {
                    TOrig& g = *o;
                    switch (c) {
                    case 0: g.SetColor((DWORD)(unsigned)a[6]); break;
                    case 1: r = g.GetStandardColor(x2 & 15); break;
                    case 2: g.SetSysColor(x2 & 15); break;
                    case 3: g.Translate(x0 / 3, x1 / 3); break;
                    case 4: g.SetPixel(x0, x1, (DWORD)x2); break;
                    case 5: g.DrawImage(&sImages[x2 & 1], x0, x1); break;
                    case 6: g.DrawImage(&sImages[2], x0, x1, x2, x3, x4, x5); break;
                    case 7: g.DrawLine(x0, x1, x2, x3); break;
                    case 8: g.DrawRect(x0, x1, x2, x3); break;
                    case 9: g.FillRect(x0, x1, x2 / 4, x3 / 4); break;
                    case 10: g.FillRectBlend(x0, x1, x2, x3, x4); break;
                    case 11: g.DrawDottedRect(x0, x1, x2 / 2, x3 / 2); break;
                    case 12: g.DrawEllipse(x0, x1, x2, x3); break;
                    case 13: g.FillEllipse(x0, x1, x2, x3); break;
                    case 14: g.DrawArc(x0, x1, x2, x3, x4, x5); break;
                    case 15: if (g.mFont.get()) g.DrawString(sTexts[x2 & 1], x0, x1); break;
                    case 16: if (g.mFont.get()) g.DrawString(sTexts[x2 & 1], (unsigned short)(x3 & 3), (unsigned short)(x4 & 7), x0, x1); break;
                    case 17: g.PerformClip(); break;
                    case 18: g.PushClipRect(x0, x1, x2, x3); break;
                    case 19: g.PopClipRect(); break;
                    case 20: g.mColorScheme = (x2 & 3) == 0 ? NULL : &sSchemes[x2 & 1]; break;
                    case 21: g.SetFont(aFonts[x2 & 1].get()); break;
                    }
                } else {
                    TPort& g = *p;
                    switch (c) {
                    case 0: g.SetColor((DWORD)(unsigned)a[6]); break;
                    case 1: r = g.GetStandardColor(x2 & 15); break;
                    case 2: g.SetSysColor(x2 & 15); break;
                    case 3: g.Translate(x0 / 3, x1 / 3); break;
                    case 4: g.SetPixel(x0, x1, (DWORD)x2); break;
                    case 5: g.DrawImage(&sImages[x2 & 1], x0, x1); break;
                    case 6: g.DrawImage(&sImages[2], x0, x1, x2, x3, x4, x5); break;
                    case 7: g.DrawLine(x0, x1, x2, x3); break;
                    case 8: g.DrawRect(x0, x1, x2, x3); break;
                    case 9: g.FillRect(x0, x1, x2 / 4, x3 / 4); break;
                    case 10: g.FillRectBlend(x0, x1, x2, x3, x4); break;
                    case 11: g.DrawDottedRect(x0, x1, x2 / 2, x3 / 2); break;
                    case 12: g.DrawEllipse(x0, x1, x2, x3); break;
                    case 13: g.FillEllipse(x0, x1, x2, x3); break;
                    case 14: g.DrawArc(x0, x1, x2, x3, x4, x5); break;
                    case 15: if (g.mFont.get()) g.DrawString(sTexts[x2 & 1], x0, x1); break;
                    case 16: if (g.mFont.get()) g.DrawString(sTexts[x2 & 1], (unsigned short)(x3 & 3), (unsigned short)(x4 & 7), x0, x1); break;
                    case 17: g.PerformClip(); break;
                    case 18: g.PushClipRect(x0, x1, x2, x3); break;
                    case 19: g.PopClipRect(); break;
                    case 20: g.mColorScheme = (x2 & 3) == 0 ? NULL : &sSchemes[x2 & 1]; break;
                    case 21: g.SetFont(aFonts[x2 & 1].get()); break;
                    }
                }
                (side == 0 ? to : tp) = sTrace;
            }
            char st[160];
            snprintf(st, sizeof(st), "%d %d %x %d %d %d %d r%x", o->mTransX, o->mTransY, (unsigned)o->mColor, o->mClipRect.x,
                     o->mClipRect.y, o->mClipRect.width, o->mClipRect.height, (unsigned)ro);
            char sp[160];
            snprintf(sp, sizeof(sp), "%d %d %x %d %d %d %d r%x", p->mTransX, p->mTransY, (unsigned)p->mColor, p->mClipRect.x,
                     p->mClipRect.y, p->mClipRect.width, p->mClipRect.height, (unsigned)rp);
            aChecks++;
            if ((to != tp || strcmp(st, sp) != 0) && aFailures++ < 20) {
                printf("FAIL s%d k%d %s %d %d %d %d %d %d %d\n  orig %s |", q, k, kNames[c], a[0], a[1], a[2], a[3], a[4], a[5], a[6], st);
                for (size_t i = 0; i < to.size(); i++) printf(" %s;", to[i].c_str());
                printf("\n  port %s |", sp);
                for (size_t i = 0; i < tp.size(); i++) printf(" %s;", tp[i].c_str());
                printf("\n");
            }
            if (q < kGolden) {
                char l[160];
                snprintf(l, sizeof(l), "s%d %s %d %d %d %d %d %d %d :", q, kNames[c], a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
                std::string line = std::string(l) + " " + st + " |";
                for (size_t i = 0; i < to.size(); i++) line += " " + to[i] + ";";
                aGolden.push_back(line);
            }
        }
        delete o;
        delete p;
    }
    if (aPrint) {
        FILE* f = fopen(aPrint, "wb");
        for (size_t i = 0; i < aGolden.size(); i++) fprintf(f, "%s\n", aGolden[i].c_str());
        fclose(f);
        printf("wrote %d lines to %s\n", (int)aGolden.size(), aPrint);
    } else {
        std::string aPath = __FILE__;
        size_t aSlash = aPath.find_last_of("/\\");
        aPath = aPath.substr(0, aSlash + 1) + "../../rust/crates/ee_won_gui/src/golden_gfxobj.txt";
        FILE* f = fopen(aPath.c_str(), "rb");
        if (f == NULL) { printf("FAIL: no %s\n", aPath.c_str()); aFailures++; }
        else {
            static char aLine[65536];
            size_t n = 0;
            while (fgets(aLine, sizeof(aLine), f)) {
                size_t m = strlen(aLine);
                while (m && (aLine[m - 1] == '\n' || aLine[m - 1] == '\r')) aLine[--m] = 0;
                if (n >= aGolden.size() || aGolden[n] != aLine) { if (aFailures++ < 20) printf("FAIL golden line %d\n", (int)n + 1); }
                n++;
            }
            fclose(f);
            if (n != aGolden.size()) { printf("FAIL golden has %d lines, the run %d\n", (int)n, (int)aGolden.size()); aFailures++; }
        }
    }
    printf("won_gfxobj_test: %d sequences, %lld checks, %d failures\n", kSeqs, aChecks, aFailures);
    return aFailures;
}
