"""TEST ONLY (final validation campaign, lane synth): write a synthetic test scenario for the scripted human.

The base map is a shipped campaign scenario read from the user's game data at test time (default: the Pacific
campaign's "the meat grinder", 100x100); every trigger, condition, effect, object, area, timed/pending effect and
dialogue of the base is dropped (no cinematic, no UI lock, no scripted camera), teams and speed are unlocked, and one
always-true trigger creates a mixed army and base for the human (infantry, cavalry, tanks, guns, engineers, medic,
priest, prophet, citizens, two Furies mechs (self destruct), a strategist and a warrior hero, fortress, towers, airport with planes, dock, transports, warships, carrier,
market, factories; a few infantry at 40% hit points as heal targets) and adds resources. The world, players and AI of the base stay as shipped.

  python -I tools/boot/synth_scenario.py --out STAGE/Synth Army.scn [--ssa ARCHIVE] [--member NAME]

Never commit the output: it is derived from copyrighted game data (generate it into a stage directory).

File layout (from our reconstructed source): u32 length, EEPSaveGameHeader::Load (name, description, version 0/250,
player names, date, game options incl. 16 player slots, chunk tags, media strings, checksum), then FSChunk records
(u32 major, minor, tag, size + data, raw or 'PK01' + u32 length + u32 0 + PKWARE DCL). Tag 6 is the scenario manager:
EEScenarioManager::Load (triggers, conditions, effects, objects, areas, timed and pending effects, ID counters, clocks,
media strings, start camera, dialogue messages, two strings); built-in condition 0 (Always True), effect 0, object 0
and areas 0/1 are not saved, so file entry k is index k+1 (areas k+2). Every shipped scenario round-trips
byte-identically through parse_sm/write_sm (lane synth notes).
"""
import argparse, pathlib, struct, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import savefile
U = 'I'; L = 'i'; B = '?'; F = 'f'; S = 's'   # S: u32 length + bytes (UString / UWideString)

TRIGGER = [('mName', S), ('mText', S), ('mIsOn', B), ('mIsLooping', B), ('mHasFired', B), ('mIsInCinematic', B),
           ('mConditionDuration', U), ('mConditionStartTime', U), ('mID', U)]
CONDITION = [('mName', S), ('mText', S), ('mChatText', S), ('mType', U), ('mObjectIndex', U), ('mPlayer', U),
             ('mAttribute', U), ('mOperator', U), ('mAmount', U), ('mOtherPlayer', U), ('mOtherAttribute', U),
             ('mTechnology', L), ('mTrigger', U), ('mIsTriggerOn', B), ('mHasTriggerFired', B),
             ('mUsesPlayerAttribute', B), ('mMaxTime', U), ('mMinTime', U), ('mHour', U), ('mUsesDifficulty', B),
             ('mUsesElapsedTime', B), ('mUsesHour', B), ('mDifficulty', U), ('mDiplomacy', U), ('mID', U)]
EFFECT_A = ([('mName', S), ('mText', S), ('mDuration', U), ('mMessage', S), ('mS64', S), ('mS78', S),
             ('mNewName', S), ('mFileName', S), ('mDescription', S)]
            + [(n, U) for n in ('mPlayerAttribute', 'mDiplomacy', 'mMessagePlayer', 'mNewOwner', 'mPlayer',
                                'mDiplomacyPlayer', 'mMediaAction', 'mULF4', 'mPlayerAction', 'mTriggerState', 'mType',
                                'mObjectAction')]
            + [('mTechnology', L)]
            + [(n, U) for n in ('mPlacedEffect', 'mPlaceArea', 'mPlaceObject', 'mDaylightHour', 'mCycleTime',
                                'mGameAction', 'mTextObject', 'mAttribute')]
            + [('mAttributePlayer', L)]
            + [(n, U) for n in ('mAttributeUnitType', 'mAttributeUnit', 'mAttributeMode', 'mAttributeAmount',
                                'mObject', 'mStance', 'mTaskAction', 'mTaskArea', 'mTaskObject', 'mCivPoints',
                                'mPlayerAttributeMode', 'mPlayerAttributeAmount', 'mLocationArea', 'mLocationObject',
                                'mTrigger')]
            + [(n, B) for n in ('mB18D', 'mIsClearText', 'mUsesLocationArea', 'mUsesPercent', 'mIsAddTechnology',
                                'mUsesPlaceArea', 'mB193')]
            + [(n, U) for n in ('mAIAction', 'mTargetArea', 'mAIAttribute')]
            + [('mIsOn', B), ('mFloatValue', F), ('mIntValue1', L), ('mIntValue2', L)])
EFFECT_B = [('mULD0', U), ('mTargetObject', U), ('mAIToggle', U), ('mUsesTargetArea', B), ('mIsAddTarget', B),
            ('mID', U), ('mIsAttack', B), ('mUsesFavorites', B)]
OBJECT_B = ([('mIsSelectOnMap', B)]
            + [(n, U) for n in ('mOwner', 'mFamily', 'mClass', 'mParent', 'mMinCount', 'mMaxCount')]
            + [('mIsDynamic', B)]
            + [(n, U) for n in ('mInAreaCheck', 'mNearCheck', 'mHasAttributeCheck', 'mVisibleByCheck',
                                'mHasStateCheck', 'mSelectedByCheck', 'mArea', 'mNearMode', 'mNearObject',
                                'mAttribute', 'mAttributeMin', 'mAttributeMax', 'mVisibleByPlayer', 'mState',
                                'mSelectedByPlayer')]
            + [('mIsAttributePercent', B), ('mL64', U)])
AREA = [('mName', S), ('mText', S), ('ax', F), ('ay', F), ('left', F), ('top', F), ('right', F), ('bottom', F),
        ('mHasRectangle', B), ('mHasAnchor', B), ('mSelectMode', U), ('mTilesTime', U), ('mID', U)]
MEDIA = [(n, S) for n in ('mAftermath', 'mHints', 'mHistory', 'mMovieFile', 'mSoundFile', 'mInstructions',
                          'mMapFile')]
DIALOG = [('mText', S), ('mUnitType', U), ('mSound', S), ('mDuration', U), ('mUnitID', U), ('mScrollSpeed', L),
          ('mIsCinematic', B)]
TAIL = [('mNextTriggerID', U), ('mNextConditionID', U), ('mNextEffectID', U), ('mNextObjectID', U),
        ('mNextAreaID', U), ('mTime', F), ('mTriggerTimer', F)]
TAIL2 = [('mIsMovingCamera', B), ('mStartX', F), ('mStartY', F), ('mShowDebugIDs', B)]


class R:
    def __init__(s, d, p=0):
        s.d = d; s.p = p

    def get(s, t):
        if t == S:
            n = struct.unpack_from('<I', s.d, s.p)[0]; v = s.d[s.p + 4:s.p + 4 + n]; s.p += 4 + n; return v
        if t == B:
            v = s.d[s.p]; s.p += 1; return v
        v = struct.unpack_from('<' + t, s.d, s.p)[0]; s.p += 4; return v

    def rec(s, schema):
        return {n: s.get(t) for n, t in schema}

    def u(s):
        return s.get(U)


class W:
    def __init__(s):
        s.b = bytearray()

    def put(s, t, v):
        if t == S:
            v = bytes(v); s.b += struct.pack('<I', len(v)) + v
        elif t == B:
            s.b.append(int(v) & 0xff)
        else:
            s.b += struct.pack('<' + t, v)

    def rec(s, schema, r):
        for n, t in schema:
            s.put(t, r[n])

    def u(s, v):
        s.put(U, v)


def _read_effect(r):
    e = r.rec(EFFECT_A); e['mObjects'] = [r.u() for _ in range(r.u())]; e.update(r.rec(EFFECT_B)); return e


def _write_effect(w, e):
    w.rec(EFFECT_A, e); w.u(len(e['mObjects']))
    for x in e['mObjects']:
        w.u(x)
    w.rec(EFFECT_B, e)


def parse_sm(d):
    r = R(d); sm = {}
    trigs = []
    for _ in range(r.u()):
        t = r.rec(TRIGGER)
        t['conds'] = [(r.get(B), r.u(), r.u()) for _ in range(r.u())]   # (not, operator, condition index)
        t['effs'] = [(r.u(), r.u(), r.u()) for _ in range(r.u())]       # (delay, operator, effect index)
        trigs.append(t)
    sm['triggers'] = trigs
    sm['conditions'] = [r.rec(CONDITION) for _ in range(r.u())]
    sm['effects'] = [_read_effect(r) for _ in range(r.u())]
    objs = []
    for _ in range(r.u()):
        o = {'mName': r.get(S)}; o['mUnits'] = [r.u() for _ in range(r.u())]; o.update(r.rec(OBJECT_B)); objs.append(o)
    sm['objects'] = objs
    sm['areas'] = [r.rec(AREA) for _ in range(r.u())]
    sm['timed'] = [(r.u(), r.u(), r.get(B)) for _ in range(r.u())]
    sm['pending'] = [_read_effect(r) for _ in range(r.u())]
    sm.update(r.rec(TAIL)); sm['media'] = r.rec(MEDIA); sm.update(r.rec(TAIL2))
    sm['messages'] = [r.rec(DIALOG) for _ in range(r.u())]
    sm['mExportFileName'] = r.get(S); sm['mLastChatText'] = r.get(S)
    sm['_end'] = r.p
    return sm


def write_sm(sm):
    w = W()
    w.u(len(sm['triggers']))
    for t in sm['triggers']:
        w.rec(TRIGGER, t); w.u(len(t['conds']))
        for n, o, v in t['conds']:
            w.put(B, n); w.u(o); w.u(v)
        w.u(len(t['effs']))
        for dl, o, v in t['effs']:
            w.u(dl); w.u(o); w.u(v)
    w.u(len(sm['conditions']))
    for c in sm['conditions']:
        w.rec(CONDITION, c)
    w.u(len(sm['effects']))
    for e in sm['effects']:
        _write_effect(w, e)
    w.u(len(sm['objects']))
    for o in sm['objects']:
        w.put(S, o['mName']); w.u(len(o['mUnits']))
        for x in o['mUnits']:
            w.u(x)
        w.rec(OBJECT_B, o)
    w.u(len(sm['areas']))
    for a in sm['areas']:
        w.rec(AREA, a)
    w.u(len(sm['timed']))
    for i, t, c in sm['timed']:
        w.u(i); w.u(t); w.put(B, c)
    w.u(len(sm['pending']))
    for e in sm['pending']:
        _write_effect(w, e)
    w.rec(TAIL, sm); w.rec(MEDIA, sm['media']); w.rec(TAIL2, sm)
    w.u(len(sm['messages']))
    for m in sm['messages']:
        w.rec(DIALOG, m)
    w.put(S, sm['mExportFileName']); w.put(S, sm['mLastChatText'])
    return bytes(w.b)


def split_file(d):
    """-> (header bytes, [(major, minor, tag, payload)]); the chunk chain is found by scanning (it ends at EOF)."""
    for o in range(0x40, min(len(d), 300000)):
        p = o; out = []
        while p + 16 <= len(d):
            ma, mi, tag, sz = struct.unpack_from('<IIII', d, p)
            if tag < 1 or tag > 12 or sz > len(d) - p - 16:
                break
            out.append((ma, mi, tag, d[p + 16:p + 16 + sz])); p += 16 + sz
        if p == len(d) and len(out) >= 5:
            return d[:o], out
    raise ValueError('no chunk chain')


def join_file(header, chunks):
    b = bytearray(header)
    for ma, mi, tag, pl in chunks:
        b += struct.pack('<IIII', ma, mi, tag, len(pl)) + pl
    return bytes(b)


GO_PLAYER = [('mType', U), ('mName', S), ('mPing', F), ('mSlot', U), ('mColor', U), ('mTeam', U),
             ('mStartingCitizens', U), ('mIsLocal', B), ('mIsReady', B), ('mB36', B)]
GO_TAIL = ([(n, U) for n in ('mGameSpeed', 'mLoadType', 'mVariant', 'mMapSize')] + [('mEpochStart', L), ('mEpochEnd', L)]
           + [(n, U) for n in ('mResources', 'mUnitLimit', 'mWondersForVictory', 'mDifficulty')]
           + [(n, B) for n in ('mVictory', 'mLockTeams', 'mLockSpeed', 'mRevealMap', 'mCheatCodes', 'mRecordGames',
                               'mAllowCustomCivs', 'mIsMultiplayer')]
           + [('mCampaignName', S), ('mScenarioName', S), ('mCampaignChecksum', U), ('mIsScenarioGame', B),
              ('mMapName', S)])
HEADER_A = [('mName', S), ('mDescription', S), ('mMajorVersion', U), ('mMinorVersion', U), ('mPlayerNames', S),
            ('mDate', S), ('mL4E4', U)]


def parse_header(d):
    """EEPSaveGameHeader::Load (minor >= 200 layout)."""
    r = R(d); h = r.rec(HEADER_A)
    h['players'] = [r.rec(GO_PLAYER) for _ in range(16)]
    h.update(r.rec(GO_TAIL)); h['mNumPlayers'] = r.u()
    h['chunks'] = [r.u() for _ in range(r.u())]
    h['media'] = r.rec(MEDIA); h['mChecksum'] = r.u(); h['_end'] = r.p
    return h


def write_header(h):
    w = W(); w.rec(HEADER_A, h)
    for p in h['players']:
        w.rec(GO_PLAYER, p)
    w.rec(GO_TAIL, h); w.u(h['mNumPlayers']); w.u(len(h['chunks']))
    for c in h['chunks']:
        w.u(c)
    w.rec(MEDIA, h['media']); w.u(h['mChecksum'])
    return bytes(w.b)


# ---- the synthetic scenario -------------------------------------------------------------------------------------

INSTALL = pathlib.Path(r'C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest')
BASE_SSA = 'Data/Campaigns/AOCPacific.ssa'
BASE_MEMBER = 'the meat grinder.scn'
EMPTY = b'\x00'

# Placement areas of the base map (the meat grinder, 100x100; tile coordinates as EESMArea rectangles: left <= x <=
# right, top <= y <= bottom), from the map chunk's terrain mesh (water bit of each tile; lane synth meshmap.py): the
# map is one island (x ~44..120, y ~26..150) in open sea. The human gets the island's south part ('base', with the
# west coast for the dock); the Japanese (player 2) units there are removed first, the rest of the island stays
# theirs. 'sea' is open water south-west of the island.
AREAS = {
    'base': (44, 112, 106, 146),
    'sea': (24, 116, 46, 150),
}
CAMERA = (76.0, 128.0)
CLEAR = [(8, 'base')]   # (owner, area): EESMEffect::KillUnits(false) (Remove) of that owner's units in the area

# (unit type, count, area[, hit points %]). Types are dbobjects.dat unit types (record +0x74; names in the comments).
PLAN = [
    # infantry, cavalry, tanks, guns
    (31, 8, 'base'),    # Inf11 - Marine
    (121, 4, 'base'),   # Inf10 - Machine Gun
    (92, 3, 'base'),    # Inf11 - Bazooka Infantry
    (149, 2, 'base'),   # Inf11 - Flame Thrower
    (152, 2, 'base'),   # Inf10 - Trench Mortar
    (19, 2, 'base'),    # AA10 - Stinger Soldier
    (2, 3, 'base'),     # Cav09 - Dragoon (gun)
    (385, 3, 'base'),   # Tank11 Sherman
    (146, 2, 'base'),   # 10 AA - Tank
    (143, 2, 'base'),   # Gun Artillery10 Artillery
    (602, 2, 'base'),   # Gun Siege10 Howitzer Cannon
    (91, 2, 'base'),    # Gun AT10 57mm AT Gun
    # support
    (27, 4, 'base'),    # x10 Engineer
    (829, 2, 'base'),   # Field Medic WWII
    (1, 1, 'base'),     # Priest
    (203, 1, 'base'),   # h1-11 Rommel (heal): strategist hero
    (685, 1, 'base'),   # h2-11 Shackelford (Morale): warrior hero
    (215, 1, 'base'),   # x10 Cargo Truck
    (688, 1, 'base'),   # x10 Spy
    (216, 1, 'base'),   # Prophet (calamities)
    (32, 4, 'base'),    # Citizen
    (259, 2, 'base'),   # Mech Furies (the only class with Self Destruct, command 0x1c)
    (31, 4, 'base', 40),    # Inf11 - Marine at 40% hit points (heal targets)
    (121, 2, 'base', 40),   # Inf10 - Machine Gun at 40% hit points
    # buildings
    (35, 1, 'base'),    # b  Town Center
    (41, 1, 'base'),    # b  Capitol
    (672, 1, 'base'),   # b  Fortress
    (465, 2, 'base'),   # b  Guard Tower - WW1
    (66, 1, 'base'),    # b  Airport
    (36, 1, 'base'),    # b  Dock
    (801, 1, 'base'),   # b  Market (CP)
    (62, 1, 'base'),    # b  Hospital
    (39, 1, 'base'),    # b  Temple
    (34, 1, 'base'),    # b  Barracks
    (63, 1, 'base'),    # b  Siege Factory
    (68, 1, 'base'),    # b  Tank Factory
    (52, 2, 'base'),    # b  Granary
    (37, 3, 'base'),    # b  House
    (145, 1, 'base'),   # b  AA10 Gun 88mm
    # no gate: a Create effect makes a single-piece gate and Lock/Unlock then crashes the original (ORIGINAL_BUGS)
    # aircraft
    (77, 3, 'base'),    # a Fighter11 P51
    (79, 2, 'base'),    # a Bomber 11 B-17
    (85, 2, 'base'),    # a CarrierFighter11 Corsair1
    # ships
    (113, 2, 'sea'),    # s10 Atomic Age Transport
    (119, 2, 'sea'),    # s11 Bismarck Battleship
    (193, 2, 'sea'),    # s10 Good Hope Frigate
    (126, 1, 'sea'),    # s11 Enterprise Aircraft Carrier
    (131, 1, 'sea'),    # s12 Nautilus Submarine
]
HUMAN = 7        # EEScenarioManager::ChoosePlayer: owner 7 = player 1 (owner - 6)
RESOURCES = 20000

EFFECT_DEFAULT = dict(
    mName=EMPTY, mText=EMPTY, mDuration=20, mMessage=EMPTY, mS64=EMPTY, mS78=EMPTY, mNewName=EMPTY, mFileName=EMPTY,
    mDescription=EMPTY, mPlayerAttribute=0, mDiplomacy=0, mMessagePlayer=7, mNewOwner=7, mPlayer=7,
    mDiplomacyPlayer=8, mMediaAction=0, mULF4=0, mPlayerAction=0, mTriggerState=0, mType=1, mObjectAction=0,
    mTechnology=0, mPlacedEffect=0, mPlaceArea=0, mPlaceObject=0, mDaylightHour=15, mCycleTime=0, mGameAction=0,
    mTextObject=0, mAttribute=0, mAttributePlayer=-1, mAttributeUnitType=0, mAttributeUnit=0xffffffff,
    mAttributeMode=0, mAttributeAmount=0, mObject=0, mStance=0, mTaskAction=0, mTaskArea=0, mTaskObject=0,
    mCivPoints=1, mPlayerAttributeMode=0, mPlayerAttributeAmount=0, mLocationArea=0, mLocationObject=0, mTrigger=0,
    mB18D=0, mIsClearText=1, mUsesLocationArea=1, mUsesPercent=0, mIsAddTechnology=1, mUsesPlaceArea=1, mB193=0,
    mAIAction=0, mTargetArea=0, mAIAttribute=0, mIsOn=0, mFloatValue=0.0, mIntValue1=0, mIntValue2=0, mObjects=[],
    mULD0=8, mTargetObject=0, mAIToggle=0, mUsesTargetArea=0, mIsAddTarget=1, mID=0, mIsAttack=0, mUsesFavorites=0)
OBJECT_DEFAULT = dict(
    mName=EMPTY, mUnits=[], mIsSelectOnMap=0, mOwner=7, mFamily=0, mClass=0, mParent=0, mMinCount=1, mMaxCount=1,
    mIsDynamic=1, mInAreaCheck=0, mNearCheck=0, mHasAttributeCheck=0, mVisibleByCheck=0, mHasStateCheck=0,
    mSelectedByCheck=0, mArea=0, mNearMode=0, mNearObject=0, mAttribute=0, mAttributeMin=0, mAttributeMax=0,
    mVisibleByPlayer=1, mState=1000, mSelectedByPlayer=7, mIsAttributePercent=0, mL64=49)


def _z(text):
    return text.encode('latin1') + b'\x00'


def read_base(ssa, member):
    """The base scenario out of a campaign archive ('rass' v1: u32 name length, name, u32 first, u32 last, u32 size per
    entry), whole-file PKWARE DCL exploded: u32 length + EEPSaveGameHeader + chunks."""
    d = pathlib.Path(ssa).read_bytes()
    if d[:4] != b'rass':
        raise SystemExit(f'{ssa}: not an .ssa archive')
    end = 16 + struct.unpack_from('<I', d, 12)[0]; off = 16
    while off < end:
        n = struct.unpack_from('<I', d, off)[0]; name = d[off + 4:off + 4 + n].rstrip(b'\0').decode('latin1')
        a, b, s = struct.unpack_from('<III', d, off + 4 + n); off += 4 + n + 12
        if name.lower().endswith(member.lower()):
            data = d[a:b + 1]
            return savefile.explode(data[12:]) if data[:4] == b'PK01' else data
    raise SystemExit(f'{ssa}: no member ending in {member!r}')


def build(base):
    hb, chunks = split_file(base)
    h = parse_header(hb[4:])
    h['mLockTeams'] = 0; h['mLockSpeed'] = 0; h['mUnitLimit'] = max(h['mUnitLimit'], 1000)
    h['mName'] = _z('Synth Army.scn')
    areas, area_index = [], {}
    for i, (key, (l, t, r, b)) in enumerate(AREAS.items()):
        areas.append(dict(mName=_z('synth ' + key), mText=EMPTY, ax=0.0, ay=0.0, left=float(l), top=float(t),
                          right=float(r), bottom=float(b), mHasRectangle=1, mHasAnchor=0, mSelectMode=0, mTilesTime=0,
                          mID=i + 1))
        area_index[key] = i + 2            # EEScenarioManager::Reset's two built-in areas come first
    objects, effects = [], []
    # objects: the built-in "None" is index 0, so file object k is index k + 1
    for owner, key in CLEAR:
        objects.append(dict(OBJECT_DEFAULT, mName=_z(f'synth clear {owner} {key}'), mOwner=owner, mMinCount=1,
                            mMaxCount=100000, mInAreaCheck=1, mArea=area_index[key]))
        effects.append(dict(EFFECT_DEFAULT, mName=_z(f'remove {owner} {key}'), mType=1, mObjectAction=4,
                            mObject=len(objects), mID=len(effects) + 1))
    for utype, count, key, *hp in PLAN:
        # a hit point percentage: CreateUnits applies the object's attribute check (EEChangeUnit HitPoints,
        # percent, min = max) to each new unit
        damage = dict(mHasAttributeCheck=1, mAttribute=0, mIsAttributePercent=1, mAttributeMin=hp[0],
                      mAttributeMax=hp[0]) if hp else {}
        objects.append(dict(OBJECT_DEFAULT, mName=_z(f'synth {utype}'), mClass=utype, mMinCount=count,
                            mMaxCount=count, mInAreaCheck=1, mArea=area_index[key], mOwner=HUMAN, **damage))
        effects.append(dict(EFFECT_DEFAULT, mName=_z(f'create {utype}'), mType=1, mObjectAction=0,
                            mObject=len(objects), mID=len(effects) + 1))
    # all resources of the human increased by RESOURCES (EESMEffect::SetPlayerAttribute: attribute 1, mode 1)
    effects.append(dict(EFFECT_DEFAULT, mName=_z('resources'), mType=2, mPlayerAction=4, mPlayerAttribute=1,
                        mPlayer=HUMAN, mPlayerAttributeMode=1, mPlayerAttributeAmount=RESOURCES, mID=len(effects) + 1))
    trigger = dict(mName=_z('synth setup'), mText=EMPTY, mIsOn=1, mIsLooping=0, mHasFired=0, mIsInCinematic=0,
                   mConditionDuration=0, mConditionStartTime=0, mID=1,
                   conds=[(0, 0, 0)],                                   # the built-in Always True condition
                   # effects (the built-in Do Nothing is 0): And (1) between entries, End (0) on the last (EESMTrigger::
                   # SetEffectOperator grows the entry list only on a non-End operator)
                   effs=[(0, 1 if i + 1 < len(effects) else 0, i + 1) for i in range(len(effects))])
    out = []
    for ma, mi, tag, pl in chunks:
        if tag == 6:
            sm = parse_sm(savefile.explode(pl[12:]) if pl[:4] == b'PK01' else pl)
            sm.update(triggers=[trigger], conditions=[], effects=effects, objects=objects, areas=areas, timed=[],
                      pending=[], messages=[], mIsMovingCamera=0, mNextTriggerID=2, mNextConditionID=1,
                      mNextEffectID=len(effects) + 1, mNextObjectID=len(objects) + 1, mNextAreaID=len(areas) + 1,
                      mTime=0.0, mTriggerTimer=0.0, mStartX=0.0, mStartY=0.0)
            pl = write_sm(sm)
        elif tag == 3:
            z = struct.unpack('<fff', pl)[2]
            pl = struct.pack('<fff', CAMERA[0], CAMERA[1], z)
        out.append((ma, mi, tag, pl))
    body = write_header(h) + join_file(b'', out)
    return struct.pack('<I', len(body)) + body


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--out', required=True, type=pathlib.Path, help='the .scn to write (a stage directory, not the repo)')
    ap.add_argument('--ssa', type=pathlib.Path, default=INSTALL / BASE_SSA, help='campaign archive with the base map')
    ap.add_argument('--member', default=BASE_MEMBER, help='base scenario member name (suffix match)')
    a = ap.parse_args()
    data = build(read_base(a.ssa, a.member))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(data)
    print(f'wrote {a.out} ({len(data)} bytes; {len(PLAN)} unit groups, {sum(row[1] for row in PLAN)} units)')


if __name__ == '__main__':
    main()
