// WON's wide strings on the wire: WriteBuffer -> bytes -> ReadBuffer must give UTF-16 little-endian, 2 bytes per
// unit, on every OS (wchar_t is 2 bytes on Windows, 4 on Linux and macOS), and 32-bit AppendLong/ReadLong fields.
// Links with the won module's ReadBuffer, WriteBuffer and ByteBuffer units. Prints one line per failed check; the exit
// code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"

using namespace WONAPI;

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bool SameBytes(const char* inData, unsigned long inLength, const unsigned char* inWant, unsigned long inWantLen)
{
    if (inLength != inWantLen || memcmp(inData, inWant, inWantLen) != 0) {
        printf("  got  ");
        for (unsigned long i = 0; i < inLength; i++) printf(" %02x", (unsigned char)inData[i]);
        printf("\n  want ");
        for (unsigned long i = 0; i < inWantLen; i++) printf(" %02x", inWant[i]);
        printf("\n");
        return false;
    }
    return true;
}

int main()
{
    // 'A', e acute (U+00E9), the euro sign (U+20AC), U+1F600 (a surrogate pair in UTF-16)
    const std::wstring text = L"Aé€\U0001F600";
    WriteBuffer out(4, false);
    out.AppendWString(text, 2);
    out.AppendLong(-2);
    out.AppendShort(0x1234);
    out.AppendWString(std::wstring(), 1);
    out.AppendLength(0xffffffffu, 4);
    static const unsigned char want[] = {
        0x05, 0x00,                                                 // 5 UTF-16 units
        0x41, 0x00, 0xe9, 0x00, 0xac, 0x20, 0x3d, 0xd8, 0x00, 0xde, // A e-acute euro, U+1F600 as D83D DE00
        0xfe, 0xff, 0xff, 0xff,                                     // AppendLong(-2): 4 bytes
        0x34, 0x12,                                                 // AppendShort
        0x00,                                                       // an empty wide string, 1-byte length
        0xff, 0xff, 0xff, 0xff};                                    // AppendLength(0xffffffff, 4)
    CHECK(SameBytes(out.data(), out.length(), want, sizeof(want)));
    CHECK(WideUtf16Length(text) == 5);

    ReadBuffer in(out.data(), out.length());
    std::wstring back;
    in.ReadWString(back, 2);
    CHECK(back == text);
    CHECK(in.ReadLong() == -2);
    CHECK(in.ReadShort() == 0x1234);
    std::wstring empty = L"x";
    in.ReadWString(empty, 1);
    CHECK(empty.empty());
    CHECK(in.ReadLength(4) == 0xfffffffful); // sign-extended to 32 bits only, as the original
    CHECK(in.Available() == 0);

    // the 4-byte length prefix in front of the message
    ConstSmartPtr<ByteBuffer> msg = out.ToByteBuffer(false);
    CHECK(msg->length() == sizeof(want) + 4);
    static const unsigned char prefix[] = {sizeof(want), 0, 0, 0};
    CHECK(SameBytes(msg->data(), 4, prefix, 4));

    // a lone low surrogate stays one unit; a 2-byte length field sign-extends (0xffff -> 0xffffffff)
    static const unsigned char lone[] = {0x01, 0x00, 0x00, 0xdc, 0xff, 0xff};
    ReadBuffer loneIn(lone, sizeof(lone));
    loneIn.ReadWString(back, 2);
    CHECK(back.length() == 1 && (unsigned)back[0] == 0xdc00);
    CHECK(loneIn.ReadLength(2) == 0xfffffffful);

    printf("won_wire_test: %d failed\n", sFailures);
    return sFailures;
}
