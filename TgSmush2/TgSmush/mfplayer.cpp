// Copyright (c) Isildur 2003, Jérémy Ansel 2014
// Licensed under the MIT license. See LICENSE.txt

#include "common.h"
#include "mfplayer.h"
#include "mfplayer.player.h"
#include <errors.h>
#include <comdef.h>

namespace
{
	PCWSTR szTitle = L"MediaFoundation Playback";
	PCWSTR szWindowClass = L"tgsmushPlayer";

	bool g_continuePlayback = true;
	bool g_userInterrupted = false;

	ATOM        g_classAtom = 0;
	HWND        g_hwnd = nullptr;
	BOOL        g_bRepaintClient = TRUE;            // Repaint the application client area?
	MFPlayer* g_pPlayer = NULL;                  // Global player object. 

													// Note: After WM_CREATE is processed, g_pPlayer remains valid until the
													// window is destroyed.

													// Forward declarations of functions included in this code module:
	BOOL                InitInstance(HWND);
	LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
	void                NotifyError(HWND hwnd, PCWSTR sErrorMessage, HRESULT hr);
	void                UpdateUI(HWND hwnd, PlayerState state);

	// Message handlers
	LRESULT             OnCreateWindow(HWND hwnd);
	void                OnPlayerEvent(HWND hwnd, WPARAM pUnkPtr);
	void                OnPaint(HWND hwnd);
	void                OnResize(WORD width, WORD height);

	HRESULT PlayVideo(HWND owner, std::wstring filename)
	{
		g_continuePlayback = true;
		g_userInterrupted = false;

		// Perform application initialization.
		if (!InitInstance(owner))
		{
			NotifyError(NULL, L"Could not initialize the application.",
				HRESULT_FROM_WIN32(GetLastError()));
			return E_FAIL;
		}

		SetForegroundWindow(g_hwnd);
		SetFocus(g_hwnd);

		while (ShowCursor(FALSE) >= 0);

		g_pPlayer->OpenURL(filename.c_str());

		MSG msg{};

		// Main message loop.
		while (g_continuePlayback && GetMessage(&msg, g_hwnd, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Clean up.
		if (g_pPlayer)
		{
			g_pPlayer->Shutdown();
			SafeRelease(&g_pPlayer);
		}

		DestroyWindow(g_hwnd);
		UnregisterClass((LPWSTR)g_classAtom, nullptr);

		return g_userInterrupted ? S_FALSE : S_OK;
	}

	//  Create the application window.
	BOOL InitInstance(HWND owner)
	{
		WNDCLASS wc{};
		wc.lpfnWndProc = WndProc;
		wc.lpszClassName = szWindowClass;

		g_classAtom = RegisterClass(&wc);

		if (!g_classAtom)
		{
			return FALSE;
		}

		RECT rect;
		GetWindowRect(owner, &rect);

		// Create the application window.
		g_hwnd = CreateWindow(
			(LPWSTR)g_classAtom,
			szTitle,
			WS_POPUP | WS_EX_TOPMOST,
			rect.left,
			rect.top,
			//GetSystemMetrics(SM_CXSCREEN),
			//GetSystemMetrics(SM_CYSCREEN),
			rect.right - rect.left,
			rect.bottom - rect.top,
			owner,
			nullptr,
			nullptr,
			nullptr);

		if (g_hwnd == 0)
		{
			return FALSE;
		}

		ShowWindow(g_hwnd, SW_NORMAL);
		UpdateWindow(g_hwnd);

		return TRUE;
	}

	//  Message handler for the main window.
	LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
			return OnCreateWindow(hwnd);

		case WM_PAINT:
			OnPaint(hwnd);
			return 0;

		case WM_SIZE:
			OnResize(LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_ERASEBKGND:
			// Suppress window erasing, to reduce flickering while the video is playing.
			return 1;

		case WM_DESTROY:
			g_continuePlayback = false;
			return 0;

		case WM_KEYDOWN:
			switch (wParam)
			{
			case VK_ESCAPE:
			case VK_SPACE:
			case VK_RETURN:
			case VK_BACK:
				g_continuePlayback = false;
				g_userInterrupted = true;
				break;
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			g_continuePlayback = false;
			g_userInterrupted = true;
			break;

		case WM_APP_PLAYER_EVENT:
			OnPlayerEvent(hwnd, wParam);
			return 0;
		}

		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	//  Handler for WM_CREATE message.
	LRESULT OnCreateWindow(HWND hwnd)
	{
		// Initialize the player object.
		HRESULT hr = MFPlayer::CreateInstance(hwnd, hwnd, &g_pPlayer);
		if (SUCCEEDED(hr))
		{
			UpdateUI(hwnd, Closed);
			UpdateUI(hwnd, OpenPending);

			ShowWindow(hwnd, SW_NORMAL);

			return 0;   // Success.
		}
		else
		{
			NotifyError(NULL, L"Could not initialize the player object.", hr);
			return -1;  // Destroy the window
		}
	}

	//  Handler for WM_PAINT messages.
	void OnPaint(HWND hwnd)
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		if (g_pPlayer && g_pPlayer->HasVideo())
		{
			// Video is playing. Ask the player to repaint.
			g_pPlayer->Repaint();
		}
		else
		{
			// The video is not playing, so we must paint the application window.
			FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
		}

		EndPaint(hwnd, &ps);
	}

	//  Handler for WM_SIZE messages.
	void OnResize(WORD width, WORD height)
	{
		if (g_pPlayer)
		{
			g_pPlayer->ResizeVideo(width, height);
		}
	}


	// Handler for Media Session events.
	void OnPlayerEvent(HWND hwnd, WPARAM pUnkPtr)
	{
		HRESULT hr = g_pPlayer->HandleEvent(pUnkPtr);
		if (FAILED(hr))
		{
			NotifyError(hwnd, L"An error occurred.", hr);
		}

		UpdateUI(hwnd, g_pPlayer->GetState());
	}


	// Update the application UI to reflect the current state.

	void UpdateUI(HWND hwnd, PlayerState state)
	{
		BOOL bWaiting = FALSE;
		BOOL bPlayback = FALSE;

		_ASSERTE(g_pPlayer != NULL);

		switch (state)
		{
		case OpenPending:
			bWaiting = TRUE;
			break;

		case Started:
			bPlayback = TRUE;
			break;

		case Paused:
			bPlayback = TRUE;
			break;

		case Stopped:
			g_continuePlayback = false;
			break;
		}

		if (bPlayback && g_pPlayer->HasVideo())
		{
			g_bRepaintClient = FALSE;
		}
		else
		{
			g_bRepaintClient = TRUE;
		}
	}

	//  Show a message box with an error message.
	void NotifyError(HWND hwnd, PCWSTR pszErrorMessage, HRESULT hrErr)
	{
		const size_t MESSAGE_LEN = 512;
		WCHAR message[MESSAGE_LEN];

		if (SUCCEEDED(StringCchPrintf(message, MESSAGE_LEN, L"%s (HRESULT = 0x%X)",
			pszErrorMessage, hrErr)))
		{
			OutputDebugString(message);
		}

		if (FAILED(hrErr))
		{
			static bool messageShown = false;

			if (!messageShown)
			{
				wchar_t text[512];
				wcscpy_s(text, __FUNCTIONW__);
				wcscat_s(text, L"\n");
				wcscat_s(text, message);
				wcscat_s(text, L"\n");
				wcscat_s(text, _com_error(hrErr).ErrorMessage());

				MessageBox(nullptr, text, __FUNCTIONW__, MB_ICONERROR);
			}

			messageShown = true;
		}
	}
}

int MFPlayVideo(std::wstring filename)
{
	HWND xwaWindow = *(HWND*)0x9F701A;
	LPDIRECTDRAW xwaDirectDraw = *(LPDIRECTDRAW*)0x9F7026;
	int& xwaUserInterrupted = *(int*)0x9F4B40;

	xwaDirectDraw->RestoreDisplayMode();

	// Free DirectDraw resources
	//((void(*)())0x5407F0)();

	int returnValue = 0;

	HRESULT hr;
	if (FAILED(hr = PlayVideo(xwaWindow, filename)))
	{
		wchar_t error[MAX_ERROR_TEXT_LEN];
		AMGetErrorText(hr, error, MAX_ERROR_TEXT_LEN);

		OutputDebugString(error);
	}
	else if (hr == S_FALSE)
	{
		xwaUserInterrupted = 1;
		returnValue = 1;
	}

	SetForegroundWindow(xwaWindow);
	ShowWindow(xwaWindow, SW_NORMAL);

	// Init DirectDraw resources
	//((void(*)())0x540370)();

	return returnValue;
}
