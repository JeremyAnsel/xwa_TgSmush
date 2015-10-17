
*** Requirements ***

This dll requires:
- Windows XP SP2 or superior
- Visual C++ 2013 Runtime (x86)

Visual C++ Redistributable Packages for Visual Studio 2013: vcredist_x86.exe
http://www.microsoft.com/en-us/download/details.aspx?id=40784


*** Usage ***

To use this dll:

1) rename the original TGSMUSH.DLL to TgSmushOrig.dll
2) place the new TgSmush.dll and TgSmush.cfg next to it.

TgSmush.cfg is a config file. Open it with a text editor for more details.

The playable formats are .snm, .avi and .wmv.

- snm files are played with the original dll.
- avi files with a resolution <= 640x480 are played using AVIFile functions (Isildur's code).
- other avi files and wmv files are played using DirectShow.


*** Credits ***

- Defiant: his unfinished smush library has inspired Isildur
- Isildur: author of XWA Cutscene Replacer (XCR) 
- Jérémy Ansel (JeremyaFr)
