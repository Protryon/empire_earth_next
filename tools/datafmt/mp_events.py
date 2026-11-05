import struct,sys
e=open(sys.argv[1],'rb').read();print('effects bytes',len(e),len(e)/88)
used=sum(1 for i in range(2000) if any(e[i*88:(i+1)*88]) and struct.unpack_from('<i',e,i*88+0x20)[0]!=0)
print('effect slots with kind!=0',used)
v=open(sys.argv[2],'rb').read();p=0;n=0;ne=0;mx=0
while p<len(v):
    p+=100;i,c=struct.unpack_from('<iI',v,p);p+=8+4*c;n+=1
    if c: ne+=1
    if c: mx=max(mx,max(struct.unpack_from('<%di'%c,v,p-4*c)))
print('events',n,'with effects',ne,'max effect idx',mx)
t=open(sys.argv[3],'rb').read();print('techtree bytes',len(t),(len(t))/268)
