"""Read-only telemetry for a staged EE-AOC test process (x86_64; the pointer size is read from the process); no input
injection.

The process's bitness decides the pointer size (IsWow64Process); member offsets come from the build's layouts
(layout.off). addresses['image_base'] (opt-in): the map's preferred load address the given VAs refer to; they are
moved to the running image's base (an ASLR exe)."""
import argparse,ctypes,json,pathlib,re,struct,sys,time
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from layout import off   # ee's VC6 offsets, translated for the build profile in use (tools/harness/layout.py)
from ctypes import wintypes

STAGE_NAME=re.compile(r'windowed_[orf](_([1-9]|[12][0-9]|3[0-2]))?')   # game_session stages (--instance K: _K)


def sample(pid,addresses=None,players=False):
    """addresses (opt-in, full-link layout): {'communications','world','physics'} VAs from fulllink_locator; default original.
    players (opt-in, round 39 ai-game): game clock and per-player result/observer/unit fields (EEWorld/EEPlayer/EEPhysics
    offsets from include/ee: mPlayers +0x74, mNumPlayers +0xcc, mCurrentPlayer +0xb84, mVictoryEnabled +0xb96;
    EEPlayer mB467 observer, mUA28 result 1 won / 2 defeated, mResourceCaps +0xaf4, mLB14/mLB18 +0xb14/+0xb18,
    mBB49/mBB4A civ flags; gPhysics mLD0 +0xd0 game ms, mBE6 +0xe6 game over)."""
    comm_va=(addresses or {}).get('communications',0x9319c8);world_va=(addresses or {}).get('world',0x930d40)
    physics_va=(addresses or {}).get('physics',0x9318f0)
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
    kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.ReadProcessMemory.argtypes=[wintypes.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    kernel.IsWow64Process.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.BOOL)]
    handle=kernel.OpenProcess(0x1010,False,pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    def read(address,size):
        data=ctypes.create_string_buffer(size);got=ctypes.c_size_t()
        if not kernel.ReadProcessMemory(handle,address,data,size,ctypes.byref(got)) or got.value!=size:
            raise ctypes.WinError(ctypes.get_last_error())
        return data.raw
    try:
        kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
        image=ctypes.create_unicode_buffer(32768);length=wintypes.DWORD(len(image))
        if not kernel.QueryFullProcessImageNameW(handle,0,image,ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        allowed=pathlib.Path(__file__).resolve().parents[2]/'build/boot'
        actual=pathlib.Path(image.value).resolve()
        if actual.name.lower()!='ee-aoc.exe' or not STAGE_NAME.fullmatch(actual.parent.name) or actual.parent.parent!=allowed:
            raise ValueError('Telemetry is restricted to staged game processes')
        wow=wintypes.BOOL()
        if not kernel.IsWow64Process(handle,ctypes.byref(wow)):raise ctypes.WinError(ctypes.get_last_error())
        P,PF=(4,'<I') if wow.value or ctypes.sizeof(ctypes.c_void_p)==4 else (8,'<Q')   # the target's pointer size
        def ptr(address):return struct.unpack(PF,read(address,P))[0]
        if addresses and addresses.get('image_base') is not None:   # move the map's VAs to the running image
            delta=exe_base(kernel,handle)-addresses['image_base']
            comm_va+=delta;world_va+=delta;physics_va+=delta
        comm=ptr(comm_va)
        if not comm:raise ValueError('No game communications object')
        state=read(comm,max(off('NESynchronousCommunications',0x2e)+1,0x30))
        extra={}
        if addresses and addresses.get('victory_detail') and P==4:   # opt-in (full-link triage): EEWorld/EEPlayer victory fields (VC6 vector reads: 32-bit only)
            extra['victory_enabled']=read(world_va+off('EEWorld',0xb96),1)[0]
            extra['wonders_to_win_b88']=struct.unpack('<i',read(world_va+off('EEWorld',0xb88),4))[0]
            extra['world_bba0']=read(world_va+off('EEWorld',0xba0),1)[0]
            teams=read(world_va+off('EEWorld',0x9b0),12)
            first,last=struct.unpack_from('<II',teams,4)
            extra['teams']=[]
            for t in range(first,last,0x18) if first and last>first and last-first<0x18*20 else []:
                body=read(t,0x18);v=struct.unpack_from('<IIII',body,4)
                vc=struct.unpack_from('<I',body,0x14)[0]
                extra['teams'].append(dict(players=[struct.unpack('<i',read(v[1]+4*k,4))[0] for k in range((v[2]-v[1])//4)] if v[1] and v[2]>v[1] and v[2]-v[1]<0x100 else [],
                                           victory_condition=hex(vc),vc_team=hex(struct.unpack('<I',read(vc+8,4))[0]) if vc else None,team=hex(t)))
            players=[]
            for i in range(1,min(struct.unpack('<I',read(world_va+off('EEWorld',0xcc),4))[0],8)):
                p=struct.unpack('<I',read(world_va+off('EEWorld',0x74)+4*i,4))[0]
                if p:players.append(dict(index=i,defeated_b467=read(p+off('EEPlayer',0x467),1)[0],population_b14=struct.unpack('<i',read(p+off('EEPlayer',0xb14),4))[0],
                                         victory_units_b18=struct.unpack('<i',read(p+off('EEPlayer',0xb18),4))[0],ua28=struct.unpack('<i',read(p+off('EEPlayer',0xa28),4))[0]))
            extra['players']=players
        if players:
            physics=ptr(physics_va)
            if physics:
                body=read(physics,off('EEPhysics',0xe8))
                extra['game_ms']=struct.unpack_from('<i',body,off('EEPhysics',0xd0))[0];extra['game_over_be6']=body[off('EEPhysics',0xe6)]
                extra['pt_ub4']=struct.unpack_from('<I',body,off('EEPhysics',0xb4))[0]   # completed PTs (gPhysics->mUB4, incremented at PT end; round 36)
            extra['local_player']=struct.unpack('<i',read(world_va+off('EEWorld',0xb84),4))[0]
            extra['victory_enabled']=read(world_va+off('EEWorld',0xb96),1)[0]
            rows=[]
            for i in range(min(struct.unpack('<I',read(world_va+off('EEWorld',0xcc),4))[0],17)):
                p=ptr(world_va+off('EEWorld',0x74)+P*i)
                if not p:continue
                body=read(p,off('EEPlayer',0xb50))
                rows.append(dict(index=i,vtable=hex(struct.unpack_from(PF,body,0)[0]),observer_b467=body[off('EEPlayer',0x467)],
                                 result_a28=struct.unpack_from('<I',body,off('EEPlayer',0xa28))[0],
                                 resources=list(struct.unpack_from('<5i',body,off('EEPlayer',0xaf4)+8)),
                                 lb14=struct.unpack_from('<i',body,off('EEPlayer',0xb14))[0],victory_units_lb18=struct.unpack_from('<i',body,off('EEPlayer',0xb18))[0],
                                 civ_wait_bb49=body[off('EEPlayer',0xb49)],civ_chosen_bb4a=body[off('EEPlayer',0xb4a)]))
            extra['player_rows']=rows
        return dict(extra,pid=pid,time=time.time(),last_tick_received=struct.unpack_from('<i',state,off('NESynchronousCommunications',4))[0],
                    physics_ticks_processed=struct.unpack_from('<I',state,off('NESynchronousCommunications',0x10))[0],paused=bool(state[off('NESynchronousCommunications',0x2d)]),
                    multiplayer=bool(state[off('NESynchronousCommunications',0x2e)]),world_players=struct.unpack('<I',read(world_va+off('EEWorld',0xcc),4))[0],
                    world_map=hex(ptr(world_va+off('EEWorld',0xbc))),pointer_size=P)
    finally:kernel.CloseHandle(handle)


def exe_base(kernel,handle):
    """The load address of the process's exe (its first module)."""
    psapi=ctypes.WinDLL('psapi',use_last_error=True)
    psapi.EnumProcessModulesEx.argtypes=[wintypes.HANDLE,ctypes.POINTER(ctypes.c_void_p),wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),wintypes.DWORD]
    mods=(ctypes.c_void_p*1)();need=wintypes.DWORD()
    if not psapi.EnumProcessModulesEx(handle,mods,ctypes.sizeof(mods),ctypes.byref(need),0x3):   # LIST_MODULES_ALL
        raise ctypes.WinError(ctypes.get_last_error())
    return mods[0] or 0

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('pid',type=int)
    parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args()
    rows=[sample(args.pid)];time.sleep(3);rows.append(sample(args.pid))
    result=dict(samples=rows,physics_advanced=rows[1]['physics_ticks_processed']>rows[0]['physics_ticks_processed'])
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result))
