// WON's message buffers (ByteBuffer, WriteBuffer, ReadBuffer): a script of writes, growths, prefixes and reads,
// one line per step (the op, then its result: a buffer's capacity, length and FNV-1a 64 of its bytes, a value read,
// or the read's exception), compared with rust/crates/ee_won/src/golden_buffer.txt. The golden was recorded over the
// C++ classes before their move to Rust (rust/crates/ee_won/src/buffer.rs; the Rust test `buffer_golden` replays the
// same script over the Rust objects). `--print` prints the lines instead. The exit code is the number of failures.
// new[] is zero-filled here: the bytes a growth or SetSize/SkipBytes adds were indeterminate in the C++ and are zero
// in Rust, so the golden sees zeros there and the defined bytes as they were.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <new>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"

using namespace WONAPI;

void* operator new[](size_t inSize)
{
    void* p = calloc(inSize ? inSize : 1, 1);
    if (p == NULL)
        throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

// The script: one step per line, "<object>.<op> [args]". w = the writer, b = the last byte buffer, r = the reader
// (over b or over w's bytes). Generated (seed 12); shared with buffer_golden.rs.
static const char* const kScript[] = {
    "w.new 2 1", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.tobuf 1", "w.state", "b.state", "w.new 0 0",
    "w.tobuf 0", "w.new 3 0", "w.byte 1", "w.tobuf 0",
    "w.new 1 1", "w.byte 7", "w.tobuf 1", "w.new 4 1",
    "w.long 5", "w.tobuf 0", "w.new 4 0", "w.long 5",
    "w.tobuf 1", "w.state", "w.new 2 0", "w.skip 65534",
    "w.tobuf 0", "w.new 2 0", "w.skip 65535", "w.tobuf 0",
    "w.new 2 1", "w.skip 65531", "w.tobuf 0", "w.new 2 1",
    "w.skip 65532", "w.tobuf 1", "w.state", "w.new 1 0",
    "w.skip 255", "w.tobuf 0", "w.skip 1", "w.tobuf 0",
    "w.new 2 1", "w.reserve 0", "w.reserve 1", "w.reserve 33",
    "w.reserve 10", "w.setsize 5", "w.setsize 100", "w.chunk 10 0",
    "w.chunk 200 150", "w.chunk 300 300", "w.chunk 64 0", "w.chunk 1000 999",
    "w.reset", "w.chunk 5 3", "w.reset", "w.skip 0",
    "w.bytes -", "w.length 5 0", "w.length 5 3", "w.length 0x89abcdef 1",
    "w.length 0x89abcdef 2", "w.length 0x89abcdef 4", "w.string 6869 0", "w.string 6869 3",
    "w.wstring d83d.de00 1", "w.wstring - 2", "w.wstring 41 4", "w.buffer - 2",
    "w.buffer -- 1", "w.buffer 0102 4", "w.buffer 0102 0", "w.setbyte 0 0xff",
    "w.setshort 1 0x1234", "w.setbytes 2 a1a2a3", "w.bool 0", "w.bool 1",
    "w.bool 2", "w.short -1", "w.long -2147483648", "w.state",
    "w.tobuf 0", "b.text NULL", "b.state", "b.text -",
    "b.state", "b.text 686921", "b.state", "b.copy -",
    "b.state", "b.copy 00ff", "b.state", "r.overb",
    "r.pos", "r.avail", "r.length", "r.byte",
    "r.byte", "r.byte", "r.avail", "r.rewind",
    "r.pos", "r.short", "r.setpos 2", "r.bytes 0",
    "r.bytes 1", "r.bytes 1", "r.pos", "w.new 0 0",
    "w.bytes 05", "w.bytes 41424344454647", "w.bytes ffffffff", "w.bytes ff7f",
    "w.bytes 0300414243", "r.overw", "r.rlen 1", "r.rawstring 5",
    "r.rlen 0", "r.rlen 3", "r.long", "r.short",
    "r.string 2", "r.rewind", "r.string 1", "r.rewind",
    "r.buf 1", "r.rewind", "r.wstring 1", "r.rewind",
    "r.rawwstring 2", "r.rawwstring 0", "r.pos", "r.setpos 13",
    "r.wstring 4", "r.pos", "r.setpos 13", "r.rlen 4",
    "r.pos", "r.setpos 13", "r.string 4", "r.setpos 13",
    "r.buf 4", "r.setpos 17", "r.wstring 2", "r.setpos 17",
    "r.rlen 2", "r.setpos 19", "r.buf 1", "r.pos",
    "r.setpos 19", "r.string 1", "r.setpos 19", "r.bool",
    "r.bool", "r.setpos 100", "r.avail", "r.pos",
    "r.setpos 19", "r.avail", "r.bytes 0", "r.bytes 1",
    "r.setpos 15", "r.long", "r.short", "r.bytes 1",
    "r.pos", "r.setpos 18", "r.bytes 1", "r.bytes 1",
    "r.pos", "w.new 0 0", "w.bytes ffffff7f00", "r.overw",
    "r.wstring 4", "r.pos", "r.rewind", "r.rawwstring 0x7fffffff",
    "r.pos", "r.rawwstring 3", "r.pos", "r.rawwstring 2",
    "r.pos", "w.new 3 1", "w.setsize 50", "w.byte 247",
    "w.buffer 74004ae1bc53ad6b1e 2", "w.reset", "w.rawstring cf2c091f72", "w.short 25394",
    "w.long 1355911088", "w.reserve 33", "w.state", "w.setshort 6 -28772",
    "w.short 1515", "w.setsize 63", "w.bytes 78ab", "w.setsize 64",
    "w.reset", "w.byte 220", "w.bool 0", "w.wstring dfff.263a.7a.dfff.0.1aa7.0.e948.20ac.20ac.41.20ac.41.0.41.e9.f46c.867d.7a.7a 2",
    "w.skip 1000", "w.setbyte 438 145", "w.setbytes 58 -", "w.byte 136",
    "w.chunk 10 10", "w.state", "w.tobuf 1", "w.new 3 0",
    "w.reset", "w.buffer -- 3", "w.chunk 65 0", "w.reserve 16",
    "w.reset", "w.setsize 50", "w.reset", "w.byte 79",
    "w.byte 19", "w.short -6532", "w.reserve 0", "w.setsize 4",
    "w.buffer fb3f6cc97d24d0b6a52aaa482e9f9f5cdfbb5a0f6bf9e3f85b1daee97aa7317203b8bb8f67dd9770 3", "w.setbytes 32 53ed", "w.reserve 500", "w.wstring dbff.ca63.d800.0.dc00.7a.4596.7a.ffff.ffff.ffff.20ac.41.ffff.0.c266.20ac.7a.41.20ac 4",
    "w.bytes 0c", "w.tobuf 0", "w.string 6497615e9f 1", "w.tobuf 0",
    "w.setsize 145", "w.state", "w.tobuf 0", "w.new 3 0",
    "w.byte 64", "w.reserve 33", "w.bytes c6d4", "w.buffer - 0",
    "w.setsize 0", "w.chunk 64 1000", "w.setsize 0", "w.length 1 2",
    "w.chunk 0 0", "w.wstring - 0", "w.reset", "w.byte 116",
    "w.reset", "w.rawstring d8a17bc82c", "w.state", "w.bool 2",
    "w.setsize 6", "w.state", "w.tobuf 0", "w.new 3 0",
    "w.buffer - 0", "w.wstring ffff.ffff.263a.e9.ffff.40f0.0.e9.20ac.7a.7a.0.e9.263a.0.0.41.e9.7a.e9 0", "w.bytes 7590227760648c", "w.long -1361314745",
    "w.bytes f7", "w.reserve 33", "w.reserve 32", "w.rawstring adb5cc9c450c2695c794f64dd7a05b84d8d31a4819e73b9d7d516b3ec2fb9d2f624a74c283bb6040",
    "w.rawstring -", "w.wstring - 2", "w.tobuf 1", "w.reset",
    "w.bool 1", "w.byte 196", "w.setsize 52", "w.short 26868",
    "w.tobuf 1", "w.chunk 0 40", "w.short 3529", "w.bytes d08f",
    "w.setbytes 0 -", "w.bool 1", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.long -1964871890", "w.length 127 0", "w.setbytes 0 b4",
    "w.wstring dbff.ea89.7a.dc00.263a.0.e9.0.0.dc00.7a.20ac.dc00.e9.20ac.7a.dfff.0.0.263a 3", "w.state", "w.long -89800648", "w.state",
    "w.tobuf 0", "w.new 2 1", "w.reserve 32", "w.rawstring 56",
    "w.long 644364642", "w.rawstring 24a7f0e62e", "w.chunk 64 10", "w.string 492b55a657 3",
    "w.bool 1", "w.setshort 2 -11899", "w.short 27572", "w.state",
    "w.tobuf 0", "w.setshort 14 -13597", "w.buffer 7e858028c9538b0fda15e64066de5d1f09043e48ce996a2de02c59972353cf0dab054d37280588bd 4", "w.buffer 3c 2",
    "w.reset", "w.long -402505772", "w.buffer -- 2", "w.long -972942849",
    "w.state", "w.tobuf 1", "w.new 2 1", "w.skip 0",
    "w.short -31815", "w.string ffc9c5222f50e456e59f9878180164a1fcc8a0594521d34328a8c303a3ec8db5269eadaf0a37acd87888631ed44569057494371a490bbc2dbc6b58de3c2e29b4f45e465bfeacda335f4d9ce33a1f0ef21d1a7ba2069f54d7d425a83f14f9331ef1d89ef6fedb7cf3cf436403eea85777f68f256a4b5d0b57fcda72d2df535bb0fea3 1", "w.string 0a347e83d2 4", "w.reset",
    "w.short -32033", "w.long -2013016540", "w.rawstring -", "w.byte 63",
    "w.chunk 64 1000", "w.byte 228", "w.reserve 16", "w.setsize 1",
    "w.reserve 32", "w.tobuf 1", "w.reserve 32", "w.wstring fa2c 0",
    "w.state", "w.tobuf 0", "w.new 2 0", "w.bool 2",
    "w.length 256 4", "w.long 723577141", "w.bool 0", "w.length 65536 2",
    "w.long -724793487", "w.byte 122", "w.setbytes 9 2c", "w.bool 0",
    "w.setbytes 4 764c", "w.bool 2", "w.setbyte 15 22", "w.setsize 0",
    "w.string 5b44fabf1235e8b9a0cb72d6a03b7197f145ca99482174226049d63e9f383189dff6487da5d567de585f7e879d94eb83fd408a4b657565a605053cebea59bff23d9c46dacfd4f9bca6bbec9db206ed598fe40db042d5bb5249a32edd70479a8dd0032c72dafe83346cc1f58fe3b57036a52b51e43ddcf41044f0fdd8c4737b74ee99 2", "w.state", "w.chunk 33 1000", "w.reset",
    "w.long 836853347", "w.reserve 0", "w.rawstring -", "w.buffer - 0",
    "w.state", "w.tobuf 0", "w.new 1 0", "w.state",
    "w.wstring 7a.dfff.41 4", "w.skip 1000", "w.skip 0", "w.setsize 1060",
    "w.setbyte 40 235", "w.chunk 700 40", "w.reserve 0", "w.setbytes 767 d142ba",
    "w.tobuf 0", "w.setsize 1", "w.bool 2", "w.bytes -",
    "w.state", "w.string 19234480c20b27d3bb80317080ed25d8ba56a5c71aff13dc973905b632b79e851a 0", "w.bytes ae", "w.tobuf 1",
    "w.reserve 32", "w.setsize 0", "w.state", "w.tobuf 0",
    "w.new 3 0", "w.short 12198", "w.bool 2", "w.byte 162",
    "w.rawstring -", "w.length 65536 4", "w.skip 100", "w.skip 100",
    "w.byte 87", "w.state", "w.long -1831861277", "w.reserve 0",
    "w.short 26691", "w.setbyte 202 57", "w.buffer 85 4", "w.reserve 0",
    "w.string 3066829f4a63b4707cb81ad534133a4efdf481a5fb8277877934256d5fa4b1ae 3", "w.chunk 64 10", "w.reset", "w.bool 1",
    "w.rawstring c94647f5eb", "w.string a29e363837 2", "w.tobuf 1", "r.overb",
    "r.bool", "r.rlen 3", "r.rlen 0", "r.buf 2",
    "r.byte", "w.bytes e354859b279537", "w.byte 81", "w.state",
    "w.tobuf 0", "w.new 2 0", "w.reserve 16", "w.buffer -- 3",
    "w.buffer - 4", "w.setbyte 0 169", "w.long 1420582206", "w.setshort 2 -13302",
    "w.bool 1", "w.setbytes 2 -", "w.wstring 20ac 4", "w.wstring d800 4",
    "w.setbyte 14 95", "w.reserve 0", "w.string b3aa680b23 2", "w.tobuf 1",
    "r.overb", "r.rewind", "r.avail", "r.bytes 5",
    "r.rawwstring 1", "w.bytes -", "w.bytes 9ce14d0f8c365322dbecd75b10006b6e85cf90c086185a0b169cf72b4b4222aabc83a98ec8c73a936cda95ac79bef44fc81a5b24434724041003d3b422b34769", "w.bytes 4e2e",
    "w.skip 1000", "w.bytes 6657", "w.bool 1", "w.setshort 825 29530",
    "w.state", "w.byte 186", "w.length 0 2", "w.reset",
    "w.skip 0", "w.state", "w.tobuf 0", "w.new 4 1",
    "w.string 71da8399628b8fdf81ae805119edae608dca678019d7b8d88260da70c2a3f812 1", "w.tobuf 1", "w.state", "w.short 32071",
    "w.short -5734", "w.byte 157", "w.buffer 2d076a7a1cf932a5aeb989e54caa4171ce2022e8bdbc715475cf0c7eb465d0e5c03846ba724d61cb 2", "w.reset",
    "w.reset", "w.string 793a49fb83f0321db83c6390186563698b161052bbbc1d20f3e285f7893109d652 1", "w.chunk 64 0", "w.chunk 65 1000",
    "w.chunk 64 64", "w.bool 2", "w.tobuf 1", "w.long 141523579",
    "w.byte 54", "w.long 1118913731", "w.wstring e9.dc00.ffff 2", "w.state",
    "w.tobuf 0", "w.new 2 0", "w.chunk 10 64", "w.chunk 65 10",
    "w.chunk 0 10", "w.byte 10", "w.buffer 2c9dca6449f7fdc2f65bd5517e3e3377aa239d81da2537e686153eab495dcda490f9a82e3696b70a 1", "w.skip 100",
    "w.long 995944533", "w.wstring - 3", "w.state", "w.tobuf 0",
    "w.new 2 0", "w.skip 100", "w.short 16601", "w.long -1435561154",
    "w.chunk 0 10", "w.buffer f3 2", "w.setshort 53 -10286", "w.wstring 41 2",
    "w.state", "w.chunk 65 10", "w.long 1522337850", "w.buffer 8675034b93e52feec76f1693127634c258f907ea929fd8dfb50a050842129057cd7036191dfaf383 2",
    "w.long -101923291", "w.long 955442322", "w.rawstring b8bbc7b075", "w.setshort 36 21245",
    "w.buffer 8f2252172f01fe2a65 1", "w.string 059667a82d0c9c004f8d2a6f05318c2e2199ae350eab6255194220667a0e55a5bb106989957460d81b86f3ca4060e7bffa242f663c3d2f5a8857178b8321d88759c76a4d852499826a5720a3e47a1c3bd3cc42f8c92f0ea4aad510ab82d734c08b7f3a84027d9dcb4570c0c659ab7e97d5ac20aacb94964f30763d21b66c0443838c 4", "w.setbyte 225 182", "w.length 4294967295 2",
    "w.bytes 886fd7bd645e3f2fc9c92491eea41affa4d1a05bbc2d33b69f644a1dbcb7bf0a8dcc939397be3994eb209050381a92adf2d57a4b91a05a74a85580807a6c93f0", "w.setbyte 348 116", "w.long 2081894987", "w.bool 0",
    "w.buffer d1 2", "w.bytes 5b", "w.setsize 391", "w.string bb68ccc602 1",
    "w.long -1308992469", "w.setsize 401", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.bool 2", "w.buffer 0804d494625fbf9537 1", "w.setbytes 5 d69a",
    "w.long 126512110", "w.bool 0", "w.setbyte 4 142", "w.rawstring -",
    "w.setbytes 12 a8", "w.short 26577", "w.bool 1", "w.reserve 32",
    "w.setshort 8 -18760", "w.tobuf 1", "r.overb", "r.byte",
    "r.setpos 2", "r.wstring 4", "r.short", "w.skip 1",
    "w.long -1629735511", "w.length 128 3", "w.chunk 700 40", "w.reset",
    "w.skip 1", "w.state", "w.tobuf 0", "w.new 4 1",
    "w.state", "w.wstring 263a.7a.dbff.e9.acb4.41.263a.0.ffff.20ac.7a.20ac.0.41.e9.20ac.41.7a.0.263a 2", "w.skip 0", "w.wstring 7a.7a.d800.e9.e9.20ac.7a.1b05.20ac.41.0.ffff.41.263a.e94d.263a.263a.46ca.ffff.41 4",
    "w.reserve 16", "w.skip 100", "w.state", "w.rawstring dbac0e82d0",
    "w.buffer fc665bea001c0dc721 4", "w.chunk 10 10", "w.setbyte 103 205", "w.state",
    "w.tobuf 1", "w.new 2 1", "w.reserve 16", "w.tobuf 0",
    "w.long 207938841", "w.chunk 10 64", "w.long 1502414919", "w.setshort 3 -30171",
    "w.bool 2", "w.length 2010463020 3", "w.bool 1", "w.chunk 0 40",
    "w.string 95f839c5dd2a2d26440659a092a7dc5ae975752c7eefcb4c21b35faec091aae3d5 4", "w.state", "w.tobuf 1", "w.new 1 0",
    "w.byte 164", "w.string 52c00caac38451354438afe87958fb552948a580cb820038980b15fdd237f3df9cff6a8d80f237a1d325ce5594c4cdaba902306b5aa194fa00670b162fc15df6fcbfe9f83c2acd278fa8f439c0b6ad060a233ee9458bf5faf59a9fa3f8b67723e072c6d482d48da7b87fa8bfa238d6ef1d81adb423ec1d39bb12e70d33d34f8179ca 3", "w.wstring - 2", "w.bool 0",
    "w.state", "w.tobuf 0", "w.new 2 1", "w.bytes af",
    "w.byte 159", "w.state", "w.bytes de05660bef8923c9b810946dfd042a6cf1a314e60690d2913635844183526089f3d20e28180f2d27b306d87ad9329e5306489e3409fbc90e59dd7e61c10f1669", "w.tobuf 1",
    "r.overb", "r.byte", "r.rewind", "r.bytes 0",
    "w.short -629", "w.wstring 263a.20ac.dc00 2", "w.state", "w.long 186462394",
    "w.setsize 13", "w.state", "w.bool 2", "w.tobuf 1",
    "r.overb", "r.buf 4", "r.byte", "w.short -1238",
    "w.state", "w.bytes -", "w.bytes -", "w.byte 12",
    "w.bool 0", "w.bytes -", "w.state", "w.tobuf 1",
    "w.new 4 1", "w.reserve 500", "w.buffer -- 4", "w.reset",
    "w.short 30041", "w.short 10569", "w.setbytes 0 01ad", "w.buffer -- 4",
    "w.bool 2", "w.tobuf 0", "r.overb", "r.rlen 3",
    "r.avail", "w.state", "w.tobuf 1", "w.new 2 1",
    "w.length 1 1", "w.rawstring be66b9eec2c2f32e93000f957d194b59e15c38a9f98abf21504d6887ba59df0e724c67f34285b94d", "w.byte 101", "w.wstring ffff.ffff.41 1",
    "w.setbytes 7 0d33", "w.bytes a6", "w.long -761461290", "w.bool 1",
    "w.setsize 0", "w.buffer a8 2", "w.buffer 6af798561c73cbd836676f81371f551865eb494691fe7e7b978494d179dd46c08ab5fdbc79e06d3d 1", "w.wstring 263a.0.20ac 0",
    "w.state", "w.tobuf 0", "r.overb", "r.rewind",
    "r.setpos 4", "r.rawstring 3", "r.short", "r.string 1",
    "r.byte", "w.wstring - 1", "w.chunk 700 0", "w.setbytes 12 8c",
    "w.short 30846", "w.bool 0", "w.string 82ddb75eaa 1", "w.setbytes 7 0ffe",
    "w.long -595381617", "w.short -1253", "w.setbyte 11 81", "w.state",
    "w.byte 237", "w.skip 1", "w.bytes 876a6c499f7d49af95639781f00f46466d4460fd37febdc5f411706343aa7254df51757ac1d9ab348c265f0a360286abc44c808d29dd09bf52a4984a2808b977", "w.state",
    "w.tobuf 1", "w.new 2 1", "w.string 4a95b8a460960791c5eab9d619335e5805a980a5090fcdb6bd1afeedcc89fd3527 3", "w.wstring 0.41.dfb4 1",
    "w.rawstring 77", "w.length 128 2", "w.string 7680397019 1", "w.buffer 94 0",
    "w.reset", "w.string f7cbc210a98c61b148aa130c813931c252803ef89960323c227d771e286b3e 4", "w.buffer 9753c73f7e76d34aee 0", "w.wstring 263a.20ac.e9.7a.ffff.7a.263a.da2d.e9.e9.263a.562f.263a.0.41.7a.d6f.ffff.20ac.7a 2",
    "w.reset", "w.tobuf 1", "w.chunk 0 10", "w.reserve 32",
    "w.setsize 0", "w.bytes 3fcc18b9074942", "w.string 829e52158c645a771d0cf30e67b7beda344b4ce1f6252a6841f5a070fec3cc4e40 4", "w.rawstring fd",
    "w.byte 51", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.reset", "w.bytes e781cb7db804cf", "w.bool 1", "w.string 018f68fa1c0db5c21d51bbf54e27f0a9d71fd523bcc6c65066d3b996301189d33234a1842ac05de04f30e159fdacd775c41cb9dae6c6df5c4c9600eb95a270f9420d6284f04a 3",
    "w.byte 35", "w.setshort 70 -29713", "w.state", "w.tobuf 0",
    "w.bytes be8d1399bd8e604960f6c59d0d4d229ed1eaaf1718a18ccede83b3a121199a51a11cfeb43e4ba20bb501f4fc8306cbca5a9e990649006e589ddcce0455fa18b5", "w.bytes 7f", "w.setshort 55 -9186", "w.state",
    "w.bool 2", "w.reserve 500", "w.long -1580868366", "w.setbyte 81 242",
    "w.short -7715", "w.tobuf 0", "w.setshort 48 21241", "w.reserve 32",
    "w.bool 0", "w.setbytes 37 -", "w.state", "w.tobuf 1",
    "w.new 0 0", "w.reset", "w.chunk 0 10", "w.short -29055",
    "w.skip 3", "w.bool 1", "w.state", "w.short -24613",
    "w.short 13125", "w.bytes f4e1248ad7052d", "w.bool 2", "w.tobuf 1",
    "r.overb", "r.rewind", "w.wstring 20ac.20ac.7a.263a.e9.8fa6.aa6f.41.3f16.20ac.41.ffff.e9.97cb.d44b.41.0.e9.263a.ffff 3", "w.wstring 79c3.41.d800.20ac.eb8a.e9.26b2.7a.7a.ffff.41.263a.0.dbd6.263a.20ac.20ac.7a.e9.e9 2",
    "w.wstring 41 0", "w.setshort 74 9392", "w.skip 1000", "w.setbyte 893 100",
    "w.setbyte 952 31", "w.length 65535 1", "w.setbytes 21 -", "w.length 0 4",
    "w.rawstring -", "w.bool 0", "w.buffer 14 2", "w.tobuf 1",
    "r.overb", "r.bool", "w.state", "w.tobuf 1",
    "w.new 1 1", "w.state", "w.byte 184", "w.skip 3",
    "w.setsize 1", "w.reset", "w.rawstring 3bc1606939bc98b9185c4f37f42a10dd1f25a27f81c950f802c1ab461d650f4eb3e77c5e77fbc16a", "w.chunk 65 40",
    "w.byte 137", "w.setbytes 17 -", "w.length 65535 3", "w.reset",
    "w.chunk 10 64", "w.skip 1", "w.string 4c4e9a969dc8e6f57af3a388220566c958f341305d25045bb478ce5a6d33076b97e9548e67c715add4bcca42c6a10713eca1e9873abf735854e4d9bfb579f61b0295c594a54ddf1c6cb9a1a00a4df7ea8b078773ef5c5015cca0cba961237ada5b45fd6fced5927843270280ef0106475add30cf4a0c346983b8531c532a3802e966 4", "w.bool 0",
    "w.setbyte 86 154", "w.short -6810", "w.state", "w.tobuf 1",
    "w.new 3 1", "w.setsize 0", "w.tobuf 1", "w.buffer 5f44de7a87a3e630e5 2",
    "w.wstring 64ec.41.e9.dbff.41.dbff.ffff.ffff.dfff.263a.41.bb2d.263a.e9.e9.0.263a.41.0.e9 2", "w.reserve 500", "w.string dc56b63eb6c98c26632b64f1b75bfba2cf2d38698ba002046590e1b2699c7885 0", "w.byte 189",
    "w.reset", "w.state", "w.tobuf 1", "w.new 0 1",
    "w.buffer e3af383364b957db60 1", "w.chunk 0 64", "w.long 208858479", "w.reset",
    "w.rawstring c76240571c", "w.bytes 88", "w.tobuf 0", "r.overb",
    "r.buf 1", "r.bytes 1", "r.bool", "r.bool",
    "r.rawwstring 1", "r.rewind", "r.wstring 1", "w.bytes -",
    "w.chunk 0 64", "w.chunk 65 10", "w.setshort 2 -28036", "w.setsize 1",
    "w.length 3199820031 2", "w.bytes ba916b566e24f3", "w.state", "w.tobuf 1",
    "w.new 4 1", "w.string c9ab3f0cdea539b2ad33d2089f045d01123c9dc7d7a480e5114a80313738f9a25b4570159d15c5091091d4f6d33ad3ca674d8721e0d0e56a9e9d73b8b99d217352ed7bd0ac771eaf1dc14f418f46667d063fe2baa86e02ca726b509468e6252cd9113aca8ebd19fe4639ff3120199c9316b34b20a55fb05ce9ce3fee384047a38279 4", "w.string 4747fc69bc16f702693e48729578fbd42550f5b100f253f63079e6b741a151eb 4", "w.bool 2",
    "w.skip 1000", "w.wstring ffff.41.263a.7a.e9.7a.7a.0.20ac.e9.263a.0.e9.0.7a.263a.263a.7a.20ac.20ac 3", "w.string de204b19e228e415405786fa6cce5d45b8ffee8086a39c5f74e3ab4c3091648371 1", "w.chunk 65 64",
    "w.setsize 1245", "w.state", "w.tobuf 1", "w.new 4 1",
    "w.reset", "w.length 256 4", "w.long -61233834", "w.state",
    "w.state", "w.short 3198", "w.rawstring 34", "w.tobuf 0",
    "r.overb", "r.avail", "r.buf 1", "r.string 2",
    "r.bool", "w.setbytes 4 ee", "w.skip 1", "w.rawstring be14d179b3a08b4a5ff2e3f6cef196fc67581e611cd9ccaac8a48a761deb57b78404935fa14713e0",
    "w.setbytes 1 e805", "w.reserve 500", "w.length 255 4", "w.length 128 4",
    "w.reset", "w.reset", "w.rawstring -", "w.state",
    "w.tobuf 1", "w.new 0 1", "w.reset", "w.buffer 6569328a8e1417b2b9 3",
    "w.state", "w.chunk 33 10", "w.setbytes 2 acb6", "w.tobuf 1",
    "w.reserve 500", "w.chunk 0 0", "w.state", "w.tobuf 0",
    "w.new 4 1", "w.tobuf 1", "w.bytes 20ef", "w.setsize 52",
    "w.reset", "w.short -21452", "w.reserve 0", "w.chunk 0 1000",
    "w.string 32b1a314a0c03741e16a104ab4c2750cf283da7ac1b2cac6d4cac1d856a7d32021a719afabd26bec8ba28d58be71b1a8169aaa9f1b674b7a482ffc4ac0cb76f3d7918079d19d3f2d47d6544a3377a439d88c8dfaf3fe72d560d3bbe88f7bb05a3a1c8528470f414f8c62bb59b634c4a5b2f822e83ae12cb8d51923f7c0f823f9111f 1", "w.setsize 1", "w.length 3333436363 4", "w.rawstring 71ee18d949",
    "w.skip 0", "w.rawstring -", "w.setbyte 4 128", "w.reserve 500",
    "w.setshort 1 13509", "w.buffer 09ac9d053b5bf56cf0 4", "w.setsize 1", "w.bytes 96bafb7ed74d213b42f207ea37437e148d1899b8b187df7212d0e8612663a918ef09bbb48ca894c419bf8973ead2bfa3430da0500554c23dc0a82381c5ad589d",
    "w.reset", "w.short 19554", "w.rawstring dd", "w.byte 142",
    "w.buffer - 2", "w.reset", "w.rawstring -", "w.state",
    "w.tobuf 1", "w.new 4 1", "w.setsize 50", "w.short 1348",
    "w.bytes 8a01fd1b5e7b36", "w.string 98b5a98f6d0b9dd2dd596ff991f0191bb27b227a3b4d60905505dcd870f9d4b2cf 0", "w.byte 236", "w.setbyte 59 155",
    "w.wstring - 3", "w.chunk 0 40", "w.reset", "w.reserve 33",
    "w.byte 106", "w.buffer 3d02c32998626d8b6a1d56274e7c254d5bc72b029c77c9c785bc4d8a9e8a8cb46e0fdfb72e6f26ec 1", "w.reset", "w.length 1 3",
    "w.chunk 10 10", "w.string 8b 2", "w.long 1670591001", "w.setsize 6",
    "w.state", "w.reset", "w.string 19 0", "w.skip 1",
    "w.string 66f004b5ea722d5456027e190b17b78f6e502a479783e7cd703a1f6e823c28 4", "w.setshort 25 2464", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.rawstring -", "w.bytes 73c2fe371c7629e258e7fb15de580b8949161e2fd854410f15598e2a73a59f4bef9f02fbd4856021d6db2b357a9708380934dc4688ecac4ccfaa853c891e9395", "w.setbyte 11 163",
    "w.setshort 29 11789", "w.setshort 2 -14637", "w.reset", "w.tobuf 1",
    "r.overb", "r.pos", "w.state", "w.buffer - 4",
    "w.setshort 0 24077", "w.bool 1", "w.byte 186", "w.wstring 7a 2",
    "w.tobuf 1", "r.overb", "r.rewind", "r.avail",
    "r.rawstring 1", "r.long", "r.string 4", "r.rlen 0",
    "w.buffer 15 2", "w.tobuf 0", "r.overb", "r.pos",
    "r.avail", "r.long", "r.setpos 2", "r.avail",
    "r.wstring 1", "r.rlen 2", "w.rawstring 610ecb7c0f82c85259ee4fb65bc789057b1a2cd66c26d5a7771fb3a7dd4decf87db5a4ff3675a829", "w.reset",
    "w.buffer -- 3", "w.state", "w.tobuf 1", "w.new 3 1",
    "w.tobuf 0", "r.overb", "r.short", "r.rlen 3",
    "r.buf 2", "r.long", "r.bytes 5", "w.buffer 69c3cdb3c10dde84f931b5636d5d85efe50a9e17f6f1cba2d301fbf851642797c0790677184b6d25 4",
    "w.bytes 1f", "w.wstring 41.20ac.7a.f138.263a.e9.263a.dc00.dbff.20ac.7a.e9.20ac.dbff.263a.20ac.d800.e9.f955.7a 2", "w.setsize 86", "w.string d4fef86bcfa2a84cfb38ec40879d81cdf98496deebb6ebddbadc07e236a07206 2",
    "w.skip 1000", "w.chunk 64 1000", "w.setshort 618 -19211", "w.string b17c905afe83942582eea9c581fc6b8e1f9161aabb776c20d997f4b66c560b 1",
    "w.bool 1", "w.setbyte 403 138", "w.state", "w.byte 208",
    "w.setbyte 400 142", "w.rawstring ac2c5ff62a", "w.setshort 311 12884", "w.length 2147483648 2",
    "w.wstring - 2", "w.reserve 500", "w.bytes 9200c7a57583ce", "w.state",
    "w.string b3444b72dc8bf1842301ebfddf89c90383011451fcede4b694f7a164fa15b6 4", "w.setbytes 233 -", "w.bool 0", "w.state",
    "w.tobuf 1", "w.new 3 1", "w.reserve 32", "w.setsize 0",
    "w.long -62165598", "w.tobuf 0", "r.overb", "r.byte",
    "r.string 2", "r.rlen 0", "r.rewind", "r.avail",
    "r.avail", "r.byte", "w.string 05afca3cf13dda594127f9a00d1226025defa39ba22a31b2269414258e2d8964 4", "w.reserve 500",
    "w.state", "w.tobuf 0", "w.new 3 0", "w.bytes 29a631fa976e01",
    "w.setsize 0", "w.buffer 38 4", "w.setsize 1", "w.buffer bfefc75b9b113012d0ff121c58674d71c9554358fa3298cb48e6e58f320316ccd4d01009a2496e52 1",
    "w.length 65535 3", "w.setbyte 6 228", "w.long 2133516836", "w.wstring dbff.29f.41 0",
    "w.buffer - 3", "w.chunk 700 40", "w.skip 3", "w.reset",
    "w.tobuf 0", "w.string e9 0", "w.bool 1", "w.reserve 33",
    "w.state", "w.tobuf 0", "w.new 4 0", "w.tobuf 0",
    "r.overb", "r.setpos 6", "r.rawstring 1", "r.pos",
    "r.long", "w.setsize 0", "w.reserve 32", "w.bytes 96ca",
    "w.length 65535 1", "w.bool 1", "w.setsize 0", "w.string 8f34c46764fa661cc0a14177178a600663c7c955387de470a484888ec44e14 4",
    "w.short 13307", "w.setsize 0", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.bool 2", "w.string 7ff257a633527912640881c1e771cde0ade1505f990186b04d488683a8dc4b12 1", "w.long 510680634",
    "w.skip 100", "w.tobuf 0", "w.setshort 43 -6971", "w.string edf8e8f2bebd90650eae9926f8a3dc690c11f704c2821b353151c6371396aa7f23afa400cbce26f170a3d81061fa5609c2be789ec58be589c16969dfe2f8b000e9d3de26624998160a42f527ef33776facad297c727b65491b0324f696b2750221900b9a7a57eab6f6fa15bd424f9ac2a59a509aa8a33655c78d26e674c0b27228f7 1",
    "w.bytes 3d", "w.state", "w.tobuf 1", "w.new 2 0",
    "w.rawstring -", "w.bytes 4fd8db2a3965b6e8b66f953ddc10174c17e939e594fc4356cd7381a597fcc68dff2f99c5390667e730b4373aa7f95e4e933898b3233478927b249580670ea2bc", "w.setbytes 31 4b", "w.setshort 51 -13157",
    "w.setsize 1", "w.chunk 64 40", "w.state", "w.tobuf 0",
    "w.new 3 0", "w.bool 2", "w.reset", "w.wstring - 4",
    "w.bool 0", "w.long -510419443", "w.chunk 33 0", "w.buffer 60 4",
    "w.tobuf 1", "w.state", "w.bool 1", "w.state",
    "w.string 52baddb260a01cced3acdaacb7154e58e16d7039fa839f40c555df1f8da24f 4", "w.setshort 22 -27233", "w.skip 1000", "w.setsize 0",
    "w.length 128 4", "w.buffer 3d 2", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.chunk 33 10", "w.byte 40", "w.byte 158",
    "w.length 65535 4", "w.short 27056", "w.reset", "w.length 65535 2",
    "w.length 65536 0", "w.state", "w.skip 3", "w.short 23679",
    "w.setsize 57", "w.reset", "w.length 4294967295 1", "w.length 256 4",
    "w.long 2134779895", "w.bytes af5ccec35ce8543122fdda8e9caeff0898de3fb5eb639c50532a987af8c8b1a40b960935bb97a2efa8143b810950367193418da790710ea07f08faa4e9902628", "w.length 128 0", "w.state",
    "w.state", "w.bool 0", "w.setshort 65 -20443", "w.skip 3",
    "w.string 0d7bc1711dc48d18ff559f411f3d100ff62f98bf22e2a2a3d213f9d53c1a2b 2", "w.string 2a24ab4d8a 2", "w.reserve 0", "w.state",
    "w.tobuf 0", "w.new 4 1", "w.string 7a067d2aaa 0", "w.buffer - 2",
    "w.string 954c6479659a0541b64fdc257e3e3d07727218694d83e8deeab11dcfa284d6dce3 4", "w.skip 1000", "w.buffer 1b 1", "w.tobuf 1",
    "w.string c309b2a4d15546afaab338d6557829eea201827f168d1fcd0f4715ee9f5cf53541 4", "w.length 256 2", "w.setshort 6 -20790", "w.string da 2",
    "w.reset", "w.length 65536 4", "w.wstring ffff.0.20ac.ffff.41.ffff.dbff.0.dbe5.0.7a.3969.dc00.e9.dbff.41.20ac.dc00.0.7a 3", "w.state",
    "w.state", "w.tobuf 0", "w.new 1 1", "w.string da5642f88f78bba62a27d81d4d2cad0f4579dffe5e005f50d1c5bc824df8598d62a513c9917b5003cc7fed3d0b3cbd71a5a5a20b3b63e20792e7786c4e9140271ab758679ed87b218b44b8dbca055eb1bfb455978306c01de2aae1c1495eda284a6d4664c156be77514fd1b46c3e355edfd1bee951521e33aa86547b02b7bd4c171e 3",
    "w.bool 0", "w.short 5183", "w.rawstring 35ba8a38e6", "w.wstring 5817.ffff.7a.dbff.3336.d286.0.41.c95b.7a.ffff.41.0.e9.0.e9.41.e9.20ac.1630 4",
    "w.string f7c6d79f085223166d2c02829fa3876d3e29702796c911e1217f069a53b48414 4", "w.short 10136", "w.rawstring -", "w.wstring - 3",
    "w.byte 129", "w.buffer 0c3df2ba1e62c594ff93ed3967b68488bacfbd39a1a32543e92ff75e1b96846796a77de5df56f5f8 3", "w.long 334162446", "w.setbytes 223 79",
    "w.string 2d1b8932c84df6a757978418ccfd4756526e670acfeaa36b222b9235c5ec8ba151 3", "w.setbyte 213 81", "w.bytes a2", "w.setbytes 145 79e3dd",
    "w.state", "w.tobuf 1", "w.new 0 0", "w.short -29953",
    "w.short -7407", "w.short 17609", "w.setbyte 0 31", "w.bytes fe",
    "w.state", "w.tobuf 0", "w.new 2 0", "w.long 1353042572",
    "w.bytes b57becd2ccebab", "w.reset", "w.setsize 0", "w.short -26183",
    "w.short 27848", "w.length 1 2", "w.setshort 0 13537", "w.long 145750271",
    "w.setshort 5 -27223", "w.long 732993355", "w.byte 179", "w.reset",
    "w.skip 3", "w.short 29026", "w.length 65535 3", "w.setbyte 1 38",
    "w.string 982823785d96f046cf5c49e99bab8c658a198ccc03e3de26a785f3587ef5f29f 4", "w.state", "w.tobuf 1", "w.new 1 1",
    "w.byte 35", "w.state", "w.chunk 64 0", "w.byte 139",
    "w.short 10141", "w.tobuf 1", "r.overb", "r.avail",
    "w.short -16484", "w.bytes eb40", "w.setshort 0 16434", "w.state",
    "w.bool 1", "w.rawstring def2b183e1e931013284a19f7f0f47ea2efbf88017c5a9939a7e6c99663fc7177b6e4f1950c86011", "w.buffer -- 0", "w.skip 1000",
    "w.setbyte 502 151", "w.reset", "w.tobuf 0", "w.chunk 0 1000",
    "w.setsize 0", "w.buffer -- 2", "w.reset", "w.skip 3",
    "w.skip 1", "w.wstring 0.41.20ac.e9.e9.263a.263a.7a.ffff.41.0.ffff.41.2eba.ffff.0.263a.41.e9.0 1", "w.long 570589297", "w.state",
    "w.tobuf 1", "w.new 4 1", "w.string ec9849697d 4", "w.reset",
    "w.string b5eb1667537173d1943bd41d550dd71c7656318276a711c25063583c64fab694 2", "w.reset", "w.tobuf 1", "r.overb",
    "r.string 2", "r.wstring 4", "r.buf 4", "r.buf 4",
    "r.bytes 5", "w.long -1723762956", "w.short 831", "w.short -17409",
    "w.state", "w.byte 240", "w.state", "w.byte 5",
    "w.tobuf 1", "w.string 8c7b361c8c2d26a5969bfd7db81a0f72b85cb044cd244ad842af2d3d767c3e647d9f315e0325be38115bca6c64bf72983b244bdaddbc0a2d01ba49b512c0fa8289927b629261 4", "w.string 8f 3", "w.skip 0",
    "w.state", "w.string ea 3", "w.bytes ae85", "w.state",
    "w.tobuf 1", "w.new 2 0", "w.tobuf 1", "r.overb",
    "r.rewind", "r.wstring 4", "r.rawstring 3", "w.bool 1",
    "w.byte 172", "w.bytes -", "w.tobuf 0", "w.buffer 94446bf05f5052e8f3 1",
    "w.reserve 0", "w.bytes a1", "w.wstring - 4", "w.reset",
    "w.chunk 33 10", "w.reserve 32", "w.setsize 1", "w.reserve 16",
    "w.tobuf 0", "w.bytes 1137323e02831b", "w.byte 11", "w.string 709b9ea5f44fc4aba11f1513b6ea9e34e9d81ea0aa514d88bf2c0e9d4b21b5e391 4",
    "w.skip 100", "w.chunk 0 64", "w.setshort 67 7821", "w.byte 56",
    "w.tobuf 0", "w.state", "w.tobuf 0", "w.new 4 0",
    "w.rawstring f1c872770a", "w.rawstring 40", "w.reserve 32", "w.short 10489",
    "w.setbytes 3 1aa455", "w.chunk 0 40", "w.string 8880af81dc 4", "w.setbyte 3 206",
    "w.string 4616a792bb0df0f900df3f4a5e0937e192cd40b8262e5131b9f75d975cadf580 2", "w.tobuf 1", "w.byte 153", "w.skip 1",
    "w.rawstring -", "w.tobuf 0", "r.overb", "r.byte",
    "r.byte", "r.byte", "r.byte", "r.setpos 0",
    "w.wstring - 4", "w.rawstring 51c7e48961", "w.long -2045289228", "w.bool 1",
    "w.reset", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.tobuf 1", "r.overb", "r.rawwstring 3", "r.rewind",
    "r.string 1", "r.rawwstring 1", "r.avail", "w.string ab2aa9c1708ca1d6c9c213c397ba9edc9e84e970961721dc04ac0460770a24 4",
    "w.bytes ed", "w.wstring - 1", "w.reset", "w.reset",
    "w.reset", "w.wstring 7a 4", "w.length 128 2", "w.setsize 0",
    "w.rawstring 701485fa262806368a5356833721161d9a3b2a6640ef20de42d0f27244cc2fbee77142cb3479c482", "w.long -984122584", "w.chunk 33 0", "w.short 29194",
    "w.rawstring 66891c54a3", "w.chunk 64 64", "w.setsize 51", "w.skip 3",
    "w.length 65535 3", "w.reserve 32", "w.tobuf 0", "r.overb",
    "r.bool", "w.short 12593", "w.tobuf 1", "r.overb",
    "r.bytes 2", "r.rawwstring 0", "r.wstring 4", "r.rlen 1",
    "r.short", "r.long", "r.byte", "w.buffer - 4",
    "w.state", "w.tobuf 1", "w.new 4 1", "w.tobuf 1",
    "r.overb", "r.buf 2", "w.state", "w.skip 3",
    "w.string 9fb0dfadabf24d99ff2e0ffedef2e71ca5b4d210be7aa73e5c6632de6bf7d362b64d8f7cf0a203da5e43b81e05ac68869f8ce453a8d5007e824201bd7f24424c271cf98b2e69 4", "w.rawstring cdf87d842a", "w.reserve 0", "w.setsize 132",
    "w.long -1863868081", "w.short 6451", "w.state", "w.setbytes 27 59",
    "w.short 14900", "w.tobuf 0", "w.setbyte 85 186", "w.bool 2",
    "w.long -485515224", "w.bool 2", "w.reserve 16", "w.length 127 4",
    "w.long -1683738173", "w.setsize 153", "w.long -160144398", "w.reset",
    "w.bool 1", "w.state", "w.tobuf 1", "w.new 2 1",
    "w.bytes 10", "w.reset", "w.long -1604242052", "w.tobuf 1",
    "w.setsize 0", "w.buffer - 0", "w.reserve 0", "w.state",
    "w.tobuf 0", "w.new 3 1", "w.bytes e7", "w.length 127 2",
    "w.string b8 4", "w.tobuf 0", "r.overb", "r.setpos 4",
    "w.setbyte 3 57", "w.byte 225", "w.rawstring -", "w.bool 2",
    "w.state", "w.setbytes 6 ff37", "w.reserve 33", "w.state",
    "w.tobuf 0", "w.new 2 1", "w.string 58f76bb973 4", "w.wstring 7a.e9.dfff 2",
    "w.wstring - 4", "w.buffer ed 1", "w.setshort 19 -19665", "w.reset",
    "w.bool 2", "w.state", "w.tobuf 0", "w.new 4 0",
    "w.buffer - 4", "w.rawstring -", "w.bool 0", "w.tobuf 0",
    "w.setshort 0 -21324", "w.byte 47", "w.state", "w.tobuf 0",
    "w.new 1 1", "w.tobuf 0", "r.overb", "r.bool",
    "r.rawstring 9", "r.byte", "r.setpos 4", "r.long",
    "r.buf 1", "r.rlen 0", "w.string 40 2", "w.byte 22",
    "w.chunk 700 64", "w.rawstring 3c", "w.byte 1", "w.short 26455",
    "w.bytes -", "w.setbytes 3 -", "w.setshort 2 -22640", "w.long 861196280",
    "w.byte 99", "w.short -17827", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.buffer 53fa57d133d001dcd65c0b20d5ef07f2f515729975a000be45f2bfa85d6ceb689939403e40a67fbc 1", "w.tobuf 1", "w.long -1152999544",
    "w.skip 1", "w.skip 3", "w.length 127 4", "w.tobuf 0",
    "r.overb", "r.rawwstring 1", "r.rawwstring 3", "r.byte",
    "r.rawstring 3", "r.wstring 1", "r.long", "r.setpos 4",
    "w.setbyte 0 67", "w.bytes d1a7", "w.length 1 4", "w.state",
    "w.buffer 51 1", "w.reserve 33", "w.state", "w.tobuf 0",
    "w.new 4 0", "w.setsize 0", "w.length 438472150 0", "w.state",
    "w.chunk 700 40", "w.skip 100", "w.chunk 33 1000", "w.string b4b073f091e143d7cb5354c03c6f79c7a35d863e89858a7fc5aef90324152fc0635742dbbd38e73a7e9d4f4eeed33cf39944731e83cee72d6a36e374eb90a170e0e163f431d14f8a29b1055f6df3cd665ccc548b47e3d0f705eb4e6d0704708adc589844529eb053391db99930c442b73a8879e730396ba3ed5a79d6880f47c9e37d 2",
    "w.long 1677168573", "w.bytes b8", "w.reset", "w.bool 0",
    "w.state", "w.wstring 20ac.263a.20ac 4", "w.skip 3", "w.setbyte 4 23",
    "w.setshort 10 25812", "w.setsize 13", "w.byte 87", "w.setbytes 6 55",
    "w.short 4971", "w.wstring e9 3", "w.setbytes 3 -", "w.long 2050584177",
    "w.state", "w.tobuf 0", "w.new 1 1", "w.skip 3",
    "w.wstring 0 2", "w.reserve 33", "w.bytes ee73", "w.long -27926296",
    "w.short 2876", "w.setbytes 7 706b", "w.chunk 0 10", "w.setshort 7 -11787",
    "w.wstring - 2", "w.state", "w.tobuf 1", "w.new 4 1",
    "w.short 25355", "w.chunk 700 0", "w.setsize 52", "w.tobuf 1",
    "w.tobuf 0", "w.tobuf 0", "w.bool 2", "w.state",
    "w.length 128 2", "w.chunk 10 64", "w.long -594013805", "w.chunk 33 1000",
    "w.chunk 33 64", "w.setbyte 3 137", "w.state", "w.rawstring -",
    "w.rawstring -", "w.wstring 41 2", "w.buffer edc29618acc1e49bb7 2", "w.short 1847",
    "w.skip 0", "w.bytes 6bed", "w.string - 2", "w.chunk 33 1000",
    "w.setsize 27", "w.state", "w.tobuf 0", "w.new 1 0",
    "w.string 15 4", "w.reserve 0", "w.skip 1", "w.setshort 2 -28530",
    "w.rawstring -", "w.state", "w.string 9285b3ae8219dfb802c876a6fd2598b4d6eeba20b01c55da568036a704054fed2c 1", "w.wstring 0.3682.41.ffff.36d6.dbff.0.e9.7a.0.263a.7a18.20ac.0.e9.263a.7a.0.dfff.7a 4",
    "w.setbyte 39 38", "w.chunk 0 0", "w.reset", "w.state",
    "w.bytes 34", "w.string ba5f53d4c9 4", "w.buffer c441f884115b66f3ec 4", "w.wstring 263a.0.7a.41.7a.ffff.ffff.d800.b3b9.9134.e9.ffff.7a.20ac.41.e9.0.41.ffff.dbff 0",
    "w.setbyte 16 1", "w.length 127 0", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.state", "w.setsize 0", "w.bool 0",
    "w.bool 1", "w.state", "w.reserve 33", "w.short -27159",
    "w.setsize 54", "w.reset", "w.bytes 0c4a", "w.length 255 0",
    "w.length 65536 4", "w.state", "w.tobuf 1", "w.new 0 1",
    "w.long -1634347254", "w.setshort 0 -3948", "w.short -4516", "w.wstring 948d 0",
    "w.tobuf 0", "w.setbytes 2 ed075a", "w.bool 0", "w.long -831305696",
    "w.reset", "w.bool 0", "w.long -651156212", "w.length 0 4",
    "w.setbytes 5 -", "w.byte 244", "w.skip 0", "w.reset",
    "w.chunk 0 40", "w.skip 1", "w.short -17389", "w.setsize 3",
    "w.buffer 0d8d7ed0e4b6f5820e 4", "w.state", "w.length 3098688235 2", "w.bool 0",
    "w.setshort 4 10848", "w.reset", "w.state", "w.tobuf 1",
    "w.new 4 0", "w.bytes aa7ba3c176d2ff", "w.short 17887", "w.state",
    "w.reserve 0", "w.rawstring 9b", "w.long 524344563", "w.rawstring -",
    "w.byte 206", "w.length 128 2", "w.state", "w.rawstring 847b5ce122eb638173c64c886a8135142a3c351d3dabc40184472fa36ecd98669f008927657a0a9a",
    "w.buffer fbc61158f129765d350edf89aece5f5a8b3f317714972b8f85483b99bc375be9beb33f265cc0950c 2", "w.state", "w.tobuf 1", "w.new 1 0",
    "w.length 65535 4", "w.bytes fc45075d927f9f", "w.setshort 6 -27882", "w.short 4401",
    "w.chunk 65 1000", "w.skip 3", "w.string 4c0b065d81 3", "w.setsize 71",
    "w.string 10 0", "w.bytes 9bc5", "w.rawstring d7", "w.byte 255",
    "w.reset", "w.reserve 33", "w.rawstring aba0febb5a", "w.reserve 0",
    "w.long 358458491", "w.long 198439024", "w.bool 1", "w.string 54030e5e932eedf4841cdec308ac60cdfb7da3e497d2d5e4645cafa0ee95c9c23f 3",
    "w.state", "w.tobuf 0", "w.new 2 0", "w.tobuf 0",
    "r.overb", "r.setpos 0", "r.rewind", "r.rawstring 3",
    "r.setpos 2", "r.rawstring 9", "r.long", "w.short -6868",
    "w.short 2711", "w.rawstring df592eaf7a4337a1feb3063e3d6a8aeb642e470f563542baddd7b4c80c3e5fc77d590b1e4beceb00", "w.byte 147", "w.bytes 94a17444745a7d",
    "w.bool 2", "w.reset", "w.setsize 0", "w.rawstring e6",
    "w.buffer - 4", "w.length 256 2", "w.byte 180", "w.long -364458552",
    "w.state", "w.tobuf 0", "w.new 4 0", "w.setsize 0",
    "w.byte 144", "w.state", "w.tobuf 1", "r.overb",
    "r.rawstring 9", "w.long -1098331048", "w.long -769090773", "w.reset",
    "w.state", "w.tobuf 0", "w.new 1 0", "w.state",
    "w.reset", "w.setsize 0", "w.skip 0", "w.state",
    "w.tobuf 1", "w.new 4 1", "w.reset", "w.tobuf 1",
    "r.overb", "r.avail", "r.byte", "r.buf 1",
    "r.rawwstring 1", "r.rawstring 0", "r.rawwstring 3", "r.byte",
    "w.length 2147483648 4", "w.setbyte 0 43", "w.state", "w.tobuf 0",
    "w.new 4 1", "w.skip 1", "w.long 1740443450", "w.bytes 7efc5386438d5f",
    "w.byte 175", "w.setshort 2 32312", "w.buffer 6754a0d39faa80642732ff2ae77a8eea3b016ba8a6fc816a4ea6e455517dab2a8e7aba7abf9c287c 2", "w.length 1642412049 0",
    "w.setshort 34 18970", "w.chunk 700 1000", "w.bool 1", "w.state",
    "w.tobuf 1", "w.new 1 1", "w.short 5468", "w.wstring - 0",
    "w.bool 0", "w.bool 1", "w.tobuf 1", "r.overb",
    "r.rewind", "r.pos", "r.rlen 1", "r.short",
    "r.short", "w.state", "w.tobuf 1", "w.new 3 1",
    "w.long 1848888074", "w.string e5 2", "w.setbyte 3 171", "w.setbyte 3 126",
    "w.wstring - 0", "w.byte 220", "w.state", "w.tobuf 1",
    "w.new 4 1", "w.length 65536 0", "w.length 4294967295 4", "w.bool 2",
    "w.long -1960764682", "w.bytes -", "w.chunk 0 0", "w.state",
    "w.state", "w.short 16535", "w.bool 0", "w.setsize 11",
    "w.skip 3", "w.byte 113", "w.buffer 2e 4", "w.short 27704",
    "w.reset", "w.buffer 9c047628c3cf0e6fc2 2", "w.reset", "w.chunk 65 40",
    "w.wstring 20ac.ffff.0.d800.0.f8e8.840d.7a.41.e9.20ac.ffff.20ac.263a.e9.4c12.7a.41.41.20ac 4", "w.buffer -- 1", "w.byte 60", "w.setbytes 32 -",
    "w.setsize 46", "w.long 1477201019", "w.wstring 4cc8 4", "w.rawstring 0281c6418e58f23ca37932c95a2a35ab531d4f1a875d8b1be3d58513f08fd15ff2d580eaf3d3ef5d",
    "w.short -27466", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.byte 199", "w.state", "w.chunk 10 10", "w.rawstring -",
    "w.bool 0", "w.length 4294967295 1", "w.long -771425177", "w.wstring - 4",
    "w.setsize 0", "w.buffer - 4", "w.tobuf 0", "w.skip 3",
    "w.reset", "w.long 85651276", "w.short 29504", "w.byte 157",
    "w.skip 1", "w.setshort 1 1407", "w.buffer -- 1", "w.long 1621329175",
    "w.long -366563251", "w.bool 2", "w.skip 100", "w.state",
    "w.tobuf 0", "w.new 3 1", "w.rawstring e006a36ef0e237690de96cae91a522f6a51a7e3d2531a4926c1507adba1af1013891418f763c0916", "w.state",
    "w.rawstring fd", "w.setbyte 23 183", "w.long -1293814826", "w.setbyte 26 198",
    "w.length 127 4", "w.wstring ffff.0.0 4", "w.state", "w.wstring ffff 2",
    "w.bytes e25e", "w.setsize 64", "w.setbyte 38 6", "w.long -2033093477",
    "w.short -7434", "w.buffer 1b38f56b9c342b2ef2760b030ffc0ad57e208eceb3b031bf53a42e4d7385f0003c29b2503ab9e538 4", "w.rawstring -", "w.skip 1000",
    "w.reserve 500", "w.state", "w.chunk 0 40", "w.reserve 32",
    "w.reserve 0", "w.setbytes 389 02baf3", "w.long -162541602", "w.setsize 1168",
    "w.state", "w.tobuf 1", "w.new 2 1", "w.buffer ab3ed5f7eec71f0621a22618dfd1893ea94dcbada560c1d4e12128c265cfd36a7d63f6f34d2510c7 2",
    "w.length 128 0", "w.buffer b1faea8e2d6f98ea82 4", "w.setsize 1", "w.tobuf 0",
    "r.overb", "r.rawstring 9", "r.pos", "w.string f342789d79ba8a99a8395b1beb51df83935db380344f8691a4d5cdba5685f4403d 1",
    "w.tobuf 0", "r.overb", "r.bool", "r.short",
    "r.bool", "r.string 1", "r.string 2", "w.buffer - 3",
    "w.short 20840", "w.setbyte 25 109", "w.long -640161634", "w.wstring dd41 3",
    "w.state", "w.setbytes 3 -", "w.chunk 33 0", "w.rawstring 0c",
    "w.setbytes 37 0be6ad", "w.string 38 2", "w.chunk 0 40", "w.state",
    "w.reserve 500", "w.reset", "w.length 0 4", "w.reset",
    "w.state", "w.tobuf 0", "w.new 2 1", "w.long 1143289190",
    "w.string 09ea885f11eed93cf7aff0fec588b7667538e3ecf5bcb8348ad422ef7ced01b9cad0923fab451ac1db22f90c30f189174c92a5bdcb912b816b2434fa696c4355a2ed4080355f04b9a8e270c128cdbb9d1bdb3df8bf5958409fce1d407f2a9694384f5ff0e7073ab02a6657c76447750d96f8a2f9c3d96a9b457666ccc0c534e9b1ef 1", "w.setshort 1 -6636", "w.bool 0", "w.short 14090",
    "w.chunk 65 10", "w.setsize 188", "w.state", "w.tobuf 1",
    "w.new 1 0", "w.chunk 65 0", "w.string 14b0c369995477932453c94c1d2a286774c4b956f8e6f0916b28b1340fbaaccc6d618990226e7f0bc3f35bd1197876e590f2ff789287d4f6029404c97f0c079811d388b1dfa4 4", "w.reserve 0",
    "w.byte 197", "w.setsize 75", "w.string - 2", "w.state",
    "w.tobuf 0", "w.new 4 1", "w.tobuf 1", "r.overb",
    "r.bytes 0", "w.reserve 32", "w.short 3230", "w.bytes -",
    "w.state", "w.tobuf 0", "w.new 2 1", "w.bool 2",
    "w.buffer ee 2", "w.setsize 1", "w.chunk 10 40", "w.reset",
    "w.state", "w.byte 102", "w.rawstring 2446c286422478ab8797f63905716bf2d8ad58053191a406fc2832b9d7e0e33896771a5f72fa890b", "w.state",
    "w.short -10799", "w.setbyte 21 254", "w.state", "w.tobuf 0",
    "w.new 4 1", "w.reserve 500", "w.length 65536 1", "w.reserve 500",
    "w.bool 2", "w.short -3850", "w.chunk 0 1000", "w.setshort 0 701",
    "w.chunk 64 1000", "w.byte 194", "w.setbytes 1 bb", "w.rawstring 3dc0ac7763ea26a7017bd3115dd46a002dcfb39c36e85a93ced6091c40f28d48ae391059a2885f34",
    "w.chunk 700 0", "w.buffer 34f05ccd4e9e412d22 1", "w.state", "w.skip 0",
    "w.reset", "w.state", "w.tobuf 0", "w.new 0 0",
    "w.wstring - 1", "w.bool 1", "w.rawstring ab", "w.wstring 41.263a.41 4",
    "w.state", "w.byte 45", "w.skip 0", "w.long -98125728",
    "w.bytes 348ecede1bd735", "w.rawstring c8", "w.short -32274", "w.skip 1000",
    "w.bytes 1cad", "w.byte 121", "w.setbyte 977 153", "w.skip 0",
    "w.string 77838a28ddb0be209fefa76f326428533a4ff1078689d2fa7638f5c0610e971a 2", "w.setsize 0", "w.setsize 0", "w.setsize 50",
    "w.tobuf 1", "w.bytes 9c78", "w.wstring 4a3a.d800.7a.ffff.7a.20ac.41.20ac.7a.20ac.56d6.0.e9.ffff.263a.f8bb.41.e9.41.41 4", "w.short 6827",
    "w.state", "w.tobuf 1", "w.new 4 0", "w.byte 9",
    "w.skip 1000", "w.reserve 32", "w.skip 1000", "w.string 4361575fd040f9c5d88723679ce5059842d6130dc9a82693c495f84079bd6eb4 2",
    "w.long 562326285", "w.wstring 263a 3", "w.skip 3", "w.rawstring 0e",
    "w.setshort 23 2444", "w.tobuf 0", "r.overb", "r.buf 2",
    "r.byte", "r.byte", "r.string 2", "r.avail",
    "r.pos", "w.state", "w.tobuf 0", "w.new 4 0",
    "w.skip 1", "w.tobuf 1", "w.setsize 50", "w.length 256 0",
    "w.bool 1", "w.byte 201", "w.reset", "w.short 18772",
    "w.bytes -", "w.wstring 41.263a.e9.20ac.dfff.263a.7a.41.dbff.ffff.41.41.e9.b9c.20ac.e9.7a.7a.20ac.263a 4", "w.string - 2", "w.reset",
    "w.setsize 0", "w.reserve 16", "w.byte 15", "w.chunk 10 10",
    "w.skip 0", "w.short 30502", "w.bool 1", "w.setbytes 0 f477",
    "w.length 256 4", "w.state", "w.tobuf 0", "w.new 1 0",
    "w.skip 3", "w.length 4294967295 2", "w.byte 192", "w.short -22514",
    "w.byte 211", "w.skip 1", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.skip 1000", "w.short 820", "w.string de 4",
    "w.buffer ad 4", "w.wstring 41.20ac.0 4", "w.bytes 3cda23fc910417", "w.skip 100",
    "w.long 1932386469", "w.byte 18", "w.reserve 500", "w.reserve 500",
    "w.short -13745", "w.reserve 32", "w.state", "w.setshort 748 12182",
    "w.buffer ed75511e6032e73cd4 0", "w.state", "w.length 127 4", "w.tobuf 0",
    "r.overb", "r.setpos 0", "r.buf 2", "w.wstring - 3",
    "w.reset", "w.wstring - 2", "w.long 923156505", "w.rawstring 5a",
    "w.state", "w.tobuf 1", "w.new 2 1", "w.tobuf 0",
    "w.bool 1", "w.bool 1", "w.long 667263966", "w.long -1119411275",
    "w.buffer f4 4", "w.reserve 16", "w.buffer f2 2", "w.length 128 4",
    "w.setsize 0", "w.buffer 599053122e6ebe56ecd718d8aba969d305c1a6bb45f4e3f2c1135695a4579b6cbe3e0a0120590c9b 4", "w.short 30491", "w.setshort 34 3114",
    "w.rawstring b8387a1e19808b2b1127d0a52773cee5621882b9fb3908ae43d4cc2838f234a0c4b61849d4df320f", "w.string 5bd21f2fee2aa3b27e1a2a5105a4ce49369ca175805e1e7e774180eca33b53e8fe110d082d486d24bcf84f6a93464449119ee78d8be145657fbcdbdeab77b397e9d0f05806b9 2", "w.setshort 7 -29437", "w.byte 174",
    "w.reset", "w.buffer 16ec58f260c20f831919ed19a6dfd4023a03184881c3e1c751858e9628318405d1e2b855297f7e98 3", "w.rawstring d775e02018a0b8c49a74a43d3f79361e318ba2c2b551a3b660b9e1461cafb0b5bee2ca713ade06b3", "w.length 2147483648 2",
    "w.setshort 32 -16464", "w.chunk 0 40", "w.long -1605579803", "w.state",
    "w.tobuf 0", "w.new 0 0", "w.skip 0", "w.short 21797",
    "w.rawstring a93058e815382a1d3f484704ae415791afa5317f462e01d51a603ade4392de77eb3d837380815b6b", "w.short -20003", "w.reset", "w.bool 0",
    "w.short 11814", "w.short 1565", "w.wstring dbff.0.20ac 2", "w.string 716e76e08e78b0e8f224dbe7a639410a4cdc6b09723a14b4178ccb6b27b29261657db32238c9bc1aeb3f9d06f4a9dfcc745ea73f009b16ed004389e21a0a65f7d3dc6b0ae540 3",
    "w.reserve 0", "w.string 88 1", "w.reserve 32", "w.tobuf 1",
    "w.wstring dbff.dfff.20ac.0.20ac.e9.d800.d800.d838.dbff.7a.7a.20ac.dfff.0.263a.ffff.20ac.41.41 3", "w.bool 1", "w.reserve 33", "w.rawstring 79411ba2b2",
    "w.length 822174443 2", "w.byte 39", "w.wstring ffff.263a.ffff.839b.263a.41.0.ffff.7a.20ac.e9.e9.7a.ffff.41.e9.20ac.263a.ffff.c34 4", "w.wstring 3afb 4",
    "w.short 8418", "w.length 1 2", "w.wstring 41 1", "w.setshort 32 11173",
    "w.length 65536 2", "w.string - 1", "w.state", "w.tobuf 1",
    "w.new 3 1", "w.buffer 2e 0", "w.buffer 19 0", "w.long 951634343",
    "w.skip 1", "w.wstring ffff.20ac.263a 2", "w.wstring dfff.e9.0.ffff.e9.ffff.ffff.f390.7a.e9.e5e3.0.e9.e9.20ac.ffff.7a.263a.0.20ac 4", "w.string 4a028957da284683e908c063b03d2e7734985b140245a2b10bc8f52244ed9ca20f 2",
    "w.short -18148", "w.reset", "w.tobuf 0", "w.string 835a3d63bfb84a835bf6d6dc00c59ff5d23049b2c3a5fb321df6dc3fc1fe0edfae 4",
    "w.setbytes 19 8b", "w.setbyte 15 124", "w.byte 91", "w.buffer 5314542338fd437502 3",
    "w.setshort 10 -1609", "w.bool 2", "w.setbytes 13 fd", "w.setsize 47",
    "w.length 1 0", "w.tobuf 0", "w.setbytes 5 306d", "w.setshort 11 25714",
    "w.long -1641177027", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.chunk 700 40", "w.state", "w.wstring 7a.0.ffff 2", "w.skip 100",
    "w.length 0 1", "w.state", "w.tobuf 0", "w.new 2 1",
    "w.length 2147483648 1", "w.setsize 0", "w.byte 236", "w.chunk 33 10",
    "w.chunk 10 64", "w.rawstring bc74d1b18ac7093c2ee28f4c27b7cb9eb7e3aa822c385e48b5c2f66e78a64d386c9485cbab3c0a16", "w.reset", "w.rawstring -",
    "w.string 67dc6288eddfcf69a9e37869ff57beb7a7535614f389001cbc331a0f13d790e9 4", "w.buffer -- 3", "w.long -1491012234", "w.long 1543615484",
    "w.bool 1", "w.rawstring 151ab28cea5722403f5b1ea3346cb3ab9aaff28103bb3d46e165cb1f9f34514919ce9211c470c3fb", "w.setbyte 12 255", "w.byte 106",
    "w.tobuf 0", "w.setbytes 56 37", "w.reserve 0", "w.setbytes 13 bb8615",
    "w.reserve 0", "w.chunk 10 40", "w.state", "w.tobuf 1",
    "w.new 1 1", "w.short -5741", "w.string e254f9f0a4dfdbfd1ae6ed84f26dca6a82926488fd0e984126e47793a49d0e7e7f8679b3d1aa36ac2cdd7f31b5f739fbe7fc58ecdf2d039ce898d03dfe603ad4c9affdc3816a9fe0ee91bf1f0ef261c74025cbc8fcab32e2d8d97574430446f7c30696f865974352d89ea6526dd65e729302edee84a87a7c802628be4723af020b3c 4", "w.byte 132",
    "w.state", "w.setbytes 132 -", "w.setsize 136", "w.byte 131",
    "w.tobuf 1", "r.overb", "r.rewind", "r.byte",
    "r.bool", "r.buf 4", "r.bool", "r.rlen 1",
    "r.buf 2", "w.skip 0", "w.reserve 500", "w.byte 185",
    "w.setsize 1", "w.state", "w.tobuf 1", "w.new 2 1",
    "w.reserve 0", "w.bool 2", "w.buffer ac 2", "w.rawstring 9b7cbe61e0",
    "w.buffer c6 2", "w.wstring dc00 3", "w.bytes 18", "w.state",
    "w.tobuf 1", "w.new 1 0", "w.reserve 500", "w.rawstring -",
    "w.bool 2", "w.string c89217c91bdba4219049670d64b960dd6450f2d59598c2e21d7bbdc4f219c5848601636ecde2000ba6e604a525fc98315d64a279722c63dbd41299a9ecd31f7ae6be57c573e98a80d63e93db4f003034ee4c4f31187f58f7b5dca74044618428a989730b447c5afd346e489c301ca9a54c8af5f8ce52db477ff6f62413eec1307676 2", "w.string 4f 3", "w.tobuf 0",
    "w.setbyte 120 53", "w.chunk 64 64", "w.long 967152417", "w.reset",
    "w.buffer -- 2", "w.bool 2", "w.state", "w.tobuf 1",
    "w.new 0 0", "w.byte 52", "w.bool 0", "w.bool 0",
    "w.short -13642", "w.short -16920", "w.reserve 0", "w.chunk 33 0",
    "w.setsize 1", "w.skip 100", "w.reserve 33", "w.tobuf 1",
    "w.chunk 65 64", "w.bool 0", "w.byte 168", "w.buffer b1 2",
    "w.tobuf 1", "w.bytes 12627c6389b74450de9affa117dec6652bc29f20606003b2ad7396fb4a4856df5f12a8d9c881cca9ee46505d9f4db6c294175f30edd789d91ac759d857bc4c94", "w.reserve 500", "w.setsize 114",
    "w.length 255 2", "w.reset", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.skip 1", "w.long 18603382", "w.byte 253",
    "w.reserve 32", "w.setshort 1 -3085", "w.state", "w.reserve 32",
    "w.reserve 16", "w.skip 1000", "w.short 5608", "w.wstring 20ac.263a.f430.2013.dbff.41.f909.dbff.7a.e9.dfff.7a.ffff.41.dc00.e9.20ac.e9.20ac.41 0",
    "w.setsize 1047", "w.short 2475", "w.reserve 32", "w.bool 2",
    "w.rawstring 360882f3a714438dfaa20b4ef0359ab9aee605ed8af03fa606fe88000126f15ea1313d718948c89e", "w.setshort 625 5693", "w.setbytes 446 05", "w.rawstring -",
    "w.length 255 0", "w.bool 2", "w.buffer 76 0", "w.setshort 800 25776",
    "w.length 0 1", "w.reset", "w.wstring 263a 4", "w.state",
    "w.tobuf 1", "w.new 2 1", "w.state", "w.rawstring -",
    "w.long -329726372", "w.long -1069921411", "w.setshort 4 -29801", "w.wstring e9.d800.ffff.20ac.0.0.20ac.20ac.e9.d800.dfff.0.41.41.263a.ffff.dbff.ffff.41.7a 3",
    "w.chunk 10 10", "w.setsize 0", "w.tobuf 1", "w.length 127 2",
    "w.reserve 32", "w.string 391eb919249839d492623669aecfd5a49f43cccc742e9bf943c166bf63ec28c46c2182987e08b160b4b6c9ddb3ca496511cc1f20ccf5de7e44908574e2e08d74150edd8d609146e02ab1746ab7f10b4d04afcb463319a2a4e6b68f351c783d472c42cd4b312f3f8e288ffeca11ed1db214266fcaa3b761d73c154e002f2f6d72e762 0", "w.tobuf 0", "w.setshort 111 13372",
    "w.buffer -- 4", "w.state", "w.tobuf 0", "w.new 2 0",
    "w.byte 204", "w.length 2142171121 4", "w.short -5203", "w.byte 249",
    "w.string 851eed158a6d991ba38f48d694ec0914bfc4648f72d26476d55f0546691520d63f94e0524cea2bd56585f91182401a2b78b03d48e6e13ce8f7b7b8a64c1da53776203bf2aec1c3d58a42301e0c68117688fac9e043d5d893d55c898f2d24dbcf223f71dc7589429370badad1eacc7cacbc299a36a848bcfd480a4497f669e32947a7 3", "w.skip 100", "w.setbyte 202 135", "w.chunk 0 10",
    "w.chunk 700 40", "w.reset", "w.reserve 500", "w.chunk 0 64",
    "w.rawstring -", "w.buffer - 0", "w.length 65535 3", "w.byte 168",
    "w.bytes 28413c562c1d265be20134e99e1a0f005ad4436d731f4f0a375e632dafd5fe2ea8c00026d53b107e40c57813b53d544167673e79d4f5c8a644d2779b65cc59cd", "w.state", "w.tobuf 1", "w.new 4 1",
    "w.state", "w.buffer -- 2", "w.setsize 0", "w.byte 204",
    "w.buffer 45f2ed50d2129fcb4526d1270d0efd98f04f448d4cec710bcc23acb9598208226d2044d3b8947933 3", "w.rawstring deaa090c88030a42dd763269efa9a73b50a03e76c81c4ae3908187c5c48e82fb08353a34e53945f8", "w.setshort 61 -18635", "w.buffer - 4",
    "w.long -557327775", "w.length 0 4", "w.byte 74", "w.string 82fd9b260d49d81fbed65709d3bdf7520872dd5f435c711fb7d965890378c4d46ccdf3fefdf040fed830450cf4dff39578c86c381a132ac2bc22609f570b640711d649a4538464ec50c15844ed72796ff32a912f02193837b3244e7caf8b4a77f9b8a705dfa75dd869061646f0095a536580afe62e09f989ce624907db3337f3ff56 3",
    "w.reset", "w.state", "w.tobuf 0", "w.new 2 0",
    "w.length 65535 2", "w.chunk 10 40", "w.short -6859", "w.skip 100",
    "w.setshort 24 13976", "w.bytes -", "w.setsize 104", "w.reserve 33",
    "w.state", "w.short 6352", "w.long 400265934", "w.setbytes 82 ad",
    "w.buffer bf 2", "w.skip 1000", "w.long 1047177315", "w.chunk 65 40",
    "w.state", "w.tobuf 0", "w.new 2 0", "w.bytes 3d6d",
    "w.string 5a 2", "w.setshort 0 25672", "w.tobuf 1", "w.chunk 10 0",
    "w.setsize 50", "w.bytes a8", "w.tobuf 0", "r.overb",
    "r.wstring 4", "r.setpos 1", "w.chunk 700 64", "w.wstring 0 2",
    "w.setbyte 2 63", "w.rawstring -", "w.reset", "w.reset",
    "w.byte 40", "w.reserve 32", "w.wstring ffff.20ac.e9.20ac.263a.263a.9f7d.0.41.ffff.0.d800.263a.7a.7a.41.41.e9.ffff.6a4e 2", "w.chunk 700 0",
    "w.state", "w.string c2852d5ae3a78fa734590d5562c625323e36bd3b228ab5afb9799b85288c9bd8 3", "w.string 195b0011ff8a6ee4f7d028019be18d1edc4cc6a67a5a204fe7fcebbde34166eed6c0e8af73a3b15a8acfea7cf582f9395496b5505ec9e0e5b1c9cfbfb5cb7e4681bb2c61ae4e 0", "w.buffer 4d38c468d5928cd7b3 4",
    "w.rawstring f1", "w.setsize 1", "w.state", "w.tobuf 0",
    "w.new 2 0", "w.wstring 0 0", "w.skip 3", "w.tobuf 1",
    "r.overb", "r.avail", "r.setpos 2", "r.wstring 1",
    "r.short", "r.wstring 2", "w.tobuf 1", "r.overb",
    "r.rawstring 0", "r.bool", "r.bytes 1", "r.byte",
    "w.length 127 1", "w.chunk 64 40", "w.setsize 0", "w.skip 1",
    "w.bool 2", "w.short -32029", "w.byte 102", "w.bool 0",
    "w.bytes 88", "w.state", "w.reset", "w.byte 238",
    "w.bool 2", "w.rawstring 5e", "w.state", "w.state",
    "w.skip 0", "w.setsize 0", "w.short 22900", "w.rawstring 4e7c681fcf",
    "w.bool 1", "w.byte 235", "w.state", "w.tobuf 0",
    "w.new 0 0", "w.buffer -- 1", "w.bool 0", "w.setsize 1",
    "w.byte 26", "w.reset", "w.short 30151", "w.buffer - 0",
    "w.wstring 0.e9.20ac.20ac.ffff.ffff.2b9f.41.7a.ffff.0.e9.11a6.ffff.e9.0.beaf.263a.7a.263a 1", "w.byte 61", "w.short -1057", "w.byte 44",
    "w.rawstring 95b45b14e673ae617baae66222b6f6faee7f2f4a26099a58e9120bf34327bede8bac95a83e8e2207", "w.skip 0", "w.setbyte 57 217", "w.byte 65",
    "w.byte 7", "w.reset", "w.skip 0", "w.state",
    "w.tobuf 0", "w.new 3 0", "w.reserve 0", "w.bytes -",
    "w.setsize 50", "w.bytes c0", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.wstring - 2", "w.short -30249", "w.long -927314344",
    "w.wstring - 2", "w.setbyte 4 189", "w.setbyte 0 23", "w.byte 17",
    "w.rawstring 41", "w.tobuf 0", "r.overb", "r.avail",
    "r.long", "r.rlen 3", "r.short", "r.string 1",
    "r.string 2", "r.rlen 3", "w.chunk 64 0", "w.long -828413472",
    "w.setbytes 7 -", "w.setshort 6 5498", "w.setshort 11 25842", "w.state",
    "w.tobuf 0", "w.new 4 1", "w.skip 3", "w.setsize 2",
    "w.string 8e3e41f5561e9bff7805b4f5bc0a273d16c79cc6072de8fc8634c802fe6adac3950302a2d2425de72e7909ef9b85411dea109c9e8d90b9a5ee7dd8fc5b267c742514f4a60979 4", "w.reserve 16", "w.buffer - 2", "w.setshort 71 23863",
    "w.byte 42", "w.chunk 33 40", "w.buffer - 4", "w.wstring - 4",
    "w.tobuf 0", "w.state", "w.state", "w.state",
    "w.rawstring f0", "w.reserve 16", "w.bool 0", "w.tobuf 1",
    "r.overb", "r.string 4", "r.byte", "w.state",
    "w.tobuf 0", "w.new 1 1", "w.long -1020431815", "w.chunk 0 64",
    "w.setbytes 0 4b8a", "w.byte 220", "w.reserve 500", "w.reset",
    "w.chunk 0 0", "w.setsize 1", "w.bool 0", "w.state",
    "w.tobuf 1", "w.new 2 0", "w.state", "w.bool 0",
    "w.short 21203", "w.bool 1", "w.long 1834712905", "w.rawstring 7ca14dc72d787c09a7f2c73eec72e4f9b7ff09de8f4e0261397367c928c5e16b28deba25b9c903f0",
    "w.wstring e9.ffff.41.e9.c541.0.41.e9.e9.0.e9.41.41.1f0f.7a.41.d845.7a.d800.dbff 4", "w.reset", "w.long 1434701472", "w.byte 61",
    "w.setbytes 0 725002", "w.reserve 0", "w.wstring d1c4.0.41 4", "w.length 1 4",
    "w.setbytes 9 -", "w.setsize 69", "w.reset", "w.setsize 0",
    "w.length 4294967295 4", "w.tobuf 0", "w.bool 1", "w.setsize 5",
    "w.state", "w.tobuf 1", "w.new 3 1", "w.bytes 61a3a19ae09ae515264d9ceb725330d7fc52faf3b0aa1c4b870460bb929c2ae85323d90e07516930d7efd0550204c442ebf2bcd89f358d2e8af3592ce3c11757",
    "w.byte 217", "w.length 0 2", "w.chunk 10 10", "w.reserve 32",
    "w.state", "w.chunk 700 10", "w.reset", "w.state",
    "w.tobuf 1", "w.new 4 1", "w.setsize 0", "w.rawstring 284ed84dbf7be1f66227f51d6ca038246aa28bdcd64dafdd471ea4639faf7c03f2e6ad3e185c7b6a",
    "w.byte 64", "w.bool 1", "w.chunk 33 10", "w.chunk 33 1000",
    "w.short -2150", "w.bytes -", "w.tobuf 1", "r.overb",
    "r.rawstring 0", "r.rawstring 0", "r.pos", "r.rewind",
    "r.short", "r.bool", "r.bool", "w.chunk 65 1000",
    "w.short -32068", "w.short -18242", "w.chunk 65 40", "w.reserve 16",
    "w.setbyte 0 196", "w.string 0b993ed8f0 3", "w.wstring 20ac.d90a.e9 1", "w.setshort 3 -28752",
    "w.setbyte 4 249", "w.reset", "w.buffer 366df094fd83610b81c99a67def8a05c0c5bb5f9f229bbfe0ec4993e4a24fef14bd50300b7acb2a7 4", "w.bool 2",
    "w.bytes -", "w.state", "w.tobuf 0", "w.new 1 1",
    "w.long -1837476838", "w.rawstring d8", "w.long -1693808375", "w.setshort 1 -11586",
    "w.rawstring -", "w.short 15249", "w.string cbf194c42963e9fa06b90685a3c02de3745f2f065ea3ea04a561684a6e35c261d802dc9a61249bae2f17b7bdefc6745b67fc5cded9a457f58c21df658748860d629dab429eee46d91acc0621ad90d28792bcdf1af7fb62eea8bd4511559899dee1cbd8212526f11d03c802bd7fc9569f9ec7ece8ca65ab7c62318628a3c1c09b7648 0", "w.string 0bf1dd4e53da2d23ea99fd008b05283c651fcba9013d9d05ce30133d43cb88 3",
    "w.byte 121", "w.skip 0", "w.bool 2", "w.setbytes 73 1349",
    "w.setshort 110 29382", "w.byte 213", "w.setshort 125 5166", "w.byte 120",
    "w.wstring 1a86.f909.263a 2", "w.skip 0", "w.skip 1", "w.long 7618923",
    "w.rawstring -", "w.long -379463352", "w.state", "w.tobuf 1",
    "w.new 2 0", "w.buffer -- 2", "w.buffer ee 0", "w.long -2023602220",
    "w.rawstring a5", "w.string 55e556500de284b7238627e9d55d24436d3cb3027dd92d2a2b02f5dee4ce47987c4899ddd83c7a92a219542b0abb26095f2a7c529409e7244786d2d22979e28dd30e7b515ce9 2", "w.setshort 15 28137", "w.skip 3",
    "w.setbyte 29 203", "w.setshort 1 15424", "w.setbytes 76 -", "w.rawstring 8116e996c967dfd5945d4c08ae73a025ad8508e311332dce0b4712aee24a46cd58b49c4457c32c09",
    "w.bool 1", "w.buffer dc92950354ad84a3a27e352e6ed9e06aa2318e36d14cae5d85106c0b5686b146d914e011edce19c4 3", "w.reserve 500", "w.tobuf 0",
    "r.overb", "r.setpos 6", "r.pos", "r.buf 2",
    "r.byte", "r.wstring 4", "w.wstring 815d.ffff.d800.ffff.ae08.263a.92d8.d800.0.263a.ffff.0.d800.41.7a.263a.20ac.ffff.2fee.dc00 2", "w.wstring - 2",
    "w.setsize 1", "w.state", "w.tobuf 0", "w.new 4 1",
    "w.bytes 1e", "w.state", "w.reset", "w.bytes e6023589bf659a",
    "w.skip 3", "w.bool 1", "w.length 256 2", "w.setbyte 6 200",
    "w.setsize 12", "w.wstring 63ec 3", "w.reserve 16", "w.state",
    "w.tobuf 0", "w.new 2 0", "w.skip 1", "w.reset",
    "w.bytes 84dbff5b0de2f3", "w.reserve 500", "w.reset", "w.tobuf 0",
    "w.long -97725902", "w.wstring - 2", "w.byte 158", "w.reserve 33",
    "w.setbyte 3 120", "w.buffer bd 2", "w.buffer - 4", "w.bool 2",
    "w.length 2147483648 2", "w.byte 189", "w.bytes 848960a01f7ca7c3b895efce1d27bc6dbb7b9e058e7ade6e4c90d219165359f1106a6df37fd454c552a00ef9458183af68c367a32127455693dc3edebb5ced93", "w.setsize 1",
    "w.length 4294967295 2", "w.state", "w.tobuf 1", "w.new 2 1",
    "w.setsize 0", "w.state", "w.reserve 32", "w.wstring ffff.41.20ac.dbff.0.263a.dc00.0.2bc5.dbff.7a.263a.ffff.0.d800.e9.19dc.41ea.e9.263a 2",
    "w.skip 1000", "w.string ed2613d75e 2", "w.buffer 0580eac19ef89f2b90 3", "w.tobuf 0",
    "r.overb", "r.setpos 6", "r.pos", "w.byte 67",
    "w.rawstring -", "w.short -14017", "w.setsize 0", "w.buffer 3a993f6750dc87aafda915a5f38727a9a0bf3c1e41e9dfd413099ab6f8624b62b5ef2741c232a4b4 1",
    "w.short 276", "w.state", "w.tobuf 1", "w.new 2 1",
    "w.setsize 0", "w.reset", "w.skip 100", "w.long 1877205540",
    "w.setsize 0", "w.string 9a39560103b416ac56d0aa15956666ca145b93a8bb971b290df1b283cb6fc3ac024184a930b85317b20b5b7aa5f8637377e0fd51c546f1493b2166e6d4d3939e058d36f4b856 2", "w.byte 243", "w.rawstring bcf43d20cb716747916360eab0e9c89621cdae7b66374602f73746f093f53df32206eb7f200f9e0c",
    "w.chunk 64 0", "w.reset", "w.tobuf 0", "w.reset",
    "w.wstring 0.dbff.263a 0", "w.buffer 856fdc28f32ebcd3214ca145860e3fe5c981148f0ee47bb5c94ebd4cc6211f66c1b2f8558451e8ad 1", "w.bytes 51d7cf4da45196", "w.setsize 53",
    "w.tobuf 0", "r.overb", "r.bytes 2", "r.rawstring 3",
    "r.pos", "r.rawstring 0", "w.state", "w.tobuf 1",
    "w.new 4 0", "w.wstring 7f02.7a.41 0", "w.tobuf 0", "r.overb",
    "r.bool", "w.setsize 56", "w.setbytes 8 -", "w.setshort 15 1488",
    "w.length 128 4", "w.buffer - 4", "w.skip 1000", "w.buffer -- 4",
    "w.wstring e8b7 2", "w.wstring - 0", "w.state", "w.length 128 4",
    "w.length 256 2", "w.setbytes 946 -", "w.state", "w.byte 80",
    "w.state", "w.long -1566143040", "w.state", "w.buffer - 4",
    "w.length 426844924 2", "w.length 128 2", "w.chunk 33 0", "w.setsize 1090",
    "w.length 63497992 1", "w.state", "w.tobuf 0", "w.new 3 0",
    "w.wstring 0.263a.7a 2", "w.setsize 1", "w.bytes -", "w.byte 134",
    "w.long 1660804692", "w.byte 226", "w.bytes 5a10", "w.chunk 65 0",
    "w.setbytes 4 4b", "w.state", "w.tobuf 1", "w.new 4 0",
    "w.string 7f02a32fbdd3d5c4bfe2ea6445947e0879f2171929252c8cd251da173d96a0898b 2", "w.wstring 7a 0", "w.setbytes 26 5133", "w.wstring 263a.dc00.7a.e9.263a.7a.41.7a.ffff.e9.41.0.ffff.0.263a.e9.7a.41.e9.41 4",
    "w.state", "w.tobuf 0", "w.new 4 0", "w.bool 1",
    "w.state", "w.chunk 10 1000", "w.long -1262104297", "w.reset",
    "w.state", "w.tobuf 1", "w.new 1 0", "w.short -6705",
    "w.wstring - 4", "w.setsize 5", "w.reserve 500", "w.long 1523318564",
    "w.buffer 610ebdb9530336b958db477056ba6eaaf4d9ee69032b66e124118756b42ed7455a54965fe2607863 4", "w.long 305660300", "w.wstring - 0", "w.reset",
    "w.rawstring 22d5689aed68f46f4ff5250718e95c0101124718916167803d1612e2318eb028ca5a6eadb1ce45b2", "w.setsize 40", "w.byte 172", "w.reserve 33",
    "w.buffer -- 2", "w.setsize 93", "w.short -22588", "w.rawstring 96edd52a1ef0a624dd510a1d63526c7e31e0308d6a88590fd420e2b1e61d6b9bac28c4efad42e389",
    "w.bool 2", "w.state", "w.tobuf 0", "w.new 2 0",
    "w.skip 1", "w.rawstring cf89414ff2950ebd519586b230ebb5ec021cdf24cccb7c7fd84abb0fbd1512b790bf3009c6b20c06", "w.short -27180", "w.setbytes 22 7bcf",
    "w.byte 222", "w.buffer - 4", "w.tobuf 0", "w.short 9854",
    "w.wstring - 4", "w.wstring - 2", "w.bool 2", "w.setshort 11 -2554",
    "w.reset", "w.state", "w.tobuf 1", "w.new 2 0",
    "w.state", "w.buffer -- 2", "w.reserve 33", "w.byte 148",
    "w.buffer -- 4", "w.setbyte 3 185", "w.setshort 0 -12591", "w.skip 1000",
    "w.setshort 925 -1652", "w.reserve 33", "w.string 22d5e12b54fc48191043d8dcb13dfb22073e5aaebb4b432530f9473f73248280 2", "w.bytes -",
    "w.reset", "w.byte 128", "w.length 255 2", "w.reset",
    "w.reserve 0", "w.state", "w.chunk 65 64", "w.chunk 700 10",
    "w.tobuf 0", "r.overb", "r.pos", "r.rawstring 1",
    "w.state", "w.tobuf 0", "w.new 4 1", "w.rawstring 93",
    "w.reset", "w.tobuf 0", "r.overb", "r.rawstring 3",
    "r.wstring 4", "r.rawstring 1", "r.buf 4", "r.bool",
    "w.rawstring -", "w.rawstring 3f", "w.reserve 500", "w.bytes 4c9b",
    "w.wstring 0.20ac.0 4", "w.rawstring 01", "w.string d1f7eddd5ec0210cbd90c88be8673ec45dde1d5caed06dffd01d51916445f3 4", "w.length 65536 2",
    "w.reset", "w.skip 3", "w.tobuf 0", "w.rawstring 6b",
    "w.buffer -- 2", "w.long -1915362295", "w.rawstring f9d8792ac0", "w.state",
    "w.tobuf 1", "w.new 3 1", "w.long 486960993", "w.byte 217",
    "w.reset", "w.state", "w.rawstring 9b1b6fab8c", "w.length 3134779666 2",
    "w.setshort 3 27685", "w.reset", "w.reserve 16", "w.buffer 26 1",
    "w.long 1532693341", "w.bool 0", "w.bool 2", "w.reserve 32",
    "w.setsize 8", "w.setbyte 3 192", "w.short 24024", "w.length 65536 2",
    "w.setbytes 1 -", "w.short -11635", "w.tobuf 0", "w.short -4398",
    "w.string e8469c59ba6416b61d322a40a3213729c08d9d3a94005899537c90ae47cfe71ccc06befd86cd24868cc49eade6555705d50d57ed97e821c2036055f55d8495c23f0cf77e68c8 0", "w.wstring 7a 2", "w.bytes ff6ae3c2745fc3f22ca68a2f0d65029b2ae4ae4f72e3fcdb36c3517a4dfdbbc0164fd63e966c0696a7cb29f15778d01646037fd1772adecfba69b86f80bb2234", "w.string - 4",
    "w.tobuf 0", "w.rawstring 60", "w.byte 158", "w.state",
    "w.tobuf 0", "w.new 3 1", "w.rawstring 9b", "w.chunk 33 10",
    "w.short 13820", "w.buffer 5a921bfac3d2405431 2", "w.buffer cb261aa2b1cb0a0ef839f6e7b4280cffcf3a79b37d0ccb4c8a0337a6234b61bd1ba9df1da8a5572b 4", "w.setshort 22 -1539",
    "w.setbytes 16 81", "w.skip 100", "w.wstring e9 4", "w.tobuf 1",
    "w.state", "w.rawstring -", "w.rawstring 602fbf05855e651cb145a29964d64840195ed08763de9b93377d5688c79bd045fb7891500d250785", "w.bytes 55f5",
    "w.setshort 22 -719", "w.reset", "w.state", "w.long 1869693287",
    "w.state", "w.reset", "w.state", "w.chunk 33 0",
    "w.tobuf 1", "r.overb", "r.avail", "r.bytes 5",
    "r.pos", "r.avail", "r.string 4", "r.pos",
    "w.state", "w.tobuf 0", "w.new 4 0", "w.buffer -- 1",
    "w.reset", "w.reserve 500", "w.setsize 0", "w.state",
    "w.tobuf 1", "w.new 4 0", "w.length 65535 4", "w.reserve 0",
    "w.length 65536 2", "w.bytes -", "w.setbytes 0 78", "w.short -8996",
    "w.length 65535 4", "w.chunk 65 1000", "w.reset", "w.reset",
    "w.reserve 0", "w.buffer 8aa27652c1aac3062ef36a654c3cb6322b266a76acad54eb9b73599f2877fe118faa3110323dc060 4", "w.bool 0", "w.wstring e9.20ac.41.f605.8463.53d3.263a.263a.0.20ac.0.263a.e9.0.41.263a.ffff.263a.e9.41 0",
    "w.setsize 84", "w.state", "w.chunk 0 64", "w.setshort 10 -9385",
    "w.state", "w.tobuf 0", "w.new 3 1", "w.wstring ffff 4",
    "w.string 52a198f928a4783aab15d68beec779a1a29ad48f7380028f63c02b8b534473b7494295efb0440f4da973a86b35e9ab22c06057ec054c412ab0e4be2bd956411018f8a062e5cd 2", "w.setsize 78", "w.length 128 4", "w.state",
    "w.tobuf 0", "w.new 0 0", "w.setsize 0", "w.tobuf 0",
    "r.overb", "r.buf 2", "r.bytes 2", "r.short",
    "r.rawwstring 0", "r.wstring 4", "r.rewind", "w.tobuf 1",
    "r.overb", "r.long", "r.string 4", "r.bytes 2",
    "r.string 1", "r.short", "w.string 1276ceb006cf5c2d43de2cc40e8518fae5a713f8360edc162aea702d326d8a7dbd 2", "w.reserve 0",
    "w.reserve 0", "w.byte 132", "w.bool 2", "w.byte 116",
    "w.reserve 33", "w.string - 4", "w.length 1410157537 0", "w.string 1debb21819 3",
    "w.setbyte 35 92", "w.wstring 7a.20ac.41 2", "w.byte 189", "w.state",
    "w.short 6547", "w.state", "w.tobuf 0", "w.new 0 1",
    "w.buffer a3 2", "w.wstring 20ac.ffff.0 4", "w.short 24869", "w.state",
    "w.long 274794421", "w.byte 6", "w.bool 2", "w.state",
    "w.tobuf 0", "w.new 2 0", "w.byte 70", "w.wstring 263a.e9.20ac.0.7a.7a.d800.41.7a.20ac.dfff.e9.7a.cd3b.ba6b.41.20ac.41.4c19.41 4",
    "w.bytes 82", "w.byte 230", "w.string e81f19cea5f1148cff19ff1af6f94acb45fcb7b530af84dc449320478e9146 1", "w.skip 3",
    "w.bool 1", "w.byte 142", "w.setbyte 3 213", "w.long -1336259359",
    "w.wstring - 4", "w.buffer - 0", "w.setsize 91", "w.string d9beb5703e9413d8b441c4830686d680b08b7c20cb247380dcba4cb8a13e0909 4",
    "w.setshort 65 14652", "w.chunk 64 10", "w.setbyte 101 79", "w.setbytes 4 63e2",
    "w.reserve 0", "w.rawstring -", "w.long 850316732", "w.short -7555",
    "w.length 1 1", "w.buffer - 2", "w.reset", "w.byte 231",
    "w.state", "w.tobuf 0", "w.new 3 1", "w.string 6ef7944384 4",
    "w.setshort 1 -1543", "w.state", "w.setbytes 5 -", "w.short 25436",
    "w.long 1773182686", "w.short 10957", "w.setbytes 6 -", "w.setshort 1 7318",
    "w.setshort 11 -4514", "w.rawstring -", "w.setbyte 6 119", "w.reserve 16",
    "w.setsize 1", "w.reserve 32", "w.long 1339572159", "w.tobuf 0",
    "r.overb", "r.avail", "r.setpos 1", "r.bytes 1",
    "r.rawstring 9", "r.long", "w.bytes 9907", "w.state",
    "w.string 0d03a7f074895d80f17b2afd09fb44a9fcde7cd079a034c8e19f419957687522c3 4", "w.string 548f675e046fa84654fe9ce441e3e0c576f7b594da296014ab74231322706c3fc4 4", "w.bytes 069b719471f698e4fa30e036de57b2de38c60ebc295dbe43c71af848b84d6307319fca057b49a01ba46862b2250ca57b5946eaebd49593ea8619bef347959a1a", "w.skip 1",
    "w.byte 74", "w.bool 1", "w.rawstring a199a108bf243304d2d3b02b2d1ff802c5bfc110146252be77ddaf842d8f82f1cce2cbc1baae1f00", "w.state",
    "w.tobuf 1", "w.new 4 0", "w.setsize 50", "w.chunk 0 64",
    "w.setbytes 25 31e9", "w.rawstring 54692187f87f01a5f56342b11da414ab000f6965be887e7fba427773ac0233dc7d71f25d379a0cb3", "w.chunk 33 40", "w.short -31953",
    "w.long -1678480449", "w.setbytes 43 ec96", "w.bool 1", "w.bool 0",
    "w.string 63f3e9190e998904bf28ef088beef53498706a4785e4709c1cd06324ba20ab3e505acea01d4f741697e338136bbccdd73dc894a4a1d30d0406831fa5bcee62b11b5f30b52295 0", "w.state", "w.tobuf 0", "r.overb",
    "r.rewind", "r.rewind", "r.wstring 2", "r.rawstring 1",
    "r.pos", "w.setshort 2 9227", "w.bytes 3a", "w.reserve 33",
    "w.setsize 0", "w.length 4294967295 1", "w.bool 2", "w.tobuf 1",
    "w.state", "w.tobuf 1", "w.new 2 1", "w.wstring ffff.263a.dc00.263a.dbff.dfff.ffff.ffff.41.dfff.0.dc00.20ac.7a.41.41.0.d800.263a.ffff 3",
    "w.skip 1000", "w.length 0 1", "w.rawstring c529192ac7", "w.reset",
    "w.skip 0", "w.skip 100", "w.buffer -- 3", "w.tobuf 0",
    "w.wstring - 4", "w.setbytes 89 9831", "w.byte 244", "w.length 255 4",
    "w.string 2b37630889 4", "w.wstring - 1", "w.state", "w.buffer e5c1b7bc0a9f8bfdd2 2",
    "w.wstring 8566.3de3.7a.dbff.dbff.8012.dc00.e9.7a.263a.e9.20ac.0.e9.20ac.0.20ac.947a.263a.0 4", "w.short 4718", "w.wstring 0 2", "w.state",
    "w.tobuf 0", "w.new 2 1", "w.byte 251", "w.rawstring -",
    "w.wstring e9.41.7a.ffff.17c1.dfff.ffff.0.dfff.20ac.7a.263a.41.ffff.0.e9.ffff.41.7a.263a 4", "w.buffer 2d42fb0e8f3c1e567b49abec60e2e3cacc443274360a6d91aff48a812a1c31794709eb50e2f8bcef 0", "w.bool 2", "w.wstring - 4",
    "w.setbytes 46 -", "w.bool 0", "w.bool 2", "w.rawstring 8523c3c3df",
    "w.state", "w.tobuf 0", "w.new 2 0", "w.reset",
    "w.long -117136217", "w.state", "w.tobuf 0", "w.new 2 0",
    "w.long 254511894", "w.bytes 27bd7ff866f3f0", "w.length 65535 1", "w.short -30208",
    "w.setsize 1", "w.byte 186", "w.wstring 0.0.0.263a.7a.0.20ac.263a.0.d800.ea3c.263a.20ac.ffff.e9.7a.ffff.ffff.dc00.263a 1", "w.reset",
    "w.state", "w.length 1 3", "w.state", "w.tobuf 1",
    "w.new 4 1", "w.reset", "w.string 56 4", "w.wstring 0.41.7a.7a.0.0.e9.7a.20ac.41.7a.cb34.422d.41.e9.d699.ffff.4773.dfff.ffff 0",
    "w.setbyte 1 91", "w.length 128 1", "w.state", "w.tobuf 1",
    "w.new 2 1", "w.chunk 0 1000", "w.long 411637135", "w.byte 37",
    "w.rawstring 6a280d1b55", "w.state", "w.setsize 0", "w.wstring ffff.41.41 0",
    "w.bytes 7217", "w.state", "w.tobuf 1",
};

static std::vector<std::string> sLines;

static std::string Hex(const void* inData, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inData)[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

// the UTF-16 units of a wide string (on UTF-32 systems re-encoded, so every OS prints the same)
static std::string Units(const std::wstring& inText)
{
    std::string aBytes, anOut;
    WideAppendUtf16LE(aBytes, inText);
    char aBuffer[12];
    for (size_t i = 0; i + 1 < aBytes.size(); i += 2) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%04x", i ? "." : "",
                 (unsigned)(unsigned char)aBytes[i] | ((unsigned)(unsigned char)aBytes[i + 1] << 8));
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::wstring WideUnits(const std::string& inArg)
{
    std::wstring anOut;
    if (inArg == "-")
        return anOut;
    const char* p = inArg.c_str();
    while (*p) {
        char* anEnd;
        WideAppendUtf16Unit(anOut, (uint16_t)strtoul(p, &anEnd, 16));
        p = *anEnd == '.' ? anEnd + 1 : anEnd;
    }
    return anOut;
}

static long long Number(const std::string& inArg) { return strtoll(inArg.c_str(), NULL, 0); }

static std::string Bytes(const std::string& inArg)
{
    std::string anOut;
    for (size_t i = 0; inArg != "-" && i + 1 < inArg.size(); i += 2)
        anOut += (char)strtoul(inArg.substr(i, 2).c_str(), NULL, 16);
    return anOut;
}

static unsigned long long Fnv(const void* inData, size_t inLength)
{
    unsigned long long h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < inLength; i++) {
        h ^= static_cast<const unsigned char*>(inData)[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

// a buffer's bytes: "null" and the length, or the length, the FNV-1a 64 and the first 16 bytes
static std::string Summary(const char* inData, unsigned long inLength)
{
    if (inData == NULL)
        return "null " + std::to_string(inLength);
    char aBuffer[64];
    snprintf(aBuffer, sizeof(aBuffer), "%lu %016llx ", inLength, Fnv(inData, inLength));
    return aBuffer + Hex(inData, inLength < 16 ? inLength : 16);
}

static WriteBuffer* sWriter;
static ByteBufferPtr sBuffer;
static ReadBuffer sReader(NULL, 0);

static std::string WriterState()
{
    return std::to_string(sWriter->capacity()) + " " + Summary(sWriter->data(), sWriter->length());
}

static std::string Step(const std::string& inOp, const std::string& inArg, const std::string& inArg2)
{
    if (inOp == "w.new") {
        delete sWriter;
        sWriter = new WriteBuffer((unsigned char)Number(inArg), Number(inArg2) != 0);
        return WriterState();
    }
    if (inOp.compare(0, 2, "w.") == 0) {
        if (inOp == "w.state") {}
        else if (inOp == "w.byte") sWriter->AppendByte((unsigned char)Number(inArg));
        else if (inOp == "w.bool") sWriter->AppendBool(Number(inArg) != 0);
        else if (inOp == "w.short") sWriter->AppendShort((short)Number(inArg));
        else if (inOp == "w.long") sWriter->AppendLong((int32_t)Number(inArg));
        else if (inOp == "w.length") sWriter->AppendLength((unsigned long)Number(inArg), (unsigned char)Number(inArg2));
        else if (inOp == "w.string") sWriter->AppendString(Bytes(inArg), (unsigned char)Number(inArg2));
        else if (inOp == "w.rawstring") sWriter->AppendRawString(Bytes(inArg));
        else if (inOp == "w.wstring") sWriter->AppendWString(WideUnits(inArg), (int)Number(inArg2));
        else if (inOp == "w.buffer") {
            if (inArg == "--")
                sWriter->AppendBuffer(NULL, (unsigned char)Number(inArg2));
            else {
                std::string aBytes = Bytes(inArg);
                ByteBufferPtr aBuffer = new ByteBuffer(aBytes.data(), (unsigned long)aBytes.size(), false);
                sWriter->AppendBuffer(aBuffer, (unsigned char)Number(inArg2));
            }
        }
        else if (inOp == "w.bytes") {
            std::string aBytes = Bytes(inArg);
            sWriter->AppendBytes(aBytes.data(), (unsigned long)aBytes.size());
        }
        else if (inOp == "w.skip") sWriter->SkipBytes((unsigned long)Number(inArg));
        else if (inOp == "w.setsize") sWriter->SetSize((unsigned long)Number(inArg));
        else if (inOp == "w.reserve") sWriter->Reserve((unsigned long)Number(inArg));
        else if (inOp == "w.chunk") sWriter->ReserveChunk((unsigned long)Number(inArg), (unsigned long)Number(inArg2));
        else if (inOp == "w.setbyte") sWriter->SetByte((unsigned long)Number(inArg), (unsigned char)Number(inArg2));
        else if (inOp == "w.setshort") sWriter->SetShort((unsigned long)Number(inArg), (short)Number(inArg2));
        else if (inOp == "w.setbytes") {
            std::string aBytes = Bytes(inArg);
            sWriter->SetBytes((unsigned long)Number(inArg), aBytes.data(), (unsigned long)aBytes.size());
        }
        else if (inOp == "w.reset") sWriter->Reset();
        else if (inOp == "w.tobuf") {
            sBuffer = sWriter->ToByteBuffer(Number(inArg) != 0);
            return Summary(sBuffer->data(), sBuffer->length()) + " / " + WriterState();
        }
        else return "unknown op";
        return WriterState();
    }
    if (inOp == "b.state") return Summary(sBuffer->data(), sBuffer->length());
    if (inOp == "b.text") {
        std::string aText = inArg == "-" ? std::string() : Bytes(inArg);
        sBuffer = new ByteBuffer(inArg == "NULL" ? (const char*)NULL : aText.c_str());
        return Summary(sBuffer->data(), sBuffer->length());
    }
    if (inOp == "b.copy") {
        std::string aBytes = Bytes(inArg);
        sBuffer = new ByteBuffer(aBytes.data(), (unsigned long)aBytes.size(), false);
        return Summary(sBuffer->data(), sBuffer->length());
    }
    char aBuffer[64];
    if (inOp == "r.overb") { sReader.SetData(sBuffer->data(), sBuffer->length()); return std::to_string(sReader.length()); }
    if (inOp == "r.overw") { sReader.SetData(sWriter->data(), sWriter->length()); return std::to_string(sReader.length()); }
    if (inOp == "r.pos") return std::to_string(sReader.pos());
    if (inOp == "r.avail") return std::to_string(sReader.Available());
    if (inOp == "r.length") return std::to_string(sReader.length());
    if (inOp == "r.rewind") { sReader.Rewind(); return std::to_string(sReader.pos()); }
    if (inOp == "r.setpos") {
        // (a position past the end makes ReadBytes' bound wrap and read outside the buffer: not asked)
        if (Number(inArg) > (long long)sReader.length())
            return "past";
        sReader.SetPos((int)Number(inArg));
        return std::to_string(sReader.pos());
    }
    if (inOp == "r.byte") return std::to_string(sReader.ReadByte());
    if (inOp == "r.bool") return sReader.ReadBool() ? "true" : "false";
    if (inOp == "r.short") return std::to_string(sReader.ReadShort());
    if (inOp == "r.long") return std::to_string(sReader.ReadLong());
    if (inOp == "r.rlen") {
        snprintf(aBuffer, sizeof(aBuffer), "%lu", sReader.ReadLength((unsigned char)Number(inArg)));
        return aBuffer;
    }
    if (inOp == "r.bytes") {
        unsigned long n = (unsigned long)Number(inArg);
        const void* p = sReader.ReadBytes(n);
        return Hex(p, n) + " " + std::to_string(sReader.pos());
    }
    if (inOp == "r.buf") {
        ByteBufferPtr aRead = sReader.ReadBuf((unsigned char)Number(inArg));
        return Summary(aRead->data(), aRead->length()) + " " + std::to_string(sReader.pos());
    }
    std::string aString = "unset";
    std::wstring aWide = L"unset";
    try {
        if (inOp == "r.rawstring") { sReader.ReadRawString(aString, (unsigned long)Number(inArg)); return Hex(aString.data(), aString.size()); }
        if (inOp == "r.string") { sReader.ReadString(aString, (unsigned char)Number(inArg)); return Hex(aString.data(), aString.size()); }
        if (inOp == "r.rawwstring") { sReader.ReadRawWString(aWide, (unsigned long)Number(inArg)); return Units(aWide); }
        if (inOp == "r.wstring") { sReader.ReadWString(aWide, (int)Number(inArg)); return Units(aWide); }
    } catch (ReadBufferException& anException) {
        // the strings as the throw left them
        return "throw " + anException.mMessage + " / " + Hex(aString.data(), aString.size()) + " / " + Units(aWide)
               + " / " + std::to_string(sReader.pos());
    }
    return "unknown op";
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_buffer.txt";
}

int main(int argc, char** argv)
{
    sWriter = new WriteBuffer(0, false);
    const size_t aSteps = sizeof(kScript) / sizeof(kScript[0]);
    for (size_t i = 0; i < aSteps; i++) {
        std::string aStep = kScript[i], anOp, anArg, anArg2;
        size_t aSpace = aStep.find(' ');
        anOp = aStep.substr(0, aSpace);
        if (aSpace != std::string::npos) {
            std::string aRest = aStep.substr(aSpace + 1);
            size_t aSecond = aRest.find(' ');
            anArg = aRest.substr(0, aSecond);
            if (aSecond != std::string::npos)
                anArg2 = aRest.substr(aSecond + 1);
        }
        std::string aResult;
        try {
            aResult = Step(anOp, anArg, anArg2);
        } catch (ReadBufferException& anException) {
            aResult = "throw " + anException.mMessage + " / " + std::to_string(sReader.pos());
        }
        sLines.push_back(anOp + " -> " + aResult);
    }
    delete sWriter;
    sBuffer = NULL;
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d (%s)\n  got  %s\n  want %s\n", (int)i + 1, i < aSteps ? kScript[i] : "-", aGot,
                       aExpect);
        }
    }
    printf("won_buffer_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
