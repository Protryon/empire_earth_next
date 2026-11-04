"""TEST ONLY (final validation campaign, lane scen): start a shipped campaign scenario through the game's own UI.

`--exercise ai-game --campaign NAME [--scenario-index K] [--campaign-difficulty V]`: main menu -> single player
(form 6) -> the campaign (learning: form 6's Learn to Play button, EEUIEPCELearningCampaign; roman/pacific/asian:
EEUIESPECampaign -> Play Campaign form 8 -> EEUIEPCECampaign1/2/3) -> Campaign Scenario Select (form 33: scenario list
box 2, difficulty combo 6, EEUIECSSEStartScenario) -> Scenario Intro (form 19: EEUIESIEStart) -> the game form. Then the
ai-game play loop runs as for a loaded game (no lobby, no civilization pre-empt).
Only open scenarios are listed: every learning scenario (StartCampaign's all-open flag), the first scenario of the
others (no campaign progress in the staged Users folder).
Controls are found by their trigger event's ProcessEvent rva, like save_load.py (original rvas from
analysis/exe/names.txt, full link by map name). Scenario load extracts archive members to a temporary disk file
(FSMemoryFile::FOpen: GetTempPath + GetTempFileName); the session's TMP/TEMP point at the stage directory
(set in session_exercise.prepare_session), so that write lands inside the staging sandbox.
"""
import pathlib,re,time

ROOT=pathlib.Path(__file__).resolve().parents[2]
SP_FORM=6;PLAY_CAMPAIGN=8;SELECT=33;INTRO=19;GAME=11
LIST=2;DIFFICULTY=6
CAMPAIGNS=dict(learning='EEUIEPCELearningCampaign',roman='EEUIEPCECampaign1',pacific='EEUIEPCECampaign2',asian='EEUIEPCECampaign3')
def _pe(cls):return f'?ProcessEvent@{cls}@@UAEJPAVUIForm@@PAVUIEntity@@@Z'
SYM=dict(sp_campaign=_pe('EEUIESPECampaign'),start=_pe('EEUIECSSEStartScenario'),intro_start=_pe('EEUIESIEStart'),
         load_scenario=_pe('EEUIELELoadScenarioGame'),
         **{k:_pe(v) for k,v in CAMPAIGNS.items()})


def add_arguments(g):
    g.add_argument('--campaign',choices=sorted(CAMPAIGNS),help='TEST ONLY (scen): play a shipped campaign scenario '
                   'instead of the random-map lobby (single player -> campaign -> scenario select -> intro -> start)')
    g.add_argument('--scenario-index',type=int,default=None,help='with --campaign: the scenario list line to select '
                   '(default: the game\'s own preselection)')
    g.add_argument('--scenario-file',type=pathlib.Path,help='TEST ONLY (scen): stage this .scn file into the stage directory '
                   'Data/Scenarios and play it as a standalone scenario (single player -> lobby, game type Scenario, '
                   'Play Scenario screen, Load, Start Game); exclusive with --campaign')
    g.add_argument('--campaign-difficulty',default='#1',help='with --campaign: difficulty combo line name or #index '
                   '(default #1; set absolutely)')


def symbol_rvas(options):
    if options is not None and getattr(options,'fulllink',None):
        from fulllink_locator import LinkMap
        m=LinkMap(pathlib.Path(options.fulllink).resolve().with_suffix('.map'))
        return {k:m.address(s)-0x400000 for k,s in SYM.items()}
    names={}
    for line in (ROOT/'analysis/exe/names.txt').read_text(encoding='utf-8').splitlines():
        part=line.split()
        if len(part)>=2 and re.fullmatch(r"[0-9a-fA-F]+",part[0]):names.setdefault(part[1],int(part[0],16))
    return {k:names[s] for k,s in SYM.items()}


def start_from_menu(ui,cur,raw,note,options,evidence,capture):
    """From the single-player form to the game form. Returns the ui state at the game form."""
    from save_load import control_for,parse_extra,_wait
    from ai_game import resolve
    rv=symbol_rvas(options);name=options.campaign
    st=_wait(cur,lambda s:s['form']==SP_FORM,60,'single-player form not up')
    if name=='learning':
        seq=ui['command'](f'click {SP_FORM} {control_for(st,rv["learning"],"Learn to Play button")}')
    else:
        seq=ui['command'](f'click {SP_FORM} {control_for(st,rv["sp_campaign"],"Campaign button")}')
        st=_wait(cur,lambda s:s['form']==PLAY_CAMPAIGN and s['command']>=seq,60,'play campaign form not up')
        (evidence/'campaign_form8.txt').write_text(raw(),encoding='utf-8')
        seq=ui['command'](f'click {PLAY_CAMPAIGN} {control_for(st,rv[name],name+" campaign button")}')
    st=_wait(cur,lambda s:s['form']==SELECT and s['command']>=seq and DIFFICULTY in s['combos'],120,'scenario select form not up')
    time.sleep(2)
    text=raw();(evidence/'campaign_form33.txt').write_text(text,encoding='utf-8')
    lists=parse_extra(text)[1]
    scen=lists.get(LIST)
    note(f'CAMPAIGN {name}: scenarios {scen}')
    if options.scenario_index is not None:
        if scen is None:raise RuntimeError(f'scenario list box {LIST} not listed (bridge list types): {sorted(lists)}')
        k=options.scenario_index
        if not 0<=k<len(scen['lines']):raise RuntimeError(f'scenario index {k} outside {scen["lines"]}')
        if scen['sel']!=k:
            seq=ui['command'](f'lselect {SELECT} {LIST} {k}')
            _wait(cur,lambda s:s['command']>=seq,30,'lselect not processed')
            end=time.monotonic()+20
            while parse_extra(raw())[1].get(LIST,{}).get('sel')!=k:
                if time.monotonic()>end:raise RuntimeError(f'scenario selection did not stick: {parse_extra(raw())[1]}')
                time.sleep(.25)
    combo=cur()['combos'][DIFFICULTY]
    index=resolve(options.campaign_difficulty,combo['lines'],'campaign difficulty')
    # set absolutely: select another line first if it already shows the wanted one (the event must run)
    for want in ([1-index if index<=1 else 0] if combo['sel']==index else [])+[index]:
        seq=ui['command'](f'select {SELECT} {DIFFICULTY} {want}')
        _wait(cur,lambda s:s['command']>=seq and s['combos'].get(DIFFICULTY,{}).get('sel')==want,30,'difficulty not set')
        time.sleep(1)
    st=cur();scen=parse_extra(raw())[1].get(LIST)
    chosen=dict(campaign=name,scenario=(scen['lines'][scen['sel']] if scen and scen['lines'] else None),
                scenario_index=(scen['sel'] if scen else None),difficulty=combo['lines'][index])
    note(f'CAMPAIGN start {chosen}')
    capture('campaign-select',SELECT)
    seq=ui['command'](f'click {SELECT} {control_for(st,rv["start"],"Start Scenario button")}')
    st=_wait(cur,lambda s:s['form']==INTRO and s['command']>=seq,120,'scenario intro form not up')
    time.sleep(2);(evidence/'intro_form19.txt').write_text(raw(),encoding='utf-8')
    capture('scenario-intro',INTRO)
    st=cur()
    seq=ui['command'](f'click {INTRO} {control_for(st,rv["intro_start"],"Start (intro) button")}')
    note('CAMPAIGN intro Start clicked')
    st=_wait(cur,lambda s:s['form']==GAME and s['command']>=seq,600,'game form not reached after the intro Start')
    return st,chosen


LOBBY=26;LOAD_FORM=4;GAME_TYPE=175;LOAD_LIST=4;START=10;LOBBY_DIFFICULTY=207


def stage_scenario(stage,path):
    import shutil
    d=pathlib.Path(stage)/'Data'/'Scenarios';d.mkdir(parents=True,exist_ok=True)
    target=d/pathlib.Path(path).name;shutil.copyfile(path,target);return target


def start_from_lobby(ui,cur,raw,note,options,evidence,capture):
    """Standalone scenario: single player -> random map lobby (form 26; the caller clicked it) -> game type 2
    (EEUIELEGameType pushes the load screen with EEUIELEOpenLoadScenarioGame) -> select the staged file -> Load
    (EEUIELELoadScenarioGame: LobbyFindFile applies the scenario's options and players, pops) -> difficulty -> Start."""
    from save_load import control_for,parse_extra,_wait
    from ai_game import resolve
    rv=symbol_rvas(options);name=pathlib.Path(options.scenario_file).stem
    st=_wait(cur,lambda s:s['form']==LOBBY and GAME_TYPE in s['combos'],60,'lobby not up')
    time.sleep(3)
    seq=ui['command'](f'select {LOBBY} {GAME_TYPE} 2')
    st=_wait(cur,lambda s:s['form']==LOAD_FORM and s['command']>=seq,60,'play scenario screen not up')
    end=time.monotonic()+30
    while True:
        lists=parse_extra(raw())[1]
        if name in lists.get(LOAD_LIST,{}).get('lines',[]):break
        if time.monotonic()>end:raise RuntimeError(f'play scenario screen does not list {name!r}: {lists}')
        time.sleep(.5)
    (evidence/'scenario_form4.txt').write_text(raw(),encoding='utf-8')
    index=lists[LOAD_LIST]['lines'].index(name)
    seq=ui['command'](f'lselect {LOAD_FORM} {LOAD_LIST} {index}')
    _wait(cur,lambda s:s['command']>=seq,30,'lselect not processed');time.sleep(1)
    seq=ui['command'](f'click {LOAD_FORM} {control_for(cur(),rv["load_scenario"],"Load (scenario) button")}')
    st=_wait(cur,lambda s:s['form']==LOBBY and s['command']>=seq,60,'lobby not back after Load')
    time.sleep(3)
    st=cur();(evidence/'scenario_lobby.txt').write_text(raw(),encoding='utf-8')
    chosen=dict(scenario_file=str(options.scenario_file),game_type=st['combos'].get(GAME_TYPE))
    combo=st['combos'].get(LOBBY_DIFFICULTY)
    if combo:
        index=resolve(options.campaign_difficulty,combo['lines'],'difficulty')
        if combo['sel']!=index:
            seq=ui['command'](f'select {LOBBY} {LOBBY_DIFFICULTY} {index}')
            _wait(cur,lambda s:s['command']>=seq and s['combos'].get(LOBBY_DIFFICULTY,{}).get('sel')==index,30,'difficulty not set')
            time.sleep(1)
        chosen['difficulty']=combo['lines'][index]
    note(f'SCENARIO lobby {chosen}')
    capture('scenario-lobby',LOBBY)
    seq=ui['command'](f'click {LOBBY} {START}');note('UI ACTION Start Game queued (scenario)')
    end=time.monotonic()+600
    while True:
        st=cur()
        if st and st['form']==GAME and st['command']>=seq:break
        if st and st['form'] not in (LOBBY,GAME) and st['command']>=seq and st['form'] not in (None,):
            (evidence/f'scenario_start_form{st["form"]}.txt').write_text(raw(),encoding='utf-8')
        if time.monotonic()>end:raise RuntimeError(f'game form not reached; last {st and st["form"]} stack {st and st["stack"]}')
        time.sleep(.5)
    return st,chosen
