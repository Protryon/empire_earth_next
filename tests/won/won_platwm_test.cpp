// PlatWindowManager as a whole Rust object (R6 WON round 40): PlatWindowManager.cpp's statements copied verbatim
// before the port (won_platwm_orig.h, with its class and PlatGUIWindow::GetClientRect) against the port's shell and
// host over ee_won's platwm.rs (won_platwm_port.h, copied from the working tree), both over the same stand-in world
// (won_platwm_world.h: windows behaving as PlatGUIWindow's methods do, the WONAPI classes the manager reads, the
// platform's window system; won_platwm_cases.h: the windows' methods and the runner). The caption font is the
// platform layer's (ee_plat_ui) with the test face of its goldens registered.
//
// 2,000 worlds from seeds: 1..6 windows (parents, frames, popups, topmost, visible, modal or not, titles, offscreen
// images of any size), a game window's client size, an event forward or none; then 60 random operations each: windows
// added, removed, raised, focused, captured, hit-tested; platform events of every type (mouse moves, presses with
// every button and click count, releases, wheels; keys with every modifier, Alt+Tab, F10, control characters; text
// of 0..64 bytes, invalid UTF-8, a character past U+FFFF; focus, close, quit, window events); the surface's size and
// pixel scale from the UI thread and another (pending until the next pass); the present rectangle; passes of the
// event loop with and without an event; paints and composites; the colour scheme; the screen; wakes; windows shown,
// hidden, closed and retitled. After each, the trace (every call on a window, its events, the platform calls, the
// forwards) and a snapshot (focus, capture, surface, scale, the pointer, every window's place and flags, the frame's
// size, pitch, serial and pixel hash) must be the same. Exit code: the number of mismatching worlds.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSurface.h"
#include "platform/PlatWindow.h"
#include "platform/PlatThread.h"
#include "platform/PlatFont.h"
#include "platform/PlatString.h"
#include "platform/PlatTime.h"
#include "rust/common.h"
#include "rust/won_gui/barobj.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/comboobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/ctobj.h"
#include "rust/won_gui/dialogobj.h"
#include "rust/won_gui/fontmap.h"
#include "rust/won_gui/gfxobj.h"
#include "rust/won_gui/grabobj.h"
#include "rust/won_gui/guicfg.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guiedit.h"
#include "rust/won_gui/guievent.h"
#include "rust/won_gui/guifocus.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/guilist.h"
#include "rust/won_gui/guimenu.h"
#include "rust/won_gui/guimulti.h"
#include "rust/won_gui/guiscroll.h"
#include "rust/won_gui/guistring.h"
#include "rust/won_gui/guistrobj.h"
#include "rust/won_gui/guitext.h"
#include "rust/won_gui/guitree.h"
#include "rust/won_gui/guiwin.h"
#include "rust/won_gui/guiwrap.h"
#include "rust/won_gui/imgdec.h"
#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/inputobj.h"
#include "rust/won_gui/labelobj.h"
#include "rust/won_gui/layoutobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/menuobj.h"
#include "rust/won_gui/msbtnobj.h"
#include "rust/won_gui/mspaint.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/paneobj.h"
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platfont.h"
#include "rust/won_gui/platgfx.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
#include "rust/won_gui/rootobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/skinlay.h"
#include "rust/won_gui/splitobj.h"
#include "rust/won_gui/stretchobj.h"
#include "rust/won_gui/surface.h"
#include "rust/won_gui/tabobj.h"
#include "rust/won_gui/textobj.h"
#include "rust/won_gui/treeobj.h"
#include "rust/won_gui/winobj.h"
#include "rust/won_gui/wmcache.h"
#include "rust/won_gui/wmloop.h"
#include "rust/won_gui/wmobj.h"
#include "rust/won_gui/wonbtnobj.h"
#include "rust/won_gui/wrapobj.h"
#include "rust/won_lobby/chatobj.h"
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/eeresource.h"
#include "rust/won_lobby/eesound.h"
#include "rust/won_lobby/eexfer.h"
#include "rust/won_lobby/ffi_lan.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbymap.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/persist.h"
#include "rust/won_lobby/prefsobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_lobby/routing.h"
#include "rust/won_lobby/scrolltext.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/stagingobj.h"
#include "rust/won_lobby/statusstr.h"
#include "rust/won_net/admin.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/auth.h"
#include "rust/won_net/authcpp.h"
#include "rust/won_net/authctx.h"
#include "rust/won_net/authflow.h"
#include "rust/won_net/authlogin.h"
#include "rust/won_net/authobj.h"
#include "rust/won_net/blowfish.h"
#include "rust/won_net/buffer.h"
#include "rust/won_net/cachemap.h"
#include "rust/won_net/cdkeystore.h"
#include "rust/won_net/cert.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/chat.h"
#include "rust/won_net/config.h"
#include "rust/won_net/configobj.h"
#include "rust/won_net/db.h"
#include "rust/won_net/debug.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/dirchange.h"
#include "rust/won_net/dirops.h"
#include "rust/won_net/elgamal.h"
#include "rust/won_net/ffi.h"
#include "rust/won_net/ffi_net.h"
#include "rust/won_net/ffi_ops.h"
#include "rust/won_net/ffi_parse.h"
#include "rust/won_net/file.h"
#include "rust/won_net/filepush.h"
#include "rust/won_net/ftp.h"
#include "rust/won_net/ftpop.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/fwop.h"
#include "rust/won_net/gamespy.h"
#include "rust/won_net/getcert.h"
#include "rust/won_net/hostauth.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostnative.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/http.h"
#include "rust/won_net/httpcache.h"
#include "rust/won_net/httpcpp.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpengine.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/ipobj.h"
#include "rust/won_net/md5.h"
#include "rust/won_net/miscops.h"
#include "rust/won_net/multiping.h"
#include "rust/won_net/ops.h"
#include "rust/won_net/patch.h"
#include "rust/won_net/path.h"
#include "rust/won_net/peerauth.h"
#include "rust/won_net/peerobj.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/perftimer.h"
#include "rust/won_net/qsock.h"
#include "rust/won_net/random.h"
#include "rust/won_net/regauth.h"
#include "rust/won_net/regkey.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqlast.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/reqop.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routeconn.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routemsg.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/servconn.h"
#include "rust/won_net/servctx.h"
#include "rust/won_net/server.h"
#include "rust/won_net/smartroute.h"
#include "rust/won_net/sockobj.h"
#include "rust/won_net/sockop.h"
#include "rust/won_net/sockops.h"
#include "rust/won_net/sockthread.h"
#include "rust/won_net/strcase.h"
#include "rust/won_net/titlecfg.h"
#include "rust/won_net/udpmgr.h"
#include "rust/won_net/udpops.h"
#include "rust/won_net/userstatus.h"
#include "rust/won_net/verop.h"
#include "rust/won_net/wonfile.h"

// the world's window system in place of the platform's (renamed: argument-dependent lookup on PlatWindow* and
// PlatInputEvent* would find the real ones too), and the manager's protected members reachable from the runner
#define PlatWindowWaitEvent WorldWaitEvent
#define PlatWindowGetClientSize WorldGetClientSize
#define PlatWindowMinimize WorldMinimize
#define protected public

#define EE_PLATWM_COMMON \
inline int PlatGUIPixels(int inLogical, float inScale) \
{ return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f); } \
static std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength) \
{ \
    std::string theText; \
    if (inText == NULL || inLength == 0) return theText; \
    uint32_t theCount = PlatWideToUtf8(inText, inLength, NULL, 0); \
    theText.resize(theCount); \
    if (theCount != 0) PlatWideToUtf8(inText, inLength, &theText[0], theCount); \
    return theText; \
}

namespace Orig {
#include "won_platwm_world.h"
EE_PLATWM_COMMON
#include "won_platwm_orig.h"
#include "won_platwm_cases.h"
}
namespace Port {
#include "won_platwm_world.h"
EE_PLATWM_COMMON
#include "won_platwm_port.h"
#include "won_platwm_cases.h"
}

int main(int argc, char** argv)
{
    std::string ttf = std::string(__FILE__);
    ttf = ttf.substr(0, ttf.find_last_of("/\\")) + "/../../rust/crates/ee_base/src/plat_ui/golden_font.ttf";
    std::ifstream in(ttf.c_str(), std::ios::binary);
    static std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty() || !PlatFontRegister("Synth", 0, 0, data.data(), (uint32_t)data.size())) { printf("no test font at %s\n", ttf.c_str()); return 1; }
    PlatFontRegister("Synth", 1, 0, data.data(), (uint32_t)data.size());
    int worlds = argc > 1 ? atoi(argv[1]) : 2000, failures = 0;
    long ops = 0, lines = 0;
    for (int n = 0; n < worlds; n++) {
        unsigned long long seed = 0x51ED270B27F3A1ull * (n + 1);
        int k = 0;
        if (getenv("PLATWM_DEBUG")) fprintf(stderr, "world %d orig\n", n);
        std::string a = Orig::Run(seed, k);
        if (getenv("PLATWM_DEBUG")) fprintf(stderr, "world %d port\n", n);
        std::string b = Port::Run(seed, k);
        ops += k;
        lines += (long)std::count(a.begin(), a.end(), '\n');
        if (a != b) {
            if (++failures < 4) {
                size_t i = 0;
                while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
                size_t s = a.rfind("\nop ", i);
                s = s == std::string::npos ? 0 : s + 1;
                printf("MISMATCH world %d at byte %zu\n--- original\n%s\n--- port\n%s\n", n, i, a.substr(s, i - s + 300).c_str(),
                       b.substr(s, i - s + 300).c_str());
            }
        }
    }
    printf("%d worlds, %ld operations, %ld trace lines, %d mismatching worlds\n", worlds, ops, lines, failures);
    return failures;
}
