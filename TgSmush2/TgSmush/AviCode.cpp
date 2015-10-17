#include "common.h"
#include "AviCode.h"
#include "AudPlay.h"
#include <WindowsX.h>

#pragma comment(lib, "Winmm")
#pragma comment(lib, "Vfw32")

namespace
{

	unsigned char* data;

#define FIXCC(fcc)  if (fcc == 0)       fcc = mmioFOURCC('N', 'o', 'n', 'e'); \
                    if (fcc == BI_RLE8) fcc = mmioFOURCC('R', 'l', 'e', '8');

	PAVIFILE	    gpfile = 0;			// the current file
	PAVISTREAM      gapavi[MAXNUMSTREAMS] = { 0 };	// the current streams
	PGETFRAME	    gapgf[MAXNUMSTREAMS] = { 0 };	// data for decompressing
	// video
	HDRAWDIB	    ghdd[MAXNUMSTREAMS] = { 0 };	// drawdib handles
	int				gcpavi = 0;			// # of streams

	PAVISTREAM      gpaviAudio = 0;                 // 1st audio stream found
	PAVISTREAM      gpaviVideo = 0;                 // 1st video stream found

	AVICOMPRESSOPTIONS  gaAVIOptions[MAXNUMSTREAMS] = { 0 };
	LPAVICOMPRESSOPTIONS  galpAVIOptions[MAXNUMSTREAMS] = { 0 };

	int width = 0;
	int height = 0;


	BYTE		    abFormat[1024];
	LPVOID		    lpAudio;		// buffer for painting

	HDC hdc = 0;
	HBITMAP	hBitmap = 0;
	BITMAPINFOHEADER	bmih = { 0 };

	char				*pdata = 0;

};

/*----------------------------------------------------------------------------*\
|    InitAvi()
|
|    Open up a file through the AVIFile handlers.
\*----------------------------------------------------------------------------*/
int InitAvi(LPCTSTR szFile)
{
	HRESULT	hr;
	PAVIFILE	pfile;

	hr = AVIFileOpen(&pfile, szFile, 0, 0L);

	if (hr != 0)
	{
		return 0;
	}

	FreeAvi();

	if (hBitmap)
		DeleteObject(hBitmap);

	hBitmap = 0;

	if (hdc)
		DeleteDC(hdc);

	hdc = 0;

	hdc = CreateCompatibleDC(0);

	InsertAVIFile(pfile, szFile);

	return 1;
}

/*----------------------------------------------------------------------------*\
|    FreeDrawStuff()
|
| Free up the resources associated with DrawDIB
\*----------------------------------------------------------------------------*/
void FreeDrawStuff()
{
	aviaudioStop();

	for (int i = 0; i < gcpavi; i++)
	{
		if (gapgf[i])
		{
			AVIStreamGetFrameClose(gapgf[i]);
			gapgf[i] = NULL;
		}

		if (ghdd[i])
		{
			//DrawDibEnd(ghdd[i]);
			DrawDibClose(ghdd[i]);
			ghdd[i] = 0;
		}
	}

	gpaviVideo = gpaviAudio = NULL;
}

/*----------------------------------------------------------------------------*\
|    FreeAvi()
|
|    Free the resources associated with an open file.
\*----------------------------------------------------------------------------*/
void FreeAvi()
{
	int	i;

	FreeDrawStuff();

	AVISaveOptionsFree(gcpavi, galpAVIOptions);

	for (i = 0; i < gcpavi; i++)
	{
		AVIStreamRelease(gapavi[i]);
	}

	if (gpfile)
		AVIFileRelease(gpfile);

	gpfile = NULL;

	// Good a place as any to make sure audio data gets freed
	if (lpAudio)
		GlobalFreePtr(lpAudio);

	lpAudio = NULL;

	gcpavi = 0;
}

/*----------------------------------------------------------------------------*\
|    InsertAVIFile()
|
|    Does most of the work of opening an AVI file.
\*----------------------------------------------------------------------------*/
void InsertAVIFile(PAVIFILE pfile, LPCTSTR lpszFile)
{
	int		i;
	PAVISTREAM	pavi;

	//
	// Get all the streams from the new file
	//
	for (i = gcpavi; i <= MAXNUMSTREAMS; i++)
	{
		if (AVIFileGetStream(pfile, &pavi, 0L, i - gcpavi) != AVIERR_OK)
			break;

		if (i == MAXNUMSTREAMS)
		{
			AVIStreamRelease(pavi);
			break;
		}

		gapavi[i] = pavi;
	}

	//
	// Couldn't get any streams out of this file
	//
	if (gcpavi == i && i != MAXNUMSTREAMS)
	{
		if (pfile)
			AVIFileRelease(pfile);

		return;
	}

	gcpavi = i;

	if (gpfile)
	{
		AVIFileRelease(pfile);
	}
	else
	{
		gpfile = pfile;
	}

	FreeDrawStuff();

	InitStreams();
}

/*----------------------------------------------------------------------------*\
|    InitStreams()
|
|    Initialize the streams of a loaded file -- the compression options, the
|    DrawDIB handles, and the scroll bars.
\*----------------------------------------------------------------------------*/
void InitStreams()
{
	AVISTREAMINFO     avis;
	LONG	lTemp = 0;
	BITMAPINFOHEADER lpbi;

	//
	// Walk through and init all streams loaded
	//
	for (int i = 0; i < gcpavi; i++)
	{

		bool w = AVIStreamInfo(gapavi[i], &avis, sizeof(avis)) == 0;

		//
		// Save and SaveOptions code takes a pointer to our compression opts
		//
		galpAVIOptions[i] = &gaAVIOptions[i];

		//
		// clear options structure to zeroes
		//
		_fmemset(galpAVIOptions[i], 0, sizeof(AVICOMPRESSOPTIONS));

		if (!w)
		{
			continue;
		}

		//
		// Initialize the compression options to some default stuff
		// !!! Pick something better
		//
		galpAVIOptions[i]->fccType = avis.fccType;

		switch (avis.fccType)
		{
		case streamtypeVIDEO:
			galpAVIOptions[i]->dwFlags = AVICOMPRESSF_VALID |
				AVICOMPRESSF_KEYFRAMES | AVICOMPRESSF_DATARATE;

			galpAVIOptions[i]->fccHandler = 0;
			galpAVIOptions[i]->dwQuality = (DWORD)ICQUALITY_DEFAULT;
			galpAVIOptions[i]->dwKeyFrameEvery = (DWORD)-1; // Default
			galpAVIOptions[i]->dwBytesPerSecond = 0;
			galpAVIOptions[i]->dwInterleaveEvery = 1;

			width = avis.rcFrame.right - avis.rcFrame.left;
			height = avis.rcFrame.bottom - avis.rcFrame.top;

			lpbi.biBitCount = 24;
			lpbi.biClrImportant = 0;
			lpbi.biClrUsed = 0;
			lpbi.biCompression = BI_RGB;
			lpbi.biHeight = height;
			lpbi.biPlanes = 1;
			lpbi.biWidth = width;
			lpbi.biXPelsPerMeter = 0;
			lpbi.biYPelsPerMeter = 0;
			lpbi.biSize = sizeof(BITMAPINFOHEADER);
			lpbi.biSizeImage = 0;

			break;

		case streamtypeAUDIO:
			galpAVIOptions[i]->dwFlags |= AVICOMPRESSF_VALID;
			galpAVIOptions[i]->dwInterleaveEvery = 1;

			AVIStreamReadFormat(gapavi[i], AVIStreamStart(gapavi[i]), NULL, &lTemp);
			galpAVIOptions[i]->cbFormat = lTemp;

			if (lTemp)
				galpAVIOptions[i]->lpFormat = GlobalAllocPtr(GHND, lTemp);

			// Use current format as default format
			if (galpAVIOptions[i]->lpFormat)
				AVIStreamReadFormat(gapavi[i], AVIStreamStart(gapavi[i]),
				galpAVIOptions[i]->lpFormat, &lTemp);
			break;

		default:
			break;
		}

		//
		// Initialize video streams for getting decompressed frames to display
		//
		if (avis.fccType == streamtypeVIDEO)
		{

			gapgf[i] = AVIStreamGetFrameOpen(gapavi[i], NULL);

			if (gapgf[i] == NULL)
				continue;

			ghdd[i] = DrawDibOpen();

			//DrawDibBegin(ghdd[i], hdc, 0, 0, &lpbi, 0, 0, 0);

			//DrawDibStart(ghdd[i], 0);

			if (gpaviVideo == NULL)
			{
				//
				// Remember the first video stream --- treat it specially
				//
				gpaviVideo = gapavi[i];
			}

		}
		else if (avis.fccType == streamtypeAUDIO)
		{
			//
			// Remember the first audio stream --- treat it specially
			//
			if (gpaviAudio == NULL)
				gpaviAudio = gapavi[i];
		}
	}

	bmih.biSize = sizeof(BITMAPINFOHEADER);					// Size Of The BitmapInfoHeader
	bmih.biPlanes = 1;											// Bitplanes	
	bmih.biBitCount = 24;										// Bits Format We Want (24 Bit, 3 Bytes)
	bmih.biWidth = 640;											// Width We Want (256 Pixels)
	bmih.biHeight = 480;										// Height We Want (256 Pixels)
	bmih.biCompression = BI_RGB;								// Requested Mode = RGB

	hBitmap = CreateDIBSection(hdc, (BITMAPINFO*)(&bmih), DIB_RGB_COLORS, (void**)&data, NULL, 0);
	SelectObject(hdc, hBitmap);								// Select hBitmap Into Our Device Context (hdc)
}

/*----------------------------------------------------------------------------*\
|    PaintStuff()
|
|    Paint the screen with what we plan to show them.
\*----------------------------------------------------------------------------*/
long PaintStuff(LONG lTime)
{
	LPBITMAPINFOHEADER lpbi;
	LONG        lSize = 0;
	LONG        lFrame;

	//
	// Walk through all streams and draw something
	//
	for (int i = 0; i < gcpavi; i++)
	{
		AVISTREAMINFO	avis;
		LONG	lEndTime, l;

		if (AVIStreamInfo(gapavi[i], &avis, sizeof(avis)) != 0)
		{
			continue;
		}

		FIXCC(avis.fccHandler);
		FIXCC(avis.fccType);

		//
		// Draw a VIDEO stream
		//
		if (avis.fccType == streamtypeVIDEO)
		{
			if (gapgf[i] == NULL)
				continue;

			l = sizeof(abFormat);
			AVIStreamReadFormat(gapavi[i], 0, &abFormat, &l);

			lpbi = (LPBITMAPINFOHEADER)abFormat;
			FIXCC(lpbi->biCompression);

			//
			// Which frame belongs at this time?
			//
			lEndTime = AVIStreamEndTime(gapavi[i]);

			if (lTime <= lEndTime)
			{
				lFrame = AVIStreamTimeToSample(gapavi[i], lTime);
			}
			else
			{	// we've scrolled past the end of this stream
				return 0;
			}

			if ((gapgf[i] != nullptr) && (lFrame >= AVIStreamStart(gapavi[i])))
			{
				lpbi = (LPBITMAPINFOHEADER)AVIStreamGetFrame(gapgf[i], lFrame);
			}
			else
			{
				lpbi = NULL;
				return 0;
			}

			if (lpbi != nullptr)
			{
				pdata = (char *)lpbi + lpbi->biSize + lpbi->biClrUsed * sizeof(RGBQUAD);

				//
				// Now draw the video frame in the computed rectangle
				//

				if (width == 640 && height == 480)
				{
					data = (unsigned char*)pdata;
				}
				else
				{
					if (!DrawDibDraw(ghdd[i], hdc, 0, 0, 640, 480, lpbi, pdata, 0, 0, width, height, 0))
					{
						return 0;
					}
				}
			}
		}
	}

	return 1;
}

unsigned char * GetDataPtr(void)
{
	return data;
}

void PlayAudio()
{
	if (!gpaviAudio)
		return;

	aviaudioPlay(
		gpaviAudio,
		AVIStreamTimeToSample(gpaviAudio, 0),
		AVIStreamEnd(gpaviAudio),
		FALSE);
}
