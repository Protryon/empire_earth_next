"""TEST ONLY (round 39, agent saveload): a mid-game save and a load through the game's own UI (ai-game exercise).

Save (`--save-at-pt N`): once gPhysics->mUB4 >= N the fixture opens the in-game options (main-game form 11, control 3 ->
form 28), clicks Save Game (EEUIEMGOESaveGame: posts EEUIESLSaveScreen, which pushes form 3 and pauses the game), types
the name into the save screen's edit box (control 5; fixture `text`, UIEditBox::SetText) and clicks Save
(EEUIESLESaveFile: single player sends EEMLoadOrSaveGame through SendToAllAndWait; its Execute writes
<startup>data\\Saved Games\\<name>.ees at a CT). The save screen is closed with its Cancel/Back button and the game is
unpaused the way it was paused. gStartupDirectory is the staged directory (build/boot/windowed_*_K), so the file is
written into the stage's own copy of Data (exe_boot.stage copies the install's Data into every stage); the install is
never written. The file is copied to the evidence (and --save-out) once its size is stable.
Load (`--load-save PATH`): prepare_session copies PATH into <stage>\\Data\\Saved Games\\<name>.ees before the process
starts; the exercise goes main menu -> single player (form 6) -> Load Game (EEUIESPELoadGame: form 13), selects the
file in the list box (control 4; fixture `lselect`, UIListBox::ChangeSelectionIndex as the screen itself does) and
clicks Load (EEUIESPLELoadFile: validates, EEMStartLoading(1, ...)). The game form then continues the ai-game play loop
(no lobby, no civilization pre-empt: the saved players carry their flags).
Controls are found by their trigger event's ProcessEvent rva (ui-state `control i event-rva X`): original rvas from
analysis/exe/names.txt, full link by map name; nothing is hardcoded but the form ids (data-defined, both images). An
event the game screens moved to Rust (rust/crates/ee_screens) has no class in the full link: it is an EEScreenEvent,
found by that class's ProcessEvent and its code (ui-state `... screen CODE`; SCREEN_CODES).
"""
import pathlib,re,shutil,time

ROOT=pathlib.Path(__file__).resolve().parents[2]
GAME=11;OPTIONS=28;SAVE_FORM=3;SP_FORM=6;LOAD_FORM=13
SYM=dict(
    options_save='?ProcessEvent@EEUIEMGOESaveGame@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
    save_file='?ProcessEvent@EEUIESLESaveFile@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
    return_to_game='?ProcessEvent@EEUIEMGOEReturnToGame@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
    sp_load='?ProcessEvent@EEUIESPELoadGame@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
    sp_load_file='?ProcessEvent@EEUIESPLELoadFile@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
)
# the events whose classes the game screens replaced with EEScreenEvent codes (EE_SCREENS_*, rust/ee_screens.h)
SCREEN_PE='?ProcessEvent@EEScreenEvent@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z'
SCREEN_CODES={'EEUIESPELoadGame':0x0605,'EEUIESPECampaign':0x0602,'EEUIEPCECampaign1':0x0802,
              'EEUIEPCECampaign2':0x0803,'EEUIEPCECampaign3':0x0804,'EEUIEPCELearningCampaign':0x0808}
SAVE_DIR=('Data','Saved Games')
EXT='.ees'


def add_arguments(g):
    g.add_argument('--save-at-pt',type=int,help='TEST ONLY (saveload): save the game through the in-game Save Game '
                   'screen once gPhysics->mUB4 >= N (the save lands on the PT where the screen pauses the game; recorded)')
    g.add_argument('--save-pinned',action='store_true',help='with --save-at-pt: open the save screen through a scripted '
                   'human (tools/boot/human, agent input) pinned to the CT that starts at server PT N: "N click 3" (options) '
                   'and "+0 click 7" (Save Game), so the save screen pauses the game at a deterministic PT')
    g.add_argument('--save-name',default='harness',help='saved game name (one word; default harness)')
    g.add_argument('--save-out',help='also copy the written .ees file to this path')
    g.add_argument('--stop-after-save',action='store_true',help='end the ai-game exercise right after the save')
    g.add_argument('--load-save',help='TEST ONLY (saveload): stage this .ees file and start the session by loading it '
                   '(single player -> Load Game) instead of the random-map lobby')


def screen_event(m,symbol):
    """A ProcessEvent symbol's rva in a full link's map; (EEScreenEvent's rva, code) for a class the screens
    replaced."""
    match=re.fullmatch(r'\?ProcessEvent@(\w+)@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',symbol)
    if symbol not in m.by_name and match and match.group(1) in SCREEN_CODES:
        return (m.address(SCREEN_PE)-0x400000,SCREEN_CODES[match.group(1)])
    return m.address(symbol)-0x400000


def symbol_rvas(options,syms=None):
    """ProcessEvent rva of each needed event in the session's image (screen_event's pair for a moved one)."""
    syms=SYM if syms is None else syms
    if options is not None and getattr(options,'fulllink',None):
        from fulllink_locator import LinkMap
        m=LinkMap(pathlib.Path(options.fulllink).resolve().with_suffix('.map'))
        return {k:screen_event(m,s) for k,s in syms.items()}
    names={}
    for line in (ROOT/'analysis/exe/names.txt').read_text(encoding='utf-8').splitlines():
        part=line.split()
        if len(part)>=2 and re.fullmatch(r"[0-9a-fA-F]+",part[0]):names.setdefault(part[1],int(part[0],16))
    return {k:names[s] for k,s in syms.items()}


def control_for(st,rva,what):
    """The one control whose trigger event is at rva (an int, or (rva, code) for an EEScreenEvent)."""
    rva,code=rva if isinstance(rva,tuple) else (rva,None)
    hits=[i for i,v in st['controls'].items()
          if int(v,16)==rva and (code is None or st.get('screens',{}).get(i)==code)]
    if len(hits)!=1:
        raise RuntimeError(f'{what}: {len(hits)} controls with event rva {rva:#x}'
                           + (f' screen {code:#x}' if code is not None else '')
                           + f' on form {st["form"]} ({st["controls"]} {st.get("screens")})')
    return hits[0]


def pinned_script(evidence,pt):
    """The --save-pinned human script (options control 3 on form 11, Save Game control 7 on form 28; data-defined form
    layouts, the same in both images)."""
    path=pathlib.Path(evidence)/'save_pinned.human'
    lines=[f'# TEST ONLY saveload: open the save screen at server PT {pt}',f'{pt} click 3','+0 click 7','']
    path.write_text('\n'.join(lines),encoding='utf-8')
    return path


def stage_save(stage,path,name):
    d=pathlib.Path(stage).joinpath(*SAVE_DIR);d.mkdir(parents=True,exist_ok=True)
    target=d/(name+EXT);shutil.copyfile(path,target);return target


def parse_extra(text):
    """edit/list lines of ui-state (presentation_bridge round 39 saveload)."""
    edits={};lists={}
    for line in text.splitlines():
        part=line.split(' ')
        if part[0]=='edit' and len(part)>=4:edits[int(part[1])]=' '.join(part[4:])
        elif part[0]=='list' and len(part)>=6:
            head,_,tail=line.partition(' lines ');count,_,names=tail.partition(' ')
            lists[int(part[1])]=dict(sel=int(head.split()[3]),lines=names.split('|') if int(count) else [])
    return edits,lists


def _wait(cur,pred,timeout,what):
    end=time.monotonic()+timeout
    while True:
        st=cur()
        if st and pred(st):return st
        if time.monotonic()>end:raise RuntimeError(f'{what} (form {st and st["form"]} stack {st and st["stack"]})')
        time.sleep(.25)


def do_save(ui,cur,raw,note,options,stage,evidence,pid,addresses,sample):
    """The in-game save. Returns a record (PTs before/while paused/after, file, sha)."""
    import hashlib
    rv=symbol_rvas(options);name=options.save_name
    target=pathlib.Path(stage).joinpath(*SAVE_DIR)/(name+EXT)
    rec=dict(name=name,requested_pt=options.save_at_pt)
    before=sample(pid,addresses,players=True);rec['pt_before']=before.get('pt_ub4')
    if cur()['form']==SAVE_FORM:
        rec['opened_by']='human-script'   # --save-pinned: the scripted human opened it at the pinned PT
    else:
        seq=ui['command'](f'click {GAME} 3')
        st=_wait(cur,lambda s:s['form']==OPTIONS and s['command']>=seq,30,'options form did not open')
        (evidence/'save_form28.txt').write_text(raw(),encoding='utf-8')
        seq=ui['command'](f'click {OPTIONS} {control_for(st,rv["options_save"],"Save Game button")}')
        st=_wait(cur,lambda s:s['form']==SAVE_FORM and s['command']>=seq,30,'save screen did not open')
    time.sleep(1);text=raw();(evidence/'save_form3.txt').write_text(text,encoding='utf-8')
    paused=sample(pid,addresses,players=True);time.sleep(1.5);paused2=sample(pid,addresses,players=True)
    rec.update(pt_paused=paused.get('pt_ub4'),pt_paused2=paused2.get('pt_ub4'),paused_flag=paused2['paused'])
    note(f'SAVE screen open: PT {paused.get("pt_ub4")} -> {paused2.get("pt_ub4")} (paused {paused2["paused"]})')
    edits,_=parse_extra(text)
    if 5 not in edits:raise RuntimeError(f'save screen: no edit box 5 ({sorted(edits)})')
    seq=ui['command'](f'text {SAVE_FORM} 5 {name}')
    _wait(cur,lambda s:s['command']>=seq,30,'text command not processed')
    end=time.monotonic()+20
    while parse_extra(raw())[0].get(5)!=name:
        if time.monotonic()>end:raise RuntimeError(f'save name not set: {parse_extra(raw())[0]}')
        time.sleep(.25)
    mtime0=target.stat().st_mtime_ns if target.exists() else None
    seq=ui['command'](f'click {SAVE_FORM} {control_for(cur(),rv["save_file"],"Save button")}')
    note(f'SAVE {name}: Save clicked')
    # the file: written by EEMLoadOrSaveGame::Execute; stable size for 3 s
    end=time.monotonic()+120;last=None;stable=0
    while True:
        st=cur()
        if st and st['form'] not in (SAVE_FORM,OPTIONS,GAME):   # a question/message form (e.g. overwrite): record
            (evidence/f'save_modal{st["form"]}.txt').write_text(raw(),encoding='utf-8')
        if target.exists() and target.stat().st_mtime_ns!=mtime0:
            now=target.stat().st_size
            stable=stable+1 if now==last and now>0 else 0;last=now
            if stable>=6:break
        if time.monotonic()>end:raise RuntimeError(f'save file {target} not written (form {st and st["form"]})')
        time.sleep(.5)
    after_write=sample(pid,addresses,players=True)
    data=target.read_bytes()
    out=evidence/(name+EXT);out.write_bytes(data)
    if options.save_out:pathlib.Path(options.save_out).parent.mkdir(parents=True,exist_ok=True);pathlib.Path(options.save_out).write_bytes(data)
    rec.update(file=str(out),size=len(data),sha256=hashlib.sha256(data).hexdigest(),pt_written=after_write.get('pt_ub4'))
    note(f'SAVE written {len(data)} B sha {rec["sha256"][:16]} at PT {after_write.get("pt_ub4")}')
    # close the save screen: whatever closes it back to the game (record the controls first)
    st=cur();(evidence/'save_form3_after.txt').write_text(raw(),encoding='utf-8')
    if not rec['size']:raise RuntimeError('empty save file')
    return rec


CANCEL_SYMS=['?ProcessEvent@EEUIESLECancelSave@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
             '?ProcessEvent@EEUIECCCECancel@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
             '?ProcessEvent@EEUIECSSEBack@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z',
             '?ProcessEvent@EEUIECancel@@UEAAJPEAVUIForm@@PEAVUIEntity@@@Z']


def cancel_rvas(options):
    """EECreateSaveLoadEvent: 0x10 EEUIESLECancelSave (its ProcessEvent is ICF-shared with EEUIECCCECancel /
    EEUIECSSEBack in the original), 0xf EEUIECancel; every name the image has."""
    out=set()
    if options is not None and getattr(options,'fulllink',None):
        from fulllink_locator import LinkMap
        m=LinkMap(pathlib.Path(options.fulllink).resolve().with_suffix('.map'))
        for s in CANCEL_SYMS:
            if s in m.by_name:out.add(m.by_name[s]-0x400000)
        return out
    for line in (ROOT/'analysis/exe/names.txt').read_text(encoding='utf-8').splitlines():
        part=line.split()
        if len(part)>=2 and part[1] in CANCEL_SYMS and re.fullmatch(r"[0-9a-fA-F]+",part[0]):out.add(int(part[0],16))
    return out


def close_save_screen(ui,cur,raw,note,options,evidence,pid,addresses,sample,rec):
    """Back to the game form: the save screen's cancel control (EEUIESLECancelSave / EEUIECancel), then Return to
    Game if the options form is on top. Records the PTs right after."""
    cancel=cancel_rvas(options);ret=symbol_rvas(options)['return_to_game'];tried=[]
    for _ in range(4):
        st=cur()
        if st['form']==GAME:break
        if st['form']==SAVE_FORM:
            hits=[i for i,v in sorted(st['controls'].items()) if int(v,16) in cancel]
            if not hits:raise RuntimeError(f'save screen: no cancel control ({st["controls"]}; cancel rvas {sorted(map(hex,cancel))})')
            seq=ui['command'](f'click {SAVE_FORM} {hits[0]}');tried.append(f'3:{hits[0]}')
        elif st['form']==OPTIONS:
            seq=ui['command'](f'click {OPTIONS} {control_for(st,ret,"Return to Game")}');tried.append('28:return')
        else:
            (evidence/f'save_close_form{st["form"]}.txt').write_text(raw(),encoding='utf-8')
            raise RuntimeError(f'unexpected form {st["form"]} while closing the save screen')
        _wait(cur,lambda s:s['command']>=seq and s['form']!=st['form'],30,f'form {st["form"]} did not close')
    rec['close_clicks']=tried
    _wait(cur,lambda s:s['form']==GAME,30,'game form not back after the save')
    a=sample(pid,addresses,players=True);time.sleep(2);b=sample(pid,addresses,players=True)
    rec.update(pt_resume_a=a.get('pt_ub4'),pt_resume_b=b.get('pt_ub4'),paused_after_close=b['paused'])
    note(f'SAVE closed via {tried}; PT {a.get("pt_ub4")} -> {b.get("pt_ub4")} paused {b["paused"]}')
    return b


def load_from_menu(ui,cur,raw,note,options,evidence,capture):
    """From the single-player form: Load Game, select the staged file, Load. Returns when the game form is up."""
    rv=symbol_rvas(options);name=options.save_name
    st=_wait(cur,lambda s:s['form']==SP_FORM,60,'single-player form not up')
    seq=ui['command'](f'click {SP_FORM} {control_for(st,rv["sp_load"],"Load Game button")}')
    end=time.monotonic()+60
    while True:
        st=cur()
        if st and st['form']==LOAD_FORM and st['command']>=seq:
            _,lists=parse_extra(raw())
            if name in lists.get(4,{}).get('lines',[]):break
        if time.monotonic()>end:raise RuntimeError(f'load screen did not list {name!r}: {parse_extra(raw())[1]}')
        time.sleep(.5)
    (evidence/'load_form13.txt').write_text(raw(),encoding='utf-8')
    capture('load-screen',LOAD_FORM)
    index=parse_extra(raw())[1][4]['lines'].index(name)
    seq=ui['command'](f'lselect {LOAD_FORM} 4 {index}')
    # the fixture reads test.command about once a second: wait for its ack before the next command (a list with no
    # selection already shows sel 0)
    _wait(cur,lambda s:s['command']>=seq,30,'lselect not processed')
    time.sleep(1)
    end=time.monotonic()+20
    while parse_extra(raw())[1].get(4,{}).get('sel')!=index:
        if time.monotonic()>end:raise RuntimeError(f'load list selection did not stick: {parse_extra(raw())[1]}')
        time.sleep(.25)
    seq=ui['command'](f'click {LOAD_FORM} {control_for(cur(),rv["sp_load_file"],"Load button")}')
    note(f'LOAD {name}: Load clicked')
    st=_wait(cur,lambda s:s['form']==GAME and s['command']>=seq,600,'game form not reached after Load')
    return st
