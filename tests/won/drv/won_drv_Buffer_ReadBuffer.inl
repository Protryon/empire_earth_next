// ReadBuffer's members only the drivers use, cut from ee/wonapi/WONBuffer.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// the next bytes, skipped; throws ReadBufferException past the end
const void* ReadBytes(unsigned long inLength) { return this->Take(inLength); }
// a length-prefixed block as a new byte buffer
ByteBufferPtr ReadBuf(unsigned char inLengthSize)
{
    unsigned long aLength=this->ReadLength(inLengthSize);
    return new ByteBuffer(this->Take(aLength),aLength,false);
}
