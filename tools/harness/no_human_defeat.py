"""TEST ONLY (round 39, owner-approved 2026-11-01): `--no-human-defeat` for --exercise ai-game.

Keeps the idle local human from ever being marked out/defeated, so a 4-AI game runs to an AI-vs-AI finish. The image is
not patched: the harness debugger (exe_boot.c --no-human-defeat) guards two function entries and returns from them at
once for the local player only, logging every hit (`TEST no-human-defeat ...`):
- EEWorld::Fn1391F2(bool victorious, unsigned long player) (orig 0x5391f2, ret 8): the "player out" transition
  (Fn13C2A2 sets EEPlayer::mB467 and the options slot type 2, Fn13BC81 takes the player out of its team, mB465, ...).
  EEWorld::Fn13C9B4 (the per-PT game-end check) calls it for a player with mB467 clear and no victory units
  (mLB18 <= 0); its "alone" path (no human with mB467 clear) is what ended single-player games at the human's defeat.
  Skipped when player == gWorld.mCurrentPlayer and victorious is false.
- EEPlayer::Fn16AF5C(long state) (orig 0x56af5c, ret 4): the only in-game writer of the result EEPlayer::mUA28 (+0xa28;
  1 victorious, 2 defeated; the other stores are EEPlayer::Reset and file Load). Skipped when this is the local player
  (gWorld.mPlayers[gWorld.mCurrentPlayer]) and state == 2.
The human's units may still die. Original rvas are fixed; the full link's come from its map by name; both bodies are
checked (length, ret n, the mCurrentPlayer/mPlayers operands, the +0xa28 store) before a guard is armed.
"""
import pathlib

OUT_SYMBOL='?RemovePlayerFromGame@EEWorld@@QAEX_NK@Z'
RESULT_SYMBOL='?SetGameResult@EEPlayer@@QAEXJ@Z'
WORLD_SYMBOL='?gWorld@@3VEEWorld@@A'
OUT_RVA_ORIGINAL=0x1391f2
RESULT_RVA_ORIGINAL=0x16af5c
WORLD_VA_ORIGINAL=0x930d40
PLAYERS_OFF=0x74          # EEWorld::mPlayers[17]
CURRENT_OFF=0xb84         # EEWorld::mCurrentPlayer
RESULT_OFF=0xa28          # EEPlayer::mUA28


def _first_ret(pe,rva,limit=0x400):
    import capstone
    md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=False
    out=[]
    for ins in md.disasm(pe.get_data(rva,limit),0x400000+rva):
        out.append(ins)
        if ins.mnemonic=='ret':return out
    raise ValueError(f'no ret within {limit:#x} bytes of rva {rva:#x}')


def locate(image_bytes,linkmap):
    """(out_rva, result_rva, world_va) by map name. ee checks both bodies against VC6's code first; every image this
    fork's harness runs is clang-built (other instructions, same functions), so the guard relies on the map and on
    the test profile keeping both functions out of line (tools/build.py HARNESS_HOOKS)."""
    return (linkmap.rva(OUT_SYMBOL),linkmap.rva(RESULT_SYMBOL),linkmap.address(WORLD_SYMBOL))


def harness_args(stage,fulllink_map=None):
    from fulllink_locator import LinkMap
    from layout import off
    linkmap=LinkMap(fulllink_map)
    out,result,world=locate(None,linkmap)
    # world is a VA of the map's image base (exe_boot.c moves it by the load delta: --image-base, game_session.py)
    return (out,result,world),('--no-human-defeat',f'{out:x}',f'{result:x}',f'{world:x}',
                               f"{off('EEWorld',PLAYERS_OFF):x}",f"{off('EEWorld',CURRENT_OFF):x}")


def record(out,result,world):
    return dict(kind='no-human-defeat',symbols=[OUT_SYMBOL,RESULT_SYMBOL],rvas=[hex(out),hex(result)],world=hex(world),
                rule='skip Fn1391F2(false, local player) and Fn16AF5C(2) on the local player (ret 8 / ret 4 at entry)')


def finish(rec,lines):
    armed=[l for l in lines if l.startswith('TEST no-human-defeat armed')]
    hits=[l for l in lines if l.startswith(('TEST no-human-defeat skip','TEST no-human-defeat pass','STOP no-human-defeat',
                                            'STOP cannot guard no-human-defeat'))]
    rec['armed']=armed
    skips=[l for l in hits if ' skip ' in l]
    rec.update(hits=len(hits),skips=len(skips),passes=[l for l in hits if ' pass ' in l],
               first=hits[:12],last=hits[-6:],stops=[l for l in hits if l.startswith('STOP')])
    return rec
