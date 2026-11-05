"""TEST ONLY (round 39): `--exercise ai-game`, an unattended offline random-map game of N computer players.

Driven through the presentation bridge's UI fixture (presentation_bridge.c: `click`/`select` commands on the top form;
the controls fire their own trigger events into the application's UI queue). Telemetry is read-only
(game_state_probe.sample(..., players=True)).

The local human stays in its slot and idles. Offline observer mode was checked and is not usable:
- the host's player-type combo (control 30, menu 3) offers [name, OBSERVE]; OBSERVE is player type 2
  (EEUIELobbyEvents.cpp LobbyShowHostRow / PlayerTypeFromMenu), which the game setup turns into EEPlayer::mB467 = 1;
- LobbyCheckStart (0x5da876) counts only types 0/1 and refuses a game without a type-1 human (string 10058 "You need
  at least 1 Human Player to play a game"); setrilo's LobbyCanStart (base game) agrees;
- even past that, EEWorld::Fn13C9B4 (0x53c9b4, the game-end check) counts humans among the players with mB467 clear:
  with none it ends the game at once ("alone": every player marked victorious). The in-game "resign and become an
  observer" (strings 8540/10044) sets mB467 the same way (WorldPlayerOut) and would end an all-computer game too.
Engine end: every player 1..n has a result (EEPlayer::mUA28: 1 victorious, 2 defeated); EEWorld::Fn13C9B4 assigns all
of them when the game ends and EEPhysics::Fn151F3A pauses the game and sets mBE6. In single player the idle human's
defeat IS the engine end: the defeat marks the human out (EEWorld::Fn1391F2 -> Fn13C2A2 sets mB467), no human is left,
and Fn13C9B4 takes its "alone" path, marking every computer still in victorious (seen: batch3 full4, 4 computers all
"victorious" at the human's defeat, game s 1755). The run stops there (outcome 'game-end'): that is the complete
game. Tried (batch5 orig4): --continue-after-engine-end goes on the way a player would (in-game options, main-form
control 3 -> form 28, "Return to Game", control 0). The form closes and the game clock (gPhysics->mLD0) races on, but
the game does not visibly go on: gPhysics->mUB4 (completed PTs) stays frozen and every computer's unit count (mLB14)
stayed the same for 9,000 game s (whether any PT still runs is for the per-PT sampler to show). The option is kept for evidence only; it then ends when <= 1 computer has victory units (never, in
practice) or at --max-game-seconds.
--no-human-defeat (owner-approved 2026-11-01, TEST ONLY; tools/boot/no_human_defeat.py): the harness debugger skips the
local player's out transition (EEWorld::Fn1391F2) and defeat result (EEPlayer::Fn16AF5C(2)), so the human stays "in"
with no units and the computers fight on. Fn13C9B4 still defeats each computer that runs out of victory units, but
with the human in (its own team) it declares no winner: the run ends 'last-computer-standing' once <= 1 computer has
victory units and the others have their results (summary 'winner', 'defeat_order'), or 'game-end' on the game's own
end (gPhysics->mBE6).
"""
import json,pathlib,re,time

LOBBY=26;GAME=11
PLAYER_TYPE=30            # + slot: player-type combo (slot 0: host menu [name, OBSERVE]; others [Open, Computer, Closed])
NAMED={'map_size':177,'map_type':179,'difficulty':207}
VISIBLE_SLOTS=8           # LobbyShowName shows slots < 8
TEAM=62                   # + slot: team combo [-, 1, 2, 3, 4]
CITIZENS=126              # + slot: EEUIELEChangeCitizenBonus combo [1..20] (EEGameOptionsPlayer::mL2C = line + 1)
# Every game option is set absolutely (the lobby otherwise shows the last game's options from the registry).
# Values are lobby line names (the retail Language.dll strings), overridable by --map-type/--map-size/--difficulty and
# --lobby CONTROL=VALUE. Defaults are the baseline lobby's values except map size Small (owner: small map).
DEFAULTS={175:'Random Map',177:'Small',179:'Continental',181:'Standard - High',183:'Prehistoric Age',185:'Space Age',
          187:'1200',189:'Off',205:'Standard',207:'Easy',209:'Very Fast'}

# Modal forms seen over the game and the deterministic action that closes each without a simulation input.
# 0x23 (35): in-game civilization selection, pushed by EEProcessTimerExpired (0x67fa40) when the civ timer
# (EEMCivTimerExpired, 300 game s) expires; its form-open event pauses the game. Control 0 is EEUIEPDCSCancel (UIManager
# Pop only; no EEMCivUpgradesRequested), control 1 is Ok (would buy a civilization: a simulation input). Cancel.
# 7: the Civilization Builder, which the same timer opens in these offline random-map games (EEProcessTimerExpired's
# gB531256 branch: Fn280FAF pauses the game, sets mBB4A so the timer does not open it again, and pushes form 7).
# Seen at game ms 301875 (run1). Control 16 is EEUIECBCancelChanges (0x67cf3d: Fn27FF7C(false): unpause, no Apply, close);
# control 17 is AcceptChanges (would request the civilization's upgrades: a simulation input). Cancel.
# An unknown form is recorded (form<id>.txt in the evidence) and, if it stalls physics for 90 s, fails the run.
DISMISS={35:'click 35 0',7:'click 7 16'}
OPTIONS=28                # in-game options (EEUIEMGDisplayOptions on main-form control 3); control 0 = EEUIEMGOEReturnToGame


def parse_state(text):
    out=dict(form=None,command=None,pid=None,stack=[],controls={},combos={},checks={})
    for line in text.splitlines():
        part=line.split(' ')
        try:
            if part[0]=='form':out['form']=int(part[1])
            elif part[0]=='command':out['command']=int(part[1])
            elif part[0]=='pid':out['pid']=int(part[1])
            elif part[0]=='stack':out['stack']=[int(v) for v in part[1:]]
            elif part[0]=='control':out['controls'][int(part[1])]=part[3]
            elif part[0]=='check':out['checks'][int(part[1])]=int(part[2])
            elif part[0]=='combo':
                head,_,tail=line.partition(' lines ')
                count,_,names=tail.partition(' ')
                lines=names.split('|') if int(count) else []
                lines=[re.sub(r'\\x([0-9a-f]{2})',lambda m:chr(int(m.group(1),16)),v) for v in lines]
                out['combos'][int(part[1])]=dict(sel=int(head.split()[3]),lines=lines)
        except (IndexError,ValueError):pass
    return out


def resolve(value,lines,what):
    value=str(value)
    if value.startswith('#'):
        index=int(value[1:])
        if not 0<=index<len(lines):raise ValueError(f'{what}: index {index} outside {lines}')
        return index
    low=[v.strip().lower() for v in lines];key=value.strip().lower()
    if key in low:return low.index(key)
    hits=[k for k,v in enumerate(low) if v.startswith(key)] or [k for k,v in enumerate(low) if key in v]
    if len(hits)!=1:raise ValueError(f'{what}: {value!r} matches {[lines[k] for k in hits]} of {lines}')
    return hits[0]


def run_ai_game(stage,evidence,addresses,options,ui,results,play_seconds):
    from game_state_probe import sample
    stage,evidence=pathlib.Path(stage),pathlib.Path(evidence)
    n=getattr(options,'ai_players',4) if options else 4
    if not 1<=n<VISIBLE_SLOTS:raise ValueError('--ai-players must be 1..7')
    addresses=dict(addresses or {})
    if options is not None and getattr(options,'fulllink',None):
        from fulllink_locator import LinkMap
        addresses['physics']=LinkMap(pathlib.Path(options.fulllink).resolve().with_suffix('.map')).address('?gPhysics@@3PEAVEEPhysics@@EA')
    log=evidence/'ai_game.log'
    def note(text):
        print(text,flush=True)
        with open(log,'a',encoding='utf-8') as f:f.write(f'{time.strftime("%H:%M:%S")} {text}\n')
    def current():
        # ui-state.txt is replaced atomically once a second; a read can still miss it (sharing violation): retry
        end=time.monotonic()+10
        while True:
            found=ui['state']()
            if found:return parse_state(found[2])
            if time.monotonic()>end:return None
            time.sleep(.1)
    def raw():
        end=time.monotonic()+10
        while True:
            found=ui['state']()
            if found or time.monotonic()>end:return found[2] if found else ''
            time.sleep(.1)
    loading=bool(getattr(options,'load_save',None)) if options else False
    save_at=getattr(options,'save_at_pt',None) if options else None
    import save_load
    scenario=bool(getattr(options,'campaign',None) or getattr(options,'scenario_file',None)) if options else False
    if loading:
        # --load-save (TEST ONLY, save_load.py): single player -> Load Game -> the staged file -> Load; no lobby
        st=save_load.load_from_menu(ui,current,raw,note,options,evidence,ui['capture'])
        chosen=[]
    elif scenario:
        # --campaign (TEST ONLY, scenario_game.py): single player -> campaign -> scenario select -> intro -> Start
        import scenario_game
        start=scenario_game.start_from_lobby if getattr(options,'scenario_file',None) else scenario_game.start_from_menu
        st,picked=start(ui,current,raw,note,options,evidence,ui['capture'])
        chosen=[picked];loading=True   # from here on like a loaded game: no lobby, no civ pre-empt, players from the file
    else:
        st=None
    # 1. lobby: wait until the player rows and the option combos are listed
    end=time.monotonic()+60
    while not loading:
        st=current()
        if st and st['form']==LOBBY and all(PLAYER_TYPE+s in st['combos'] for s in range(VISIBLE_SLOTS)) and all(c in st['combos'] for c in NAMED.values()):break
        if time.monotonic()>end:raise RuntimeError(f'lobby combos not listed: {st and sorted(st["combos"])}')
        time.sleep(.5)
    if not loading:
        time.sleep(3)   # local-server readiness and queued lobby updates (as the random-game fixture)
        (evidence/'lobby_before.txt').write_text(raw(),encoding='utf-8')
        wanted=[]       # (control, value, label), applied in order, absolute
        wanted.append((PLAYER_TYPE,'#0','slot 0 human'))   # [name, OBSERVE]: the human stays a player (see module doc)
        for s in range(1,VISIBLE_SLOTS):
            wanted.append((PLAYER_TYPE+s,'Computer' if s<=n else 'Open',f'slot {s}'))
        options_by_control=dict(DEFAULTS)
        for key,control in NAMED.items():
            value=getattr(options,key,None) if options else None
            if value:options_by_control[control]=value
        for item in (getattr(options,'lobby',[]) if options else []):
            control,_,value=item.partition('=');options_by_control[int(control)]=value
        for s in range(n+1):   # the playing slots: no team (free for all), 5 starting citizens (baseline)
            options_by_control.setdefault(TEAM+s,'-');options_by_control.setdefault(CITIZENS+s,'5')
        for control,value in options_by_control.items():wanted.append((control,value,f'control {control}'))
        chosen=[]
        for control,value,label in wanted:
            st=current();combo=st['combos'].get(control)
            if combo is None:raise RuntimeError(f'{label}: control {control} is not a combo on form {st["form"]}')
            index=resolve(value,combo['lines'],label)
            if combo['sel']!=index:
                seq=ui['command'](f'select {LOBBY} {control} {index}');ui['wait'](LOBBY,seq,timeout=30)
                end=time.monotonic()+20
                while True:
                    st=current()
                    if st['combos'].get(control,{}).get('sel')==index:break
                    if time.monotonic()>end:raise RuntimeError(f'{label}: selection {index} did not stick ({st["combos"].get(control)})')
                    time.sleep(.5)
                time.sleep(1)
            chosen.append(dict(control=control,label=label,value=value,index=index,line=combo['lines'][index]))
            note(f'LOBBY {label}: control {control} = {index} {combo["lines"][index]!r}')
        for item in (getattr(options,'lobby_check',[]) if options else []):   # TEST ONLY: --lobby-check CONTROL=0|1
            control,_,value=item.partition('=');control=int(control);value=int(value)
            st=current()
            if control not in st['checks']:raise RuntimeError(f'lobby check {control} is not a check box on form {st["form"]}: {sorted(st["checks"])}')
            if st['checks'][control]!=value:
                seq=ui['command'](f'check {LOBBY} {control} {value}');ui['wait'](LOBBY,seq,timeout=30)
                end=time.monotonic()+20
                while current()['checks'].get(control)!=value:
                    if time.monotonic()>end:raise RuntimeError(f'lobby check {control} = {value} did not stick')
                    time.sleep(.5)
            note(f'LOBBY check {control} = {value}')
        time.sleep(2)
        st=current()
        drift=[c for c in chosen if st['combos'].get(c['control'],{}).get('sel')!=c['index']]
        if drift:raise RuntimeError(f'lobby selections changed after setting: {drift}')
        (evidence/'lobby.json').write_text(json.dumps(dict(chosen=chosen,combos=st['combos'],checks=st['checks']),indent=2)+'\n',encoding='utf-8')
        ui['capture']('ai-lobby',LOBBY)
        # 2. start
        seq=ui['command'](f'click {LOBBY} 10');note('UI ACTION Start Game queued')
        deadline=time.monotonic()+600
        while True:
            st=current()
            if st and st['form']==GAME and st['command']>=seq:break
            if st and st['form'] not in (LOBBY,GAME) and st['form'] is not None and st['command']>=seq:
                # the lobby's check refused (a message form) or a loading form: record what is on top
                pass
            if time.monotonic()>deadline:raise RuntimeError(f'game form not reached; last {st and st["form"]} stack {st and st["stack"]}')
            time.sleep(.5)
    pid=st['pid']
    preempt=None
    if not loading and not (options is not None and getattr(options,'no_civ_preempt',False)):
        # The civilization timer (EEPhysics::CivilizationTimer) sends EEMCivTimerExpired at 240/270/300 game s through the
        # local input path, so the PT where it executes depends on wall time (hash agent: inputs counter and the computers'
        # mBB49 differ between two original runs from PT ~11214). Execute (0x4bd008) does nothing when the local player's
        # mBB4A is set. Opening the civilization builder (main-form control 5, EEUIEMGDisplayCivBuilder) sets mBB4A (and
        # the local mBB49) like a player who looks at it; Cancel sends nothing. Done at once so only those two flags of
        # the idle human change, early, on the UI thread.
        before=sample(pid,addresses,players=True)
        seq=ui['command'](f'click {GAME} 5');end=time.monotonic()+30
        while True:
            st=current()
            if st and st['form'] in DISMISS and st['command']>=seq:break
            if time.monotonic()>end:raise RuntimeError(f'civilization builder did not open (form {st and st["form"]})')
            time.sleep(.25)
        opened=sample(pid,addresses,players=True);form=st['form']
        seq=ui['command'](DISMISS[form]);end=time.monotonic()+30
        while True:
            st=current()
            if st and st['form']==GAME and st['command']>=seq:break
            if time.monotonic()>end:raise RuntimeError(f'civilization form {form} did not close (form {st and st["form"]})')
            time.sleep(.25)
        after=sample(pid,addresses,players=True)
        local=next(r for r in after['player_rows'] if r['index']==after['local_player'])
        preempt=dict(form=form,pt_before=before.get('pt_ub4'),pt_open=opened.get('pt_ub4'),pt_after=after.get('pt_ub4'),
                     game_ms_open=opened.get('game_ms'),civ_chosen_bb4a=local['civ_chosen_bb4a'],civ_wait_bb49=local['civ_wait_bb49'])
        note(f'CIV PREEMPT form {form} opened at PT {opened.get("pt_ub4")} (game ms {opened.get("game_ms")}), cancelled; '
             f'local mBB4A {local["civ_chosen_bb4a"]} mBB49 {local["civ_wait_bb49"]}')
        if not local['civ_chosen_bb4a']:raise RuntimeError('civilization preempt did not set the local mBB4A')
    stop_after=getattr(options,'stop_after_script',None) if options and getattr(options,'human_script',None) else None;script_done_ms=None
    first=sample(pid,addresses,players=True);end=time.monotonic()+120
    while True:
        time.sleep(2);now=sample(pid,addresses,players=True)
        if now['world_players']>=(2 if scenario else n+2) and now['world_map']!='0x0' and now['physics_ticks_processed']>first['physics_ticks_processed']:break
        if time.monotonic()>end:raise RuntimeError(f'gameplay telemetry did not advance: {now}')
    note(f'GAME players {now["world_players"]} (world + human + {n} computers expected) local {now["local_player"]}')
    if not scenario and now['world_players']!=n+2:raise RuntimeError(f'expected {n+2} world players, got {now["world_players"]}')
    if scenario:   # big scenarios under --no-render may present no frame within the capture's 15 s: best effort
        try:ui['capture']('gameplay',GAME)
        except RuntimeError as error:note(f'CAPTURE gameplay skipped: {error}')
    else:ui['capture']('gameplay',GAME)
    # 3. play to the end
    samples=[now];forms={};modal_since=None;last_ticks=(now.get('pt_ub4'),now.get('game_ms'));last_advance=time.monotonic()
    started=time.monotonic();last_keep=0;results_seen={}
    max_game_ms=(getattr(options,'max_game_seconds',0) or 0)*1000 if options else 0
    outcome=None;engine_end=None;continuing=False;cstate=None;cstamp=0;resume=None
    keep_going=bool(getattr(options,'continue_after_engine_end',False)) if options else False
    no_defeat=bool(getattr(options,'no_human_defeat',False)) if options else False
    if no_defeat and keep_going:raise ValueError('--no-human-defeat and --continue-after-engine-end are exclusive')
    standing=0;defeat_order=[]
    last_note=time.monotonic()
    save_rec=None
    if loading and not scenario:
        save_rec=dict(loaded=str(options.load_save),first_pt=now.get('pt_ub4'),game_ms=now.get('game_ms'),paused=now['paused'])
        note(f'LOADED {options.load_save}: PT {now.get("pt_ub4")} game ms {now.get("game_ms")} paused {now["paused"]}')
    def dump(state):
        (evidence/'ai_game.json').write_text(json.dumps(dict(state=state,forms=forms,lobby=chosen,samples=samples),indent=2)+'\n',encoding='utf-8')
    while True:
        time.sleep(1)
        try:st=current();now=sample(pid,addresses,players=True)
        except OSError as error:
            dump(f'telemetry failed: {error}');raise RuntimeError(f'telemetry failed (game process gone?): {error}')
        ticks=now['physics_ticks_processed']
        pinned=bool(getattr(options,'save_pinned',False))
        if save_at is not None and save_rec is None and st and (st['form']==save_load.SAVE_FORM if pinned else
                                                               (now.get('pt_ub4') or 0)>=save_at and st['form']==GAME):
            # --save-at-pt (TEST ONLY, save_load.py): the in-game Save Game screen; the game pauses while it is open
            save_rec=save_load.do_save(ui,current,raw,note,options,stage,evidence,pid,addresses,sample)
            if getattr(options,'stop_after_save',False):outcome='saved';break
            save_load.close_save_screen(ui,current,raw,note,options,evidence,pid,addresses,sample,save_rec)
            (evidence/'save.json').write_text(json.dumps(save_rec,indent=2)+'\n',encoding='utf-8')
            last_advance=time.monotonic();continue
        # progress: game clock or completed PTs (gPhysics->mUB4 stops counting after the engine end; the clock goes on)
        if (now.get('pt_ub4'),now.get('game_ms'))!=last_ticks:last_ticks=(now.get('pt_ub4'),now.get('game_ms'));last_advance=time.monotonic()
        if time.monotonic()-last_keep>=10:samples.append(now);last_keep=time.monotonic()
        if time.monotonic()-last_note>=60:
            last_note=time.monotonic()
            note(f'PLAY PT {now.get("pt_ub4")} CT {ticks} game s {(now.get("game_ms") or 0)/1000:.0f} form {st and st["form"]} paused {now["paused"]} units '
                 + ' '.join(f'{r["index"]}:{r["lb14"]}' for r in now['player_rows'] if r['index']>=1))
        for row in now['player_rows']:
            if row['result_a28'] and results_seen.get(row['index'])!=row['result_a28']:
                results_seen[row['index']]=row['result_a28']
                note(f'RESULT player {row["index"]} {"victorious" if row["result_a28"]==1 else "defeated" if row["result_a28"]==2 else row["result_a28"]} at game ms {now.get("game_ms")} PT {now.get("pt_ub4")} CT {ticks}')
                if row['result_a28']==2:defeat_order.append(dict(player=row['index'],pt=now.get('pt_ub4'),game_ms=now.get('game_ms')))
        if st and st['form']!=GAME:
            if st['form'] not in forms:
                forms[st['form']]=dict(first_game_ms=now.get('game_ms'),first_pt=now.get('pt_ub4'),first_ct=ticks,stack=st['stack'])
                (evidence/f'form{st["form"]}.txt').write_text(raw(),encoding='utf-8')
                note(f'UI MODAL form {st["form"]} stack {st["stack"]} at game ms {now.get("game_ms")} PT {now.get("pt_ub4")} CT {ticks}')
            action=DISMISS.get(st['form']) if not (continuing and st['form']==OPTIONS) else None
            if action and (modal_since is None or time.monotonic()-modal_since>5):
                seq=ui['command'](action);modal_since=time.monotonic()
                forms[st['form']].setdefault('dismissed',[]).append(dict(action=action,game_ms=now.get('game_ms'),pt=now.get('pt_ub4'),ct=ticks))
                note(f'UI DISMISS form {st["form"]}: {action}')
        else:modal_since=None
        players=[r for r in now['player_rows'] if r['index']>=1]
        local=now['local_player']
        computers=[r for r in players if r['index']!=local]
        alive=[r['index'] for r in computers if r['victory_units_lb18']>0]
        if no_defeat and players:
            # --no-human-defeat: the harness skips the local player's out/defeat transition, so the human never gets a
            # result and the single-player "alone" end cannot happen. The game still ends on its own (gPhysics->mBE6, e.g.
            # a wonder victory) or, normally, the last computer is left: the game's own check (EEWorld::Fn13C9B4) marks
            # every computer without victory units defeated, but with the human still in (and in its own team) it
            # declares no winner, so the run stops once <= 1 computer has victory units and every other computer has its
            # result (two consecutive samples).
            human=next((r for r in players if r['index']==local),None)
            if human and human['result_a28']:
                dump('no-human-defeat failed');raise RuntimeError(f'--no-human-defeat: the local player got result {human["result_a28"]}')
            if now.get('game_over_be6'):outcome='game-end';break
            if len(alive)<=1 and all(r['result_a28'] for r in computers if r['index'] not in alive):
                standing+=1
                if standing>=2:outcome='last-computer-standing';break
            else:standing=0
        elif engine_end is None and players and all(r['result_a28'] for r in players):
            engine_end=dict(pt=now.get('pt_ub4'),ct=ticks,game_ms=now.get('game_ms'),
                            results={r['index']:r['result_a28'] for r in players},alive_computers=alive)
            human=next((r for r in players if r['index']==local),None)
            if keep_going and human and human['result_a28']==2 and len(alive)>=2:
                # Single player: the idle human's defeat ends the game (EEWorld::Fn13C9B4 "alone": no human with mB467
                # clear; every other player is marked victorious) and EEPhysics::Fn151F3A pauses it. The game itself
                # offers to go on ("Continue playing the current game"): options form, Return to Game. The game
                # checks no further results after that (mScenarioManager->mL200 is set); the harness ends the run when
                # at most one computer has victory units left (EEPlayer::mLB18, the count the game's own defeat test uses).
                note(f'ENGINE END at PT {engine_end["pt"]} (human defeated; computers alive {alive}): continuing the game')
                continuing=True;cstate='open';cstamp=time.monotonic();ui['command'](f'click {GAME} 3')
            else:
                outcome='game-end';break
        if continuing:
            if cstate=='open' and st and st['form']==OPTIONS:
                ui['command'](f'click {OPTIONS} 0');cstate='return';cstamp=time.monotonic()
            elif cstate=='return' and st and st['form']==GAME:
                cstate='check';cstamp=time.monotonic();resume=dict(pt=now.get('pt_ub4'),game_ms=now.get('game_ms'))
            elif cstate=='check' and now.get('game_ms')!=resume['game_ms']:
                cstate='running';resume['resumed']=True;last_advance=time.monotonic()
                note(f'CONTINUE: game clock running again from game ms {resume["game_ms"]} (PT counter mUB4 stays {resume["pt"]})')
            elif cstate in ('open','return','check') and time.monotonic()-cstamp>30:
                dump(f'continue failed in state {cstate}')
                raise RuntimeError(f'continue after the engine end failed (state {cstate}, form {st and st["form"]})')
            if cstate in ('open','return','check'):last_advance=time.monotonic()
            if cstate=='running' and len(alive)<=1:outcome='last-computer-standing';break
        if stop_after is not None and now.get('game_ms') is not None:   # --stop-after-script (TEST ONLY): the human driver's log
            if script_done_ms is None:
                hl=stage/'human.log'
                if hl.exists() and 'script complete' in hl.read_text(encoding='utf-8',errors='replace'):
                    script_done_ms=now['game_ms'];note(f'HUMAN script complete at game ms {script_done_ms}')
            elif now['game_ms']>=script_done_ms+stop_after*1000:outcome='script-complete';break
        if max_game_ms and (now.get('game_ms') or 0)>=max_game_ms:outcome='time-cap';break   # --max-game-seconds (per-player units in the summary's players)
        if play_seconds and time.monotonic()-started>=play_seconds:outcome='play-seconds';break
        if time.monotonic()-last_advance>90:
            dump('physics stalled')
            raise RuntimeError(f'physics stalled 90 s at PT {now.get("pt_ub4")} CT {ticks} (form {st and st["form"]} stack {st and st["stack"]}); forms {forms}')
    samples.append(now)
    time.sleep(3)
    final=sample(pid,addresses,players=True);samples.append(final)
    rows=final['player_rows']
    summary=dict(outcome=outcome,final_pt=final.get('pt_ub4'),final_ct=final['physics_ticks_processed'],game_ms=final.get('game_ms'),
                 game_over_be6=final.get('game_over_be6'),local_player=final['local_player'],
                 winners=[r['index'] for r in rows if r['index']>=1 and r['result_a28']==1],
                 defeated=[r['index'] for r in rows if r['index']>=1 and r['result_a28']==2],
                 players=[dict(index=r['index'],human=r['index']==final['local_player'],result=r['result_a28'],
                               units_lb14=r['lb14'],victory_units_lb18=r['victory_units_lb18'],resources=r['resources'])
                          for r in rows if r['index']>=1],
                 engine_end=engine_end,continued_after_engine_end=resume,civ_preempt=preempt,
                 alive_computers=[r['index'] for r in rows if r['index']>=1 and r['index']!=final['local_player'] and r['victory_units_lb18']>0],
                 no_human_defeat=no_defeat,defeat_order=defeat_order,
                 forms=forms,lobby=chosen,save_load=save_rec,wall_seconds=round(time.monotonic()-started,1))
    # the last computer standing (the game itself names no winner while the idle human is kept in; see the stop rule)
    summary['winner']=(summary['alive_computers'][0] if len(summary['alive_computers'])==1 else None) if outcome=='last-computer-standing' else \
        (summary['winners'] if outcome=='game-end' else None)
    (evidence/'ai_game.json').write_text(json.dumps(dict(summary=summary,samples=samples),indent=2)+'\n',encoding='utf-8')
    note(f'AI GAME {outcome}: PT {summary["final_pt"]} game s {(summary["game_ms"] or 0)/1000:.0f} winners {summary["winners"]} defeated {summary["defeated"]}'
         +(f' winner (last computer) {summary["winner"]} defeat order {[(d["player"],d["pt"]) for d in defeat_order]}' if no_defeat else '')
         +(' units '+' '.join(f'{r["index"]}:{r["units_lb14"]}/{r["victory_units_lb18"]}' for r in summary['players']) if outcome=='time-cap' else ''))
    label='game-end' if outcome in ('game-end','last-computer-standing') else 'game-stopped'
    if scenario:
        try:ui['capture'](label,None)
        except RuntimeError as error:note(f'CAPTURE {label} skipped: {error}')
    else:ui['capture'](label,None)
    results.append(dict(milestone='ai-game-final',outcome=outcome,summary=summary))
