// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// WinMM (waveOut) sound channel for the WON sound thread (not identified in the original exe)
#include <windows.h>
#include <Mmsystem.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWONSound.h"
#include "ee/wonapi/WONLobbyMSSound.h"
using namespace WONAPI;
// The channel class is declared in this file, as in the SDK; the older MSSound/MSSoundDecoder classes are
// not part of it
class MSSoundChannel : public WONSoundChannel
{
protected:
	HWAVEOUT		mWaveOut;
	WAVEHDR			*mWaveHeader; //[NUM_BUFFERS]

	static void CALLBACK waveOutProc(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2);
	void waveOutProcPrv(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2);
	bool CalcSettings();

	virtual bool OpenHook();
	virtual void SetBuffersHook();
	virtual void StopHook();
	virtual void PlaySoundsHook(int theBuf);
	virtual void SetVolumeHook(unsigned char theVolume);

	MSSoundChannel();
	virtual ~MSSoundChannel();

public:
	static WONSoundChannel* Factory() { return new MSSoundChannel; }
};
typedef SmartPtr<MSSoundChannel> MSSoundChannelPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Creates a channel with no wave device or headers
MSSoundChannel::MSSoundChannel()
{
	this->mWaveHeader = NULL;
	this->mWaveOut = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Destroys the channel: resets and closes the wave device and deletes the header array (with scalar delete)
MSSoundChannel::~MSSoundChannel()
{
	if(this->mWaveOut!=NULL)
	{
		waveOutReset(this->mWaveOut);
		waveOutClose(this->mWaveOut);
	}

	if(this->mWaveHeader!=NULL)
		delete this->mWaveHeader;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// waveOut callback: forwards to the channel passed as instance data
void CALLBACK MSSoundChannel::waveOutProc(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2)
{
	MSSoundChannel *aChannel = reinterpret_cast<MSSoundChannel*>(dwInstance);
	aChannel->waveOutProcPrv(hwo,uMsg,dwInstance,dwParam1,dwParam2);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Marks the finished buffer free and wakes the sound thread when the device reports a buffer done
void MSSoundChannel::waveOutProcPrv(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2)
{
	if(uMsg!=WOM_DONE)
		return;
	for(int i=0; i<this->NUM_BUFFERS; i++)
	{
		if(dwParam1==reinterpret_cast<DWORD>(&this->mWaveHeader[i]))
		{
			this->mBufferFree[i] = true;
			break;
		}
	}

	WONSoundThread::WakeUp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Picks the first format the wave mapper accepts, from 44/22/11 kHz stereo 16-bit down to mono 8-bit;
// the byte rate and block align ignore the channel count
bool MSSoundChannel::CalcSettings()
{
	WAVEOUTCAPS aCaps;
	MMRESULT aResult = waveOutGetDevCaps(WAVE_MAPPER,&aCaps,sizeof(aCaps));
	if(aResult!=MMSYSERR_NOERROR)
		return false;

	
/*	int flags[12] = { WAVE_FORMAT_4S16, WAVE_FORMAT_4S08, WAVE_FORMAT_4M16, WAVE_FORMAT_4M08, 
					WAVE_FORMAT_2S16, WAVE_FORMAT_2S08, WAVE_FORMAT_2M16, WAVE_FORMAT_2M08,
					WAVE_FORMAT_1S16, WAVE_FORMAT_1S08, WAVE_FORMAT_1M16, WAVE_FORMAT_1M08 };*/

	int frequency[12] = {	44100, 22050, 11025, 44100,
							22050, 11025, 44100, 22050,
							11025, 44100, 22050, 11025 };

	bool stereo[12] = {	true, true, true, true,
						true, true, false, false,
						false, false, false, false };

	int numbytespersample[12] = {	2, 2, 2, 1,
									1, 1, 2, 2,
									2, 1, 1, 1 };

	for(int i=0; i<12; i++)
	{
		WAVEFORMATEX	wfx;
		wfx.wFormatTag = 1;		// PCM standard
		wfx.nChannels = stereo[i]?2:1;		
		wfx.nSamplesPerSec = frequency[i];
		wfx.nAvgBytesPerSec = frequency[i]*numbytespersample[i];
		wfx.nBlockAlign = numbytespersample[i];
		wfx.wBitsPerSample = numbytespersample[i]*8;
		wfx.cbSize = sizeof(wfx);

		aResult = waveOutOpen(NULL,WAVE_MAPPER,&wfx,0,0,WAVE_FORMAT_QUERY);
		if(aResult!=MMSYSERR_NOERROR)
			continue;
		this->mFrequency = frequency[i];
		this->mIsStereo = stereo[i];
		this->mNumBytesPerSample = numbytespersample[i];

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Opens the wave mapper in the chosen format with the callback; false when no format works or open fails
bool MSSoundChannel::OpenHook()
{
	MMRESULT aResult;

	if(!this->CalcSettings())
		return false;

	this->mSoundFlags = 0;
	if(this->mIsStereo)
		this->mSoundFlags |= WONSoundFlag_IsStereo;
	if(this->mNumBytesPerSample==2)
		this->mSoundFlags |= WONSoundFlag_IsSixteenBit;

	WAVEFORMATEX	wfx;
	wfx.wFormatTag = 1;		// PCM standard
	wfx.nChannels = this->mIsStereo?2:1;		
	wfx.nSamplesPerSec = this->mFrequency;
	wfx.nAvgBytesPerSec = this->mFrequency*this->mNumBytesPerSample;
	wfx.nBlockAlign = this->mNumBytesPerSample;
	wfx.wBitsPerSample = this->mNumBytesPerSample*8;
	wfx.cbSize = sizeof(wfx);
	aResult = waveOutOpen(			&this->mWaveOut,
									WAVE_MAPPER,
									&wfx,
									(DWORD)MSSoundChannel::waveOutProc,
									reinterpret_cast<DWORD>(this),					// User data.
									(DWORD)CALLBACK_FUNCTION);
//									(DWORD)NULL,					// User data.
//									(DWORD)CALLBACK_NULL);

	if (aResult!= MMSYSERR_NOERROR) 
		return false;


	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Allocates the wave headers and sample buffers (buffer duration in ms) and prepares each header
void MSSoundChannel::SetBuffersHook()
{
	this->mWaveHeader = new WAVEHDR[this->NUM_BUFFERS];

	this->mBufferSize = ((this->mBufferDuration * this->mFrequency) / 1000) * this->mNumBytesPerSample;
	
	if(this->mIsStereo)
		this->mBufferSize*=2;


	// Alloc the sample buffers.
	for (int i=0; i<this->NUM_BUFFERS; i++)
	{
		this->mSoundBuffer[i] = new char[this->mBufferSize];
		this->mWaveHeader[i].lpData = this->mSoundBuffer[i];
		this->mWaveHeader[i].dwBufferLength = this->mBufferSize;
		this->mWaveHeader[i].dwFlags = 0;
		this->mWaveHeader[i].dwLoops = 0;

		waveOutPrepareHeader(this->mWaveOut,&this->mWaveHeader[i],sizeof(WAVEHDR));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the device volume on both channels from the 0-255 volume
void MSSoundChannel::SetVolumeHook(unsigned char theVolume)
{
	if(this->mWaveOut!=NULL)
	{
		unsigned long aVolume = theVolume<<8 | theVolume<<24;
		waveOutSetVolume(this->mWaveOut,aVolume);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Stops playback, returning every queued buffer
void MSSoundChannel::StopHook()
{
	if(this->mWaveOut!=NULL)
		waveOutReset(this->mWaveOut);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Re-prepares the buffer's header and queues it on the wave device
void MSSoundChannel::PlaySoundsHook(int theBuf)
{
	if (this->mWaveHeader[theBuf].dwFlags&WHDR_PREPARED)
		waveOutUnprepareHeader(this->mWaveOut,&this->mWaveHeader[theBuf],sizeof(WAVEHDR));

	this->mWaveHeader[theBuf].lpData = this->mSoundBuffer[theBuf];
	this->mWaveHeader[theBuf].dwBufferLength = this->mBufferSize;
	waveOutPrepareHeader(this->mWaveOut,&this->mWaveHeader[theBuf],sizeof(WAVEHDR));

	waveOutWrite(this->mWaveOut,&this->mWaveHeader[theBuf],sizeof(WAVEHDR));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Starts the sound thread with WinMM channels
void WONAPI::WONSound_InitMS()
{
	WONSoundThread::Init(MSSoundChannel::Factory);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Stops the sound thread
void WONAPI::WONSound_DestroyMS()
{
	WONSoundThread::Destroy();
}

