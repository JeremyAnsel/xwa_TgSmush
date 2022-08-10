// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#include "targetver.h"
#include "SharedMem.h"
#include "mfplayer.player.h"
#include "config.h"
#include <vector>
//#include <assert.h>

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "Mf")
#pragma comment(lib, "Mfplat")
#pragma comment(lib, "Mfuuid")
#pragma comment(lib, "strmiids")

static int g_videoFrameIndex = -1;
static int g_videoWidth = 0;
static int g_videoHeight = 0;

class SampleGrabberCB : public IMFSampleGrabberSinkCallback
{
private:
	long m_cRef;

	SampleGrabberCB() : m_cRef(1) {}

public:
	static HRESULT CreateInstance(SampleGrabberCB** ppCB);

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFClockStateSink methods
	STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
	STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
	STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
	STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
	STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

	// IMFSampleGrabberSinkCallback methods
	STDMETHODIMP OnSetPresentationClock(IMFPresentationClock* pClock);
	STDMETHODIMP OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
		LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE* pSampleBuffer,
		DWORD dwSampleSize);
	STDMETHODIMP OnShutdown();
};

HRESULT SampleGrabberCB::CreateInstance(SampleGrabberCB** ppCB)
{
	*ppCB = new (std::nothrow) SampleGrabberCB();

	if (ppCB == NULL)
	{
		return E_OUTOFMEMORY;
	}
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(SampleGrabberCB, IMFSampleGrabberSinkCallback),
		QITABENT(SampleGrabberCB, IMFClockStateSink),
		{ 0 }
	};
	return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) SampleGrabberCB::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) SampleGrabberCB::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);

	if (cRef == 0)
	{
		delete this;
	}

	return cRef;
}

STDMETHODIMP SampleGrabberCB::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockStop(MFTIME hnsSystemTime)
{
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockPause(MFTIME hnsSystemTime)
{
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockRestart(MFTIME hnsSystemTime)
{
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnSetPresentationClock(IMFPresentationClock* pClock)
{
	return S_OK;
}

unsigned char clip(int v)
{
	if (v < 0)
	{
		v = 0;
	}
	else if (v > 255)
	{
		v = 255;
	}

	return (unsigned char)v;
}

STDMETHODIMP SampleGrabberCB::OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
	LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE* pSampleBuffer,
	DWORD dwSampleSize)
{
	static std::vector<char> s_colors;

	if (guidMajorMediaType != GUID_NULL && guidMajorMediaType != MFMediaType_Video)
	{
		return S_OK;
	}

	g_videoFrameIndex++;

	// Display information about the sample.
	//OutputDebugString((L"Sample: start = " + std::to_wstring(llSampleTime) + L", duration = " + std::to_wstring(llSampleDuration) + L", bytes = " + std::to_wstring(dwSampleSize)).c_str());

	int length = dwSampleSize / 2;

	if (s_colors.capacity() < length * 4)
	{
		s_colors.reserve(length * 4);
	}

	unsigned char* ptrIn = (unsigned char*)pSampleBuffer;
	unsigned char* ptrOut = (unsigned char*)s_colors.data();

	for (int i = 0; i < length / 2; i++)
	{
		int y0 = ptrIn[0];
		int u0 = ptrIn[1];
		int y1 = ptrIn[2];
		int v0 = ptrIn[3];
		ptrIn += 4;
		int c = y0 - 16;
		int d = u0 - 128;
		int e = v0 - 128;
		ptrOut[0] = clip((298 * c + 516 * d + 128) >> 8); // blue
		ptrOut[1] = clip((298 * c - 100 * d - 208 * e + 128) >> 8); // green
		ptrOut[2] = clip((298 * c + 409 * e + 128) >> 8); // red
		ptrOut[3] = 255;
		c = y1 - 16;
		ptrOut[4] = clip((298 * c + 516 * d + 128) >> 8); // blue
		ptrOut[5] = clip((298 * c - 100 * d - 208 * e + 128) >> 8); // green
		ptrOut[6] = clip((298 * c + 409 * e + 128) >> 8); // red
		ptrOut[7] = 255;
		ptrOut += 8;
	}

	auto sharedMem = GetTgSmushVideoSharedMem();

	if (sharedMem)
	{
		sharedMem->videoFrameIndex = g_videoFrameIndex;
		sharedMem->videoFrameWidth = g_videoWidth;
		sharedMem->videoFrameHeight = g_videoHeight;
		sharedMem->videoDataLength = length * 4;
		sharedMem->videoDataPtr = s_colors.data();
	}

	return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnShutdown()
{
	return S_OK;
}

template <class Q>
HRESULT GetEventObject(IMFMediaEvent* pEvent, Q** ppObject)
{
	*ppObject = NULL;   // zero output

	PROPVARIANT var;
	HRESULT hr = pEvent->GetValue(&var);
	if (SUCCEEDED(hr))
	{
		if (var.vt == VT_UNKNOWN)
		{
			hr = var.punkVal->QueryInterface(ppObject);
		}
		else
		{
			hr = MF_E_INVALIDTYPE;
		}
		PropVariantClear(&var);
	}
	return hr;
}

HRESULT CreateMediaSource(PCWSTR pszURL, IMFMediaSource** ppSource);

HRESULT CreatePlaybackTopology(IMFMediaSource* pSource,
	IMFPresentationDescriptor* pPD, HWND hVideoWnd, IMFTopology** ppTopology);

//  Static class method to create the MFPlayer object.

HRESULT MFPlayer::CreateInstance(
	HWND hVideo,                  // Video window.
	HWND hEvent,                  // Window to receive notifications.
	MFPlayer** ppPlayer)           // Receives a pointer to the MFPlayer object.
{
	if (ppPlayer == NULL)
	{
		return E_POINTER;
	}

	MFPlayer* pPlayer = new (std::nothrow) MFPlayer(hVideo, hEvent);
	if (pPlayer == NULL)
	{
		return E_OUTOFMEMORY;
	}

	HRESULT hr = pPlayer->Initialize();
	if (SUCCEEDED(hr))
	{
		*ppPlayer = pPlayer;
	}
	else
	{
		pPlayer->Release();
	}
	return hr;
}

HRESULT MFPlayer::Initialize()
{
	// Start up Media Foundation platform.
	HRESULT hr = MFStartup(MF_VERSION);
	if (SUCCEEDED(hr))
	{
		m_hCloseEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (m_hCloseEvent == NULL)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	return hr;
}

MFPlayer::MFPlayer(HWND hVideo, HWND hEvent) :
	m_pSession(NULL),
	m_pSource(NULL),
	m_pVideoDisplay(NULL),
	m_hwndVideo(hVideo),
	m_hwndEvent(hEvent),
	m_state(Closed),
	m_hCloseEvent(NULL),
	m_nRefCount(1)
{
}

MFPlayer::~MFPlayer()
{
	_ASSERTE(m_pSession == NULL);
	// If FALSE, the app did not call Shutdown().

	// When MFPlayer calls IMediaEventGenerator::BeginGetEvent on the
	// media session, it causes the media session to hold a reference 
	// count on the MFPlayer. 

	// This creates a circular reference count between MFPlayer and the 
	// media session. Calling Shutdown breaks the circular reference 
	// count.

	// If CreateInstance fails, the application will not call 
	// Shutdown. To handle that case, call Shutdown in the destructor. 

	Shutdown();
}

// IUnknown methods

HRESULT MFPlayer::QueryInterface(REFIID riid, void** ppv)
{
#pragma warning(disable: 4838)
	static const QITAB qit[] =
	{
		QITABENT(MFPlayer, IMFAsyncCallback),
		{ 0 }
	};
#pragma warning(default: 4838)

	return QISearch(this, qit, riid, ppv);
}

ULONG MFPlayer::AddRef()
{
	return InterlockedIncrement(&m_nRefCount);
}

ULONG MFPlayer::Release()
{
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0)
	{
		delete this;
	}
	return uCount;
}

//  Open a URL for playback.
HRESULT MFPlayer::OpenURL(const WCHAR* sURL)
{
	// 1. Create a new media session.
	// 2. Create the media source.
	// 3. Create the topology.
	// 4. Queue the topology [asynchronous]
	// 5. Start playback [asynchronous - does not happen in this method.]

	IMFTopology* pTopology = NULL;
	IMFPresentationDescriptor* pSourcePD = NULL;

	// Create the media session.
	HRESULT hr = CreateSession();
	if (FAILED(hr))
	{
		goto done;
	}

	// Create the media source.
	hr = CreateMediaSource(sURL, &m_pSource);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create the presentation descriptor for the media source.
	hr = m_pSource->CreatePresentationDescriptor(&pSourcePD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create a partial topology.
	hr = CreatePlaybackTopology(m_pSource, pSourcePD, m_hwndVideo, &pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the topology on the media session.
	hr = m_pSession->SetTopology(0, pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = OpenPending;

	// If SetTopology succeeds, the media session will queue an 
	// MESessionTopologySet event.

done:
	if (FAILED(hr))
	{
		m_state = Closed;
	}

	SafeRelease(&pSourcePD);
	SafeRelease(&pTopology);

	return hr;
}

//  Pause playback.
HRESULT MFPlayer::Pause()
{
	if (m_state != Started)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL || m_pSource == NULL)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = m_pSession->Pause();
	if (SUCCEEDED(hr))
	{
		m_state = Paused;
	}

	return hr;
}

// Stop playback.
HRESULT MFPlayer::Stop()
{
	if (m_state != Started && m_state != Paused)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = m_pSession->Stop();
	if (SUCCEEDED(hr))
	{
		m_state = Stopped;
	}
	return hr;
}

//  Repaint the video window. Call this method on WM_PAINT.

HRESULT MFPlayer::Repaint()
{
	if (m_pVideoDisplay)
	{
		return m_pVideoDisplay->RepaintVideo();
	}
	else
	{
		return S_OK;
	}
}

//  Resize the video rectangle.
//
//  Call this method if the size of the video window changes.

HRESULT MFPlayer::ResizeVideo(WORD width, WORD height)
{
	if (m_pVideoDisplay)
	{
		// Set the destination rectangle.
		// Leave the default source rectangle (0,0,1,1).

		RECT rcDest = { 0, 0, width, height };

		return m_pVideoDisplay->SetVideoPosition(NULL, &rcDest);
	}
	else
	{
		return S_OK;
	}
}

//  Callback for the asynchronous BeginGetEvent method.

HRESULT MFPlayer::Invoke(IMFAsyncResult* pResult)
{
	MediaEventType meType = MEUnknown;  // Event type

	IMFMediaEvent* pEvent = NULL;

	// Get the event from the event queue.
	HRESULT hr = m_pSession->EndGetEvent(pResult, &pEvent);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the event type. 
	hr = pEvent->GetType(&meType);
	if (FAILED(hr))
	{
		goto done;
	}

	if (meType == MESessionClosed)
	{
		// The session was closed. 
		// The application is waiting on the m_hCloseEvent event handle. 
		SetEvent(m_hCloseEvent);
	}
	else
	{
		// For all other events, get the next event in the queue.
		hr = m_pSession->BeginGetEvent(this, NULL);
		if (FAILED(hr))
		{
			goto done;
		}
	}

	// Check the application state. 

	// If a call to IMFMediaSession::Close is pending, it means the 
	// application is waiting on the m_hCloseEvent event and
	// the application's message loop is blocked. 

	// Otherwise, post a private window message to the application. 

	if (m_state != Closing)
	{
		// Leave a reference count on the event.
		pEvent->AddRef();

		PostMessage(m_hwndEvent, WM_APP_PLAYER_EVENT,
			(WPARAM)pEvent, (LPARAM)meType);
	}

done:
	SafeRelease(&pEvent);
	return S_OK;
}

HRESULT MFPlayer::HandleEvent(UINT_PTR pEventPtr)
{
	HRESULT hrStatus = S_OK;
	MediaEventType meType = MEUnknown;

	IMFMediaEvent* pEvent = (IMFMediaEvent*)pEventPtr;

	if (pEvent == NULL)
	{
		return E_POINTER;
	}

	// Get the event type.
	HRESULT hr = pEvent->GetType(&meType);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the event status. If the operation that triggered the event 
	// did not succeed, the status is a failure code.
	hr = pEvent->GetStatus(&hrStatus);

	// Check if the async operation succeeded.
	if (SUCCEEDED(hr) && FAILED(hrStatus))
	{
		hr = hrStatus;
	}

	if (FAILED(hr))
	{
		goto done;
	}

	switch (meType)
	{
	case MESessionTopologyStatus:
		hr = OnTopologyStatus(pEvent);
		break;

	case MEEndOfPresentation:
		hr = OnPresentationEnded(pEvent);
		break;

	case MENewPresentation:
		hr = OnNewPresentation(pEvent);
		break;

	default:
		hr = OnSessionEvent(pEvent, meType);
		break;
	}

done:
	SafeRelease(&pEvent);

	return hr;
}

//  Release all resources held by this object.
HRESULT MFPlayer::Shutdown()
{
	// Close the session
	HRESULT hr = CloseSession();

	// Shutdown the Media Foundation platform
	MFShutdown();

	if (m_hCloseEvent)
	{
		CloseHandle(m_hCloseEvent);
		m_hCloseEvent = NULL;
	}

	return hr;
}

/// Protected methods

HRESULT MFPlayer::OnTopologyStatus(IMFMediaEvent* pEvent)
{
	UINT32 status;

	HRESULT hr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status);
	if (SUCCEEDED(hr) && (status == MF_TOPOSTATUS_READY))
	{
		SafeRelease(&m_pVideoDisplay);

		// Get the IMFVideoDisplayControl interface from EVR. This call is
		// expected to fail if the media file does not have a video stream.

		(void)MFGetService(m_pSession, MR_VIDEO_RENDER_SERVICE,
			IID_PPV_ARGS(&m_pVideoDisplay));

		if (m_pVideoDisplay)
		{
			m_pVideoDisplay->SetAspectRatioMode(g_config.AspectRatioPreserved ? MFVideoARMode_PreservePicture : MFVideoARMode_None);
		}

		hr = StartPlayback();
	}

	return hr;
}


//  Handler for MEEndOfPresentation event.
HRESULT MFPlayer::OnPresentationEnded(IMFMediaEvent* pEvent)
{
	// The session puts itself into the stopped state automatically.
	m_state = Stopped;
	return S_OK;
}

//  Handler for MENewPresentation event.
//
//  This event is sent if the media source has a new presentation, which 
//  requires a new topology. 

HRESULT MFPlayer::OnNewPresentation(IMFMediaEvent* pEvent)
{
	IMFPresentationDescriptor* pPD = NULL;
	IMFTopology* pTopology = NULL;

	// Get the presentation descriptor from the event.
	HRESULT hr = GetEventObject(pEvent, &pPD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create a partial topology.
	hr = CreatePlaybackTopology(m_pSource, pPD, m_hwndVideo, &pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the topology on the media session.
	hr = m_pSession->SetTopology(0, pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = OpenPending;

done:
	SafeRelease(&pTopology);
	SafeRelease(&pPD);

	return S_OK;
}

//  Create a new instance of the media session.
HRESULT MFPlayer::CreateSession()
{
	// Close the old session, if any.
	HRESULT hr = CloseSession();
	if (FAILED(hr))
	{
		goto done;
	}

	_ASSERTE(m_state == Closed);

	// Create the media session.
	hr = MFCreateMediaSession(NULL, &m_pSession);
	if (FAILED(hr))
	{
		goto done;
	}

	// Start pulling events from the media session
	hr = m_pSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = Ready;

done:
	return hr;
}

//  Close the media session. 
HRESULT MFPlayer::CloseSession()
{
	//  The IMFMediaSession::Close method is asynchronous, but the 
	//  MFPlayer::CloseSession method waits on the MESessionClosed event.
	//  
	//  MESessionClosed is guaranteed to be the last event that the 
	//  media session fires.

	HRESULT hr = S_OK;

	SafeRelease(&m_pVideoDisplay);

	// First close the media session.
	if (m_pSession)
	{
		DWORD dwWaitResult = 0;

		m_state = Closing;

		hr = m_pSession->Close();
		// Wait for the close operation to complete
		if (SUCCEEDED(hr))
		{
			dwWaitResult = WaitForSingleObject(m_hCloseEvent, 5000);
			if (dwWaitResult == WAIT_TIMEOUT)
			{
				_ASSERTE(FALSE);
			}
			// Now there will be no more events from this session.
		}
	}

	// Complete shutdown operations.
	if (SUCCEEDED(hr))
	{
		// Shut down the media source. (Synchronous operation, no events.)
		if (m_pSource)
		{
			(void)m_pSource->Shutdown();
		}
		// Shut down the media session. (Synchronous operation, no events.)
		if (m_pSession)
		{
			(void)m_pSession->Shutdown();
		}
	}

	SafeRelease(&m_pSource);
	SafeRelease(&m_pSession);
	m_state = Closed;
	return hr;
}

//  Start playback from the current position. 
HRESULT MFPlayer::StartPlayback()
{
	_ASSERTE(m_pSession != NULL);

	PROPVARIANT varStart;
	PropVariantInit(&varStart);

	HRESULT hr = m_pSession->Start(&GUID_NULL, &varStart);
	if (SUCCEEDED(hr))
	{
		// Note: Start is an asynchronous operation. However, we
		// can treat our state as being already started. If Start
		// fails later, we'll get an MESessionStarted event with
		// an error code, and we will update our state then.
		m_state = Started;
	}
	PropVariantClear(&varStart);
	return hr;
}

//  Start playback from paused or stopped.
HRESULT MFPlayer::Play()
{
	if (m_state != Paused && m_state != Stopped)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL || m_pSource == NULL)
	{
		return E_UNEXPECTED;
	}
	return StartPlayback();
}


//  Create a media source from a URL.
HRESULT CreateMediaSource(PCWSTR sURL, IMFMediaSource** ppSource)
{
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

	IMFSourceResolver* pSourceResolver = NULL;
	IUnknown* pSource = NULL;

	// Create the source resolver.
	HRESULT hr = MFCreateSourceResolver(&pSourceResolver);
	if (FAILED(hr))
	{
		goto done;
	}

	// Use the source resolver to create the media source.

	// Note: For simplicity this sample uses the synchronous method to create 
	// the media source. However, creating a media source can take a noticeable
	// amount of time, especially for a network source. For a more responsive 
	// UI, use the asynchronous BeginCreateObjectFromURL method.

	hr = pSourceResolver->CreateObjectFromURL(
		sURL,                       // URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,        // Receives the created object type. 
		&pSource            // Receives a pointer to the media source.
	);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the IMFMediaSource interface from the media source.
	hr = pSource->QueryInterface(IID_PPV_ARGS(ppSource));

done:
	SafeRelease(&pSourceResolver);
	SafeRelease(&pSource);
	return hr;
}

HRESULT GetDescriptorMajorType(IMFStreamDescriptor* descriptor, GUID* pMajorType)
{
	HRESULT hr = S_OK;
	IMFMediaTypeHandler* mediaTypeHandler = nullptr;
	GUID majorType{};

	if (SUCCEEDED(hr))
	{
		hr = descriptor->GetMediaTypeHandler(&mediaTypeHandler);
	}

	if (SUCCEEDED(hr))
	{
		hr = mediaTypeHandler->GetMajorType(&majorType);
	}

	if (SUCCEEDED(hr))
	{
		*pMajorType = majorType;
	}

	return hr;
}

HRESULT GetAudioStreamDescriptor(IMFPresentationDescriptor* sourcePD, int sourceStreamCount, IMFStreamDescriptor** pDescriptor)
{
	HRESULT hr = S_OK;
	int streamIndex = -1;

	for (int i = 0; i < sourceStreamCount; i++)
	{
		IMFStreamDescriptor* descriptor = nullptr;
		GUID majorType{};

		if (SUCCEEDED(hr))
		{
			BOOL isSelected = FALSE;
			hr = sourcePD->GetStreamDescriptorByIndex(i, &isSelected, &descriptor);
		}

		if (SUCCEEDED(hr))
		{
			hr = GetDescriptorMajorType(descriptor, &majorType);
		}

		SafeRelease(&descriptor);

		if (majorType != MFMediaType_Audio)
		{
			continue;
		}

		if (SUCCEEDED(hr))
		{
			streamIndex = i;
			break;
		}
	}

	if (streamIndex != -1)
	{
		IMFStreamDescriptor* streamDescriptor = nullptr;
		BOOL streamIsSelected = FALSE;
		hr = sourcePD->GetStreamDescriptorByIndex(streamIndex, &streamIsSelected, &streamDescriptor);

		if (SUCCEEDED(hr))
		{
			*pDescriptor = streamDescriptor;
		}
	}

	return hr;
}

HRESULT GetVideoStreamDescriptor(IMFPresentationDescriptor* sourcePD, int sourceStreamCount, IMFStreamDescriptor** pDescriptor)
{
	HRESULT hr = S_OK;
	int streamIndex = -1;

	for (int i = 0; i < sourceStreamCount; i++)
	{
		IMFStreamDescriptor* descriptor = nullptr;
		GUID majorType{};

		if (SUCCEEDED(hr))
		{
			BOOL isSelected = FALSE;
			hr = sourcePD->GetStreamDescriptorByIndex(i, &isSelected, &descriptor);
		}

		if (SUCCEEDED(hr))
		{
			hr = GetDescriptorMajorType(descriptor, &majorType);
		}

		SafeRelease(&descriptor);

		if (majorType != MFMediaType_Video)
		{
			continue;
		}

		if (SUCCEEDED(hr))
		{
			streamIndex = i;
			break;
		}
	}

	if (streamIndex != -1)
	{
		IMFStreamDescriptor* streamDescriptor = nullptr;
		BOOL streamIsSelected = FALSE;
		hr = sourcePD->GetStreamDescriptorByIndex(streamIndex, &streamIsSelected, &streamDescriptor);

		if (SUCCEEDED(hr))
		{
			*pDescriptor = streamDescriptor;
		}
	}

	return hr;
}

HRESULT CreateMediaSinkActivate(
	IMFStreamDescriptor* pSourceSD,     // Pointer to the stream descriptor.
	HWND hVideoWindow,                  // Handle to the video clipping window.
	IMFActivate** ppActivate
)
{
	HRESULT hr = S_OK;
	IMFMediaTypeHandler* pHandler = NULL;
	IMFActivate* pActivate = NULL;

	// Get the media type handler for the stream.

	if (SUCCEEDED(hr))
	{
		hr = pSourceSD->GetMediaTypeHandler(&pHandler);
	}

	GUID guidMajorType;

	if (SUCCEEDED(hr))
	{
		hr = pHandler->GetMajorType(&guidMajorType);
	}

	// Create an IMFActivate object for the renderer, based on the media type.
	if (MFMediaType_Audio == guidMajorType)
	{
		if (SUCCEEDED(hr))
		{
			hr = MFCreateAudioRendererActivate(&pActivate);
		}
	}
	else if (MFMediaType_Video == guidMajorType)
	{
		if (SUCCEEDED(hr))
		{
			hr = MFCreateVideoRendererActivate(hVideoWindow, &pActivate);
		}
	}
	else
	{
		if (SUCCEEDED(hr))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		pActivate->AddRef();
		*ppActivate = pActivate;
	}

	SafeRelease(&pHandler);
	SafeRelease(&pActivate);
	return hr;
}

HRESULT CreateMediaSinkActivateCallback(
	IMFStreamDescriptor* pSourceSD,     // Pointer to the stream descriptor.
	HWND hVideoWindow,                  // Handle to the video clipping window.
	IMFActivate** ppActivate)
{
	HRESULT hr = S_OK;
	IMFMediaTypeHandler* pHandler = NULL;
	IMFActivate* pActivate = NULL;

	if (SUCCEEDED(hr))
	{
		hr = pSourceSD->GetMediaTypeHandler(&pHandler);
	}

	// Get the major media type.
	GUID guidMajorType;

	if (SUCCEEDED(hr))
	{
		hr = pHandler->GetMajorType(&guidMajorType);
	}

	if (SUCCEEDED(hr))
	{
		if (MFMediaType_Video == guidMajorType)
		{
			IMFMediaType* pType = nullptr;
			IMFMediaType* pVideoOutType = nullptr;
			SampleGrabberCB* pCallback = nullptr;

			pHandler->GetCurrentMediaType(&pType);

			hr = MFCreateMediaType(&pVideoOutType);
			pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
			pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive);
			pVideoOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
			UINT32 numerator;
			UINT32 denominator;
			MFGetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, &numerator, &denominator);
			MFSetAttributeRatio(pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO, numerator, denominator);
			UINT32 width;
			UINT32 height;
			MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
			MFSetAttributeSize(pVideoOutType, MF_MT_FRAME_SIZE, width, height);

			g_videoFrameIndex = -1;
			g_videoWidth = width;
			g_videoHeight = height;

			if (SUCCEEDED(hr))
			{
				hr = SampleGrabberCB::CreateInstance(&pCallback);
			}

			if (SUCCEEDED(hr))
			{
				hr = MFCreateSampleGrabberSinkActivate(pVideoOutType, pCallback, &pActivate);
			}

			if (SUCCEEDED(hr))
			{
				hr = pActivate->SetUINT32(MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, TRUE);
			}

			SafeRelease(&pVideoOutType);
			SafeRelease(&pType);
			SafeRelease(&pCallback);
		}
		else
		{
			// Unknown stream type.
			hr = E_FAIL;
			// Optionally, you could deselect this stream instead of failing.
		}
	}

	if (SUCCEEDED(hr))
	{
		if (pActivate)
		{
			pActivate->AddRef();
		}

		*ppActivate = pActivate;
	}

	SafeRelease(&pHandler);
	SafeRelease(&pActivate);

	return hr;
}

// Add a source node to a topology.
HRESULT AddSourceNode(
	IMFTopology* pTopology,           // Topology.
	IMFMediaSource* pSource,          // Media source.
	IMFPresentationDescriptor* pPD,   // Presentation descriptor.
	IMFStreamDescriptor* pSD,         // Stream descriptor.
	IMFTopologyNode** ppNode)         // Receives the node pointer.
{
	IMFTopologyNode* pNode = NULL;

	// Create the node.
	HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the attributes.
	hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode(pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SafeRelease(&pNode);
	return hr;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
	IMFTopology* pTopology,     // Topology.
	IMFActivate* pActivate,     // Media sink activation object.
	DWORD dwId,                 // Identifier of the stream sink.
	IMFTopologyNode** ppNode)   // Receives the node pointer.
{
	IMFTopologyNode* pNode = NULL;

	// Create the node.
	HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the object pointer.
	hr = pNode->SetObject(pActivate);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the stream sink ID attribute.
	hr = pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
	if (FAILED(hr))
	{
		goto done;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode(pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SafeRelease(&pNode);
	return hr;
}

//  Add a topology branch for one stream.
//
//  For each stream, this function does the following:
//
//    1. Creates a source node associated with the stream.
//    2. Creates an output node for the renderer.
//    3. Connects the two nodes.
//
//  The media session will add any decoders that are needed.

//  Create a playback topology from a media source.
HRESULT CreatePlaybackTopology(
	IMFMediaSource* pSource,          // Media source.
	IMFPresentationDescriptor* pPD,   // Presentation descriptor.
	HWND hVideoWnd,                   // Video window.
	IMFTopology** ppTopology)         // Receives a pointer to the topology.
{
	HRESULT hr = S_OK;
	IMFTopology* pTopology = NULL;
	IMFStreamDescriptor* pAudioSD = NULL;
	IMFStreamDescriptor* pVideoSD = NULL;
	IMFActivate* pSinkActivate = NULL;
	IMFTopologyNode* pSourceNode = NULL;
	IMFTopologyNode* pOutputNode = NULL;

	IMFActivate* pCallbackSinkActivate = NULL;
	IMFTopologyNode* pCallbackOutputNode = NULL;
	IMFTopologyNode* pTeeOutputNode = NULL;
	IMFTopologyNode* pTransformNode = NULL;

	DWORD cSourceStreams = 0;

	if (SUCCEEDED(hr))
	{
		hr = MFCreateTopology(&pTopology);
	}

	if (SUCCEEDED(hr))
	{
		hr = pPD->GetStreamDescriptorCount(&cSourceStreams);
	}

	{
		if (SUCCEEDED(hr))
		{
			hr = GetAudioStreamDescriptor(pPD, cSourceStreams, &pAudioSD);
		}

		if (SUCCEEDED(hr))
		{
			hr = CreateMediaSinkActivate(pAudioSD, hVideoWnd, &pSinkActivate);
		}

		if (SUCCEEDED(hr))
		{
			hr = AddSourceNode(pTopology, pSource, pPD, pAudioSD, &pSourceNode);
		}

		if (SUCCEEDED(hr))
		{
			hr = AddOutputNode(pTopology, pSinkActivate, 0, &pOutputNode);
		}

		if (SUCCEEDED(hr))
		{
			hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
		}

		SafeRelease(&pAudioSD);
		SafeRelease(&pSinkActivate);
		SafeRelease(&pSourceNode);
		SafeRelease(&pOutputNode);
	}

	{
		if (SUCCEEDED(hr))
		{
			hr = GetVideoStreamDescriptor(pPD, cSourceStreams, &pVideoSD);
		}

		if (SUCCEEDED(hr))
		{
			hr = CreateMediaSinkActivate(pVideoSD, hVideoWnd, &pSinkActivate);
		}

		if (SUCCEEDED(hr))
		{
			hr = CreateMediaSinkActivateCallback(pVideoSD, hVideoWnd, &pCallbackSinkActivate);
		}

		if (SUCCEEDED(hr))
		{
			hr = AddSourceNode(pTopology, pSource, pPD, pVideoSD, &pSourceNode);
		}

		if (SUCCEEDED(hr))
		{
			hr = AddOutputNode(pTopology, pSinkActivate, 0, &pOutputNode);
		}

		if (SUCCEEDED(hr))
		{
			hr = AddOutputNode(pTopology, pCallbackSinkActivate, 0, &pCallbackOutputNode);
		}

		if (SUCCEEDED(hr))
		{
			hr = MFCreateTopologyNode(MF_TOPOLOGY_TEE_NODE, &pTeeOutputNode);
		}

		if (SUCCEEDED(hr))
		{
			//pSourceNode->ConnectOutput(0, pOutputNode, 0);

			pSourceNode->ConnectOutput(0, pTeeOutputNode, 0);
			pTeeOutputNode->ConnectOutput(0, pOutputNode, 0);
			pTeeOutputNode->ConnectOutput(1, pCallbackOutputNode, 0);
		}

		SafeRelease(&pVideoSD);
		SafeRelease(&pSinkActivate);
		SafeRelease(&pSourceNode);
		SafeRelease(&pOutputNode);

		SafeRelease(&pCallbackSinkActivate);
		SafeRelease(&pCallbackOutputNode);
		SafeRelease(&pTeeOutputNode);
		SafeRelease(&pTransformNode);
	}

	if (SUCCEEDED(hr))
	{
		pTopology->AddRef();
		*ppTopology = pTopology;
	}

	SafeRelease(&pTopology);
	SafeRelease(&pSinkActivate);
	SafeRelease(&pSourceNode);
	SafeRelease(&pOutputNode);
	SafeRelease(&pAudioSD);
	SafeRelease(&pVideoSD);

	SafeRelease(&pCallbackSinkActivate);
	SafeRelease(&pCallbackOutputNode);
	SafeRelease(&pTeeOutputNode);
	SafeRelease(&pTransformNode);

	return hr;
}
