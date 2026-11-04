"""Bounded live game integration session. Request clean harness stop via stage/stop.request.

Round 39 (TEST ONLY, opt-in): --instance K stages build/boot/windowed_{o,f,r}_K with its own logs and evidence, lets
the harness debugger bypass the single-instance mutex (logged, image unpatched) and redirects the game's registry key
to its own subkey EE-AOC\\windowed-test instance K (seeded from the snapshot plus this run's pins);
--registry-managed-by-batch leaves the snapshot/restore to tools/boot/game_batch.py. Plugin hooks (optional files):
timeshim.py, session_exercise.py, differential/session_hook.py may define add_session_arguments(ap),
prepare_session(ctx), start_session(ctx) -> obj with .stop(), finish_session(ctx, lines); see docs/LIVE_GAME_TESTING.md.
"""
import argparse
import hashlib
import importlib
import importlib.util
import inspect
import json
import pathlib
import sys
import threading
import datetime
import shutil

HERE=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import exe_boot as boot


SEED_SYMBOL='?Execute@EEMStartLoading@@UAEXXZ'
SEED_RVA_ORIGINAL=0xc7cdc   # original/experimental layout (orig 0x4c7cdc)
PHYSICS_SYMBOL='?gPhysics@@3PAVEEPhysics@@A'
PHYSICS_VA_ORIGINAL=0x9318f0
MUTEX_SYMBOL='?EECreateInstanceMutex@@YA_NXZ'
MUTEX_RVA_ORIGINAL=0x14398d   # original/experimental layout (orig 0x54398d)
MUTEX_GETLASTERROR='__imp__GetLastError@0'
MUTEX_GETLASTERROR_ORIGINAL=0x837148
PORT_STEP=16   # --instance K: TCP Protocol's fixed ports 33334/33335 move to +16*K (exe_boot.c --port-offset)
BASE_EXERCISES=['settings','random-game','tour']
PLUGIN_FILES=[('timeshim',HERE/'timeshim.py'),('session_exercise',HERE/'session_exercise.py'),
              ('session_hook',HERE/'differential'/'session_hook.py'),
              ('session_mp',HERE/'mp'/'session_mp.py')]   # round 40 (agent mp): opt-in --mp-role (tools/boot/mp_game.py)


def load_plugins():
    """Optional hook modules owned by the other harness agents; a missing file is skipped, a broken one reported."""
    plugins={}
    for name,path in PLUGIN_FILES:
        if not path.exists():continue
        try:
            if path.parent==HERE:module=importlib.import_module(name)
            else:
                spec=importlib.util.spec_from_file_location('differential_'+name,path)
                module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
            plugins[name]=module
        except Exception as error:
            print(f'PLUGIN {name} ({path}) failed to import: {error!r}; skipped',flush=True)
    return plugins


def session_verdict(lines,hit_ok,exercise_mode,exercise_result,extra_required=None):
    stops=[line for line in lines if line.startswith('STOP ')]
    ok=(stops==['STOP <windowed session complete> reached']
        and 'checkpoint windowed frame presented' in lines
        and 'process gone: yes; windows left: 0' in lines and hit_ok
        and not any(line.startswith('HARNESS FAILURE') for line in lines))
    if exercise_mode:
        required={'main-menu'}
        if exercise_mode in ('settings','tour'):required.update(['settings-video','settings-audio','settings-interface','settings-closed'])
        if exercise_mode in ('random-game','tour'):required.update(['single-player','random-map-setup','gameplay'])
        if extra_required:required.update(extra_required.get(exercise_mode,()))
        ok=ok and exercise_result['done'] and exercise_result['error'] is None
        ok=ok and required<={row['milestone'] for row in exercise_result['milestones']}
    return bool(ok)


def mutex_bypass_rva(image_bytes,linkmap):
    """RVA of the `test eax,eax` after EECreateInstanceMutex's GetLastError call (original layout fixed, full link by map
    name); the function body and its IAT slot are checked so the guard can only land on that instruction."""
    import pefile
    pe=pefile.PE(data=image_bytes,fast_load=True)
    start=(linkmap.address(MUTEX_SYMBOL)-0x400000) if linkmap else MUTEX_RVA_ORIGINAL
    slot=linkmap.address(MUTEX_GETLASTERROR) if linkmap else MUTEX_GETLASTERROR_ORIGINAL
    body=pe.get_data(start,0x28)
    want=b'\xff\x15'+slot.to_bytes(4,'little')+b'\x85\xc0\x75'
    if body[:1]!=b'\x68' or body.count(want)!=1:
        raise ValueError(f'EECreateInstanceMutex at rva {start:#x}: unexpected body {body.hex()}; refusing mutex bypass')
    return start+body.index(want)+6


def main():
    plugins=load_plugins()
    exercise_module=plugins.get('session_exercise')
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--build-dir',type=pathlib.Path,
                    help='the build under test: a test-profile build (default build/clang-test; tools/harness/hbuild.py)')
    ap.add_argument('--timeout',type=int,default=300000)
    ap.add_argument('--exercise',choices=BASE_EXERCISES+[m for m in getattr(exercise_module,'EXERCISES',[]) if m not in BASE_EXERCISES])
    ap.add_argument('--play-seconds',type=int,default=0,help='opt-in: after the gameplay milestone keep the game running N s (telemetry every 10 s), then a final capture')
    ap.add_argument('--trace-symbol',action='append',default=[],help='TEST ONLY: count entries of this map symbol (at most 8)')
    ap.add_argument('--seed',type=lambda v:int(v,0),
                    help='TEST ONLY (round 36): the harness debugger writes this fixed start seed into '
                         'EEMStartLoading::[this+0x14] at each EEMStartLoading::Execute entry (replacing the lobby value '
                         'srand(TMGetTime()); rand() value); every write is logged and recorded in the session json')
    ap.add_argument('--pt-rate',type=int,
                    help='TEST ONLY (round 36): pin Min = Max PTs per Second to N in the snapshot-restored EE-AOC key '
                         '(the CPU-tuned simulation rate stays fixed)')
    ap.add_argument('--trace-once',action='store_true',
                    help='TEST ONLY (round 36): experimental candidate entries are logged on their first execution only '
                         '(the guard is dropped; hot candidates no longer slow the game)')
    ap.add_argument('--rng-trace',action='store_true',
                    help='TEST ONLY (round 36, read-only): the harness logs every engine URandom::Generate/NextInSeries '
                         'call on gPhysics->mRandom (seed before the call, return address, caller frame return) into the raw log')
    ap.add_argument('--instance',type=int,
                    help='TEST ONLY (round 39): parallel instance K (1..32): stage build/boot/windowed_{o,f,r}_K, logs '
                         'build/boot/windowed_*_K.session.*, the harness bypasses the single-instance mutex (logged; image '
                         'unpatched) and gives the game key its own subkey EE-AOC\\windowed-test instance K')
    ap.add_argument('--registry-managed-by-batch',action='store_true',
                    help='TEST ONLY (round 39, with --instance): a live tools/boot/game_batch.py owns the EE-AOC '
                         'snapshot/restore; this session only seeds and uses its own instance subkey')
    for module in plugins.values():
        if hasattr(module,'add_session_arguments'):module.add_session_arguments(ap)
    a=ap.parse_args()
    if a.seed is not None and not 0<=a.seed<=0xffffffff:ap.error('--seed must fit in 32 bits')
    if a.pt_rate is not None and not 1<=a.pt_rate<=1000:ap.error('--pt-rate out of range')
    if a.instance is not None and not 1<=a.instance<=32:ap.error('--instance must be 1..32')
    if a.registry_managed_by_batch and a.instance is None:ap.error('--registry-managed-by-batch requires --instance')
    import hbuild
    build=hbuild.Build(a.build_dir)
    import layout;layout.use(build.dir)   # every VC6 offset the plugins read is translated for this build
    # the rest of the session (plugins, telemetry) runs on ee's full-link path: everything by map name
    a.fulllink=str(build.exe);a.original=False;a.engine=None;a.tcp=None;a.default_dll=None;a.experimental=None;a.hotfix=[]
    name='windowed_f'
    if a.instance is not None:name+=f'_{a.instance}'
    image=build.exe
    fulllink_map=build.exe_map_path
    manifest=None
    harness=boot.build_harness()
    stage=boot.stage(name,build)
    engine_sha=tcp_sha=default_sha=None
    linkmap0=build.map('exe')
    globals_={n:dict(va=hex(linkmap0.address(n))) for n in ('?gCommunications@@3PAVNESynchronousCommunications@@A',
              '?gWorld@@3VEEWorld@@A','?gUIManager@@3PAVEEUIManager@@A','?gMouse@@3PAVEEMouse@@A')}
    located=dict(globals=globals_,audio_entry_rva=hex(linkmap0.address('?EEInitializeSounds@@YA_NXZ')-0x400000))
    (stage/'EE-AOC.windowed.json').write_text(json.dumps(dict(test_only=True,kind='clang-build-v1',build=str(build.dir),
        **located),indent=2)+'\n',encoding='utf-8')
    g=located['globals']
    addresses=dict(communications=int(g['?gCommunications@@3PAVNESynchronousCommunications@@A']['va'],16),
                   world=int(g['?gWorld@@3VEEWorld@@A']['va'],16),victory_detail=bool(a.trace_symbol))
    activate,present=build.renderer_guards()
    harness_extra=('--audio-entry',located['audio_entry_rva'],'--stack-scan','512',
                   '--renderer-guards',f'{activate:x}',f'{present:x}')
    from presentation_bridge import build_presentation_bridge
    build_presentation_bridge(stage,build)
    trace=None
    from fulllink_locator import LinkMap
    if a.fulllink and a.trace_symbol:   # opt-in triage: count entries of up to 8 functions, by map name
        linkmap=LinkMap(fulllink_map)
        trace=stage/'fulllink.trace'
        trace.write_text(''.join(f'{linkmap.address(s)-0x400000:x}\n' for s in a.trace_symbol),encoding='ascii')
        trace_names={linkmap.address(s)-0x400000:s for s in a.trace_symbol}
    if a.seed is not None:   # opt-in test intervention; the image is not patched
        seed_rva=(LinkMap(fulllink_map).address(SEED_SYMBOL)-0x400000 if a.fulllink else SEED_RVA_ORIGINAL)
        harness_extra=tuple(harness_extra)+('--seed-pin',f'{seed_rva:x}',f"{layout.off('EEMStartLoading',0x14):x}",str(a.seed))
        print(f'TEST seed pin {a.seed} at {SEED_SYMBOL} rva {seed_rva:#x} [ecx+0x14]',flush=True)
    reg_subtree=None;mutex_rva=None
    if a.instance is not None:   # round 39 opt-in: parallel instance; the image is not patched
        mutex_rva=mutex_bypass_rva((stage/'EE-AOC.exe').read_bytes(),LinkMap(fulllink_map) if a.fulllink else None)
        harness_extra=tuple(harness_extra)+('--mutex-bypass',f'{mutex_rva:x}','--port-offset',str(PORT_STEP*a.instance))
        reg_subtree=boot.INSTANCE_SUBTREE.format(a.instance)
        print(f'TEST instance {a.instance}: stage {name}; mutex bypass at {MUTEX_SYMBOL} rva {mutex_rva:#x} '
              f'(GetLastError 183 -> 0); registry view HKCU\\Software\\Mad Doc Software\\EE-AOC\\{reg_subtree}'
              f'{" (batch-managed snapshot)" if a.registry_managed_by_batch else ""}',flush=True)
    if a.trace_once:harness_extra=tuple(harness_extra)+('--trace-once',)
    if a.rng_trace:
        physics_va=LinkMap(fulllink_map).address(PHYSICS_SYMBOL) if a.fulllink else PHYSICS_VA_ORIGINAL
        harness_extra=tuple(harness_extra)+('--rng-trace',f'{physics_va:x}')
    reg_pins=[f'Min PTs per Second=dword:{a.pt_rate}',f'Max PTs per Second=dword:{a.pt_rate}'] if a.pt_rate else []
    if manifest:
        trace=stage/'experimental.trace'
        trace.write_text(''.join(f'{row["candidate_rva"]:x}\n' for row in manifest['functions']),encoding='ascii')
    path=boot.BOOT/(name+'.session')
    role='clang'
    if a.engine:role+='-engine-relinked'
    if a.tcp:role+='-tcp-staged'
    if a.default_dll:role+='-default-staged'
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence=boot.ROOT/'build/menu_validation'/role/(stamp+(f'_i{a.instance}' if a.instance is not None else ''))
    evidence.mkdir(parents=True)
    ctx=dict(args=a,instance=a.instance,name=name,stage=stage,image='fulllink' if a.fulllink else 'original' if a.original else 'experimental',
             image_path=image,map=fulllink_map,located=located,evidence=evidence,log=path,role=role)
    plugin_records={};plugin_interventions=[];plugin_pins=[]
    for key,module in plugins.items():   # opt-in hooks: each acts only on its own options
        if not hasattr(module,'prepare_session'):continue
        result=module.prepare_session(ctx) or {}
        harness_extra=tuple(harness_extra)+tuple(result.pop('harness_extra',()))
        plugin_pins+=list(result.pop('registry',()))
        plugin_interventions+=list(result.pop('interventions',()))
        if result:plugin_records.setdefault(key,{}).update(result)
    (stage/'session.context.json').write_text(json.dumps(dict(test_only=True,name=name,instance=a.instance,image=ctx['image'],
        image_path=str(image),map=str(fulllink_map) if fulllink_map else None,stage=str(stage),log=str(path),
        evidence=str(evidence),registry_subtree=reg_subtree,mutex_bypass_rva=hex(mutex_rva) if mutex_rva else None),indent=2)+'\n',encoding='utf-8')
    print(f'LIVE TEST {stage}; log {path}.raw.txt',flush=True)
    exercise_result=dict(done=False,error=None,milestones=[])
    if a.exercise:
        from session_exercise import exercise
        extra={'options':a} if 'options' in inspect.signature(exercise).parameters else {}
        def drive():
            try:
                exercise_result['milestones']=exercise(stage,evidence,a.exercise,addresses,a.play_seconds,**extra)
                exercise_result['done']=True
            except Exception as error:
                exercise_result['error']=str(error)
                print(f'UI EXERCISE FAILED: {error}',flush=True)
                (stage/'stop.request').write_text('exercise failed\n',encoding='ascii')
    started={}
    def start_plugins():   # once the registry is ours (a session may wait for its owner lock first)
        if a.exercise:threading.Thread(target=drive,daemon=True).start()
        for key,module in plugins.items():
            if hasattr(module,'start_session'):
                handle=module.start_session(ctx)
                if handle is not None:started[key]=handle
    try:
        lines=boot.run(harness,stage,'run',a.timeout,str(path)+'.bmp',
                       ['Game Bit Depth=dword:32','Texture Bit Depth=dword:32','Music Enabled=dword:0']+reg_pins+plugin_pins,
                       str(path)+'.reg',windowed=True,checkpoint='session',trace_functions=trace,live_log=str(path)+'.raw.txt',loopback=True,
                       harness_extra=harness_extra,registry='batch' if a.registry_managed_by_batch else 'self',
                       reg_subtree=reg_subtree,on_start=start_plugins)
    finally:
        for key,handle in started.items():
            try:
                stopped=handle.stop()
                if stopped is not None:plugin_records.setdefault(key,{})['run']=stopped
            except Exception as error:
                plugin_records.setdefault(key,{})['run_error']=repr(error)
                print(f'PLUGIN {key} stop failed: {error!r}',flush=True)
    for key,module in plugins.items():
        if hasattr(module,'finish_session'):
            finished=module.finish_session(ctx,lines)
            if finished is not None:plugin_records.setdefault(key,{})['finish']=finished
    pathlib.Path(str(path)+'.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    stops=[line for line in lines if line.startswith('STOP ')]
    print('\n'.join(stops),flush=True)
    if a.fulllink:   # full-link gaps the session reached (exe_fulllink trap/WON stubs report through debug strings)
        for line in lines:
            if line.startswith('debugstring EE-') or 'exited with 400' in line:print(line,flush=True)
        if a.trace_symbol:
            import collections
            counts=collections.Counter(int(l.rsplit(' ',1)[1],16) for l in lines if l.startswith('experimental hit rva '))
            first={}
            for k,l in enumerate(lines):
                if l.startswith('experimental hit rva '):first.setdefault(int(l.rsplit(' ',1)[1],16),k)
            for rva,s in trace_names.items():print(f'TRACE {s}: {counts[rva]} entries (first at log line {first.get(rva)})',flush=True)
    hit_ok,hits=boot.experimental_hits(lines,manifest) if manifest else (True,[])
    completed=session_verdict(lines,hit_ok,a.exercise,exercise_result,getattr(exercise_module,'REQUIRED_MILESTONES',None))
    interventions=[]
    if a.seed is not None:
        writes=[line for line in lines if ' seed pin ' in line]
        interventions.append(dict(kind='seed-pin',symbol=SEED_SYMBOL,value=a.seed,log=writes))
        print('\n'.join(writes) or 'TEST seed pin: no write (EEMStartLoading::Execute not reached)',flush=True)
    if a.pt_rate:interventions.append(dict(kind='pt-rate-pin',registry=reg_pins))
    if a.instance is not None:
        bypass=[line for line in lines if line.startswith(('TEST mutex bypass','STOP cannot guard mutex bypass'))]
        interventions.append(dict(kind='mutex-bypass',symbol=MUTEX_SYMBOL,rva=hex(mutex_rva),log=bypass))
        interventions.append(dict(kind='loopback-port-offset',offset=PORT_STEP*a.instance,
                                  log=[line for line in lines if line.startswith('TEST instance port ')]))
        interventions.append(dict(kind='registry-instance-subtree',key=boot.REG_KEY+'\\'+reg_subtree,
                                  snapshot='batch' if a.registry_managed_by_batch else 'session',
                                  redirects=sum(line.startswith('regredirect-instance ') for line in lines)))
        print('\n'.join(bypass) or 'TEST mutex bypass: guard not reached',flush=True)
    interventions+=plugin_interventions
    pathlib.Path(str(path)+'.json').write_text(json.dumps(dict(test_only=True,completed=completed,stops=stops,
        **(dict(test_interventions=interventions) if interventions else {}),
        **(dict(instance=a.instance,stage=name) if a.instance is not None else {}),
        **(dict(plugins=plugin_records) if plugin_records else {}),
        experimental_execution=hits,exercise=exercise_result,engine_sha256=engine_sha,tcp_sha256=tcp_sha,default_sha256=default_sha,
        original_backed_hotfixes=located.get('original_backed_hotfixes',[]) if located else [],loopback_only=True,evidence=str(evidence),
        image_sha256=hashlib.sha256((stage/'EE-AOC.exe').read_bytes()).hexdigest(),
        overlays=[json.loads(file.read_text(encoding='utf-8')) for file in stage.glob('*.json') if file.name!='session.context.json'],
        limitations='Queued application UI events and framebuffer evidence; physical mouse/keyboard input, audio output and full gameplay semantics are not certified'),indent=2)+'\n',encoding='utf-8')
    for file in boot.BOOT.glob(name+'.session.*'):shutil.copyfile(file,evidence/file.name)
    for file in stage.glob('*.json'):shutil.copyfile(file,evidence/file.name)
    latest='latest.json' if a.instance is None else f'latest.{name}.json'
    (evidence.parent/latest).write_text(json.dumps(dict(evidence=str(evidence),completed=completed),indent=2)+'\n',encoding='utf-8')
    print(f'EVIDENCE {evidence}',flush=True)
    sys.exit(0 if completed else 1)


if __name__=='__main__':main()
