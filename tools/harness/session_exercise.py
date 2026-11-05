"""Drive the staged game's own UI event queue, without Windows input injection."""
import json,pathlib,struct,time

EXERCISES=['ai-game']
REQUIRED_MILESTONES={'ai-game':{'main-menu','single-player','random-map-setup','ai-lobby','gameplay','ai-game-final'}}


def add_session_arguments(ap):
    """Options of --exercise ai-game (round 39, TEST ONLY): a random-map game of N computer players plus the idle local
    human (offline observer mode is refused by the lobby and would end the game at once; see ai_game.py)."""
    g=ap.add_argument_group('ai-game exercise (round 39)')
    g.add_argument('--ai-players',type=int,default=4,help='computer players in lobby slots 1..N (1..7; default 4)')
    g.add_argument('--map-type',help='random-map type: a lobby combo line (case-insensitive name) or #index')
    g.add_argument('--map-size',help='map size: a lobby combo line (e.g. Tiny, Small) or #index')
    g.add_argument('--difficulty',help='difficulty: a lobby combo line or #index')
    g.add_argument('--lobby',action='append',default=[],metavar='CONTROL=VALUE',
                   help='any other lobby combo (control index on form 26) set to a line name or #index, e.g. 209=#2')
    g.add_argument('--lobby-check',action='append',default=[],metavar='CONTROL=0|1',
                   help='TEST ONLY: a lobby check box (form 26) set through the bridge check op (UICheckBox::SetChecked, the box '
                        'fires its own event), e.g. 217=0 (lock teams off), 218=0 (lock speed off)')
    g.add_argument('--no-civ-preempt',action='store_true',
                   help='do not open+cancel the civilization builder at game start (then the civ-timer inputs at '
                        '240/270/300 game s land on wall-time-dependent PTs and change the computers\' mBB49)')
    g.add_argument('--continue-after-engine-end',action='store_true',
                   help="evidence only: after the game's own end (single player: the idle human's defeat) choose options -> "
                        "Return to Game; the clock runs on but nothing is simulated (see ai_game.py)")
    g.add_argument('--no-human-defeat',action='store_true',
                   help='TEST ONLY (owner-approved 2026-11-01): the harness debugger keeps the idle human from being marked '
                        'out/defeated (EEWorld::Fn1391F2 / EEPlayer::Fn16AF5C entries, logged, image unpatched; '
                        'tools/boot/no_human_defeat.py), so the computers fight to a finish; the run ends when <= 1 '
                        'computer has victory units (or at the game\'s own end)')
    g.add_argument('--max-game-seconds',type=float,default=0,
                   help='stop the ai-game exercise at this game time (0: run to the game end; the session --timeout still applies)')
    import save_load   # --save-at-pt / --load-save (TEST ONLY, round 39 saveload; tools/boot/save_load.py)
    save_load.add_arguments(g)
    import scenario_game   # --campaign (TEST ONLY, final validation lane scen; tools/boot/scenario_game.py)
    scenario_game.add_arguments(g)
    # TEST ONLY (round 39, agent input): the scripted human (tools/boot/human/human.py)
    g.add_argument('--human-script',type=pathlib.Path,
                   help='TEST ONLY: a scripted human player (tools/boot/human/human.py: "PT OP ARGS" lines); each input is '
                        'given through the game input path and pinned to the CT that starts at that server PT')
    g.add_argument('--stop-after-script',type=int,default=None,metavar='GAME_S',
                   help='with --human-script: end the play loop GAME_S game seconds after the driver logged "script complete" '
                        '(outcome script-complete; the per-PT streams cover the script and that tail)')
    g.add_argument('--human-settle-ms',type=int,default=150,help='with --human-script: real ms the UI gets after physics caught up, before each input')
    g.add_argument('--input-check',action='store_true',
                   help='tour/random-game with --play-seconds: after the play capture, press F3 twice through window '
                        'messages (WM_KEYDOWN/UP posted to the game window) and require the game to pause and resume: '
                        'the key travels the window layer, the Input System and the main form\'s hot key')


def _prepare_session(ctx):
    """--no-human-defeat (ai-game only): locate and check the two guarded functions in the staged image (original
    rvas fixed, full link by map name) and hand them to the harness debugger."""
    a=ctx['args']
    if getattr(a,'load_save',None) or getattr(a,'save_at_pt',None) is not None:
        if a.exercise!='ai-game':raise SystemExit('--save-at-pt / --load-save require --exercise ai-game')
    if getattr(a,'load_save',None):
        # TEST ONLY (save_load.py): the saved game goes into the stage's own Data/Saved Games (never the install)
        import save_load
        staged=save_load.stage_save(ctx['stage'],a.load_save,a.save_name)
        REQUIRED_MILESTONES['ai-game']={'main-menu','single-player','load-screen','ai-game-final'}
        print(f'TEST load-save: staged {a.load_save} as {staged}',flush=True)
    if getattr(a,'save_pinned',False):
        import save_load
        if a.save_at_pt is None or getattr(a,'human_script',None):raise SystemExit('--save-pinned needs --save-at-pt and no --human-script')
        a.human_script=save_load.pinned_script(ctx['evidence'],a.save_at_pt)
        print(f'TEST save-pinned: human script {a.human_script}',flush=True)
    if getattr(a,'campaign',None) or getattr(a,'scenario_file',None):
        if a.exercise!='ai-game' or getattr(a,'load_save',None):raise SystemExit('--campaign/--scenario-file require --exercise ai-game and no --load-save')
        if a.campaign and a.scenario_file:raise SystemExit('--campaign and --scenario-file are exclusive')
        if a.scenario_file:
            import scenario_game
            staged=scenario_game.stage_scenario(ctx['stage'],a.scenario_file)
            print(f'TEST scenario-file: staged {a.scenario_file} as {staged}',flush=True)
            REQUIRED_MILESTONES['ai-game']={'main-menu','single-player','random-map-setup','scenario-lobby','ai-game-final'}
        else:REQUIRED_MILESTONES['ai-game']={'main-menu','single-player','campaign-select','scenario-intro','ai-game-final'}
        # scenario loading writes archive members to GetTempPath (FSMemoryFile::FOpen): keep it inside the stage
        import os
        os.environ['TMP']=os.environ['TEMP']=str(pathlib.Path(ctx['stage']).resolve())
        print(f'TEST campaign: TMP/TEMP of the game process = {os.environ["TMP"]}',flush=True)
    if not getattr(a,'no_human_defeat',False):return None
    if a.exercise!='ai-game':raise SystemExit('--no-human-defeat requires --exercise ai-game')
    import no_human_defeat
    (out,result,world),extra=no_human_defeat.harness_args(ctx['stage'],ctx.get('map'))
    rec=no_human_defeat.record(out,result,world);ctx['no_human_defeat']=rec
    print(f'TEST no-human-defeat: {no_human_defeat.OUT_SYMBOL} rva {out:#x}, {no_human_defeat.RESULT_SYMBOL} rva {result:#x}, '
          f'gWorld {world:#x} (local player only; image unpatched)',flush=True)
    return dict(harness_extra=extra,interventions=[rec])


def _finish_session(ctx,lines):
    rec=ctx.get('no_human_defeat')
    if rec is None:return None
    import no_human_defeat
    no_human_defeat.finish(rec,lines)   # the same dict is in the session json's test_interventions
    print(f'TEST no-human-defeat: {rec["hits"]} hits, {rec["skips"]} skipped'+(f'; {rec["stops"]}' if rec['stops'] else ''),flush=True)
    return dict(no_human_defeat=dict(hits=rec['hits'],skips=rec['skips']))


# Rendering gate (tour/random-game): a capture only proves that some frame was presented, and the UI draws through
# the same device as the 3D view, so a black game view passed for a long time (x64: a NaN light fade). Regions are
# fractions of the frame, laid out on the 800x600 game screen: the game view between the top bar and the bottom
# panel, left of the mini map frame; the mini map's inside. Calibrated on correct x64/i686 frames (view: 78% lit,
# ~3,000 colours; mini map at the start: 0.8-1.3% terrain pixels, grass and snow maps) and on the black x64 frames
# (view: 0.1% lit, 181 colours: the cursor; their mini map was right).
VIEW_REGION=(0.0,0.06,0.80,0.88)        # x0, y0, x1, y1
MINIMAP_REGION=(0.85,0.74,0.995,0.935)
VIEW_MIN_LIT=0.30                       # share of pixels with a channel above 24
VIEW_MIN_COLOURS=512                    # distinct colours at 5 bits per channel (not one uniform fill)
MINIMAP_MIN_TERRAIN=0.002               # share of lit pixels that are not the white view outline or a blue/red unit dot


def game_window(pid):
    """The game's main window (class "Empire Earth Expansion", visible, the largest) of process pid -> HWND or None"""
    import ctypes,ctypes.wintypes as w
    user=ctypes.windll.user32
    found=[]
    @ctypes.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
    def visit(hwnd,_):
        owner=w.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        name=ctypes.create_unicode_buffer(64);user.GetClassNameW(hwnd,name,64)
        if owner.value==pid and name.value=='Empire Earth Expansion' and user.IsWindowVisible(hwnd):
            r=w.RECT();user.GetClientRect(hwnd,ctypes.byref(r));found.append(((r.right-r.left)*(r.bottom-r.top),hwnd))
        return True
    user.EnumWindows(visit,0)
    return max(found)[1] if found else None


def press_key(hwnd,vk,scan):
    """A key press and release posted to the window (the window layer reads WM_KEYDOWN/UP as a keyboard would)"""
    import ctypes
    user=ctypes.windll.user32
    user.PostMessageW(hwnd,0x100,vk,1|(scan<<16))
    time.sleep(.3)
    user.PostMessageW(hwnd,0x101,vk,1|(scan<<16)|0xC0000000)


def input_check(pid,addresses):
    """F3 (the main game form's pause hot key) twice: paused, then running again -> evidence dict; raises on failure"""
    from game_state_probe import sample
    hwnd=game_window(pid)
    if not hwnd:raise RuntimeError('input check: no visible game window')
    states=[sample(pid,addresses)['paused']]
    for _ in range(2):
        press_key(hwnd,0x72,0x3d)   # VK_F3, scan code DIK_F3
        end=time.monotonic()+15
        while time.monotonic()<end:
            time.sleep(.5)
            now=sample(pid,addresses)['paused']
            if now!=states[-1]:break
        states.append(now)
    ok=states==[False,True,False]
    print(f'INPUT CHECK F3 paused {states[0]} -> {states[1]} -> {states[2]}: {"ok" if ok else "FAILED"}',flush=True)
    if not ok:raise RuntimeError(f'input check failed: paused states {states}')
    return dict(key='F3',paused=states,ok=ok)


def frame_regions(pixels):
    """Measures the game view and the mini map of a 24/32-bit BMP (bytes): lit share, colours, terrain share."""
    off=struct.unpack_from('<I',pixels,10)[0];w,h=struct.unpack_from('<ii',pixels,18);bpp=struct.unpack_from('<H',pixels,28)[0]
    if bpp not in (24,32):raise RuntimeError(f'Rendering check: unsupported {bpp}-bit frame')
    b=bpp//8;stride=(w*b+3)&~3;H=abs(h)
    def measure(region):
        x0,y0,x1,y1=int(region[0]*w),int(region[1]*H),int(region[2]*w),int(region[3]*H)
        n=lit=terrain=0;colours=set()
        for y in range(y0,y1):
            row=off+((H-1-y) if h>0 else y)*stride
            for x in range(x0,x1):
                blue,green,red=pixels[row+x*b:row+x*b+3];n+=1
                top=max(red,green,blue)
                if top>24:lit+=1
                colours.add((red>>3,green>>3,blue>>3))
                if top>24 and min(red,green,blue)<235 and blue<=2*max(red,green) and red<=2*max(green,blue):terrain+=1
        return dict(pixels=n,lit=lit/n,colours=len(colours),terrain=terrain/n)
    return dict(width=w,height=H,view=measure(VIEW_REGION),minimap=measure(MINIMAP_REGION))


def check_rendering(pixels,label,minimap):
    """Fails the session when the game view is black or uniform (and, after play, when the mini map shows no terrain)."""
    found=frame_regions(pixels);view=found['view'];problems=[]
    if view['lit']<VIEW_MIN_LIT or view['colours']<VIEW_MIN_COLOURS:
        problems.append(f"game view black or uniform: {view['lit']:.1%} lit (need {VIEW_MIN_LIT:.0%}), "
                        f"{view['colours']} colours (need {VIEW_MIN_COLOURS})")
    if minimap and found['minimap']['terrain']<MINIMAP_MIN_TERRAIN:
        problems.append(f"mini map shows no terrain: {found['minimap']['terrain']:.2%} terrain pixels (need {MINIMAP_MIN_TERRAIN:.1%})")
    print(f"RENDER CHECK {label}: view {view['lit']:.1%} lit, {view['colours']} colours; mini map "
          f"{found['minimap']['terrain']:.2%} terrain{'' if minimap else ' (not checked)'}: {'FAIL' if problems else 'ok'}",flush=True)
    found.update(label=label,minimap_checked=minimap,problems=problems)
    if problems:raise RuntimeError(f'Rendering check failed on {label}: '+'; '.join(problems))
    return found


def exercise(stage,evidence,mode,addresses=None,play_seconds=0,options=None):
    stage,evidence=pathlib.Path(stage),pathlib.Path(evidence)
    evidence.mkdir(parents=True,exist_ok=True)
    results=[];seq=0
    def state():
        try:
            text=(stage/'ui-state.txt').read_text()
            lines=text.splitlines()
            return int(lines[0].split()[1]),int(lines[1].split()[1]),text
        except (OSError,ValueError,IndexError):return None
    def wait(form=None,ack=0,timeout=120):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            found=state()
            if found and (form is None or found[0]==form) and found[1]>=ack:return found
            time.sleep(.25)
        raise RuntimeError(f'UI milestone timed out: form={form}, command={ack}; last={state()}')
    def capture(label,expected):
        found=wait(expected,seq)
        # The command is enqueued, then processed by the original UI thread.
        # Wait for a subsequent actual presented frame before recording pixels.
        stamp=(stage/'render.bmp').stat().st_mtime_ns
        end=time.monotonic()+15
        while time.monotonic()<end and (stage/'render.bmp').stat().st_mtime_ns<=stamp:time.sleep(.25)
        if (stage/'render.bmp').stat().st_mtime_ns<=stamp:raise RuntimeError('No subsequent framebuffer presented')
        time.sleep(.5)
        end=time.monotonic()+15
        while True:
            pixels=(stage/'render.bmp').read_bytes()
            if len(pixels)>=54 and pixels[:2]==b'BM' and struct.unpack_from('<I',pixels,2)[0]==len(pixels):break
            if time.monotonic()>end:raise RuntimeError('Incomplete framebuffer capture')
            time.sleep(.25)
        if len(set(pixels[54:]))<16:raise RuntimeError('Framebuffer capture is blank')
        (evidence/(label+'.bmp')).write_bytes(pixels)
        (evidence/(label+'.txt')).write_text(found[2],encoding='utf-8')
        results.append(dict(milestone=label,form=found[0],command=seq))
        (evidence/'milestones.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
        print(f'UI MILESTONE {label}: form {found[0]}',flush=True)
        return pixels
    def trigger(control,expected,label):
        nonlocal seq
        seq+=1;(stage/'test.command').write_text(f'{seq} {control}\n',encoding='ascii')
        capture(label,expected)
    capture('main-menu',12)
    if mode=='ai-game':
        from ai_game import run_ai_game
        def command(text):
            nonlocal seq
            seq+=1;(stage/'test.command').write_text(f'{seq} {text}\n',encoding='ascii')
            return seq
        trigger(0,6,'single-player')
        if not getattr(options,'load_save',None) and not getattr(options,'campaign',None):
            trigger(1,26,'random-map-setup')   # --load-save: ai_game loads; --campaign: scenario_game starts
        try:
            run_ai_game(stage,evidence,addresses,options,dict(state=state,wait=wait,capture=capture,command=command,
                                                             seq=lambda:seq),results,play_seconds)
        finally:
            (stage/'stop.request').write_text('exercise complete\n',encoding='ascii')
        return results
    if mode in ('settings','tour'):
        trigger(3,5,'settings-video')
        trigger(2,5,'settings-audio')
        trigger(3,5,'settings-interface')
        trigger(19,12,'settings-closed')
    if mode in ('random-game','tour'):
        trigger(0,6,'single-player')
        trigger(1,26,'random-map-setup')
        time.sleep(5) # Allow local-server readiness and queued lobby updates to settle.
        seq+=1;(stage/'test.command').write_text(f'{seq} 10\n',encoding='ascii')
        print('UI ACTION Start Game queued',flush=True)
        gameplay=wait(11,seq,timeout=150)
        from game_state_probe import sample
        pid=int(next(line.split()[1] for line in gameplay[2].splitlines() if line.startswith('pid ')))
        before=sample(pid,addresses);end=time.monotonic()+60
        while time.monotonic()<end:
            time.sleep(3);after=sample(pid,addresses)
            if after['world_players']>=2 and after['world_map']!='0x0' and after['physics_ticks_processed']>before['physics_ticks_processed']:break
        else:raise RuntimeError('Loaded gameplay/physics-processing telemetry did not advance')
        (evidence/'simulation.json').write_text(json.dumps(dict(samples=[before,after],physics_advanced=True),indent=2)+'\n',encoding='utf-8')
        rendered=[check_rendering(capture('gameplay',11),'gameplay',False)]
        print('GAME LAUNCHED: world rendered; physics-processing counter advanced; physical input and unit orders not tested',flush=True)
        if play_seconds:   # opt-in: keep the unattended game running, sampling the read-only telemetry every 10 s
            # the civilization timer (240/270/300 game s) opens a civilization form over the game and pauses it; a
            # long play reaches it: cancel it as the AI game does (ai_game.DISMISS: no simulation input)
            from ai_game import DISMISS

            def dismiss():
                nonlocal seq
                found=state()
                if found and found[0] in DISMISS:
                    seq+=1;(stage/'test.command').write_text(f'{seq} {DISMISS[found[0]]}\n',encoding='ascii')
                    wait(11,seq,timeout=30)
                    print(f'PLAY civilization form {found[0]} cancelled',flush=True)
            samples=[after];end=time.monotonic()+play_seconds
            while time.monotonic()<end:
                time.sleep(10);dismiss();samples.append(sample(pid,addresses))
                print(f"PLAY t+{len(samples)*10-10}s physics_ticks={samples[-1]['physics_ticks_processed']} paused={samples[-1]['paused']}",flush=True)
            (evidence/'play.json').write_text(json.dumps(dict(seconds=play_seconds,samples=samples),indent=2)+'\n',encoding='utf-8')
            time.sleep(2);dismiss()   # the form may open between the last sample and the capture
            # the mini map shows the explored terrain around the start once the game has run (checked from 10 s of play)
            rendered.append(check_rendering(capture('play',11),'play',play_seconds>=10))
            if getattr(options,'input_check',False):
                (evidence/'input.json').write_text(json.dumps(input_check(pid,addresses),indent=2)+'\n',encoding='utf-8')
        (evidence/'rendering.json').write_text(json.dumps(rendered,indent=2)+'\n',encoding='utf-8')
        (stage/'stop.request').write_text('exercise complete\n',encoding='ascii')
    else:
        (stage/'stop.request').write_text('exercise complete\n',encoding='ascii')
    return results


def prepare_session(ctx):
    """_prepare_session, plus --human-script (TEST ONLY, round 39 agent input; tools/boot/human/human.py)."""
    result=_prepare_session(ctx)
    a=ctx['args']
    if getattr(a,'human_script',None):
        if a.exercise!='ai-game':raise SystemExit('--human-script requires --exercise ai-game')
        import sys;sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent/'human'))
        import human
        # a scenario / campaign / loaded game has no civilization-builder preempt (ai_game): the driver must not wait for it
        no_civ=bool(getattr(a,'campaign',None) or getattr(a,'scenario_file',None) or getattr(a,'load_save',None))
        rec=human.prepare(ctx,a.human_script,a.human_settle_ms,no_civ_wait=no_civ);ctx['human']=rec
        result=dict(result or {});result['interventions']=list(result.get('interventions',()))+[rec]
    return result


def finish_session(ctx,lines):
    result=_finish_session(ctx,lines)
    if ctx.get('human') is not None:
        import human
        result=dict(result or {});result['human']=human.finish(ctx,ctx['human'],lines)
    return result
