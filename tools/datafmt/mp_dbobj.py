import struct,sys,collections
d=open(sys.argv[1],'rb').read()
n=struct.unpack_from('<I',d,0)[0];R=0x7a8
print('records',n,'size ok',4+n*R==len(d))
types=[];cats=collections.Counter();en=0;dup=collections.Counter();mism=0
for i in range(n):
    b=4+i*R
    t=struct.unpack_from('<i',d,b+0x74)[0];c=struct.unpack_from('<i',d,b+0x120)[0];e=d[b+0xe4]
    if e and t!=-1:
        en+=1;types.append(t);cats[c]+=1;dup[t]+=1
        if t!=i: mism+=1
print('enabled',en,'max type',max(types),'dups',sum(1 for v in dup.values() if v>1),'type!=index',mism)
print('categories',sorted(cats.items()))
w=open(sys.argv[2],'rb').read();print('dbworld entries',struct.unpack_from('<I',w,0)[0],(len(w)-4)/28)
