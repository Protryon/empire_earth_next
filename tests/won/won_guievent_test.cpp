// WONGUI's event dispatch decisions (Window.cpp, WindowManager.cpp's DeliverEvent / ProcessEvents /
// CheckPopupClick, RootContainer.cpp, Container.cpp's mouse routing): the original's statements (copied from the C++
// before the port, the windows, components and events replaced by the flags, ticks and numbers they read, their
// calls by logged steps) against ee_won's guievent module through its C ABI, over generated inputs (a fixed LCG).
// Prints one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <list>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 31337;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 300) - 30;
    }
}
typedef uint32_t DWORD;
static DWORD Tick()
{
    switch (Next() % 6) {
    case 0: return 0;
    case 1: return 0xffffffffu - Next() % 400;
    case 2: return Next() * 2654435761u;
    default: return Next() % 3000;
    }
}
static bool B() { return Next() & 1; }

// WONLobbyGUI.h's numbers
enum { InputEvent_MouseDown = 3, InputEvent_MouseDoubleClick = 4, InputEvent_MouseDownCheckDoubleClick = 5,
       InputEvent_KeyDown = 8, InputEvent_KeyUp = 9, InputEvent_KeyChar = 10 };
enum WindowState { WindowState_Open, WindowState_Closing, WindowState_Closed };
enum { PopupFlag_EndOnMouseMove = 0x0008, PopupFlag_EndOnKeyDown = 0x0010, PopupFlag_EndOnEscape = 0x0020,
       PopupFlag_WantInput = 0x0080, PopupFlag_DontWantMouse = 0x4000 };
#define VK_ESCAPE 0x1B

static void WindowRules()
{
    for (int n = 0; n < 1000000; n++) {
        // HandleMouseEvent: the enter, the move's double-click end, the release's exit
        {
            bool mMouseIsInClientArea = B(), down0 = B();
            CHECK((!mMouseIsInClientArea && !down0) == ee_won_gui_win_mouse_enters(mMouseIsInClientArea, down0));
            bool mDetectDoubleClick = B();
            int mMouseX = Next() % 3, mMouseY = Next() % 3, dx = Next() % 3, dy = Next() % 3;
            bool o = false;
            if(mDetectDoubleClick && mMouseX!=dx || mMouseY!=dy)
                o = true;
            CHECK(o == ee_won_gui_win_move_ends_double(mDetectDoubleClick, mMouseX, mMouseY, dx, dy));
            int aButton = Next() % 3;
            bool exits = false;
            if(aButton==0)
            {
                if(!mMouseIsInClientArea)
                    exits = true;
            }
            CHECK(exits == ee_won_gui_win_release_exits(aButton, mMouseIsInClientArea));
        }
        // HandleMouseDown's double click
        {
            int type = Next() % 3 == 0 ? InputEvent_MouseDown : Next() % 2 ? InputEvent_MouseDownCheckDoubleClick
                                                                            : InputEvent_MouseDoubleClick;
            bool mDetectDoubleClick = B();
            DWORD ticks[3] = { Tick(), Tick(), Tick() };
            int aButton = Next() % 3;
            DWORD now = Next() % 4 == 0 ? ticks[aButton] + Next() % 300 : Tick();
            DWORD t0[3] = { ticks[0], ticks[1], ticks[2] };
            bool det0 = mDetectDoubleClick;
            bool isDoubleClick = false;
            bool started = false;
            if(type==InputEvent_MouseDoubleClick)
                isDoubleClick = true;
            else if(type==InputEvent_MouseDownCheckDoubleClick)
            {
                if(!mDetectDoubleClick)
                {
                    for(int i=0; i<3; i++)
                        ticks[i] = 0;
                    mDetectDoubleClick = true;
                    started = true;
                }
                DWORD &aPrevTick = ticks[aButton];
                DWORD aTick = now;
                if(aTick==0)
                    aTick = 1;
                if(aPrevTick!=0 && aTick-aPrevTick<250)
                {
                    isDoubleClick = true;
                    aPrevTick = 0;
                }
                else
                    aPrevTick = aTick;
            }
            bool rs, rset;
            DWORD rt;
            bool rd = ee_won_gui_win_double_click(type, det0, t0[aButton],
                                                  type==InputEvent_MouseDownCheckDoubleClick ? now : 0, &rs, &rset, &rt);
            DWORD r[3] = { t0[0], t0[1], t0[2] };
            if (rs) r[0] = r[1] = r[2] = 0;
            if (rset) r[aButton] = rt;
            CHECK(rd == isDoubleClick);
            CHECK(rs == started);
            CHECK(r[0] == ticks[0] && r[1] == ticks[1] && r[2] == ticks[2]);
        }
        // HandleInputEvent's size change, CheckResize, IdleUpdate, TimerEvent
        {
            int w = Next() % 4 ? (int)(Next() % 5) - 1 : Any(), h = Next() % 4 ? (int)(Next() % 5) - 1 : Any();
            int rw = Next() % 5, rh = Next() % 5;
            bool o = false;
            if(w!=-1 && h!=-1)
            {
                if(rw!=w || rh!=h)
                    o = true;
            }
            CHECK(o == ee_won_gui_win_size_resizes(w, h, rw, rh));
            bool mNeedResize = B();
            DWORD theTick = Tick(), last = Next() % 3 ? theTick - Next() % 400 : Tick(), delay = Next() % 3 ? Next() % 300 : Tick();
            int susp = (int)(Next() % 5) - 2;
            bool due = true;
            if(!mNeedResize || !(theTick-last>delay) || !(susp<=0))
                due = false;
            CHECK(due == ee_won_gui_win_resize_due(mNeedResize, theTick, last, delay, susp));
            DWORD aTick = Tick(), lastCheck = Next() % 2 ? aTick - Next() % 2000 : Tick();
            CHECK((aTick - lastCheck >= 1000) == ee_won_gui_win_cursor_forced(true, aTick, lastCheck));
            DWORD mLastMoveTick = Next() % 4 ? Tick() : 0, now = Next() % 2 ? mLastMoveTick + Next() % 300 : Tick();
            bool in = B(), d0 = Next() % 4 == 0, d1 = Next() % 4 == 0, d2 = Next() % 4 == 0;
            bool hover = false;
            DWORD he = 0;
            if(in && mLastMoveTick!=0)
            {
                if(!d0 && !d1 && !d2)
                {
                    DWORD anElapsed = now - mLastMoveTick;
                    if(anElapsed >= 100) { hover = true; he = anElapsed; }
                }
            }
            DWORD re = 0;
            CHECK(hover == ee_won_gui_win_hover(in, mLastMoveTick, d0, d1, d2, now, &re));
            if (hover) CHECK(re == he);
        }
        // HandleMouseExit's place, Center, CheckSetCursor, Close
        {
            int l = Any(), t = Any(), w = Any(), h = Any();
            int y = 0;
            CHECK((l+w)+1 == ee_won_gui_win_exit_point(l, t, w, h, &y));
            CHECK((t+h)+1 == y);
            bool inParent = B(), hasParent = B(), vis = B();
            CHECK(!(!inParent || !hasParent || !vis) == ee_won_gui_win_center_on_parent(inParent, hasParent, vis));
            int sx = Any(), sy = Any(), sw = Any(), sh = Any(), ww = Any(), wh = Any();
            int left = sx + (sw - ww)/2;
            int top = sy + (sh - wh)/2;
            if(left<0)
                left = 0;
            if(top<0)
                top = 0;
            int rt = 0;
            CHECK(left == ee_won_gui_win_center(sx, sy, sw, sh, ww, wh, &rt));
            CHECK(top == rt);
            bool force = B(), same = B();
            CHECK(!(!force && same) == ee_won_gui_win_cursor_refresh(force, same));
            int state = Next() % 4;
            CHECK((state!=WindowState_Open ? 0 : 1) == ee_won_gui_win_close_step(state));
        }
    }
}

static void ManagerRules()
{
    for (int n = 0; n < 1000000; n++) {
        // ProcessEvents
        {
            DWORD aTick = Tick(), last = Next() % 2 ? aTick - Next() % 2000 : Tick();
            DWORD anElapsed = aTick - last;
            bool runs = false;
            if(anElapsed >= 50)
            {
                if(anElapsed >= 1000)
                    anElapsed = 1000;
                runs = true;
            }
            DWORD re = 0;
            CHECK(runs == ee_won_gui_wm_timer_step(aTick, last, &re));
            if (runs) CHECK(re == anElapsed);
            bool mPopupIsUp = B(), focusIsOwner = Next() % 3 == 0, hasFocus = Next() % 4 != 0;
            bool focusIsPopup = hasFocus && B(), mEndPopup = B();
            int mPopupFlags = Next() % 3 ? (int)(Next() % 0x8000) : Any();
            int step;
            if(mPopupIsUp && !focusIsOwner && ((bool)(((unsigned)mPopupFlags>>6)&1)))
            {
                if(hasFocus && focusIsPopup)
                    step = 1;
                else if(mEndPopup)
                    step = 2;
                else
                    step = 3;
            }
            else
                step = 0;
            CHECK(step == ee_won_gui_wm_popup_focus_step(mPopupIsUp, focusIsOwner, mPopupFlags, hasFocus, focusIsPopup,
                                                         mEndPopup));
            DWORD lose = Next() % 4 ? Tick() : 0;
            DWORD t2 = Next() % 2 ? lose + Next() % 120 : Tick();
            CHECK((lose!=0 && !hasFocus && t2-lose>50) == ee_won_gui_wm_restore_due(lose, hasFocus, t2));
        }
        // DeliverEvent
        {
            bool focusIsWindow = B(), isPopup = B(), notModal = B();
            bool o = (!focusIsWindow && !isPopup && !notModal);
            CHECK(o == ee_won_gui_wm_press_focuses(focusIsWindow, isPopup, !focusIsWindow && !isPopup && notModal));
            int button = Next() % 3;
            bool popup = B(), support = B();
            CHECK((button==0 && (!popup || !support)) == ee_won_gui_wm_capture_changes(button, popup, support));
            int flags = Next() % 3 ? (int)(Next() % 0x8000) : Any();
            CHECK((popup && ((flags & (PopupFlag_EndOnMouseMove))?true:false)) == ee_won_gui_wm_move_ends_popup(popup, flags));
            CHECK((popup && !((flags & (PopupFlag_DontWantMouse))?true:false)) == ee_won_gui_wm_wheel_to_popup(popup, flags));
            int type = InputEvent_KeyDown + (int)(Next() % 3);
            int aKey = Next() % 3 ? VK_ESCAPE : (int)(Next() % 256);
            int ks = 0;
            do {
                if(popup)
                {
                    if(type==InputEvent_KeyDown)
                    {
                        if(((flags & (PopupFlag_EndOnKeyDown))?true:false) || ((flags & (PopupFlag_EndOnEscape))?true:false) && aKey==VK_ESCAPE)
                        { ks = 1; break; }
                    }
                    if(((flags & (PopupFlag_WantInput))?true:false))
                        ks = 2;
                }
            } while (0);
            CHECK(ks == ee_won_gui_wm_popup_key(popup, type, flags, aKey));
            int s1[4], s2[4], r2[4];
            for (int i = 0; i < 4; i++) { s1[i] = Any(); s2[i] = Next() % 2 ? -1 : Any(); r2[i] = s2[i]; }
            if(s2[0]==-1) s2[0] = s1[0];
            if(s2[1]==-1) s2[1] = s1[1];
            if(s2[2]==-1) s2[2] = s1[2];
            if(s2[3]==-1) s2[3] = s1[3];
            ee_won_gui_wm_size_merge(s1, r2);
            CHECK(r2[0] == s2[0] && r2[1] == s2[1] && r2[2] == s2[2] && r2[3] == s2[3]);
            bool isOwner = B();
            bool ends = false;
            if(popup)
                if(isOwner || isPopup)
                    ends = true;
            CHECK(ends == ee_won_gui_wm_close_ends_popup(popup, isOwner, isPopup));
        }
        // CheckPopupClick's walk: a chain of clicked children, each a component or a container
        {
            int depth = 1 + Next() % 5;
            bool hasInput = Next() % 4 != 0;
            int inputAt = hasInput && Next() % 2 ? (int)(Next() % depth) : -1;
            struct C { bool null, container; uint32_t bits; } chain[5];
            for (int i = 0; i < depth; i++) {
                chain[i].null = Next() % 6 == 0;
                chain[i].container = Next() % 4 != 0;
                chain[i].bits = Next() % 7 == 0 ? 0x10000u : (Next() % 3 ? 0 : Next() << 4);
            }
            bool endPopup = true;
            int at = 0;
            while(true)
            {
                if(at==depth) break;
                bool aClickChild = !chain[at].null;
                if(aClickChild)
                {
                    if(at==inputAt || ((chain[at].bits & 0x10000)?true:false))
                    {
                        endPopup = false;
                        break;
                    }
                    else if(!hasInput)
                        break;
                    else
                    {
                        if(!chain[at].container)
                            break;
                        at++;
                    }
                }
                else
                    break;
            }
            bool rEnd = true;
            int rAt = 0;
            while(true)
            {
                if(rAt==depth) break;
                uint8_t s = ee_won_gui_wm_popup_click_step(!chain[rAt].null, !chain[rAt].null && rAt==inputAt,
                                                           chain[rAt].null ? 0 : chain[rAt].bits, hasInput,
                                                           !chain[rAt].null && chain[rAt].container);
                if(s==1) { rEnd = false; break; }
                else if(s==0) break;
                rAt++;
            }
            CHECK(rEnd == endPopup);
            CHECK(rAt == at);
        }
    }
}

static void RootAndContainer()
{
    for (int n = 0; n < 1000000; n++) {
        // RootContainer
        {
            int theButton = Next() % 3;
            bool allow = B(), drags = B();
            CHECK(!(theButton!=0 || !allow || !drags) == ee_won_gui_root_drag_starts(theButton, allow, drags));
            int L = Any(), T = Any(), x = Any(), y = Any(), dx = Next() % 2 ? x : Any(), dy = Next() % 2 ? y : Any();
            int aNewX = L + (x - dx);
            int aNewY = T + (y - dy);
            bool moves = !(aNewX==L && aNewY==T);
            int rx = 0, ry = 0;
            CHECK(moves == ee_won_gui_root_drag_place(L, T, x, y, dx, dy, &rx, &ry));
            if (moves) CHECK(rx == aNewX && ry == aNewY);
            int wx = Any(), wy = Any(), oy = 0;
            CHECK(wx+aNewX == ee_won_gui_root_drag_window(wx, wy, aNewX, aNewY, &oy));
            CHECK(wy+aNewY == oy);
            CHECK((theButton==0) == ee_won_gui_root_drag_ends(theButton));
            int mx = Any(), my = Any(), cx = x, cy = y;
            cx-=mx; cy-=my;
            int py = 0;
            CHECK(cx == ee_won_gui_root_cursor_place(x, y, mx, my, &py));
            CHECK(cy == py);
        }
        // Container: UpdateMouseChild, MouseUp
        {
            static int comps[3];
            int* mMouseChild = Next() % 3 ? &comps[Next() % 2] : NULL;
            int* aChild = Next() % 3 ? &comps[Next() % 2] : NULL;
            int steps = 0;
            do {
                if(aChild==mMouseChild)
                    break;
                if(mMouseChild!=NULL)
                    steps |= 1;
                if(aChild!=NULL) steps |= 2;
            } while (0);
            CHECK(steps == ee_won_gui_ct_mouse_child(aChild==mMouseChild, mMouseChild!=NULL, aChild!=NULL));
            CHECK((aChild!=NULL && aChild==mMouseChild) == ee_won_gui_ct_release_to_child(aChild!=NULL, aChild==mMouseChild));
        }
        // GetDesiredSize, FindTop
        if (n % 4 == 0) {
            int count = Next() % 7;
            std::vector<int32_t> rects;
            int right = 0, bottom = 0;
            for (int i = 0; i < count; i++) {
                int l = Next() % 5 ? (int)(Next() % 300) - 40 : Any(), t = Next() % 5 ? (int)(Next() % 300) - 40 : Any();
                int w = Next() % 5 ? (int)(Next() % 200) : Any(), h = Next() % 5 ? (int)(Next() % 200) : Any();
                rects.push_back(l); rects.push_back(t); rects.push_back(w); rects.push_back(h);
                if(l+w > right)
                    right = l + w;
                if(t+h > bottom)
                    bottom = t + h;
            }
            int rh = 0;
            CHECK(right == ee_won_gui_ct_desired_size(count ? &rects[0] : NULL, count, &rh));
            CHECK(bottom == rh);
            std::list<int> kids;
            std::vector<uint8_t> flags;
            for (int i = 0; i < count; i++) { int f = Next() % 3 == 0; kids.push_back(f); flags.push_back((uint8_t)f); }
            int from = count ? (int)(Next() % (count + 1)) : 0;
            std::list<int>::iterator i = kids.begin();
            std::advance(i, from);
            // the original's FindTop
            std::list<int>::iterator o;
            if(i==kids.begin()) o = kids.begin();
            else {
                bool done = false;
                do {
                    --i;
                    if(!*i) { o = ++i; done = true; break; }
                } while(i!=kids.begin());
                if (!done) o = i;
            }
            size_t place = ee_won_gui_ct_find_top(count ? &flags[0] : NULL, count, from);
            CHECK((size_t)std::distance(kids.begin(), o) == place);
        }
    }
}

int main()
{
    WindowRules();
    ManagerRules();
    RootAndContainer();
    printf("won_guievent_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
