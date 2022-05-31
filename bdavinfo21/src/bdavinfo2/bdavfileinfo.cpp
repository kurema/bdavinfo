// bdavfileinfo.cpp
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include "bdavfileinfo.h"
#include "tsprocess.h"
#include "convtounicode.h"

// #include <conio.h>


// 定数など




// マクロ定義

#define		printMsg(fmt, ...)		_tprintf(_T(fmt), __VA_ARGS__)
#define		printErrMsg(fmt, ...)	_tprintf(_T(fmt), __VA_ARGS__)


//

bool readBdavDiskInfo(_TCHAR* fname, BdavDiskInfo *info, CopyParams *param)
{
	// info.bdavファイルを開いて読み込む

	HANDLE	hFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		printErrMsg("BDAVファイル %s を開くのに失敗しました.\n", fname);
		return	false;
	}

	int64_t		fsize = GetFileDataSize(hFile);
	if (fsize > BDAVFILESIZE) {
		printErrMsg("BDAVファイル %s のサイズが大きすぎます.\n", fname);
		CloseHandle(hFile);
		return false;
	}

	uint8_t		*buf = new uint8_t [BDAVFILESIZE];
	uint32_t	numRead;

	ReadFileData(hFile, buf, (uint32_t)fsize, &numRead);
	CloseHandle(hFile);

	if (numRead != (uint32_t)fsize) {
		printErrMsg("BDAVファイル %s の読み込みに失敗しました.\n", fname);
		delete[] buf;
		return false;
	}


	// type indicator チェック

	if (memcmp(buf, "BDAV", 4) != 0) {																			// info.bdavファイルは先頭4バイトが "BDAV" となっている
		printErrMsg("ファイル %s 正常なBDAVファイルではありません.\n", fname);
		delete[] buf;
		return false;
	}


	// ファイルパス等取得

	WCHAR	drive[_MAX_DRIVE];
	WCHAR	dir[_MAX_DIR];
	WCHAR	filename[_MAX_FNAME];
	WCHAR	fext[_MAX_EXT];

	_wfullpath(info->fullpath, fname, _MAX_PATH);																// BDAVファイルのフルパス名取得
	_wsplitpath_s(info->fullpath, drive, _MAX_DRIVE, dir, _MAX_DIR, filename, _MAX_FNAME, fext, _MAX_EXT);		// ドライブとディレクトリ名取得

	_snwprintf_s(info->infoname, _MAX_PATH, L"%s%s", filename, fext);
	_snwprintf_s(info->bdavdir, _MAX_PATH, L"%s%s", drive, dir);
	_snwprintf_s(info->playlistdir, _MAX_PATH, L"%s%sPLAYLIST\\", drive, dir);
	_snwprintf_s(info->streamdir, _MAX_PATH, L"%s%sSTREAM\\", drive, dir);


	// ディスク名取得

	const uint8_t	*appinfo = buf + ADR_BDAVAPPINFO;

	info->disknamelen = (int32_t)conv_to_unicode((char16_t*)info->diskname, CONVBUFSIZE, appinfo + ADR_BDAVNAME + 1, (size_t)appinfo[ADR_BDAVNAME], param->bCharSize, param->bIVS);


	// プレイリスト項目の取得

	const uint8_t	*playlist = buf + (buf[ADR_BDAVPLAYLIST] << 24) + (buf[ADR_BDAVPLAYLIST + 1] << 16) + (buf[ADR_BDAVPLAYLIST + 2] << 8) + buf[ADR_BDAVPLAYLIST + 3];
	
	info->numRpls = playlist[ADR_NUMPLAYLIST] * 256 + playlist[ADR_NUMPLAYLIST + 1];							// プレイリスト項目数
	info->numRpls = (info->numRpls > MAXTITLENUM) ? MAXTITLENUM : info->numRpls;								// 項目数の最大値を制限する．sony, panaのレコーダはディスク1枚で最大200タイトルの模様（未確認）

	for (int32_t i = 0; i < info->numRpls; i++)
	{
		for (int32_t j = 0; j < RPLSNAMELEN; j++) info->rplsname[i][j] = (WCHAR)playlist[ADR_PLAYITEM + i * RPLSNAMELEN + j];			// 各rpls項目名
		info->rplsname[i][RPLSNAMELEN] = 0;
	}

	delete [] buf;

	return true;
}