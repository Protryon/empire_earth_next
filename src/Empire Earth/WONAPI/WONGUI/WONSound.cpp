// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Software audio mixing and sound channel scheduling of the WON sound thread (not identified in the original exe)
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <cstring>
#include <cassert>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyWONSound.h"
namespace WONAPI {
WONSoundThread::ChannelFactory WONSoundThread::mChannelFactory = NULL;
static WONSoundThread *gWONSoundThread = NULL;
static int gMaxSoundChannels = 2;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundData::WONSoundData()
{
	this->mDataPos = this->mDataLen = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundData::~WONSoundData()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundData::SetDataPos(int thePos, int theLen)
{
	this->mDataPos = thePos;
	this->mDataLen = theLen;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const char* WONSoundData::GetData(int &theLen)
{
	if(!this->mReader.IsOpen())
		return NULL;

	int aBytesLeft = this->mDataPos + this->mDataLen - this->mReader.pos();
	if(aBytesLeft<=0)
		return NULL;

	if(this->mReader.mBuffer!=NULL)
	{
		const char *aData = this->mReader.mBuffer + this->mReader.pos();
		theLen = aBytesLeft;
		this->mReader.SkipBytes(theLen);
		return aData;
	}

	int aNumToRead = aBytesLeft>BUF_SIZE ? BUF_SIZE : aBytesLeft;
	theLen = this->mReader.ReadMaxBytes(this->mFileBuf,aNumToRead);
	return this->mFileBuf;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundData::Rewind()
{
	if(this->mDataLen==0)
		return;

	try
	{
		this->mReader.SetPos(this->mDataPos);
	}
	catch(FileReaderException&)
	{
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::Init()
{
	this->mNumLoops = 0;
	this->mLoopCount = 0;

	this->mCurData = NULL;
	this->mCurDataLen = 0;
	this->mIsPlaying = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSound::WONSound(WONSoundDataBase* theDataSource, int theFrequency, int theFlags, const SoundDescriptor &theDesc) : 
	Sound(theDesc)
{
	this->Init();
	this->mFrequency = theFrequency;
	this->mSoundFlags = theFlags;
	this->mDataSource = theDataSource;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::PlayHook()
{
	if(this->mIsPlaying)
		this->Stop();

	this->RewindHook();
	this->mNumLoops = this->mIsLooping?0:1;
	this->mIsPlaying = true;
	WONSoundThread::AddSound(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::ResumeHook()
{
	this->mIsPlaying = true;
	WONSoundThread::AddSound(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline void ClampAdd(short *theVal, int theAdd)
{
	int aVal = *theVal + theAdd;
	if(aVal>32767)
		aVal = 32767;
	if(aVal<-32768)
		aVal = -32768;
 
	*theVal = aVal;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline void ClampAdd(unsigned char *theVal, int theAdd)
{
	int aVal = *theVal + theAdd;
	if(aVal>255)
		aVal = 255;
	if(aVal<0)
		aVal = 0;

	*theVal = aVal;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::MixPrv(const char *buf1, int len1, int freq1, int flags1,
					char *buf2, int len2, int freq2, int flags2,
					int &numMixed1, int &numMixed2)
{
	int pos1 = 0;
	bool isStereo1 = flags1&WONSoundFlag_IsStereo?true:false;
	bool isSixteenBit1 = flags1&WONSoundFlag_IsSixteenBit?true:false;
	int sampSize1 = isSixteenBit1?2:1;
	if(isStereo1)
		sampSize1 *= 2;


	int pos2 = 0;
	bool isSixteenBit2 = flags2&WONSoundFlag_IsSixteenBit?true:false;
	bool isStereo2 = flags2&WONSoundFlag_IsStereo?true:false;
	int sampSize2 = isSixteenBit2?2:1;
	if(isStereo2)
		sampSize2 *= 2;

	int aLeftSample;
	int aRightSample;
	int aFreqCount = freq2-freq1;

	while(pos1+sampSize1<=len1 && pos2+sampSize2<=len2)
	{
		aFreqCount += freq1;
		if(aFreqCount >= freq2)
		{
			// Get Mix Data
			if(isSixteenBit1)
				aLeftSample = *(short*)(buf1 + pos1);
			else
				aLeftSample = ((*(unsigned char*)(buf1 + pos1) - 128) << 8);

			if(isStereo1)
			{
				if(isSixteenBit1)
					aRightSample = *(short*)(buf1 + pos1 + 2);
				else
					aRightSample = ((*(unsigned char*)(buf1 + pos1 + 1) - 128) << 8);
			}
			else
				aRightSample = aLeftSample;

			if(!isStereo2)
				aLeftSample = (aLeftSample + aRightSample)>>1;

			while(aFreqCount>=freq2 && pos1+sampSize1<=len1)
			{
				aFreqCount -= freq2;
				pos1+=sampSize1;
			}
		}

		// Put Mix Data
		if(isSixteenBit2)
			ClampAdd((short*)(buf2 + pos2),aLeftSample);
		else
			ClampAdd((unsigned char*)(buf2 + pos2),(aLeftSample>>8));

		if(isStereo2)
		{
			if(isSixteenBit2)
				ClampAdd((short*)(buf2 + pos2 + 2),aRightSample);
			else
				ClampAdd((unsigned char*)(buf2 + pos2 + 1),(aRightSample>>8));
		}
		pos2 += sampSize2;
	}

	numMixed1 = pos1;
	numMixed2 = pos2;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::Mix(char *theBuf, int theNumBytes, int theFreq, int theFlags)
{
	while(true)
	{
		if(this->mCurDataLen<=0)
		{
			this->mCurData = this->mDataSource->GetData(this->mCurDataLen);
			if(this->mCurData==NULL || this->mCurDataLen==0) // should we loop?
			{
				if(this->mNumLoops<=0 || ++this->mLoopCount<this->mNumLoops)
				{
					this->RewindPrv(false);
					continue;
				}
				else
				{
					this->mIsPlaying = false;
					break;
				}
			
			}
		}

		int mix1, mix2;
		WONSound::MixPrv(this->mCurData, this->mCurDataLen, this->mFrequency, this->mSoundFlags,theBuf,theNumBytes,theFreq,theFlags,mix1,mix2);		
		this->mCurData += mix1;
		this->mCurDataLen -= mix1;
		if(mix2<theNumBytes)
		{
			theBuf+=mix2;
			theNumBytes-=mix2;
		}
		else
			break;
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::StopHook()
{
	if(this->mIsPlaying)
	{
		this->mIsPlaying = false;
		WONSoundThread::RemoveSound(this);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::RewindPrv(bool resetLoopCount)
{
	this->mCurData = NULL;
	this->mCurDataLen = 0;
	this->mDataSource->Rewind();

	if(resetLoopCount)
		this->mLoopCount = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSound::RewindHook()
{
	this->RewindPrv(true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundChannel::WONSoundChannel()
{
	this->mBufferFree = NULL;
	this->mBufferSilent = NULL;
	this->mSoundBuffer = NULL;

	this->mIsMusic = false;
	this->mStopped = false;
	this->mIsStereo = false;
	this->NUM_BUFFERS = 0;
	this->mBufferDuration = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::SetIsMusic()
{
	this->mIsMusic = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundChannel::~WONSoundChannel()
{
	delete [] this->mBufferFree;
	delete [] this->mBufferSilent;
	
	if(this->mSoundBuffer!=NULL)
	{
		for(int i=0; i<this->NUM_BUFFERS; i++)
			delete [] this->mSoundBuffer[i];

		delete [] this->mSoundBuffer;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::Open()
{

	return this->OpenHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::SetBuffers(int theNumBuffers, int theBufferDuration)
{
	this->NUM_BUFFERS = theNumBuffers;
	this->mBufferDuration = theBufferDuration;

	this->mBufferFree = new bool[this->NUM_BUFFERS]; 
	this->mBufferSilent = new bool[this->NUM_BUFFERS];
	this->mSoundBuffer = new char*[this->NUM_BUFFERS];
	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		this->mBufferFree[i] = true;
		this->mBufferSilent[i] = false;
		this->mSoundBuffer[i] = NULL;
	}

	this->SetBuffersHook();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::SetVolume(unsigned char theVolume)
{
	this->SetVolumeHook(theVolume);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::Stop()
{
	this->mStopped = true;
	this->mSoundSet.clear();
	this->StopHook();

	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		this->mBufferFree[i] = true;
		this->mBufferSilent[i] = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::Reset()
{
	this->CheckFreeBuffers();
	if(this->NeedPlaySilence())
	{
		if(this->AllBuffersFreeOrSilent())
		{
			this->Stop();
			this->mStopped = false; 
			return true;
		}

		for(int i=0; i<this->NUM_BUFFERS; i++)
		{
			if(!this->mBufferFree[i])
				continue;
			if(this->mSoundFlags&WONSoundFlag_IsSixteenBit)
				memset(this->mSoundBuffer[i],0,this->mBufferSize);
			else
				memset(this->mSoundBuffer[i],128,this->mBufferSize);

			this->mBufferFree[i] = false;
			this->mBufferSilent[i] = true;
			this->PlaySoundsHook(i);
		}
	}
	else
	{
		if(this->AllBuffersFree())
		{
			this->mSoundSet.clear();
			this->mStopped = false;
			return true;
		}
	}
		
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::PlaySounds(int theBuf)
{
	// Send the buffer the the WaveOut queue
	SoundSet::iterator anItr = this->mSoundSet.begin();
	if(this->mSoundFlags&WONSoundFlag_IsSixteenBit)
		memset(this->mSoundBuffer[theBuf],0,this->mBufferSize);
	else
		memset(this->mSoundBuffer[theBuf],128,this->mBufferSize);

	bool playing = false;
	while(anItr!=this->mSoundSet.end())
	{
		WONSound *aSound = *anItr;
		if(!this->mStopped && aSound->IsPlaying())
		{
			playing = true;
			aSound->Mix(this->mSoundBuffer[theBuf],this->mBufferSize,this->mFrequency,this->mSoundFlags);
			++anItr;
		}
		else
			this->mSoundSet.erase(anItr++);
	}

	if(playing)
	{
		this->mBufferFree[theBuf] = false;
		this->PlaySoundsHook(theBuf);
	}

	return playing;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::PlaySounds()
{
	this->CheckFreeBuffers();

	bool playing = false;
	bool channelFree = false;
	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		if(!this->mBufferFree[i])
			continue;
		channelFree = true;
		if(this->PlaySounds(i))
			playing = true;
	}

	return playing || !channelFree;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::AddSound(WONSound *theSound)
{
	this->mSoundSet.insert(theSound);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::AllBuffersFree()
{
	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		if(!this->mBufferFree[i])
			return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::AllBuffersFreeOrSilent()
{
	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		if(!this->mBufferFree[i] && !this->mBufferSilent[i])
			return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundChannel::ClearSilentFlags()
{
	for(int i=0; i<this->NUM_BUFFERS; i++)
		this->mBufferSilent[i] = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundChannel::HasSound(WONSound *theSound)
{
	return this->mSoundSet.find(theSound)!=this->mSoundSet.end();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int WONSoundChannel::GetNumSounds()
{
	return this->mSoundSet.size();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundThread::WONSoundThread()
{
	int aNumChannels = gMaxSoundChannels;
	this->mHaveMusicChannel = false;
	this->mNumChannels = 0;
	for(int i=0; i<aNumChannels; i++)
	{
		WONSoundChannelPtr aChannel = WONSoundThread::mChannelFactory();
		if(aChannel->Open())
		{
			this->mNumChannels++;
			if(this->mNumChannels==2) // make the second channel a music channel
			{
				aChannel->SetIsMusic();
		//		aChannel->SetVolume(20);
				this->mHaveMusicChannel = true;
			}

			this->mFreeChannels.insert(aChannel);
		}
		else
			break;
	}


	for(ChannelSet::iterator anItr = this->mFreeChannels.begin(); anItr!=this->mFreeChannels.end(); ++anItr)
	{
		WONSoundChannel *aChannel = *anItr;
		if(this->mNumChannels==1)
		{
			aChannel->SetBuffers(2,200);
			this->SetPriority(ThreadPriority_High);
		}
		else if(aChannel->IsMusic())
			aChannel->SetBuffers(2,500); 
		else
			aChannel->SetBuffers(2,100); 
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSoundThread::~WONSoundThread()
{
	this->Stop();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::PlaySounds()
{
	AutoCrit aCrit(this->mDataCrit);
	ChannelSet::iterator anItr = this->mBusyChannels.begin();
	while(anItr!=this->mBusyChannels.end())
	{
		WONSoundChannel *aChannel = *anItr;

		if(aChannel->PlaySounds())
		{
			++anItr;
		}
		else
		{
			this->mPendingFreeChannels.insert(aChannel);
			this->mBusyChannels.erase(anItr++);
		}
	}

	anItr = this->mPendingFreeChannels.begin();
	while(anItr!=this->mPendingFreeChannels.end())
	{
		WONSoundChannel *aChannel = *anItr;
		if(aChannel->Reset())
		{
			this->mFreeChannels.insert(aChannel);
			this->mPendingFreeChannels.erase(anItr++);
		}
		else
			++anItr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::StopSounds()
{
	AutoCrit aCrit(this->mDataCrit);
	ChannelSet::iterator anItr = this->mBusyChannels.begin();
	while(anItr!=this->mBusyChannels.end())
	{
		WONSoundChannel *aChannel = *anItr;
		aChannel->Stop();
		++anItr;
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::ThreadFunc()
{
	while(!this->mStopped)
	{
		this->mSignalEvent.WaitFor(50);
		if(this->mStopped)
			break;

		this->PlaySounds();
	}

	this->StopSounds();
	while(!this->mBusyChannels.empty())
	{
		this->mSignalEvent.WaitFor(50);
		this->PlaySounds();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONSoundThread::PutSoundInChannel(WONSound *theSound, ChannelSet &theSet)
{
	// Should already be in DataCrit
	bool isMusic = (theSound->mDescriptor.mSoundFlags & 0x0002)!=0;

//	if(isMusic && theSet==mBusyChannels) // can't add music to an already playing channel
//		return false;

	for(ChannelSet::iterator anItr = theSet.begin(); anItr!=theSet.end(); ++anItr)
	{
		WONSoundChannel *aChannel = *anItr;
		if(this->mHaveMusicChannel && aChannel->IsMusic()!=isMusic)
			continue;
		if(theSet!=this->mBusyChannels)
		{
			this->mBusyChannels.insert(aChannel);
			theSet.erase(aChannel);

			aChannel->ClearSilentFlags();
		}
		aChannel->AddSound(theSound);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::AddSoundPrv(WONSound *theSound)
{
	AutoCrit aCrit(this->mDataCrit);
	bool needSignal = false;
	if(this->PutSoundInChannel(theSound,this->mFreeChannels))
	{
		aCrit.Leave();
		this->Signal(); // wake up, we can play it right now
		return;
	}
	
	if(this->PutSoundInChannel(theSound,this->mPendingFreeChannels))
		return;
	
	if(this->PutSoundInChannel(theSound,this->mBusyChannels))
		return;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::RemoveSoundPrv(WONSound *theSound)
{
	AutoCrit aCrit(this->mDataCrit);
	ChannelSet::iterator anItr = this->mBusyChannels.begin();
	while(anItr!=this->mBusyChannels.end())
	{
		WONSoundChannel *aChannel = *anItr;
		if(aChannel->HasSound(theSound))
		{
			if(aChannel->GetNumSounds()==1)
				aChannel->Stop();

			break;
		}
		
		++anItr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::AddSound(WONSound *theSound)
{
	if(gWONSoundThread!=NULL)
		gWONSoundThread->AddSoundPrv(theSound);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::RemoveSound(WONSound *theSound)
{
	if(gWONSoundThread !=NULL)
		gWONSoundThread->RemoveSoundPrv(theSound);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::WakeUp()
{
	if(gWONSoundThread !=NULL)
		gWONSoundThread->Signal();

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::Init(ChannelFactory theFactory, int theMaxChannels)
{
	assert(gWONSoundThread==NULL);

	gMaxSoundChannels = theMaxChannels;
	WONSoundThread::mChannelFactory = theFactory;
	gWONSoundThread = new WONSoundThread;
	gWONSoundThread->SetPriority(ThreadPriority_High);
	gWONSoundThread->Start();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WONSoundThread::Destroy()
{
	if(gWONSoundThread==NULL)
		return;
	gWONSoundThread->Stop(true);
	delete gWONSoundThread;
	gWONSoundThread = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
