#pragma once

#include <windows.h>
#include <stdint.h>

#include "bdavinfo2.h"


// 定数など

#define		ADR_BDAVAPPINFO		0x28
#define		ADR_BDAVNAME		0x18
#define		ADR_BDAVPLAYLIST	0x08
#define		ADR_BDAVMPDATA		0x0C
#define		ADR_NUMPLAYLIST		0x04
#define		ADR_PLAYITEM		0x06


#define		MAXTITLENUM			256
#define		RPLSNAMELEN			10
#define		M2TSNAMELEN			10

#define		BDAVFILESIZE		1048576
#define		CONVBUFSIZE			16384



// ディスク情報構造体

typedef struct {
	WCHAR		diskname[CONVBUFSIZE];
	int32_t		disknamelen;
	WCHAR		fullpath[_MAX_PATH + 1];
	WCHAR		infoname[_MAX_PATH + 1];
	WCHAR		bdavdir[_MAX_PATH + 1];
	WCHAR		playlistdir[_MAX_PATH + 1];
	WCHAR		streamdir[_MAX_PATH + 1];
	int32_t		numRpls;
	WCHAR		rplsname[MAXTITLENUM][RPLSNAMELEN + 1];
} BdavDiskInfo;


// プロトタイプ宣言

bool	readBdavDiskInfo(_TCHAR*, BdavDiskInfo *, CopyParams *param);
