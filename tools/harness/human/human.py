"""TEST ONLY (round 39, agent input): `--human-script FILE` for --exercise ai-game: a scripted, deterministic human.

The in-process driver is tools/boot/human/human_input.c (staged as <stage>/TestHuman.dll, loaded by the presentation
bridge only when <stage>/human.cfg exists; see the C file's header for what it drives and how inputs are pinned to a
PT). This module parses the script, resolves the few game addresses the driver needs (original: fixed VAs from
analysis/exe/names.txt; full link: the LINK map by name), builds the DLL with MSYS2 mingw32 gcc into the stage and
writes human.cfg. After the run it copies human.log / human-state.txt into the evidence and summarises.

Script format (one action per line; '#' starts a comment; blank lines ignored):

    PT  OP  ARGS...           PT: the server PT the input rides in (absolute), or +N (N after the previous line)

  key KEYSPEC                     a key press through UIManager::UpdateUI (hot keys): e.g. `key h`, `key ctrl+1`,
                                  `key period`; names in human_input.c KEYS (letters, digits, f1..f12, period, comma,
                                  space, esc, tab, enter, delete, home, end, arrows, dikNN)
  click CTRL [right] [shown] [mod=..] left (or right) click at the centre of control CTRL of the TOP form (form 11 is the
                                  main game form: 0 diplomacy, 1 statistics, 2 chat, 3 options, 5 civ builder, 228..233
                                  stances, 236..243 formations, 244 cancel mode; see human-state.txt for the rest)
  button TECH|cmd:N [right]       click the shown status-box button of technology TECH (train / research / build) or
                                  of command button N
  lclick|rclick TARGET [mod=..]   click a world point in the game view (left: select / place; right: command)
  box X0 Y0 X1 Y1 [mod=..]        drag-select the tile rectangle (left button)
  minimap|rminimap TARGET         left (centre the camera) / right (send the selection) click on the mini map
  camera TARGET                   centre the camera on TARGET (a mini-map left click)
  dump [LABEL]                    append the UI/unit state to human-state.txt (no input)
  button pick:K|pickcmd:K|picktech:K [right]   the (K mod count)-th shown status button (any / commands / techs)
  type TEXT                       characters as the window procedure's WM_CHAR handler passes them
                                  (gUIManager->UpdateTranslatedKey); '_' types a space
  recover                         close a form over the main game form with its own cancel control (27: 186,
                                  28: 0, 7: 16, 35: 0, 29: a press outside it), else Escape (up to 4 rounds)

  TARGET: `X Y` (tile coordinates, floats) | `unit ID` | `of P TYPE [N]` (N-th live unit, list order, of player P with
  class type TYPE) | `mine TYPE [N]` (the local player's) | `anyof P TYPE K` (the (K mod count)-th live unit, TYPE < 0 any) ; `screen X Y` for lclick/rclick only (raw screen point).
  mod=shift|ctrl|alt (or shift+ctrl): modifier keys held during the click.
World points are converted to screen points with the game's own screen->world function (EEFn27A77A, Newton
iterations); a point outside the game view first centres the camera through the mini map. Unit types (class
mUnitType): see human-state.txt `player N units: id:type@x,y`.

EE_HUMAN_LOCKSTEP=1 (opt-in, timing only): the driver holds the server's CT lock from the game start and lets it send
the next CT only when physics has executed every PT already sent (the same gate it uses around each action). It arms
before the game runs (game server closed/running with the map loaded; PT-less CTs pass one at a time), so the gate
covers the first CT that carries PTs (log: `lockstep armed ... PT 0`, `lockstep on: server PT 14 ...`). Inputs
the game posts from inside a PT (EEMReplantFarms when any player's granary completes) then ride the next CT in every
run; without it a later-epoch start diverges at the first granary (the send thread's lead is wall-time).
"""
import hashlib, json, os, pathlib, re, subprocess

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
NAMES = ROOT / 'analysis/exe/names.txt'
GCC = pathlib.Path('C:/msys64/mingw32/bin/gcc.exe')

SYMBOLS = dict(
    ui_manager='?gUIManager@@3PAVEEUIManager@@A', world='?gWorld@@3VEEWorld@@A',
    communications='?gCommunications@@3PAVNESynchronousCommunications@@A',
    user_interface='?gUserInterface@@3PAVEEUserInterface@@A', mouse='?gMouse@@3PAVEEMouse@@A',
    physics='?gPhysics@@3PAVEEPhysics@@A',
    screen_to_world='?EEPointUnitOrTile@@YA_NABVGE2DPoint@@AAPAVEETile@@AAVGE3DPoint@@_N@Z',
    minimap_transformation='?gMiniMapTransformation@@3VGETransformation@@A',
    game_window='?gGameWindow@@3PAVGEWindow@@A',
    vt_minimap='??_7EEUIMiniMap@@6B@', vt_status_button='??_7EEUIFMGStatusButton@@6B@',
    vt_select_units='??_7EEUICSelectUnits@@6B@', vt_command_units='??_7EEUICCommandUnits@@6B@')
OPS = {'key', 'click', 'button', 'lclick', 'rclick', 'box', 'minimap', 'rminimap', 'camera', 'dump', 'type', 'recover'}


def parse_script(path):
    """[(line, pt, op, args)] sorted by PT (stable)."""
    out, last = [], 0
    for n, raw in enumerate(pathlib.Path(path).read_text(encoding='utf-8').splitlines(), 1):
        text = raw.split('#', 1)[0].strip()
        if not text:
            continue
        parts = text.split(None, 2)
        if len(parts) < 2:
            raise ValueError(f'{path}:{n}: expected "PT OP ARGS"')
        pt = last + int(parts[0][1:], 0) if parts[0].startswith('+') else int(parts[0], 0)
        op = parts[1].lower()
        if op not in OPS:
            raise ValueError(f'{path}:{n}: unknown op {op!r} (known: {sorted(OPS)})')
        args = parts[2] if len(parts) > 2 else ''
        if len(args) > 190:
            raise ValueError(f'{path}:{n}: arguments too long')
        out.append((n, pt, op, args))
        last = pt
    out.sort(key=lambda r: r[1])
    return out


def original_addresses():
    want = {v: k for k, v in SYMBOLS.items()}
    found = {}
    for line in NAMES.read_text(encoding='latin-1').splitlines():
        part = line.split()
        if len(part) >= 2 and part[1] in want and want[part[1]] not in found:
            found[want[part[1]]] = 0x400000 + int(part[0], 16)
    missing = set(SYMBOLS) - set(found)
    if 'game_window' in missing:   # include/ee/EmpireEarth.h: gGameWindow 0x531920
        found['game_window'] = 0x931920; missing.discard('game_window')
    if missing:
        raise ValueError(f'names.txt lacks {sorted(missing)}')
    return found


def fulllink_addresses(map_path):
    import sys
    sys.path.insert(0, str(HERE.parent))
    from fulllink_locator import LinkMap
    lm = LinkMap(map_path)
    return {k: lm.address(v) for k, v in SYMBOLS.items()}


REPORT_SYMBOL = '?ReportStatistics@EEPhysics@@QAEXXZ'
REPORT_RVA_ORIGINAL = 0x153836


def report_statics(image_path, linkmap_path=None):
    """(sLastReport VA, sReportPeriod VA) of EEPhysics::ReportStatistics (orig 0x553836): the first `mov [imm32], eax`
    is the function-local static sLastReport (its initialisation), the first `mov [imm32], edx` the file static
    sReportPeriod (rand() % 2500 + 2500). Original: 0x931cc8 / 0x931d00. Both images are read from their code."""
    import capstone, pefile
    pe = pefile.PE(str(image_path), fast_load=True)
    if linkmap_path:
        import sys
        sys.path.insert(0, str(HERE.parent))
        from fulllink_locator import LinkMap
        rva = LinkMap(linkmap_path).address(REPORT_SYMBOL) - 0x400000
    else:
        rva = REPORT_RVA_ORIGINAL
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
    last = period = None
    for ins in md.disasm(pe.get_data(rva, 0xe1), 0x400000 + rva):
        if ins.mnemonic == 'mov' and len(ins.operands) == 2 and ins.operands[0].type == capstone.x86.X86_OP_MEM                 and ins.operands[0].mem.base == 0 and ins.operands[0].mem.index == 0 and ins.operands[1].type == capstone.x86.X86_OP_REG:
            reg = ins.reg_name(ins.operands[1].reg)
            if reg == 'eax' and last is None: last = ins.operands[0].mem.disp & 0xffffffff
            if reg == 'edx' and period is None: period = ins.operands[0].mem.disp & 0xffffffff
    if not last or not period:
        raise ValueError(f'ReportStatistics at rva {rva:#x}: statics not found')
    return last, period


def build_dll(stage):
    stage = pathlib.Path(stage)
    out = stage / 'TestHuman.dll'
    src = HERE / 'human_input.c'
    subprocess.run([str(GCC), '-shared', '-O1', '-Wall', '-Wno-unused-function', '-Wno-format-truncation', '-static-libgcc', '-o', str(out), str(src),
                    '-Wl,--kill-at'], check=True, env=dict(os.environ, PATH=str(GCC.parent) + os.pathsep + os.environ['PATH']))
    return out, hashlib.sha256(src.read_bytes()).hexdigest(), hashlib.sha256(out.read_bytes()).hexdigest()


def prepare(ctx, script, settle_ms=150, margin=0, no_civ_wait=False):
    stage = pathlib.Path(ctx['stage'])
    actions = parse_script(script)
    addresses = fulllink_addresses(ctx['map']) if ctx.get('map') else original_addresses()
    image = pathlib.Path(ctx['image_path'])
    addresses['last_report'], addresses['report_period'] = report_statics(image, ctx.get('map'))
    if not ctx.get('map') and (addresses['last_report'], addresses['report_period']) != (0x931cc8, 0x931d00):
        raise ValueError('original ReportStatistics statics moved?')
    dll, src_sha, dll_sha = build_dll(stage)
    lines = [f'{k} {v:x}' for k, v in sorted(addresses.items())]
    lines += [f'settle_ms {settle_ms:x}', f'margin {margin:x}']
    if no_civ_wait:   # multiplayer (session_mp): no civilization-builder preempt to wait for
        lines.append('no_civ_wait 1')
    lockstep = os.environ.get('EE_HUMAN_LOCKSTEP') == '1'
    if lockstep:      # opt-in (human_input.c "lockstep"): every CT gated on physics from the game start
        lines.append('lockstep 1')
    lines += [f'act {n} {pt} {op} {args}' for n, pt, op, args in actions]
    (stage / 'human.cfg').write_text('\n'.join(lines) + '\n', encoding='ascii')
    rec = dict(kind='scripted-human-input', test_only=True, script=str(pathlib.Path(script).resolve()),
               script_sha256=hashlib.sha256(pathlib.Path(script).read_bytes()).hexdigest(), actions=len(actions),
               driver_source_sha256=src_sha, driver_sha256=dll_sha, lockstep=lockstep,
               addresses={k: hex(v) for k, v in addresses.items()},
               how='input-device state + gUIManager->UpdateUI under the UI thread locks; each input pinned to the CT '
                   'starting at the scripted server PT by holding the server\'s mSendCommunicationsTickLock')
    print(f'TEST human script {script}: {len(actions)} actions; driver {dll.name} sha256 {dll_sha[:16]}', flush=True)
    return rec


ACT = re.compile(r'^(?:\[\d+\] )?act (\d+) line (\d+) PT (\d+) (\S+) (.*?) \| server PT (\d+) CT (\d+) PTs/CT (\d+) physics (\d+) '
                 r'stepped (\d+)( SKIPPED-CT| REPORT-RACE)? \| inputs \+(-?\d+) \| selected (\d+) \| (.*?)(?: \| queued (.*))?$')
STRAY = re.compile(r'^(?:\[\d+\] )?  stray input\(s\) queued at server PT (\d+) CT (\d+)(?: physics \d+)?: (.*)$')


def finish(ctx, rec, lines):
    stage, evidence = pathlib.Path(ctx['stage']), pathlib.Path(ctx['evidence'])
    log = stage / 'human.log'
    acts = []
    for name in ('human.log', 'human-state.txt', 'human.cfg'):
        if (stage / name).exists():
            (evidence / name).write_bytes((stage / name).read_bytes())
    if log.exists():
        for line in log.read_text(encoding='utf-8', errors='replace').splitlines():
            m = ACT.match(line)
            if m:
                acts.append(dict(index=int(m.group(1)), line=int(m.group(2)), pt=int(m.group(3)), op=m.group(4),
                                 args=m.group(5), server_pt=int(m.group(6)), ct=int(m.group(7)), pts_per_ct=int(m.group(8)),
                                 physics=int(m.group(9)), stepped=int(m.group(10)), skipped=m.group(11) == ' SKIPPED-CT', report_race=m.group(11) == ' REPORT-RACE',
                                 inputs=int(m.group(12)), selected=int(m.group(13)), result=m.group(14),
                                 queued=m.group(15) or ''))
            if 'DROPPED act ' in line:
                rec.setdefault('dropped', []).append(line.split('] ', 1)[-1])
            s = STRAY.match(line)
            if s:
                rec.setdefault('stray_inputs', []).append(dict(server_pt=int(s.group(1)), ct=int(s.group(2)), labels=s.group(3)))
    rec['performed'] = len(acts)
    rec['failed'] = [a for a in acts if 'FAILED' in a['result']]
    rec['late'] = [a['index'] for a in acts if a['server_pt'] >= a['pt'] + max(a['pts_per_ct'], 1)]
    rec['skipped_ct'] = [a['index'] for a in acts if a['skipped']]
    rec['report_race'] = [a['index'] for a in acts if a['report_race']]
    (evidence / 'human_actions.json').write_text(json.dumps(acts, indent=2) + '\n', encoding='utf-8')
    print(f'TEST human: {len(acts)}/{rec["actions"]} actions performed, {len(rec.get("dropped", []))} dropped (before the driver started), {len(rec["failed"])} failed, late {rec["late"]}, '
          f'skipped CT {rec["skipped_ct"]}', flush=True)
    return dict(performed=len(acts), failed=len(rec['failed']), late=rec['late'])
