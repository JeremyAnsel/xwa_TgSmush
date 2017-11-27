// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#include "common.h"
#include "AudPlay.h"
#include <WindowsX.h>

#pragma comment(lib, "Winmm")
#pragma comment(lib, "Vfw32")

/*--------------------------------------------------------------+
| ****************** AUDIO PLAYING SUPPORT ******************** |
+--------------------------------------------------------------*/

namespace
{
	class CriticalSection
	{
	public:
		CriticalSection()
		{
			InitializeCriticalSection(&this->_critical);
		}

		~CriticalSection()
		{
			DeleteCriticalSection(&this->_critical);
		}

		CRITICAL_SECTION* operator &()
		{
			return &this->_critical;
		}

	private:
		CRITICAL_SECTION _critical;
	};

	CriticalSection g_critical_avi;

	HWAVEOUT	shWaveOut = 0;	/* Current MCI device ID */
	LONG		slBegin;
	LONG		slCurrent;
	LONG		slEnd;
	BOOL		sfPlaying = FALSE;
	BOOL        sfAviAudioIsStopped = TRUE;

#define MAX_AUDIO_BUFFERS	16
#define MIN_AUDIO_BUFFERS	2
#define AUDIO_BUFFER_SIZE	16384

	UINT		swBuffers = 0;	    // total # buffers
	UINT		swBuffersOut;	    // buffers device has
	UINT		swNextBuffer;	    // next buffer to fill
	LPWAVEHDR	salpAudioBuf[MAX_AUDIO_BUFFERS + 2] = { 0 };

	PAVISTREAM	spavi;		    // stream we're playing
	LONG		slSampleSize;	    // size of an audio sample

	LONG		sdwBytesPerSec;
	LONG		sdwSamplesPerSec;

};

/*---------------------------------------------------------------+
| aviaudioCloseDevice -- close the open audio device, if any.    |
+---------------------------------------------------------------*/
void aviaudioCloseDevice(void)
{
	EnterCriticalSection(&g_critical_avi);

	if (shWaveOut != 0)
	{
		MMRESULT w = waveOutReset(shWaveOut);

		while (swBuffers > 0)
		{
			--swBuffers;
			waveOutUnprepareHeader(shWaveOut, salpAudioBuf[swBuffers], sizeof(WAVEHDR));
			//GlobalFreePtr((LPBYTE) salpAudioBuf[swBuffers]);
		}

		w = waveOutClose(shWaveOut);

		shWaveOut = NULL;
	}

	LeaveCriticalSection(&g_critical_avi);
}

/*--------------------------------------------------------------+
| aviaudioOpenDevice -- get ready to play waveform data.	|
+--------------------------------------------------------------*/
BOOL aviaudioOpenDevice(PAVISTREAM pavi)
{
	MMRESULT	w;
	LPVOID		lpFormat;
	LONG		cbFormat = 0;
	AVISTREAMINFO	strhdr;

	if (!pavi)		// no wave data to play
	{
		return FALSE;
	}

	if (shWaveOut)	// already something playing
	{
		return TRUE;
	}

	spavi = pavi;

	if (AVIStreamInfo(pavi, &strhdr, sizeof(strhdr)) != 0)
	{
		return FALSE;
	}

	slSampleSize = (LONG)strhdr.dwSampleSize;

	if (slSampleSize <= 0 || slSampleSize > AUDIO_BUFFER_SIZE)
	{
		return FALSE;
	}

	if (AVIStreamFormatSize(pavi, 0, &cbFormat) != 0)
	{
		return FALSE;
	}

	lpFormat = GlobalAllocPtr(GHND, cbFormat);

	if (!lpFormat)
	{
		return FALSE;
	}

	if (AVIStreamReadFormat(pavi, 0, lpFormat, &cbFormat) != 0)
	{
		return FALSE;
	}

	sdwSamplesPerSec = ((LPWAVEFORMAT)lpFormat)->nSamplesPerSec;
	sdwBytesPerSec = ((LPWAVEFORMAT)lpFormat)->nAvgBytesPerSec;

	w = waveOutOpen(&shWaveOut, WAVE_MAPPER, (LPCWAVEFORMATEX)lpFormat, (DWORD)aviwaveOutProc, 0L, CALLBACK_FUNCTION);

	if (w != MMSYSERR_NOERROR)
	{
		sndPlaySound(NULL, 0);
		w = waveOutOpen(&shWaveOut, WAVE_MAPPER, (LPCWAVEFORMATEX)lpFormat, (DWORD)aviwaveOutProc, 0L, CALLBACK_FUNCTION);
	}

	GlobalFreePtr((LPBYTE)lpFormat);

	if (w != MMSYSERR_NOERROR)
	{
		return FALSE;
	}

	for (swBuffers = 0; swBuffers < MAX_AUDIO_BUFFERS; swBuffers++)
	{
		if (!salpAudioBuf[swBuffers])
		{
			if (!(salpAudioBuf[swBuffers] = (LPWAVEHDR)GlobalAllocPtr(GMEM_MOVEABLE | GMEM_SHARE,
				(DWORD)(sizeof(WAVEHDR) + AUDIO_BUFFER_SIZE))))
				break;
		}

		salpAudioBuf[swBuffers]->dwFlags = WHDR_DONE;
		salpAudioBuf[swBuffers]->lpData = (LPSTR)((LPBYTE)salpAudioBuf[swBuffers] + sizeof(WAVEHDR));
		salpAudioBuf[swBuffers]->dwBufferLength = AUDIO_BUFFER_SIZE;

		if (!waveOutPrepareHeader(shWaveOut, salpAudioBuf[swBuffers], sizeof(WAVEHDR)))
		{
			continue;
		}

		//GlobalFreePtr((LPBYTE) salpAudioBuf[swBuffers]);

		break;
	}

	if (swBuffers < MIN_AUDIO_BUFFERS)
	{
		aviaudioCloseDevice();

		return FALSE;
	}

	EnterCriticalSection(&g_critical_avi);
	swBuffersOut = 0;
	swNextBuffer = 0;

	sfPlaying = FALSE;

	LeaveCriticalSection(&g_critical_avi);

	return TRUE;
}


//
// Return the time in milliseconds corresponding to the currently playing
// audio sample, or -1 if no audio is playing.
// WARNING: Some sound cards are pretty inaccurate!
//
LONG aviaudioTime(void)
{
	MMTIME	mmtime;

	if (!sfPlaying)
	{
		return -1;
	}

	mmtime.wType = TIME_SAMPLES;

	waveOutGetPosition(shWaveOut, &mmtime, sizeof(mmtime));

	if (mmtime.wType == TIME_SAMPLES)
	{
		return AVIStreamSampleToTime(spavi, slBegin) + MulDiv(mmtime.u.sample, 1000, sdwSamplesPerSec);
	}
	else if (mmtime.wType == TIME_BYTES)
	{
		return AVIStreamSampleToTime(spavi, slBegin) + MulDiv(mmtime.u.cb, 1000, sdwBytesPerSec);
	}
	else
	{
		return -1;
	}
}


//
// Fill up any empty audio buffers and ship them out to the device.
//
BOOL aviaudioiFillBuffers(void)
{
	LONG		lRead;
	UINT		w;
	LONG		lSamplesToPlay;

	/*if(sfAviAudioIsStopped)
	{
	LeaveCriticalSection(&g_critical_avi);
	return TRUE;
	}*/

	/* We're not playing, so do nothing. */
	if (!sfPlaying)
	{
		return TRUE;
	}

	while (swBuffersOut < swBuffers)
	{
		if (slCurrent >= slEnd)
		{
			break;
		}

		/* Figure out how much data should go in this buffer */
		lSamplesToPlay = slEnd - slCurrent;

		if (lSamplesToPlay > AUDIO_BUFFER_SIZE / slSampleSize)
			lSamplesToPlay = AUDIO_BUFFER_SIZE / slSampleSize;

		if (AVIStreamRead(spavi, slCurrent, lSamplesToPlay,
			salpAudioBuf[swNextBuffer]->lpData,
			AUDIO_BUFFER_SIZE,
			(LPLONG)&salpAudioBuf[swNextBuffer]->dwBufferLength,
			&lRead) != 0)
		{
			return FALSE;
		}

		if (lRead != lSamplesToPlay)
		{
			return FALSE;
		}

		slCurrent += lRead;

		w = waveOutWrite(shWaveOut, salpAudioBuf[swNextBuffer], sizeof(WAVEHDR));

		if (w != 0)
		{
			return FALSE;
		}

		++swBuffersOut;
		++swNextBuffer;

		if (swNextBuffer >= swBuffers)
			swNextBuffer = 0;
	}

	if (swBuffersOut == 0 && slCurrent >= slEnd)
	{
		//aviaudioStop();
	}

	return TRUE;
}

/*--------------------------------------------------------------+
| aviaudioPlay -- Play audio, starting at a given frame		|
|								|
+--------------------------------------------------------------*/
BOOL aviaudioPlay(PAVISTREAM pavi, LONG lStart, LONG lEnd, BOOL fWait)
{
	if (lStart < 0)
		lStart = AVIStreamStart(pavi);

	if (lEnd < 0)
		lEnd = AVIStreamEnd(pavi);

	if (lStart >= lEnd)
	{
		return FALSE;
	}

	if (!aviaudioOpenDevice(pavi))
	{
		return FALSE;
	}

	//sfAviAudioIsStopped = FALSE;

	if (!sfPlaying)
	{
		//
		// We're beginning play, so pause until we've filled the buffers
		// for a seamless start
		//
		waveOutPause(shWaveOut);

		slBegin = lStart;
		slCurrent = lStart;
		slEnd = lEnd;
		sfPlaying = TRUE;
	}
	else
	{
		slEnd = lEnd;
	}

	aviaudioiFillBuffers();

	//
	// Now unpause the audio and away it goes!
	//
	waveOutRestart(shWaveOut);

	//
	// Caller wants us not to return until play is finished
	//
	EnterCriticalSection(&g_critical_avi);
	sfAviAudioIsStopped = 0;
	LeaveCriticalSection(&g_critical_avi);

	if (fWait)
	{
		while (swBuffersOut > 0)
			Yield();
	}

	return TRUE;
}

/*--------------------------------------------------------------+
| aviaudioMessage -- handle wave messages received by		|
| window controlling audio playback.  When audio buffers are	|
| done, this routine calls aviaudioiFillBuffers to fill them	|
| up again.							|
+--------------------------------------------------------------*/
void aviaudioMessage(unsigned msg, WPARAM wParam, LPARAM lParam)
{
	/*	if (msg == MM_WOM_CLOSE)
	{
	EnterCriticalSection(&g_critical_avi);
	shWaveOut = 0;
	LeaveCriticalSection(&g_critical_avi);
	}*/

	if (!sfAviAudioIsStopped)
	{
		if (msg == MM_WOM_DONE)
		{
			EnterCriticalSection(&g_critical_avi);
			--swBuffersOut;
			LeaveCriticalSection(&g_critical_avi);
			aviaudioiFillBuffers();
		}
	}
}


/*--------------------------------------------------------------+
| aviaudioStop -- stop playing, close the device.				|
+--------------------------------------------------------------*/
void aviaudioStop(void)
{
	//    UINT	w;

	EnterCriticalSection(&g_critical_avi);
	sfAviAudioIsStopped = TRUE;
	LeaveCriticalSection(&g_critical_avi);

	//if(!sfAviAudioIsStopped)
	//{

	if (shWaveOut != 0)
	{
		//w = waveOutReset(shWaveOut);

		EnterCriticalSection(&g_critical_avi);
		sfPlaying = FALSE;
		LeaveCriticalSection(&g_critical_avi);

		aviaudioCloseDevice();
		//sfAviAudioIsStopped = TRUE;
	}
	//}
	//sfAviAudioIsStopped = TRUE;
}


void CALLBACK aviwaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
	DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg != MM_WOM_DONE) return;

	if (!sfAviAudioIsStopped)
	{
		//switch(uMsg)
		//{
		//	case MM_WOM_OPEN:
		//	case MM_WOM_DONE:
		//	case MM_WOM_CLOSE:
		//		{	

		aviaudioMessage(uMsg, dwParam1, dwParam2);
		//		}
		//		break;
		//	}
	}
}
