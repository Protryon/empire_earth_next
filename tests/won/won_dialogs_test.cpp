// The lobby dialogs' decisions (WONLobby: CreateGameDialog.cpp, CreateRoomDialog.cpp, LanPortDialog.cpp,
// AddFriendDialog.cpp, ChooseNetworkAdapterDialog.cpp, StatusDialog.cpp): the original's statements (copied from
// the C++ before the port, the controls replaced by the numbers and flags they read) against ee_won's dialogs
// module through its C ABI, over generated inputs (a fixed LCG). Prints one line per failed check; the exit code is
// the number of failed checks.
#include <stdio.h>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 8086;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 2000) - 100;
    }
}

int main()
{
    for (int n = 0; n < 1000000; n++) {
        // CheckInput (create game / room), AddFriendDialog
        unsigned short nameLen = (unsigned short)(Next() % 3), pwLen = (unsigned short)(Next() % 3);
        bool pwShown = (Next() & 1) != 0;
        bool aEnable = true;
        if (nameLen == 0) aEnable = false;
        else if (pwShown && pwLen == 0) aEnable = false;
        CHECK(aEnable == ee_won_dlg_create_enabled(nameLen, pwShown, pwLen));
        CHECK((nameLen != 0) == ee_won_dlg_name_enabled(nameLen));
        // GetGame's skill, SetGameType
        int aSkill = (int)(Next() % 9) - 2, s = aSkill;
        if (n % 9 == 0) aSkill = s = Any();
        if (s < 0 || s >= 5) s = 0;
        CHECK(s == ee_won_dlg_skill_level(aSkill));
        int inType = (int)(Next() % 4);
        CHECK((inType == 2) == ee_won_dlg_internet_access(inType));
        // LanPortDialog::CheckInput
        int aPort = n % 3 == 0 ? Any() : (int)(Next() % 70000);
        unsigned short mGamePort = (unsigned short)(Next() % 3 == 0 ? aPort : (int)(Next() % 70000));
        CHECK((aPort != mGamePort && aPort > 4999 && aPort < 65536) == ee_won_dlg_lan_port_ok(aPort, mGamePort));
        // NetworkAdapterListItem::Compare, GetSelectedAddress
        long mHost = (long)(Next() % 3), aHost = (long)(Next() % 3);
        int o;
        if (mHost == aHost) o = 0; else if (mHost == 0) o = -1; else if (aHost == 0) o = 1; else o = 2;
        CHECK(o == ee_won_dlg_adapter_order((int32_t)mHost, (int32_t)aHost));
        bool sel = (Next() & 1) != 0;
        unsigned int host = Next(), local = Next();
        CHECK((sel ? host : local) == ee_won_dlg_selected_address(sel, host, local));
        // StatusDialog::SetText
        int textW = Any(), mW = (int)(Next() % 20), rootW = Any();
        int aBodyWidth = textW;
        int aMinWidth = mW * 30;
        if (aBodyWidth < aMinWidth) aBodyWidth = aMinWidth;
        if (aBodyWidth > rootW / 2) aBodyWidth = rootW / 2;
        CHECK(aBodyWidth == ee_won_dlg_status_body_width(textW, mW, rootW));
        int labelH = Any(), lineH = (int)(Next() % 30) + 1;
        int aNumLines = labelH / lineH, pad = -1;
        if (aNumLines < 5) pad = (5 - aNumLines) * lineH / 2;
        int rp = ee_won_dlg_status_pad(labelH, lineH);
        CHECK(aNumLines < 5 ? rp == pad : rp == -1);
        int newW = Any(), newH = Any(), rootH = Any(), wc = Any(), oldW = Any();
        bool extra = newH > newW || newH >= rootH;
        int ex = newW * 5 / 4 - wc - oldW, rex = 0;
        bool rextra = ee_won_dlg_status_extra_width(newW, newH, rootH, wc, oldW, &rex);
        CHECK(rextra == extra && (!extra || rex == ex));
        unsigned int now = Next() << 8, start = n % 2 ? now - (Next() % 100) : Next();
        CHECK((now - start < 50) == ee_won_dlg_status_young(now, start));
        int a = Any(), b = Any(), g = a - b;
        if (g < 0) g = 0;
        CHECK(g == ee_won_dlg_growth(a, b));
    }
    printf("won_dialogs_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
