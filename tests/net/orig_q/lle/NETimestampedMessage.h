// C:\EEX\Low-Level Engine\Network Engine\NETimestampedMessage.h (RECONSTRUCTION; NETimestampedMessage.cpp
// 0x4b04f..0x4b190). A message with the time it was sent / is due. Requires NEMessage.
// GetMessage is exported as ?GetMessageA@...: the source saw windows.h's GetMessage macro.
#ifndef LLE_NETIMESTAMPEDMESSAGE_H
#define LLE_NETIMESTAMPEDMESSAGE_H

class LLE_API NETimestampedMessage {       // proven sizeof 8 (NELagLossSimulator::Process: operator new(8))
public:
    // proven inline (0x3d5e4 / 0x3d5f8, the first Network Engine unit); no Reference() on the message
    NETimestampedMessage(NEMessage* inMessage, unsigned long& inTimestamp)
    {
        this->mMessage = inMessage;
        this->mTimestamp = inTimestamp;
    }
    NETimestampedMessage(unsigned char* inData);
    ~NETimestampedMessage() { this->mMessage->Dereference(); }
    NEMessage* GetMessage() { return this->mMessage; }
    unsigned long GetTimestamp() { return this->mTimestamp; }
    void ExportMessage(unsigned char** outData, unsigned long& outSize);
    void ImportMessage(unsigned char* inData);

    NEMessage* mMessage;                    // +0x00
    uint32_t mTimestamp;                    // +0x04 (4 bytes in the record file)
};

#endif
