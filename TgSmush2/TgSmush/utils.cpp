
#include "targetver.h"
#include "utils.h"

#define STRICT
#include <Windows.h>

#include <memory>
#include <gdiplus.h>
#include <shellapi.h>

#pragma comment(lib, "Gdiplus")

using namespace Gdiplus;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0;
	UINT size = 0;

	ImageCodecInfo* pImageCodecInfo = nullptr;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == nullptr)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

class GdiInitializer
{
public:
	GdiInitializer()
	{
		this->status = GdiplusStartup(&token, &gdiplusStartupInput, nullptr);

		GetEncoderClsid(L"image/png", &this->pngClsid);
	}

	~GdiInitializer()
	{
		if (this->status == 0)
		{
			GdiplusShutdown(token);
		}
	}

	bool hasError()
	{
		return this->status != 0;
	}

	ULONG_PTR token;
	GdiplusStartupInput gdiplusStartupInput;

	Status status;
	CLSID pngClsid;
};

static GdiInitializer g_gdiInitializer;

void saveSurface(std::wstring name, char* buffer, int width, int height)
{
	if (g_gdiInitializer.hasError())
		return;

	static int index = 0;

	if (index == 0)
	{
		SHFILEOPSTRUCT file_op = {
			NULL,
			FO_DELETE,
			L"_screenshots",
			L"",
			FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
			false,
			0,
			L"" };

		SHFileOperation(&file_op);

		CreateDirectory(L"_screenshots", nullptr);
	}

	std::wstring filename = L"_screenshots\\" + std::to_wstring(index) + L"_" + name + L".png";
	index++;

	std::unique_ptr<Bitmap> bitmap(new Bitmap(width, height, width * 4, PixelFormat32bppRGB, (BYTE*)buffer));

	bitmap->Save(filename.c_str(), &g_gdiInitializer.pngClsid, nullptr);
}
