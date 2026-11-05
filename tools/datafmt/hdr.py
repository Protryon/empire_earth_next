import sys, os, struct
D = sys.argv[1]
for f, s in [('dbstartingresources',132),('dbunitbehavior',124),('dbcolortable',168),('dbbuttons',224),('dbcalamity',236),('dbobjects',1960),('dbgfxeffects',232),('dbterraintype',128),('dbweapontohit',132),('dbfamily',380),('dbcpbehavior',56),('dbuifonts',128),('dbuihotkey',124)]:
    data = open(os.path.join(D, f+'.dat'), 'rb').read()
    n = struct.unpack_from('<I', data, 0)[0]
    print('==', f, n)
    for i in [0,1,n-1]:
        r = data[4+i*s:4+(i+1)*s]
        name = r[:100].split(b'\0')[0]
        tail = r[100:104]
        print(f'  [{i}] name={name!r} junk_after_nul={r[len(name):100].strip(bytes([0]))[:20]!r} +64={struct.unpack_from("<i",r,100)[0]} +68={struct.unpack_from("<i",r,104)[0]} +6c={struct.unpack_from("<i",r,108)[0]}')
