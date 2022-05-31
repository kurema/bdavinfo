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


// �萔�Ȃ�




// �}�N����`

#define		printMsg(fmt, ...)		_tprintf(_T(fmt), __VA_ARGS__)
#define		printErrMsg(fmt, ...)	_tprintf(_T(fmt), __VA_ARGS__)


//

bool readBdavDiskInfo(_TCHAR* fname, BdavDiskInfo *info, CopyParams *param)
{
	// info.bdav�t�@�C�����J���ēǂݍ���

	HANDLE	hFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		printErrMsg("BDAV�t�@�C�� %s ���J���̂Ɏ��s���܂���.\n", fname);
		return	false;
	}

	int64_t		fsize = GetFileDataSize(hFile);
	if (fsize > BDAVFILESIZE) {
		printErrMsg("BDAV�t�@�C�� %s �̃T�C�Y���傫�����܂�.\n", fname);
		CloseHandle(hFile);
		return false;
	}

	uint8_t		*buf = new uint8_t [BDAVFILESIZE];
	uint32_t	numRead;

	ReadFileData(hFile, buf, (uint32_t)fsize, &numRead);
	CloseHandle(hFile);

	if (numRead != (uint32_t)fsize) {
		printErrMsg("BDAV�t�@�C�� %s �̓ǂݍ��݂Ɏ��s���܂���.\n", fname);
		delete[] buf;
		return false;
	}


	// type indicator �`�F�b�N

	if (memcmp(buf, "BDAV", 4) != 0) {																			// info.bdav�t�@�C���͐擪4�o�C�g�� "BDAV" �ƂȂ��Ă���
		printErrMsg("�t�@�C�� %s �����BDAV�t�@�C���ł͂���܂���.\n", fname);
		delete[] buf;
		return false;
	}


	// �t�@�C���p�X���擾

	WCHAR	drive[_MAX_DRIVE];
	WCHAR	dir[_MAX_DIR];
	WCHAR	filename[_MAX_FNAME];
	WCHAR	fext[_MAX_EXT];

	_wfullpath(info->fullpath, fname, _MAX_PATH);																// BDAV�t�@�C���̃t���p�X���擾
	_wsplitpath_s(info->fullpath, drive, _MAX_DRIVE, dir, _MAX_DIR, filename, _MAX_FNAME, fext, _MAX_EXT);		// �h���C�u�ƃf�B���N�g�����擾

	_snwprintf_s(info->infoname, _MAX_PATH, L"%s%s", filename, fext);
	_snwprintf_s(info->bdavdir, _MAX_PATH, L"%s%s", drive, dir);
	_snwprintf_s(info->playlistdir, _MAX_PATH, L"%s%sPLAYLIST\\", drive, dir);
	_snwprintf_s(info->streamdir, _MAX_PATH, L"%s%sSTREAM\\", drive, dir);


	// �f�B�X�N���擾

	const uint8_t	*appinfo = buf + ADR_BDAVAPPINFO;

	info->disknamelen = (int32_t)conv_to_unicode((char16_t*)info->diskname, CONVBUFSIZE, appinfo + ADR_BDAVNAME + 1, (size_t)appinfo[ADR_BDAVNAME], param->bCharSize, param->bIVS);


	// �v���C���X�g���ڂ̎擾

	const uint8_t	*playlist = buf + (buf[ADR_BDAVPLAYLIST] << 24) + (buf[ADR_BDAVPLAYLIST + 1] << 16) + (buf[ADR_BDAVPLAYLIST + 2] << 8) + buf[ADR_BDAVPLAYLIST + 3];
	
	info->numRpls = playlist[ADR_NUMPLAYLIST] * 256 + playlist[ADR_NUMPLAYLIST + 1];							// �v���C���X�g���ڐ�
	info->numRpls = (info->numRpls > MAXTITLENUM) ? MAXTITLENUM : info->numRpls;								// ���ڐ��̍ő�l�𐧌�����Dsony, pana�̃��R�[�_�̓f�B�X�N1���ōő�200�^�C�g���̖͗l�i���m�F�j

	for (int32_t i = 0; i < info->numRpls; i++)
	{
		for (int32_t j = 0; j < RPLSNAMELEN; j++) info->rplsname[i][j] = (WCHAR)playlist[ADR_PLAYITEM + i * RPLSNAMELEN + j];			// �erpls���ږ�
		info->rplsname[i][RPLSNAMELEN] = 0;
	}

	delete [] buf;

	return true;
}