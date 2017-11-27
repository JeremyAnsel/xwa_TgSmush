#include "common.h"
#include "xcr.player.h"
#include "AudPlay.h"
#include "AviCode.h"

struct SMUSHHEADER
{
	DWORD d01;
	DWORD d02;
	DWORD d03;
	DWORD d04;
	DWORD d05;
	DWORD d06;
	DWORD d07;
	DWORD d08;
	DWORD d09;
	DWORD d10;
	DWORD d11_pixels;
	DWORD d12_width;
	DWORD d13_height;
	DWORD d14;
	DWORD d15;
	DWORD d16;
	DWORD d17;
	DWORD d18;
	DWORD d19;
	DWORD d20;
	DWORD d21;
	DWORD d22;
	DWORD d23;
	DWORD d24;
	DWORD d25;
	DWORD d26;
	DWORD d27;
	DWORD d28;
	DWORD d29;
	DWORD d30;
	DWORD d31;
};

typedef int (WINAPIV *TDefplayCallback)(DWORD parm1, DWORD parm2);

namespace
{

	TDefplayCallback		EXT_PlayCallBack;

	BOOL g_bContinue;
	int frame = 0;

	unsigned char* data = 0;

}

int XcrPlayVideo(std::wstring filename, void* playCallback)
{
	SMUSHHEADER* pHeader = NULL;
	DWORD retVal = 0;
	int startTime = 0;

	g_bContinue = TRUE;

	EXT_PlayCallBack = (TDefplayCallback)playCallback;

	pHeader = (SMUSHHEADER*)VirtualAlloc(NULL, 0x97000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (!pHeader)
	{
		return 1;
	}

	// prepare header
	pHeader->d01 = (DWORD)(&pHeader);
	pHeader->d02 = (DWORD)(&pHeader);
	pHeader->d03 = 0;
	pHeader->d04 = 0;
	pHeader->d05 = 0x00097000;
	pHeader->d06 = 0x00097000;
	pHeader->d07 = 0x00000FA0;
	pHeader->d08 = 0x00000B00;
	pHeader->d09 = 0;
	pHeader->d10 = 0;
	pHeader->d11_pixels = (DWORD)pHeader + 0x7C;
	pHeader->d12_width = 640;
	pHeader->d13_height = 480;
	pHeader->d14 = 2;
	pHeader->d15 = 0x10;
	pHeader->d16 = 0x500;
	pHeader->d17 = 0;
	pHeader->d18 = 0;
	pHeader->d19 = 1;
	pHeader->d20 = 5;
	pHeader->d21 = 6;
	pHeader->d22 = 5;
	pHeader->d23 = 0x0B;
	pHeader->d24 = 5;
	pHeader->d25 = 0;
	pHeader->d26 = 3;
	pHeader->d27 = 2;
	pHeader->d28 = 3;
	pHeader->d29 = 0;
	pHeader->d30 = 0;
	pHeader->d31 = 0;

	retVal = InitAvi(filename.c_str());

	if (!retVal)
		goto ErrorHandler;

	PlayAudio();

	startTime = GetTickCount();

	frame = 0;

	do
	{
		// get the avi frame
		retVal = PaintStuff(GetTickCount() - startTime);

		if (!retVal)
			break;

		// get the frame ptr
		data = GetDataPtr();

		if (!data)
			break;

		BYTE* pixels = (BYTE*)pHeader + 0x7C;

		for (int i = 480; i != 0; i--)
		{
			BYTE* rgbPtr = data + ((i - 1) * 640 * 3);

			for (int j = 0; j < 640 * 3; j += 3)
			{
				BYTE b = rgbPtr[j];
				BYTE g = rgbPtr[j + 1];
				BYTE r = rgbPtr[j + 2];

				r = (r * (0x1f * 2) + 0xff) / (0xff * 2);
				g = (g * (0x3f * 2) + 0xff) / (0xff * 2);
				b = (b * (0x1f * 2) + 0xff) / (0xff * 2);

				*(WORD*)pixels = (r << 11) | (g << 5) | b;

				pixels += 2;
			}
		}

		// call the XWA call back to display the frame

		if (EXT_PlayCallBack((DWORD)(&pHeader->d11_pixels), 1))
		{
			g_bContinue = FALSE;
		}

	} while (retVal == 1 && g_bContinue);

	FreeAvi();

ErrorHandler:
	if (pHeader)
		VirtualFree(pHeader, 0, MEM_RELEASE);

	pHeader = NULL;

	/////////////////////////////////////////////////////////////////

	return g_bContinue == FALSE ? 0 : 1;
}

int XcrGetFrameCount()
{
	return frame++;
}
