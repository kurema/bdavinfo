// rplsproginfo.cpp
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include "rplsproginfo.h"
#include "bdavinfo2.h"
#include "bdavfileinfo.h"

#include "convtounicode.h"
#include "tsprocess.h"


// �萔�Ȃ�

#define		RPLSFILESIZE			1048576


// �}�N����`

#define		printMsg(fmt, ...)		_tprintf(_T(fmt), __VA_ARGS__)
#define		printErrMsg(fmt, ...)	_tprintf(_T(fmt), __VA_ARGS__)


//

bool readRplsFileProgInfo(int32_t num, BdavDiskInfo *bdav, ProgInfo* info, const CopyParams* param)
{
	wmemcpy(info->rplsname, bdav->rplsname[num], RPLSNAMELEN + 1);											// rpls�t�@�C�����̃R�s�[
	_snwprintf_s(info->rplsfullpath, _MAX_PATH, L"%s%s", bdav->playlistdir, bdav->rplsname[num]);			// rpls�t�@�C���̃t���p�X���쐬


	// rpls�t�@�C�����J���ēǂݍ���

	HANDLE	hFile = CreateFile(info->rplsfullpath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;														// �ԑg��񌳃t�@�C�����J���̂Ɏ��s����

	int64_t		fsize = GetFileDataSize(hFile);
	if (fsize > RPLSFILESIZE) {																				// rpls�t�@�C�����傫������
		CloseHandle(hFile);
		return false;
	}

	uint8_t		*rplsbuf = new uint8_t [RPLSFILESIZE];
	uint32_t	numRead;

	SeekFileData(hFile, 0);
	ReadFileData(hFile, rplsbuf, RPLSFILESIZE, &numRead);
	CloseHandle(hFile);

	if (numRead != (uint32_t)fsize) {																		// �t�@�C���ǂݍ��ُ݈�
		delete[] rplsbuf;
		return false;
	}


	// type indicator �`�F�b�N�Ɣԑg���̓ǂݍ���

	if (memcmp(rplsbuf, "PLST", 4) != 0) {																	// rpls�t�@�C���͐擪4�o�C�g�� "PLST" �ƂȂ��Ă��Ȃ���΂Ȃ�Ȃ�
		delete[] rplsbuf;
		return false;
	}

	readRplsProgInfo(rplsbuf, info, param);																	// �ԑg���̎擾
	

	// �㏈��

	delete[] rplsbuf;

	return	true;
}


bool rplsMakerCheck(const uint8_t *buf, const int32_t idMaker)
{
	const uint32_t	mpadr = (buf[ADR_MPDATA] << 24) + (buf[ADR_MPDATA + 1] << 16) + (buf[ADR_MPDATA + 2] << 8) + buf[ADR_MPDATA + 3];
	if (mpadr == 0) return false;

	const uint8_t	*mpdata = buf + mpadr;
	const int32_t	makerid = (mpdata[ADR_MPMAKERID] << 8) + mpdata[ADR_MPMAKERID + 1];

	if (makerid != idMaker) return false;

	return true;
}


void readRplsProgInfo(const uint8_t *buf, ProgInfo *proginfo, const CopyParams *param)
{
	// rpls���̎擾

	const bool	bSonyRpls = rplsMakerCheck(buf, MAKERID_SONY);														// ���[�J�[�`�F�b�N�Dsony�y��panasonic
	const bool	bPanaRpls = rplsMakerCheck(buf, MAKERID_PANASONIC);


	// �Ή����� m2ts �t�@�C�����̎擾
	// �A���A��� .rpls �ɕ����� .m2ts �t�@�C�����Ή�����悤�ȏꍇ�i���炭���R�[�_��ŃN���b�v�����������ꍇ�Ȃǁj�ɂ͑Ή��ł��Ȃ��̂Œ��ӂ��K�v

	const uint8_t	*playlist = buf + (buf[ADR_PLAYLIST] << 24) + (buf[ADR_PLAYLIST + 1] << 16) + (buf[ADR_PLAYLIST + 2] << 8) + buf[ADR_PLAYLIST + 3];		// m2ts�v���C���X�g�̐擪

	for (uint32_t i = 0, j = 0; i < M2TSNAMELEN; i++) {
		proginfo->m2tsname[j++] = (WCHAR)tolower(playlist[ADR_MTSNAME + i]);
		if (i == 4) proginfo->m2tsname[j++] = L'.';
	}
	proginfo->m2tsname[M2TSNAMELEN] = 0x0000;


	// �ȉ��ԑg���

	const uint8_t	*appinfo = buf + ADR_APPINFO;
	const uint8_t	*mpdata  = buf + (buf[ADR_MPDATA] << 24) + (buf[ADR_MPDATA + 1] << 16) + (buf[ADR_MPDATA + 2] << 8) + buf[ADR_MPDATA + 3];

	// �^�����

	proginfo->recyear	= (appinfo[ADR_RECYEAR]  >> 4) * 1000 + (appinfo[ADR_RECYEAR]  & 0x0F) * 100 + (appinfo[ADR_RECYEAR + 1] >> 4) * 10 + (appinfo[ADR_RECYEAR + 1] & 0x0F);
	proginfo->recmonth	= (appinfo[ADR_RECMONTH] >> 4) * 10   + (appinfo[ADR_RECMONTH] & 0x0F);
	proginfo->recday	= (appinfo[ADR_RECDAY]   >> 4) * 10   + (appinfo[ADR_RECDAY]   & 0x0F);
	proginfo->rechour	= (appinfo[ADR_RECHOUR]  >> 4) * 10   + (appinfo[ADR_RECHOUR]  & 0x0F);
	proginfo->recmin	= (appinfo[ADR_RECMIN]   >> 4) * 10   + (appinfo[ADR_RECMIN]   & 0x0F);
	proginfo->recsec	= (appinfo[ADR_RECSEC]   >> 4) * 10   + (appinfo[ADR_RECSEC]   & 0x0F);

	// �^�����

	proginfo->durhour	= (appinfo[ADR_DURHOUR] >> 4) * 10 + (appinfo[ADR_DURHOUR] & 0x0F);
	proginfo->durmin	= (appinfo[ADR_DURMIN]  >> 4) * 10 + (appinfo[ADR_DURMIN]  & 0x0F);
	proginfo->dursec	= (appinfo[ADR_DURSEC]  >> 4) * 10 + (appinfo[ADR_DURSEC]  & 0x0F);

	// �^�C���]�[��

	proginfo->rectimezone = appinfo[ADR_TIMEZONE];

	// ���[�J�[ID, ���[�J�[�@��R�[�h

	proginfo->makerid	= appinfo[ADR_MAKERID]   * 256 + appinfo[ADR_MAKERID + 1];
	proginfo->modelcode	= appinfo[ADR_MODELCODE] * 256 + appinfo[ADR_MODELCODE + 1];

	// ������ʏ��ipanasonic���R�����j

	proginfo->recsrc	= bPanaRpls ? mpdata[ADR_RECSRC_PANA] * 256 + mpdata[ADR_RECSRC_PANA + 1] : -1;				// ������ʏ����擾�D�p�i�ȊO�Ȃ������ʏ�񖳂�(-1)

	// �`�����l���ԍ�, �`�����l�����i�����ǖ��j

	proginfo->chnum		= appinfo[ADR_CHANNELNUM] * 256 + appinfo[ADR_CHANNELNUM + 1];
	proginfo->chnamelen = (int32_t)conv_to_unicode((char16_t*)proginfo->chname, CONVBUFSIZE, appinfo + ADR_CHANNELNAME + 1, (size_t)appinfo[ADR_CHANNELNAME], param->bCharSize, param->bIVS);

	// �ԑg��

	proginfo->pnamelen = (int32_t)conv_to_unicode((char16_t*)proginfo->pname, CONVBUFSIZE, appinfo + ADR_PNAME + 1, (size_t)appinfo[ADR_PNAME], param->bCharSize, param->bIVS);

	// �ԑg���e

	const size_t	pdetaillen = appinfo[ADR_PDETAIL] * 256 + appinfo[ADR_PDETAIL + 1];
	proginfo->pdetaillen = (int32_t)conv_to_unicode((char16_t*)proginfo->pdetail, CONVBUFSIZE, appinfo + ADR_PDETAIL + 2, pdetaillen, param->bCharSize, param->bIVS);

	if (bSonyRpls)		// sony���R�[�_�[�̏ꍇ
	{
		// �ԑg���e�ڍ�

		const int32_t	pextendlen = mpdata[ADR_PEXTENDLEN] * 256 + mpdata[ADR_PEXTENDLEN + 1];
		proginfo->pextendlen = (int32_t)conv_to_unicode((char16_t*)proginfo->pextend, CONVBUFSIZE, appinfo + ADR_PDETAIL + 2 + pdetaillen, pextendlen, param->bCharSize, param->bIVS);

		// �ԑg�W���������

		for (int32_t i = 0; i < 3; i++) proginfo->genre[i] = (mpdata[ADR_GENRE + i * 4 + 0] == 0x01) ? mpdata[ADR_GENRE + i * 4 + 1] : -1;
	}

	if (bPanaRpls)		// panasonic���R�[�_�[�̏ꍇ
	{
		proginfo->pextendlen = -1;														// �u�ԑg���e�ڍׁv��L���Ȃ�

		// �ԑg�W���������

		for (int32_t i = 0; i < 3; i++) proginfo->genre[i] = -1;

		switch (mpdata[ADR_GENRE_PANA])
		{
		case 0xD5:
			proginfo->genre[2] = ((mpdata[ADR_GENRE_PANA + 1] & 0x0F) << 4) + (mpdata[ADR_GENRE_PANA + 1] >> 4);
		case 0xE5:
			proginfo->genre[1] = ((mpdata[ADR_GENRE_PANA + 2] & 0x0F) << 4) + (mpdata[ADR_GENRE_PANA + 2] >> 4);
		case 0xE9:
			proginfo->genre[0] = ((mpdata[ADR_GENRE_PANA + 3] & 0x0F) << 4) + (mpdata[ADR_GENRE_PANA + 3] >> 4);
			break;
		default:
			break;
		}
	}

	if (!bSonyRpls && !bPanaRpls)		// sony, pana�ȊO
	{
		for (int32_t i = 0; i < 3; i++) proginfo->genre[i] = -1;						// �ԑg�W��������񖳂�
		proginfo->pextendlen = -1;														// �u�ԑg���e�ڍׁv��L���Ȃ�
	}

	return;
}


int compareForRecSrcStr(const void *item1, const void *item2)
{
	return wcscmp(*(WCHAR**)item1, *(WCHAR**)item2);
}


size_t getRecSrcStr(WCHAR *dst, const size_t maxbufsize, const int32_t src)
{
	static const WCHAR	*nameList[] =
	{
		L"TD",		L"�n��f�W�^��",
		L"BD",		L"BS�f�W�^��",
		L"C1",		L"CS�f�W�^��1",
		L"C2",		L"CS�f�W�^��2",
		L"iL",		L"i.LINK(TS)",
		L"MV",		L"AVCHD",
		L"SK",		L"�X�J�p�[(DLNA)",
		L"DV",		L"DV����",
		L"TA",		L"�n��A�i���O",
		L"NL",		L"���C������"
	};

	static bool		bTableInitialized = false;

	if (!bTableInitialized)
	{
		qsort(nameList, sizeof(nameList) / sizeof(WCHAR*) / 2, sizeof(WCHAR*) * 2, compareForRecSrcStr);
		bTableInitialized = true;
	}

	static const WCHAR	*errNameList[] =
	{
		L"unknown",
		L"n/a"
	};

	const WCHAR	*srcStr = errNameList[1];

	if (src != -1)
	{
		WCHAR  key[3];
		key[0] = (WCHAR)((src >> 8) & 0x00FF);
		key[1] = (WCHAR)(src & 0x00FF);
		key[2] = 0;
		WCHAR *pkey = key;

		void *result = bsearch(&pkey, nameList, sizeof(nameList) / sizeof(WCHAR*) / 2, sizeof(WCHAR*) * 2, compareForRecSrcStr);

		if (result != NULL) {
			srcStr = *((WCHAR**)result + 1);
		}
		else {
			srcStr = errNameList[0];
		}
	}

	size_t i = 0;

	while (i < maxbufsize)
	{
		dst[i] = srcStr[i];
		if (srcStr[i++] == 0) break;
	}

	return i - 1;
}

