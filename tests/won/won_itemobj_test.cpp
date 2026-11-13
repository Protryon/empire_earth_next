// TextListItem's and MenuItem's measure and MenuItem's paint (R6 WON round 25): the original statements of
// TextListItem::CalcDimensions, MenuItem::CalcDimensions and MenuItem::Paint (TextListItem.cpp, MenuBehavior.cpp,
// copied from the C++ before the port) over a recording Graphics and scripted font and image measures, against
// ee_won's guidraw (ee_won_gui_text_item_size, ee_won_gui_draw_menu_item: the plan the C++ executes). TextListItem's
// paint has been guidraw's text_row since round 11 (won's golden_draw.txt).
//
// 200,000 random items (text and font sizes, with and without an icon, selected or not, full-row select, disabled,
// with a submenu, every selection colour mode; negative and odd sizes) must give the same sizes and the same draw
// operations. The first 400 cases are rust/crates/ee_won/src/golden_itemobj.txt, which the Rust tests replay;
// `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned R(unsigned n) { sSeed = sSeed * 69069u + 1u; return n ? (sSeed >> 16) % n : 0; }

// the operations a Graphics call makes, in guidraw's codes (image, font and string slot 0)
static std::vector<EeWonDrawOp> sOps;
static void Rec(int32_t kind, int32_t a = 0, int32_t b = 0, int32_t c = 0, int32_t d = 0)
{
    EeWonDrawOp o;
    memset(&o, 0, sizeof(o));
    o.kind = kind; o.a = a; o.b = b; o.c = c; o.d = d;
    sOps.push_back(o);
}
enum { StandardColor_3DHilight = 2, StandardColor_3DShadow = 3 };
struct Graphics {
    void SetColor(uint32_t c) { Rec(EE_WON_DRAW_SET_COLOR, (int32_t)c); }
    void SetSysColor(int c) { Rec(EE_WON_DRAW_SET_SYS_COLOR, c); }
    void FillRect(int x, int y, int w, int h) { Rec(EE_WON_DRAW_FILL_RECT, x, y, w, h); }
    void DrawRect(int x, int y, int w, int h) { Rec(EE_WON_DRAW_RECT, x, y, w, h); }
    void DrawLine(int x1, int y1, int x2, int y2) { Rec(EE_WON_DRAW_LINE, x1, y1, x2, y2); }
    void DrawImage(void*, int x, int y) { Rec(EE_WON_DRAW_IMAGE, 0, x, y); }
    void SetFont(void*) { Rec(EE_WON_DRAW_SET_FONT, 0); }
    void DrawString(const std::string&, int x, int y) { Rec(EE_WON_DRAW_STRING_ALL, 0, x, y); }
    void SetPixel(int x, int y, uint32_t c) { Rec(EE_WON_DRAW_PIXEL, x, y, (int32_t)c); }
};
struct Image { int mWidth, mHeight; };
struct Font {
    int mTextWidth, mHeight;
    int GetStringWidth(const std::string&) { return mTextWidth; }
    int GetHeight() { return mHeight; }
};
struct SelectionColor {
    uint32_t mBackgroundColor, mForegroundColor;
    int mMode;
    uint32_t GetBackgroundColor(Graphics&) const { return mBackgroundColor; }
    uint32_t GetForegroundColor(Graphics&, uint32_t original) const
    {
        return ee_won_gui_sel_foreground(mBackgroundColor, mForegroundColor, mMode, original);
    }
};
struct ListArea {
    Font* mFont;
    uint32_t mTextColor;
    int mFlags, mPaintWidth;
    SelectionColor mSelColor;
    struct { int width; } mScrollArea;
    uint32_t GetTextColor() { return mTextColor; }
    bool ListFlagSet(int f) { return (mFlags & f) != 0; }
    int GetPaintWidth() { return mPaintWidth; }
};

// ---- the original
namespace orig {
struct MenuItem {
    int mWidth, mHeight;
    bool mSelected;
    std::string mText;
    long mColor;
    Font* mFont;
    Image* mImage;
    void* mSubMenu;
    bool mDisabled;
    // TextListItem::CalcDimensions (0x6fa2c0)
    void TextCalcDimensions(ListArea* inList)
    {
        if (this->mFont == NULL) this->mFont = inList->mFont;
        if (this->mColor == -1) this->mColor = inList->GetTextColor();
        this->mWidth = this->mFont->GetStringWidth(this->mText) + 2;
        this->mHeight = this->mFont->GetHeight();
        if (this->mImage != NULL) {
            this->mWidth += this->mImage->mWidth + 2;
            if (this->mImage->mHeight > this->mHeight) this->mHeight = this->mImage->mHeight;
        }
    }
    // MenuItem::CalcDimensions (0x6fcee0)
    void CalcDimensions(ListArea* area)
    {
        this->TextCalcDimensions(area);
        ee_won_gui_menu_item_size(&this->mWidth, &this->mHeight);
    }
    // MenuItem::Paint (0x6fcf10)
    void Paint(Graphics& g, ListArea* area)
    {
        int color = this->mColor;
        if (this->mImage != NULL) {
            int y = (this->mHeight - this->mImage->mHeight) / 2;
            if (y < 0) y = 0;
            g.DrawImage(this->mImage, 2, y);
        }
        if (this->mSelected) {
            g.SetColor(area->mSelColor.GetBackgroundColor(g));
            int width = area->GetPaintWidth();
            if (!area->ListFlagSet(0x10)) width = this->mWidth;
            int x = 0;
            if (this->mImage != NULL) x = 20;
            g.FillRect(x, 0, width, this->mHeight);
            if (this->mImage != NULL) {
                g.SetSysColor(StandardColor_3DHilight);
                int boxHeight = this->mHeight;
                g.DrawRect(0, 0, 20, boxHeight);
                g.SetSysColor(StandardColor_3DShadow);
                g.DrawLine(19, 0, 19, boxHeight - 1);
                g.DrawLine(0, boxHeight - 1, boxHeight - 1, boxHeight - 1);
            }
            color = area->mSelColor.GetForegroundColor(g, this->mColor);
        }
        g.SetColor(color);
        g.SetFont(this->mFont);
        int y = (this->mHeight - this->mFont->GetHeight()) / 2;
        if (this->mDisabled) {
            g.SetColor(0xffffff);
            if (!this->mSelected) {
                g.DrawString(this->mText, 23, y + 1);
                g.SetColor(0x848484);
            } else g.SetColor(0xd0d0d0);
            g.DrawString(this->mText, 22, y);
        } else g.DrawString(this->mText, 22, y);
        if (this->mSubMenu != NULL) {
            int width = area->mScrollArea.width;
            int height = this->mHeight;
            bool disabled = this->mDisabled && !this->mSelected;
            int x = width - 8, y = height / 2;
            for (int i = 0; i < 4; ++i) {
                g.DrawLine(x + 3 - i, y - i, x + 3 - i, y + i);
                if (disabled && i < 3) {
                    g.SetColor(0xffffff);
                    g.DrawLine(x + 3 - i, y + i + 1, x + 3 - i, y + i + 2);
                    g.SetColor(0x848484);
                }
            }
            if (disabled) g.SetPixel(x + 4, y + 1, 0xffffff);
        }
    }
};
}

static std::string OpText(const EeWonDrawOp& o)
{
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "op %d %d %d %d %d", o.kind, o.a, o.b, o.c, o.d);
    return aLine;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kCases = aPrint ? 400 : 200000;
    size_t aOps = 0;
    for (int n = 0; n < kCases; n++) {
        sSeed = (unsigned)n * 2654435761u + 7u;
        Font aListFont = {(int)R(200) - 5, (int)R(30) - 2};
        Font anOwnFont = {(int)R(200) - 5, (int)R(30) - 2};
        Image anImage = {(int)R(40) - 2, (int)R(50) - 4};
        static const uint32_t kColors[4] = {0x8000000e, 0x123456, 0xffffffff, 0};
        ListArea aList;
        aList.mFont = &aListFont;
        aList.mTextColor = kColors[R(4)];
        aList.mFlags = R(2) ? 0x10 : 0;
        aList.mPaintWidth = (int)R(300) - 10;
        aList.mSelColor.mBackgroundColor = kColors[R(4)];
        aList.mSelColor.mForegroundColor = kColors[R(4)];
        aList.mSelColor.mMode = (int)R(5);
        aList.mScrollArea.width = (int)R(300) - 10;
        orig::MenuItem m;
        m.mWidth = m.mHeight = 0;
        m.mSelected = R(2) != 0;
        m.mColor = R(3) == 0 ? -1 : (long)kColors[R(4)];
        m.mFont = R(2) ? &anOwnFont : NULL;
        m.mImage = R(2) ? &anImage : NULL;
        m.mSubMenu = R(2) ? (void*)1 : NULL;
        m.mDisabled = R(3) == 0;
        bool aMenu = R(2) != 0;
        if (aPrint)
            printf("case %d %d %d %d %d %d %d %d %d %d %u %u %d %u %u %d %d %d %d %d %d\n", aListFont.mTextWidth,
                   aListFont.mHeight, anOwnFont.mTextWidth, anOwnFont.mHeight, anImage.mWidth, anImage.mHeight,
                   aList.mFlags, aList.mPaintWidth, aList.mScrollArea.width, aList.mSelColor.mMode,
                   (unsigned)aList.mSelColor.mBackgroundColor, (unsigned)aList.mSelColor.mForegroundColor,
                   (int)m.mColor, (unsigned)aList.mTextColor, (unsigned)m.mSelected, m.mFont != NULL,
                   m.mImage != NULL, m.mSubMenu != NULL, m.mDisabled, aMenu, 0);
        // the measure: the font and colour defaults are C++ data, the sizes Rust's
        Font* aFont = m.mFont != NULL ? m.mFont : aList.mFont;
        int w = 0, h = 0;
        ee_won_gui_text_item_size(aFont->GetStringWidth(m.mText), aFont->GetHeight(), m.mImage != NULL,
                                  m.mImage ? m.mImage->mWidth : 0, m.mImage ? m.mImage->mHeight : 0, aMenu, &w, &h);
        if (aMenu) m.CalcDimensions(&aList);
        else m.TextCalcDimensions(&aList);
        CHECK(w == m.mWidth && h == m.mHeight);
        if (aPrint) printf("size %d %d\n", m.mWidth, m.mHeight);
        if (R(4) == 0) { m.mWidth = (int)R(300) - 10; m.mHeight = (int)R(40) - 5; }
        // the paint
        sOps.clear();
        Graphics g;
        m.Paint(g, &aList);
        std::vector<EeWonDrawOp> anOrig = sOps;
        EeWonMenuItemPaint r;
        memset(&r, 0, sizeof(r));
        r.selected = m.mSelected;
        r.full_row_select = aList.ListFlagSet(0x10);
        r.has_image = m.mImage != NULL;
        r.disabled = m.mDisabled;
        r.has_submenu = m.mSubMenu != NULL;
        r.sel.background = aList.mSelColor.mBackgroundColor;
        r.sel.foreground = aList.mSelColor.mForegroundColor;
        r.sel.mode = aList.mSelColor.mMode;
        r.color = (uint32_t)m.mColor;
        r.paint_width = aList.GetPaintWidth();
        r.width = m.mWidth;
        r.height = m.mHeight;
        r.image_height = m.mImage ? m.mImage->mHeight : 0;
        r.font_height = m.mFont->GetHeight();
        r.view_width = aList.mScrollArea.width;
        EeWonDrawOp aBuf[64];
        size_t k = ee_won_gui_draw_menu_item(&r, aBuf, 64);
        CHECK(k == anOrig.size());
        for (size_t i = 0; i < k && i < anOrig.size(); i++) CHECK(OpText(aBuf[i]) == OpText(anOrig[i]));
        aOps += anOrig.size();
        if (aPrint) {
            printf("paint %d %d\n", m.mWidth, m.mHeight);
            for (size_t i = 0; i < anOrig.size(); i++) printf("%s\n", OpText(anOrig[i]).c_str());
        }
    }
    if (!aPrint)
        printf("won_itemobj_test: %d cases, %u draw operations, %d checks, %d failures\n", kCases, (unsigned)aOps,
               sChecks, sFailures);
    return sFailures;
}
