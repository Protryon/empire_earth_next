// WONGUI's focus rules, hit-test verdict and buttons (ComponentTraversal.cpp, ContainerOrdering.cpp,
// ContainerInteraction.cpp, Container.cpp, Button.cpp, WONButton.cpp): the original's statements (copied from the C++
// before the port, the components replaced by their flags, focus ids and states, the calls they made recorded as
// actions) against ee_won's guifocus module through its C ABI: WantFocus, FindChildAt's verdict, GetNextFocusId,
// SetFocusToLastFocusChild's pick, TabFocus's start and loop bound, ButtonUp, MouseDrag, KeyDown, TimerEvent and
// WONButton::GetButtonState's state and fallback, over every flag combination and generated id lists (a fixed
// LCG). Prints one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 31337;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }

// ---- the original's statements

static bool OrigWantFocus(unsigned long mComponentFlags, bool visible, bool disabled, bool hasParent, int mModalCount)
{
    if(!(mComponentFlags & 0x8)) return false;
    if(!visible || disabled) return false;
    if(hasParent && mModalCount>0 && !(mComponentFlags & 0x8000)) return false;
    return true;
}
static bool OrigHitChild(bool disabled, int mModalCount, unsigned long flags)
{
    if(disabled) return false;
    if(mModalCount>0 && !(flags & 0x8000)) return false;
    return true;
}
static unsigned char OrigNextFocusId(unsigned char& mNextFocusId)
{
    if(++mNextFocusId==0) ++mNextFocusId;
    return mNextFocusId;
}
static int OrigLastFocusChild(unsigned char current, bool skipCurrent, const std::vector<unsigned char>& ids)
{
    int last=-1;
    unsigned char minimum=0;
    for(size_t i=0;i<ids.size();++i) {
        unsigned char id=ids[i];
        if(id==0 || (id==current && skipCurrent)) continue;
        if(minimum==0 || int(current)-int(id)<int(minimum)) {
            minimum=(unsigned char)(current-id);
            last=(int)i;
        }
    }
    return last;
}
// TabFocus's start: 0 false, 1 RequestFocus and true, 2 walk the children
static int OrigTabStart(bool wantFocus, bool init, bool mIsolateTabFocus, unsigned long flags, bool empty)
{
    if(!wantFocus) return 0;
    if(init) {
        if(mIsolateTabFocus) return 0;
        if(flags & 0x4000) return 1;
    }
    if(empty) return 0;
    return 2;
}
// ButtonUp: 0 nothing, 1 released and the repeat timer stopped, 2 released, 3 released and activated
static int OrigButtonUp(unsigned long mButtonFlags, int mMouseDownState, bool mKeyDown, bool activate)
{
    if(!(mButtonFlags&0x20) || mMouseDownState==2 || mKeyDown) return 0;
    if(mMouseDownState==1) activate=false;
    if(mButtonFlags&0x02) return 1;
    if(activate && !(mButtonFlags&0x40)) return 3;
    return 2;
}
static int OrigButtonDrag(int mMouseDownState, int x, int y, int aWidth, int aHeight)
{
    if(mMouseDownState==0) return -1;
    if(x>=0 && y>=0 && x<aWidth && y<aHeight) return 0;
    return 1;
}
// KeyDown: 0 false, 1 activate, 2 press, 3 previous radio, 4 next radio
static int OrigButtonKey(int key, bool isDefault, bool mKeyDown, bool radio)
{
    if(key==13) {
        if(isDefault) return 1;
    } else if(key==32 && !mKeyDown) {
        return 2;
    }
    if(radio) {
        bool forward;
        if(key==38 || key==37) forward=false;
        else if(key==40 || key==39) forward=true;
        else return 0;
        return forward ? 4 : 3;
    }
    return 0;
}
static bool OrigRepeat(int& mDelayAcc, int delta)
{
    mDelayAcc+=delta;
    if(mDelayAcc>500) { mDelayAcc-=100; return true; }
    return false;
}
struct Cell { int mImage, mColor, mText; }; // 0 = no image / text
static void OrigButtonState(bool disabled, unsigned long mButtonFlags, bool isDefault, bool mHasCheck,
                            Cell mStates[5][2], int mDefaultTextColor, int& outColor, int& outText, int& outImage)
{
    int aState = 0;
    if(disabled)
        aState = 2;
    else if(mButtonFlags&0x20)
        aState = 1;
    else if((mButtonFlags&0x10) && ((mButtonFlags&1)!=0))
        aState = 3;
    else if(isDefault)
        aState = 4;
    int aCheckedIndex = (mHasCheck && (mButtonFlags&0x04))?1:0;
    Cell &aStateStruct = mStates[aState][aCheckedIndex];
    int anImage = aStateStruct.mImage;
    int aColor = aStateStruct.mColor;
    int aText = aStateStruct.mText;
    if(anImage==0) anImage = mStates[0][aCheckedIndex].mImage;
    if(aColor==-1) aColor = mStates[0][aCheckedIndex].mColor;
    if(aText==0) aText = mStates[0][aCheckedIndex].mText;
    if(aCheckedIndex==1)
    {
        if(anImage==0) anImage = mStates[0][0].mImage;
        if(aColor==-1) aColor = mStates[0][0].mColor;
        if(aText==0) aText = mStates[0][0].mText;
    }
    if(aColor==-1)
        aColor = mDefaultTextColor;
    outColor = aColor; outText = aText; outImage = anImage;
}

// the port's GetButtonState as WONButton.cpp now runs it
static void PortButtonState(bool disabled, unsigned long mButtonFlags, bool isDefault, bool mHasCheck,
                            Cell mStates[5][2], int mDefaultTextColor, int& outColor, int& outText, int& outImage)
{
    int aState = 0;
    int aChecked = ee_won_gui_button_state(disabled, (uint32_t)mButtonFlags, isDefault, mHasCheck, &aState);
    int anImage = 0, aColor = -1, aText = 0, s = 0, c = 0;
    for (int k = 0; ee_won_gui_button_state_cell(aState, aChecked, k, &s, &c); k++) {
        Cell& aCell = mStates[s][c];
        if (anImage == 0) anImage = aCell.mImage;
        if (aColor == -1) aColor = aCell.mColor;
        if (aText == 0) aText = aCell.mText;
    }
    if (aColor == -1) aColor = mDefaultTextColor;
    outColor = aColor; outText = aText; outImage = anImage;
}

// ---- the checks

int main()
{
    static const unsigned long kFlags[] = { 0, 0x8, 0x8000, 0x8008, 0x4000, 0x4008, 0xc008, 0xffffffff };
    for (int f = 0; f < 8; f++)
        for (int b = 0; b < 32; b++) {
            bool visible = b & 1, disabled = (b & 2) != 0, hasParent = (b & 4) != 0;
            int aModal = (b & 8) ? ((b & 16) ? 2 : 1) : ((b & 16) ? -1 : 0);
            CHECK(OrigWantFocus(kFlags[f], visible, disabled, hasParent, aModal) ==
                  ee_won_gui_want_focus((uint32_t)kFlags[f], visible, disabled, hasParent && aModal > 0));
            CHECK(OrigHitChild(disabled, aModal, kFlags[f]) == ee_won_gui_hit_child(disabled, aModal > 0, (uint32_t)kFlags[f]));
            for (int i = 0; i < 4; i++) {
                bool w = i & 1, init = (i & 2) != 0;
                CHECK(OrigTabStart(w, init, visible, kFlags[f], disabled) ==
                      ee_won_gui_tab_focus_start(w, init, visible, (uint32_t)kFlags[f], !disabled));
            }
        }
    for (int i = 0; i < 256; i++) {
        unsigned char id = (unsigned char)i;
        unsigned char o = OrigNextFocusId(id);
        CHECK(o == ee_won_gui_next_focus_id((uint8_t)i));
    }
    for (int n = 0; n < 300000; n++) {
        std::vector<unsigned char> ids;
        int aCount = (int)(Next() % 10);
        for (int i = 0; i < aCount; i++) ids.push_back(Next() % 4 == 0 ? 0 : (unsigned char)(Next() % 256));
        unsigned char current = (unsigned char)(Next() % 256);
        if (aCount > 0 && n % 3 == 0) ids[Next() % aCount] = current;
        bool skip = (n & 1) != 0;
        CHECK(OrigLastFocusChild(current, skip, ids) ==
              ee_won_gui_last_focus_child(current, skip, ids.empty() ? NULL : &ids[0], ids.size()));
    }
    for (unsigned long c = 0; c < 40; c++)
        for (size_t s = 0; s < 40; s++) {
            unsigned long count = c; // TabFocus's ++count>=size
            CHECK((++count>=s) == ee_won_gui_tab_focus_done(c, s));
        }
    for (unsigned long fl = 0; fl < 0x80; fl++)
        for (int st = 0; st < 3; st++)
            for (int k = 0; k < 4; k++) {
                unsigned long flags = ((fl & 0x10) ? 0x20 : 0) | ((fl & 0x20) ? 0x40 : 0) | (fl & 0xf) | ((fl & 0x40) ? 0x10 : 0);
                CHECK(OrigButtonUp(flags, st, k & 1, (k & 2) != 0) ==
                      (int)ee_won_gui_button_up((uint32_t)flags, st, k & 1, (k & 2) != 0));
            }
    for (int n = 0; n < 200000; n++) {
        int st = (int)(Next() % 3), x = (int)(Next() % 60) - 10, y = (int)(Next() % 60) - 10;
        int w = (int)(Next() % 50) - 2, h = (int)(Next() % 50) - 2;
        CHECK(OrigButtonDrag(st, x, y, w, h) == ee_won_gui_button_drag(st, x, y, w, h));
        int d = (int)(Next() % 700) - 50, delta = (int)(Next() % 200), od = d, rd = -1;
        bool of = OrigRepeat(od, delta);
        bool rf = ee_won_gui_button_repeat(d, delta, &rd);
        CHECK(of == rf && od == rd);
    }
    for (int key = 0; key < 128; key++)
        for (int b = 0; b < 8; b++)
            CHECK(OrigButtonKey(key, b & 1, (b & 2) != 0, (b & 4) != 0) ==
                  (int)ee_won_gui_button_key(key, b & 1, (b & 2) != 0, (b & 4) != 0));
    for (int n = 0; n < 200000; n++) {
        Cell aStates[5][2];
        for (int s = 0; s < 5; s++)
            for (int c = 0; c < 2; c++) {
                aStates[s][c].mImage = Next() % 3 == 0 ? 0 : (int)(Next() % 100) + 1;
                aStates[s][c].mColor = Next() % 3 == 0 ? -1 : (int)(Next() % 100);
                aStates[s][c].mText = Next() % 3 == 0 ? 0 : (int)(Next() % 100) + 1;
            }
        unsigned long flags = Next() & 0x3f;
        bool disabled = Next() % 5 == 0, isDefault = Next() % 3 == 0, hasCheck = Next() % 2 == 0;
        int dc = (int)(Next() % 100) + 200;
        int oc, ot, oi, rc, rt, ri;
        OrigButtonState(disabled, flags, isDefault, hasCheck, aStates, dc, oc, ot, oi);
        PortButtonState(disabled, flags, isDefault, hasCheck, aStates, dc, rc, rt, ri);
        CHECK(oc == rc && ot == rt && oi == ri);
    }
    printf("won_guifocus_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
