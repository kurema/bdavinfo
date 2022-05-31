// bdavinfo2.cpp
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <new.h>
#include <locale.h>
#include <wchar.h>
#include <fcntl.h>
#include <io.h>

#include "bdavinfo2.h"
#include "bdavfileinfo.h"
#include "rplsproginfo.h"
#include "tsproginfo.h"
#include "tsprocess.h"

#include "conio.h"


// �萔�Ȃ�

#ifdef _WIN64
#define		NAMESTRING				"\nbdavinfo version 2.1 (64bit)\n"
#else
#define		NAMESTRING				"\nbdavinfo version 2.1 (32bit)\n"
#endif



// �}�N����`

#define		printMsg(fmt, ...)		_tprintf(_T(fmt), __VA_ARGS__)
#define		printErrMsg(fmt, ...)	_tprintf(_T(fmt), __VA_ARGS__)


// �v���g�^�C�v�錾

void	initCopyParams(CopyParams*);
bool	parseCopyParams(const int32_t, _TCHAR *[], CopyParams *);
bool	parseCopyParamsM(const WCHAR, const int32_t, CopyParams *);
void	initProgInfo(ProgInfo*);
size_t	convForCsv(WCHAR*, const size_t, const WCHAR*, const size_t, const CopyParams*);
void	outputProgInfo(FILE*, const ProgInfo*, const CopyParams*);


//

int _tmain(int argc, _TCHAR* argv[])
{

	_tsetlocale(LC_ALL, _T(""));


	// �p�����[�^�`�F�b�N

	if (argc == 1) {
		printMsg(NAMESTRING);
		exit(1);
	}

	CopyParams	param;
	initCopyParams(&param);

	if (!parseCopyParams(argc, argv, &param)) exit(1);										// �p�����[�^�ُ�Ȃ�I��


	// info.bdav�t�@�C������v���C���X�g����ǂݍ���

	BdavDiskInfo	*bdav = new BdavDiskInfo;
	memset(bdav, 0, sizeof(BdavDiskInfo));

	if (!readBdavDiskInfo(argv[1], bdav, &param)) exit(1);									// ���s������I��																					


	// �K�v�Ȃ�o�̓t�@�C�����J��

	FILE*	pWriteFile = stdout;

	if (!param.bDisplay)
	{
		if (_wfopen_s(&pWriteFile, argv[param.argDest], L"ab") != 0) {
			printErrMsg("�ۑ���t�@�C�� %s ���J���܂���ł���.\n", argv[param.argDest]);
			exit(1);
		}

		_fseeki64(pWriteFile, 0, FILE_END);
		if (param.bUseBOM && (_ftelli64(pWriteFile) == 0)) {
			const uint8_t	bom[2] = { 0xFF, 0xFE };
			fwrite(bom, sizeof(uint8_t), 2, pWriteFile);									// �t�@�C������̏ꍇ�A�K�v�Ȃ�BOM�o��
		}

		_setmode(_fileno(pWriteFile), _O_WTEXT);
	}


	// �f�B�X�N���̏o��

	if (param.bShowDiskName)
	{
		WCHAR	dstr[CONVBUFSIZE];
		convForCsv(dstr, CONVBUFSIZE, bdav->diskname, (size_t)bdav->disknamelen, &param);

		fwprintf(pWriteFile, L"%s\n", dstr);
	}


	// �ԑg���̎擾�Əo��

	ProgInfo	*info = new ProgInfo;

	for (int32_t i = 0; i < bdav->numRpls; i++)												// BDAV�v���C���X�g���̊erpls�ɂ��čs��
	{
		initProgInfo(info);

		readRplsFileProgInfo(i, bdav, info, &param);										// .rpls �t�@�C������̔ԑg���̎擾										
		if (param.bM2tsInfo) readTsFileProgInfo(info, bdav, &param);						// .m2ts �t�@�C������̔ԑg���̎擾. �R�s�[���䂳�ꂽ�f�B�X�N�ł�.m2ts�t�@�C���͈Í�������Ă���̂Ŏ��s����

		outputProgInfo(pWriteFile, info, &param);											// �o�̓I�v�V�����ɏ]���ďo�͂���
	}


	// �I������

	if (!param.bDisplay) fclose(pWriteFile);

	delete info;
	delete bdav;

    return 0;
}


void initCopyParams(CopyParams *param)
{
	param->argSrc  = 0;
	param->argDest = 0;

	param->bShowDiskName = false;

	param->separator	= S_NORMAL;
	param->bNoControl	= true;
	param->bNoComma		= true;
	param->bDQuot		= false;
	param->bDisplay		= false;
	param->bCharSize	= false;
	param->bIVS			= false;
	param->bUseBOM		= true;
	param->bM2tsInfo	= false;

	memset(param->flags, 0, sizeof(param->flags));

	return;
}


bool parseCopyParams(int32_t argn, _TCHAR *args[], CopyParams *param)
{
	int32_t		fnum = 0;

	for (int32_t i = 1; i < argn; i++) {

		if (args[i][0] == L'-') {

			for (int32_t j = 1; j < (int32_t)wcslen(args[i]); j++) {

				switch (args[i][j])
				{
					case L'D':
						param->bShowDiskName = true;
						break;
					case L'f':
						param->flags[fnum++] = F_FileName;
						break;
					case L'j':
						param->flags[fnum++] = F_MtsName;
						break;
					case L'k':
						param->flags[fnum++] = F_MtsSize;
						param->bM2tsInfo = true;
						break;
					case L'd':
						param->flags[fnum++] = F_RecDate;
						break;
					case L't':
						param->flags[fnum++] = F_RecTime;
						break;
					case L'p':
						param->flags[fnum++] = F_RecDuration;
						break;
					case L'z':
						param->flags[fnum++] = F_RecTimeZone;
						break;
					case L'a':
						param->flags[fnum++] = F_MakerID;
						break;
					case L'o':
						param->flags[fnum++] = F_ModelCode;
						break;
					case L'c':
						param->flags[fnum++] = F_ChannelName;
						break;
					case L'n':
						param->flags[fnum++] = F_ChannelNum;
						break;
					case L's':
						param->flags[fnum++] = F_RecSrc;
						break;
					case L'b':
						param->flags[fnum++] = F_ProgName;
						break;
					case L'i':
						param->flags[fnum++] = F_ProgDetail;
						break;
					case L'e':
						param->flags[fnum++] = F_ProgExtend;
						break;
					case L'g':
						param->flags[fnum++] = F_ProgGenre;
						break;
					case L'T':
						param->separator  = S_TAB;
						param->bNoControl = true;
						param->bNoComma   = false;
						param->bDQuot     = false;
						break;
					case L'S':
						param->separator  = S_SPACE;
						param->bNoControl = true;
						param->bNoComma   = false;
						param->bDQuot     = false;
						break;
					case L'C':
						param->separator  = S_CSV;
						param->bNoControl = false;
						param->bNoComma   = false;
						param->bDQuot     = true;
						break;
					case L'y':
						param->bCharSize = true;
						break;
					case L'u':
						param->bIVS		 = true;
						break;
					case L'q':
						param->bUseBOM   = false;
						break;
					case L'M':
						if (parseCopyParamsM(args[i][j++ + 1], fnum++, param)) break;
					default:
						printErrMsg("�����ȃX�C�b�`���w�肳��܂���.\n");
						return	false;
				}

				if (fnum == MAXFLAGNUM) {
					printErrMsg("�X�C�b�`�w�肪�������܂�.\n");
					return	false;
				}
			}

		}
		else {

			if (param->argSrc == 0) {
				param->argSrc = i;
			}
			else if (param->argDest == 0) {
				param->argDest = i;
			}
			else {
				printErrMsg("�p�����[�^���������܂�.\n");
				return false;
			}
		}
	}

	if (param->argSrc == 0) {
		printErrMsg("�p�����[�^������܂���.\n");
		return	false;
	}

	if (param->argDest == 0) param->bDisplay = true;

	return	true;
}


bool parseCopyParamsM(const WCHAR arg, const int32_t fnum, CopyParams *param)
{
	switch (arg)
	{
		case L'd':
			param->flags[fnum] = F_MtsRecDate;
			break;
		case L't':
			param->flags[fnum] = F_MtsRecTime;
			break;
		case L'p':
			param->flags[fnum] = F_MtsRecDuration;
			break;
		case L'c':
			param->flags[fnum] = F_MtsChannelName;
			break;
		case L'n':
			param->flags[fnum] = F_MtsChannelNum;
			break;
		case L'b':
			param->flags[fnum] = F_MtsProgName;
			break;
		case L'i':
			param->flags[fnum] = F_MtsProgDetail;
			break;
		case L'e':
			param->flags[fnum] = F_MtsProgExtend;
			break;
		case L'g':
			param->flags[fnum] = F_MtsProgGenre;
			break;
		default:
			return	false;
	}

	param->bM2tsInfo = true;

	return true;
}


void initProgInfo(ProgInfo* info)
{
	memset(info, 0, sizeof(ProgInfo));

	for (int32_t i = 0; i < 3; i++) {
		info->genre[i]     = -1;
		info->m2tsgenre[i] = -1;
	}

	info->pextendlen = -1;

	return;
}


size_t convForCsv(WCHAR* dbuf, const size_t bufsize, const WCHAR* sbuf, const size_t slen, const CopyParams* param)
{
	size_t	dst = 0;

	if (param->bDQuot && (dst < bufsize)) dbuf[dst++] = 0x0022;		//  �u"�v						// CSV�p�o�͂Ȃ獀�ڂ̑O����u"�v�ň͂�

	for (size_t src = 0; src < slen; src++)
	{
		const WCHAR	s = sbuf[src];
		bool	bOutput = true;

		if (param->bNoControl && (s >= 0) && (s < L' ')) bOutput = false;							// ����R�[�h�͏o�͂��Ȃ�
		if (param->bNoComma && (s == L','))              bOutput = false;							// �R���}�͏o�͂��Ȃ�

		if (param->bDQuot && (s == 0x0022) && (dst < bufsize)) dbuf[dst++] = 0x0022;				// CSV�p�o�͂Ȃ�u"�v�̑O�Ɂu"�v�ŃG�X�P�[�v
		if (bOutput && (dst < bufsize)) dbuf[dst++] = s;											// �o��
	}

	if (param->bDQuot && (dst < bufsize)) dbuf[dst++] = 0x0022;		//  �u"�v

	if (dst < bufsize) dbuf[dst] = 0x0000;

	return dst;
}


void outputProgInfo(FILE* pWriteFile, const ProgInfo* proginfo, const CopyParams* param)
{
	WCHAR		sstr[CONVBUFSIZE];
	WCHAR		dstr[CONVBUFSIZE];

	int32_t		i = 0;

	while (param->flags[i] != 0) {

		size_t	slen = 0;

		// flags�ɉ����ďo�͂��鍀�ڂ�sstr�ɃR�s�[����

		switch (param->flags[i])
		{
			case F_FileName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->rplsname);
				break;
			case F_MtsName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->m2tsname);
				break;
			case F_MtsSize:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%lld", proginfo->m2tssize);
				break;
			case F_RecDate:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.4d/%.2d/%.2d", proginfo->recyear, proginfo->recmonth, proginfo->recday);
				break;
			case F_RecTime:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.2d:%.2d:%.2d", proginfo->rechour, proginfo->recmin, proginfo->recsec);
				break;
			case F_RecDuration:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.2d:%.2d:%.2d", proginfo->durhour, proginfo->durmin, proginfo->dursec);
				break;
			case F_RecTimeZone:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%d", proginfo->rectimezone);
				break;
			case F_MakerID:
				if (proginfo->makerid != -1) {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%d", proginfo->makerid);
				}
				else {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"n/a");
				}
				break;
			case F_ModelCode:
				if (proginfo->modelcode != -1) {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%d", proginfo->modelcode);
				}
				else {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"n/a");
				}
				break;
			case F_RecSrc:
				slen += getRecSrcStr(sstr + slen, CONVBUFSIZE - slen, proginfo->recsrc);
				break;
			case F_ChannelNum:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.3dch", proginfo->chnum);
				break;
			case F_ChannelName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->chname);
				break;
			case F_ProgName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->pname);
				break;
			case F_ProgDetail:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->pdetail);
				break;
			case F_ProgExtend:
				if (proginfo->pextendlen == -1) {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"n/a");
				}
				else {
					slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->pextend);
				}
				break;
			case F_ProgGenre:
				slen += putGenreStr(sstr + slen, CONVBUFSIZE - slen, proginfo->genre);
				break;
			case F_MtsRecDate:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.4d/%.2d/%.2d", proginfo->m2tsrecyear, proginfo->m2tsrecmonth, proginfo->m2tsrecday);
				break;
			case F_MtsRecTime:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.2d:%.2d:%.2d", proginfo->m2tsrechour, proginfo->m2tsrecmin, proginfo->m2tsrecsec);
				break;
			case F_MtsRecDuration:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.2d:%.2d:%.2d", proginfo->m2tsdurhour, proginfo->m2tsdurmin, proginfo->m2tsdursec);
				break;
			case F_MtsChannelNum:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%.3dch", proginfo->m2tschnum);
				break;
			case F_MtsChannelName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->m2tschname);
				break;
			case F_MtsProgName:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->m2tspname);
				break;
			case F_MtsProgDetail:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->m2tspdetail);
				break;
			case F_MtsProgExtend:
				slen += swprintf_s(sstr + slen, CONVBUFSIZE - slen, L"%s", proginfo->m2tspextend);
				break;
			case F_MtsProgGenre:
				slen += putGenreStr(sstr + slen, CONVBUFSIZE - slen, proginfo->m2tsgenre);
				break;
			default:
				slen += 0;
				break;
		}


		// �o�͌`���ɉ�����sstr�𒲐�

		size_t	dlen = convForCsv(dstr, CONVBUFSIZE - 2, sstr, slen, param);


		// �Z�p���[�^�Ɋւ��鏈��

		if (param->flags[i + 1] != 0)
		{
			switch (param->separator)
			{
				case S_NORMAL:
				case S_CSV:
					dstr[dlen++] = L',';								// COMMA
					break;
				case S_TAB:
					dstr[dlen++] = 0x0009;								// TAB
					break;
				case S_SPACE:
					dstr[dlen++] = L' ';								// SPACE
					break;
				default:
					break;
			}
		}
		else
		{
			dstr[dlen++] = 0x000A;										// �S���ڏo�͌�̉��s
		}


		// �f�[�^�o��

		dstr[dlen] = 0x0000;
		fwprintf(pWriteFile, L"%s", dstr);								// �R���\�[�� or �t�@�C���o��

		i++;
	}

	return;
}

