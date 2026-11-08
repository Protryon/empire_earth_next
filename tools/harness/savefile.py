"""TEST ONLY (round 39, agent saveload): parse and compare Empire Earth AoC saved games (.ees).

  python -I tools/boot/savefile.py dump FILE [--out DIR]       chunk table; with --out the decompressed chunks
  python -I tools/boot/savefile.py diff A.ees B.ees [--max N]  chunk by chunk, first differing offsets (decompressed)

Layout (EESaveLoad.cpp Fn14D34B/Fn14CE06/Fn14CCFB, FSChunk, FSMemoryFile::Save, UDataCompression):
  u32 length of the rest; EEPSaveGameHeader::Save (version 0/250 ... date text, chunk tags, media strings, options);
  then one FSChunk per persistent of Fn14C7A7's list: u32 major (0), minor (250; the port's saves 251), tag, size, size bytes of data.
  Data is either raw (a short chunk FSMemoryFile::Compress kept) or 'PK01' + u32 raw length + u32 0 + a PKWARE DCL
  implode stream (decompressed here by a port of Mark Adler's blast.c).
Tags (Fn14C7A7 order): 1 EEPChecksum, 2 gUserInterface->mRandomSeed, 4 EEMap, 3 camera GE3DPoint, 6 EEScenarioManager,
  10 EEEffectManager, 12 EEEnvironmentManager, 5 EEPPlayerHelper (players: EEPlayer/RTSPlayer, units, AI), 7 EEPWorldHelper,
  8 EEPMultiplayerHelper, 11 EEPCalamityHelper.
"""
import argparse,pathlib,struct,sys

TAGS={1:'checksum',2:'random seed',3:'camera',4:'map',5:'players',6:'scenario',7:'world',8:'multiplayer',
      10:'effects',11:'calamities',12:'environment'}


class _Blast:
    """PKWARE DCL explode (blast.c, Mark Adler; zlib licence): literal coding 0/1, dictionary 4..6."""
    LITLEN=[11,124,8,7,28,7,188,13,76,4,10,8,12,10,12,10,8,23,8,9,7,6,7,8,7,6,55,8,23,24,12,11,7,9,11,12,6,7,22,5,
            7,24,6,11,9,6,7,22,7,11,38,7,9,8,25,11,8,11,9,12,8,12,5,38,5,38,5,11,7,5,6,21,6,10,53,8,7,24,10,27,
            44,253,253,253,252,252,252,13,12,45,12,45,12,61,12,45,44,173]
    LENLEN=[2,35,36,53,38,23]
    DISTLEN=[2,20,53,230,247,151,248]
    BASE=[3,2,4,5,6,7,8,9,10,12,16,24,40,72,136,264]
    EXTRA=[0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8]

    @staticmethod
    def construct(rep):
        length=[]
        for r in rep:
            n=(r>>4)+1;l=r&15;length+= [l]*n
        count=[0]*14
        for l in length:count[l]+=1
        offs=[0]*14
        for l in range(1,13):offs[l+1]=offs[l]+count[l]
        symbol=[0]*len(length)
        for s,l in enumerate(length):
            if l:symbol[offs[l]]=s;offs[l]+=1
        return count,symbol

    def __init__(self,data):
        self.data=data;self.pos=0;self.bitbuf=0;self.bitcnt=0
        c=_Blast
        if not hasattr(c,'_lit'):
            c._lit=c.construct(c.LITLEN);c._len=c.construct(c.LENLEN);c._dist=c.construct(c.DISTLEN)

    def bits(self,need):
        val=self.bitbuf
        while self.bitcnt<need:
            if self.pos>=len(self.data):raise ValueError('blast: input ends')
            val|=self.data[self.pos]<<self.bitcnt;self.pos+=1;self.bitcnt+=8
        self.bitbuf=val>>need;self.bitcnt-=need
        return val&((1<<need)-1)

    def decode(self,h):
        count,symbol=h
        code=first=index=0
        bitbuf=self.bitbuf;left=self.bitcnt;l=1
        while True:
            while left:
                code|=(bitbuf&1)^1;bitbuf>>=1
                c=count[l]
                if code<first+c:
                    self.bitbuf=bitbuf;self.bitcnt=(self.bitcnt-l)&7
                    return symbol[index+(code-first)]
                index+=c;first+=c;first<<=1;code<<=1;l+=1;left-=1
            left=14-l
            if left<=0 or self.pos>=len(self.data):raise ValueError('blast: bad code')
            bitbuf=self.data[self.pos];self.pos+=1
            if left>8:left=8

    def run(self):
        lit=self.bits(8)
        if lit>1:raise ValueError('blast: literal flag')
        dict_=self.bits(8)
        if not 4<=dict_<=6:raise ValueError('blast: dictionary size')
        out=bytearray()
        while True:
            if self.bits(1):
                symbol=self.decode(self._len)
                length=self.BASE[symbol]+self.bits(self.EXTRA[symbol])
                if length==519:break
                symbol=2 if length==2 else dict_
                dist=self.decode(self._dist)<<symbol
                dist+=self.bits(symbol)+1
                if dist>len(out):raise ValueError('blast: distance too far')
                start=len(out)-dist
                for k in range(length):out.append(out[start+k])
            else:
                out.append(self.decode(self._lit) if lit else self.bits(8))
        return bytes(out)


def explode(data):
    return _Blast(data).run()


def parse(path):
    raw=pathlib.Path(path).read_bytes()
    total=struct.unpack_from('<I',raw,0)[0]
    if total!=len(raw)-4:raise ValueError(f'{path}: length field {total} != {len(raw)-4}')
    # the chunk list starts at the first (0, 250 or 251, tag 1, size) record after the header
    start=None
    for p in range(4,len(raw)-16):
        if raw[p:p+12] in (struct.pack('<III',0,250,1),struct.pack('<III',0,251,1)):start=p;break   # 251: the port's saves
    if start is None:raise ValueError('no checksum chunk')
    chunks=[];p=start
    while p<len(raw):
        major,minor,tag,size=struct.unpack_from('<IIII',raw,p)
        body=raw[p+16:p+16+size]
        if body[:4]==b'PK01':
            rawlen=struct.unpack_from('<I',body,4)[0];data=explode(body[12:])
            if len(data)!=rawlen:raise ValueError(f'chunk {tag}: {len(data)} != {rawlen}')
            comp=True
        else:data=body;comp=False
        chunks.append(dict(offset=p,tag=tag,name=TAGS.get(tag,'?'),version=(major,minor),size=size,compressed=comp,data=data))
        p+=16+size
    return dict(raw=raw,header=raw[4:start],header_offset=4,chunks=chunks)


def ranges(a,b,limit=20):
    """[(start, end)] differing byte ranges of two equal-or-unequal byte strings (positional)."""
    out=[];n=min(len(a),len(b));k=0
    while k<n and len(out)<limit:
        if a[k]!=b[k]:
            s=k
            while k<n and a[k]!=b[k]:k+=1
            out.append((s,k))
        else:k+=1
    return out


def diff(a,b,limit=20):
    A,B=parse(a),parse(b);lines=[]
    hr=ranges(A['header'],B['header'],limit)
    lines.append(f'header: {len(A["header"])} | {len(B["header"])} B; differing ranges (offset in file) '
                 +(' '.join(f'{s+4:#x}..{e+4:#x}' for s,e in hr) or 'none'))
    for ca,cb in zip(A['chunks'],B['chunks']):
        if ca['tag']!=cb['tag']:lines.append(f'chunk order differs: {ca["tag"]} vs {cb["tag"]}');break
        da,db=ca['data'],cb['data']
        if da==db:lines.append(f'chunk {ca["tag"]:2} {ca["name"]:12} identical ({len(da)} B)');continue
        r=ranges(da,db,limit)
        lines.append(f'chunk {ca["tag"]:2} {ca["name"]:12} DIFFERS: {len(da)} | {len(db)} B decompressed; first ranges '
                     +' '.join(f'{s:#x}..{e:#x}' for s,e in r[:limit]))
    if len(A['chunks'])!=len(B['chunks']):lines.append(f'chunk count {len(A["chunks"])} vs {len(B["chunks"])}')
    return lines,A,B


def main():
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    sub=ap.add_subparsers(dest='cmd',required=True)
    p=sub.add_parser('dump');p.add_argument('file');p.add_argument('--out')
    p=sub.add_parser('diff');p.add_argument('a');p.add_argument('b');p.add_argument('--max',type=int,default=20)
    a=ap.parse_args()
    if a.cmd=='dump':
        S=parse(a.file)
        print(f'header {len(S["header"])} B at 0x4')
        for c in S['chunks']:
            print(f'chunk tag {c["tag"]:2} {c["name"]:12} at {c["offset"]:#x} stored {c["size"]} B '
                  f'{"PK01" if c["compressed"] else "raw"} -> {len(c["data"])} B')
            if a.out:
                d=pathlib.Path(a.out);d.mkdir(parents=True,exist_ok=True)
                (d/f'chunk{c["tag"]:02}_{c["name"].replace(" ","_")}.bin').write_bytes(c['data'])
        if a.out:(pathlib.Path(a.out)/'header.bin').write_bytes(S['header'])
    else:
        lines,_,_=diff(a.a,a.b,a.max);print('\n'.join(lines))


if __name__=='__main__':main()
