#pragma once

#include <Vfw.h>

#define MAXNUMSTREAMS   50

void FreeAvi();

void InsertAVIFile(PAVIFILE pfile, LPCTSTR lpszFile);

int InitAvi(LPCTSTR szFile);

void InitStreams();

long PaintStuff(LONG lTime);

unsigned char * GetDataPtr(void);

void PlayAudio();
