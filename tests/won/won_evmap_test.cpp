// The lobby shells' event choices (R6 WONLobby round 29: ee_won_lobby evmap.rs) against the original switches
// (won_evmap_orig.h) over every type from -1,000 to 200,000 (the window events, the component events, the
// contexts and the lobby's 100001..): the kind each chooses must be the same. The exit code is the number of
// mismatches.
#include <stdio.h>
#include <stdint.h>
#include "ee/wonapi/WONEventTypes.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "rust/won_lobby/evmap.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbypopup.h"
#include "won_evmap_orig.h"

int main()
{
    int aFailures = 0, aChecks = 0, aNamed[3] = {0, 0, 0};
    for (int t = -1000; t <= 200000; t++) {
        WonEvmapOrig::ComponentEvent e = {t, NULL};
        WonEvmapOrig::LobbyRustDialog d = {-1};
        WonEvmapOrig::LobbyRustPopup p = {-1};
        d.HandleComponentEvent(&e);
        p.HandleComponentEvent(&e);
        const uint32_t aOrig[3] = {(uint32_t)WonEvmapOrig::CtlEventOf(t), (uint32_t)d.mKind, (uint32_t)p.mKind};
        for (int m = 0; m < 3; m++) {
            aChecks++;
            uint32_t aRust = ee_won_lobby_event_of(m, t);
            if (aOrig[m] != 0) aNamed[m]++;
            if (aRust != aOrig[m]) {
                if (aFailures++ < 20) printf("FAIL map %d type %d: original %u, Rust %u\n", m, t, aOrig[m], aRust);
            }
        }
    }
    printf("won_evmap_test: %d checks, types named %d/%d/%d, %d mismatches\n", aChecks, aNamed[0], aNamed[1], aNamed[2],
           aFailures);
    return aFailures;
}
