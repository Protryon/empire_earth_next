import sys, os, struct
D = sys.argv[1]
sizes = dict(dbaiunittargeting=160, dbambientsounds=152, dbanimals=272, dbareaeffect=188, dbbuttons=224, dbcalamity=236,
 dbcivpowers=152, dbcliffterrain=124, dbcolortable=168, dbcpbehavior=56, dbfamily=380, dbgamevariant=276, dbgfxeffects=232,
 dbmusic=196, dbobjects=1960, dbstartingresources=132, dbterraintype=128, dbweapontohit=132, dbupgrade=240,
 dbunitbehavior=124, dbunitset=184, dbworld=28, dbrandommap=0x84, dbuiforms=268, dbuifonts=128, dbuihotkey=124,
 dbuiback=308, dbuicontrolevents=116, dbuiformevents=116, dbuicontrols=860, dbtechtree=268, dbevents=88, dbeffects=88,
 dbcivilization=120, dbpremadecivs=600)
for f in sorted(os.listdir(D)):
    b = f[:-4]
    data = open(os.path.join(D, f), 'rb').read()
    n = struct.unpack_from('<I', data, 0)[0]
    s = sizes.get(b)
    rem = None
    if s:
        rem = len(data) - 4 - n*s
    print(f"{f:28s} len={len(data):8d} count={n:6d} rec={s} rem={rem}")
