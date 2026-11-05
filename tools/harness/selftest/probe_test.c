/* TEST ONLY: game_state_probe.py's target in the harness self test (selftest_inproc.py): a stand-in "EE-AOC.exe"
 * with gCommunications, gWorld and gPhysics laid out at the offsets the build's record layouts give for the members
 * the probe reads (probe_offsets.h, written by selftest_inproc.py from layout.json of the architecture). It fills
 * known values, writes probe.ready and waits to be killed. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "probe_offsets.h"

static unsigned char communications[0x400], physics[0x400], players[3][PLAYER_BYTES], map[16];
unsigned char selftest_world_object[WORLD_BYTES];   /* gWorld (an object) */
#define world selftest_world_object
void *selftest_communications = communications;     /* gCommunications (a pointer) */
void *selftest_physics = physics;                   /* gPhysics (a pointer) */

#define PUT(buf, off, type, v) (*(type *)((buf) + (off)) = (type)(v))

int main(void)
{
    int i;
    PUT(communications, NESC_LAST_TICK, long, 1234);
    PUT(communications, NESC_TICKS, unsigned long, 5678);
    PUT(communications, NESC_PAUSED, unsigned char, 1);
    PUT(communications, NESC_MP, unsigned char, 0);
    PUT(physics, PHYS_MS, long, 4321);
    PUT(physics, PHYS_OVER, unsigned char, 0);
    PUT(physics, PHYS_PT, unsigned long, 99);
    PUT(world, WORLD_COUNT, unsigned long, 3);
    PUT(world, WORLD_LOCAL, long, 1);
    PUT(world, WORLD_VICTORY, unsigned char, 1);
    PUT(world, WORLD_MAP, void *, map);
    for (i = 1; i < 3; i++) {
        unsigned char *p = players[i];
        PUT(world, WORLD_PLAYERS + i * sizeof(void *), void *, p);
        PUT(p, 0, void *, (void *)(ULONG_PTR)(0x1000 + i));   /* the vtable slot: read back as a pointer */
        PUT(p, PLAYER_OBSERVER, unsigned char, i == 2);
        PUT(p, PLAYER_RESULT, unsigned long, i);
        for (int r = 0; r < 5; r++) PUT(p, PLAYER_RESOURCES + 4 * r, long, 10 * i + r);
        PUT(p, PLAYER_LB14, long, 7 * i);
        PUT(p, PLAYER_LB18, long, 8 * i);
        PUT(p, PLAYER_BB49, unsigned char, 1);
        PUT(p, PLAYER_BB4A, unsigned char, 0);
    }
    printf("world %p\n", (void *)world);
    FILE *f = fopen("probe.ready", "w");
    if (f) { fprintf(f, "%p\n", (void *)world); fclose(f); }
    Sleep(INFINITE);
    return 0;
}
