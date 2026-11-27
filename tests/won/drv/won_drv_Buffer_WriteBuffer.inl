// WriteBuffer's members only the drivers use, cut from ee/wonapi/WONBuffer.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void ReserveChunk(unsigned long theNumBytesToReserve,unsigned long theMaxSize=0)
{ ee_won_writer_reserve_chunk(this->mBuffer,theNumBytesToReserve,theMaxSize); }
void SkipBytes(unsigned long inLength) { ee_won_writer_skip(this->mBuffer,inLength); }
void AppendLength(unsigned long inLength, unsigned char inSize)
{ ee_won_writer_append_length(this->mBuffer,inLength,inSize); }
void AppendRawString(const std::string& theStr) { this->AppendString(theStr,0); }
// a length field, then the bytes; a NULL buffer writes only a zero length
void AppendBuffer(const ByteBuffer* inBuffer, unsigned char inLengthFieldSize = 0)
{
    ee_won_writer_append_field(this->mBuffer,inBuffer!=NULL ? reinterpret_cast<const uint8_t*>(inBuffer->data()) : NULL,
                               inBuffer!=NULL ? inBuffer->length() : 0,inLengthFieldSize);
}
// (R6 WON round 39)
unsigned long capacity() const { return ee_won_writer_capacity(this->mBuffer); }
void AppendString(const std::string& inText, unsigned char inSize = 2)
{
    ee_won_writer_append_field(this->mBuffer,reinterpret_cast<const uint8_t*>(inText.data()),
                               static_cast<unsigned long>(inText.length()),inSize);
}
