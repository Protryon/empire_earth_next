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


def locate(image_bytes,linkmap=None):
    """(out_rva, result_rva, world_va) after checking both bodies; raises ValueError on anything unexpected."""
    import pefile
    pe=pefile.PE(data=image_bytes,fast_load=True)
    if linkmap:
        out=linkmap.address(OUT_SYMBOL)-0x400000;result=linkmap.address(RESULT_SYMBOL)-0x400000;world=linkmap.address(WORLD_SYMBOL)
    else:out,result,world=OUT_RVA_ORIGINAL,RESULT_RVA_ORIGINAL,WORLD_VA_ORIGINAL
    body=_first_ret(pe,out)
    text=[(i.mnemonic,i.op_str) for i in body]
    size=body[-1].address+body[-1].size-0x400000-out
    want=[('mov','ebx, dword ptr [esp + 0xc]'),('mov','edi, dword ptr [esi + ebx*4 + 0x74]'),
          ('mov','eax, dword ptr [esi + 0xb84]'),('cmp','edi, dword ptr [esi + eax*4 + 0x74]'),('ret','8')]
    if size!=0x72 or text[0]!=('push','ebx') or any(w not in text for w in want) or sum(m=='call' for m,_ in text)!=4:
        raise ValueError(f'{OUT_SYMBOL} at rva {out:#x}: unexpected body ({size:#x} bytes); refusing --no-human-defeat')
    body=_first_ret(pe,result)
    text=[(i.mnemonic,i.op_str) for i in body]
    stores=[o for m,o in text if m=='mov' and o.endswith(f'+ {RESULT_OFF:#x}], ebx')]
    refs=[o for m,o in text if f'{world+CURRENT_OFF:#x}]' in o]+[o for m,o in text if f'*4 + {world+PLAYERS_OFF:#x}]' in o]
    if (text[:3]!=[('push','ebp'),('mov','ebp, esp'),('sub','esp, 0x5c')] or text[-1]!=('ret','4') or len(stores)!=1
            or ('mov','ebx, dword ptr [ebp + 8]') not in text or ('cmp','ebx, 2') not in text or len(refs)!=2):
        raise ValueError(f'{RESULT_SYMBOL} at rva {result:#x}: unexpected body; refusing --no-human-defeat')
    return out,result,world


def harness_args(stage,fulllink_map=None):
    from fulllink_locator import LinkMap
    out,result,world=locate((pathlib.Path(stage)/'EE-AOC.exe').read_bytes(),LinkMap(fulllink_map) if fulllink_map else None)
    return (out,result,world),('--no-human-defeat',f'{out:x}',f'{result:x}',f'{world:x}',f'{PLAYERS_OFF:x}',f'{CURRENT_OFF:x}')


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
