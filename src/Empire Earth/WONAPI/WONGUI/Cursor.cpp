// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
using namespace WONAPI;
// R6 WON GUI round 35: the cursors are cursorobj.rs's (the kinds, the standard cursors' table); this is what they ask
// of the C++: the references a cursor keeps (a CursorRef's cursor, a ComponentCursor's component), another cursor's
// answers, and the handles
intptr_t WONAPI::WONCursorHost(EeWonCursorOp op, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonCursorOp Op;
    switch (op) {
    case Op::Hold: ((Cursor*)a)->AddRef(); return 0;
    case Op::Release: ((Cursor*)a)->Release(); return 0;
    case Op::CompHold: ((Component*)a)->AddRef(); return 0;
    case Op::CompRelease: ((Component*)a)->Release(); return 0;
    case Op::Native: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::Component: return (intptr_t)((Cursor*)a)->GetComponent(*(int*)b, *(int*)c);
    case Op::Wrap: return (intptr_t)new Cursor((EeWonCursor*)a);
    case Op::RustOf: return (intptr_t)((Cursor*)a)->mRust;
    }
    return 0;
}
