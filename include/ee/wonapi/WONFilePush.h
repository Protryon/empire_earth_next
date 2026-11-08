// No original address identified: WON SDK authenticated file-push op
#ifndef EE_WONAPI_WONFILEPUSH_H
#define EE_WONAPI_WONFILEPUSH_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
class FilePushOp : public ServerOp
{
public:
	FilePushOp(const IPAddr &theAddr);
	virtual ~FilePushOp(void);

	void SetFile(const std::string& theFileName) { this->mFileName = theFileName; }
	void SetRemoteFile(const std::string& theRemoteFileName) { this->mRemoteFileName = theRemoteFileName; }
	void SetCompress(bool isCompress) { this->mIsCompress = isCompress; }
	void SetModifyTime(FILETIME theModifyTime) { this->mModifyTime = theModifyTime; }
	void SetProgressCompletion(OpCompletionBasePtr theCompletion) { this->mProgressCompletion = theCompletion; }
	void SetBlockTransferSize(DWORD theSize) { this->mBlockTransferSize = theSize; }
	void SetComment(const std::string& theComment) { this->mComment = theComment; }

	DWORD GetTotalBytesSent() const { return this->mTotalBytesSent; }
	DWORD GetLastBytesSent() const { return this->mLastBytesSent; }
private:
	class AutoFile
	{
	private:
		FILE *mFileP;
		
	public:
		AutoFile() : mFileP(NULL) { }
		~AutoFile() { this->Detach(); }

		void Attach(FILE *theFileP) { this->Detach(); this->mFileP = theFileP; }
		void Detach() { if(this->mFileP) fclose(this->mFileP); this->mFileP = NULL; }
		FILE* GetFile() { return this->mFileP; }
	};

	AutoFile mFile;
	IPAddr mConnectAddr;

protected:
	std::string         mFileName;
	std::string         mRemoteFileName;
	bool                mIsCompress;
	FILETIME            mModifyTime;
	OpCompletionBasePtr mProgressCompletion;
	DWORD				mTotalBytesSent;
	DWORD				mLastBytesSent;
	Blowfish			mEncryptKey;
	DWORD				mBlockTransferSize;
	std::string			mComment;

	enum TrackTypes
	{
		TRACK_AUTH				= 1,
		TRACK_STREAM_REQUEST	= 2,
		TRACK_STATUS_REPLY		= 3,
		TRACK_ACK				= 4
	};

	virtual bool CallbackHook(AsyncOp *theOp, int theId);

	bool CheckError(WONStatus theStatus);
	bool GetEncryptKey(PeerAuthOp *theOp);
	void DoProgressCompletion();

	bool HandleFactStatusReply(const ByteBuffer *theReply);
	bool RecvFactStatusReply();

	bool HandleAck(char theAck);
	bool RecvAck();

	bool SendFileChunk();

	bool SendStreamFileRequest();
	
	virtual void RunHook(void);
	virtual void CleanupHook(void);

};

typedef SmartPtr<FilePushOp> FilePushOpPtr;
}
#endif
