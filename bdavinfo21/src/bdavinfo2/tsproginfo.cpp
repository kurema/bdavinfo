// proginfo.cpp
//


#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <wchar.h>

#include "tsproginfo.h"
#include "bdavinfo2.h"
#include "bdavfileinfo.h"
#include "tsprocess.h"

#include "convtounicode.h"


// �萔�Ȃ�

// #define		__USE_UTF_CODE_CRLF__


// �}�N����`

#define		printMsg(fmt, ...)		_tprintf(_T(fmt), __VA_ARGS__)
#define		printErrMsg(fmt, ...)	_tprintf(_T(fmt), __VA_ARGS__)


//


bool readTsFileProgInfo(ProgInfo *proginfo, const BdavDiskInfo *bdav, const CopyParams *param)
{
	if (proginfo->m2tsname[0] == 0)					// .m2ts �t�@�C�������s���ȏꍇ�irpls�t�@�C�����J���Ȃ������ꍇ�Ȃǁj�́A���̂܂܃t�@�C���� *****.rpls �ɑΉ����� *****.m2ts �Ƃ���B
	{
		wmemcpy(proginfo->m2tsname, proginfo->rplsname, 6);
		wmemcpy(proginfo->m2tsname + 6, L"m2ts", 4);
		proginfo->m2tsname[M2TSNAMELEN] = 0;
	}

	_snwprintf_s(proginfo->m2tsfullpath, _MAX_PATH, L"%s%s", bdav->streamdir, proginfo->m2tsname);			// m2ts�t�@�C���̃t���p�X���쐬


	// m2ts�t�@�C�����J���ă`�F�b�N����

	HANDLE	hFile = CreateFile(proginfo->m2tsfullpath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;													// m2ts �t�@�C�����J���̂Ɏ��s����

	proginfo->m2tssize = GetFileDataSize(hFile);														// �t�@�C���T�C�Y�擾

	const int32_t	psize = tsFileCheck(hFile);															// �t�@�C���`�F�b�N�i188�p�P�b�g, 192�p�P�b�g,  �R�s�[���䂳�ꂽ�Í����t�@�C��)

	if (psize == FILE_INVALID) {																		// TS�t�@�C���ł͂Ȃ��s���ȃt�@�C���i�Í�������Ă��铙�j�Ȃ玸�s
		CloseHandle(hFile);
		return false;
	}

	// m2ts�t�@�C������̔ԑg���̎擾�@(192 byte packet, .m2ts�t�@�C���C�p�[�V����TS�Ȃ̂� SIT �ɔԑg��񂪑��݂���D�K�i�O����BDAV�f�B�X�N�����l���Ĉꉞ EIT ���`�F�b�N����j

	uint8_t	psibuf[PSIBUFSIZE] = { 0 };

	const int32_t	tstype = getSitEit(hFile, psibuf, psize, DEFAULTTSFILEPOS, DEFAULTPACKETLIMIT);

	if (tstype == PID_SIT) {																			// SIT��L����TS�������ꍇ
		parseSit(psibuf, proginfo, param);
	}
	else if (tstype == PID_EIT) {																		// EIT��L����TS�������ꍇ
		const int32_t serviceid = parseEit(psibuf, proginfo, param);
		if (getSdt(hFile, psibuf, psize, serviceid, DEFAULTTSFILEPOS, DEFAULTPACKETLIMIT)) parseSdt(psibuf, proginfo, serviceid, param);
	}
	else {
		CloseHandle(hFile);
		return false;																					// �ԑg���(SIT, EIT)�����o�ł��Ȃ������ꍇ
	}

	CloseHandle(hFile);

	return true;
}


int32_t tsFileCheck(HANDLE hReadFile)
{
	uint32_t	numRead;
	uint8_t		buf[1024] = { 0 };

	SeekFileData(hReadFile, 0);
	ReadFileData(hReadFile, buf, 1024, &numRead);

	if ((buf[188 * 0] == 0x47) && (buf[188 * 1] == 0x47) && (buf[188 * 2] == 0x47) && (buf[188 * 3] == 0x47)) {								// 188 byte packet ts
		return	FILE_188TS;
	}
	else if ((buf[192 * 0 + 4] == 0x47) && (buf[192 * 1 + 4] == 0x47) && (buf[192 * 2 + 4] == 0x47) && (buf[192 * 3 + 4] == 0x47)) {		// 192 byte packet ts
		return	FILE_192TS;
	}

	return	FILE_INVALID;																													// unknown file (�Í�������Ă���ꍇ�Ȃǁj
}


int32_t getSitEit(HANDLE hFile, uint8_t *psibuf, const int32_t psize, const int32_t startpos, const int64_t max_packet_count)
{
	uint8_t		*buf;
	uint8_t		sitbuf[PSIBUFSIZE] = { 0 };
	uint8_t		eitbuf[PSIBUFSIZE] = { 0 };
	uint8_t		patbuf[PSIBUFSIZE] = { 0 };

	bool		bSitFound = false;
	bool		bEitFound = false;
	bool		bPatFound = false;

	bool		bSitStarted = false;
	bool		bEitStarted = false;
	bool		bPatStarted = false;

	int32_t		sitlen = 0;
	int32_t		eitlen = 0;
	int32_t		patlen = 0;

	int32_t		service_id = 0;

	TsReadProcess	tsin;
	initTsFileRead(&tsin, hFile, psize);
	setPosTsFileRead(&tsin, startpos);

	int64_t		packet_count = 0;

	while (1)
	{
		buf = getPacketTsFileRead(&tsin);
		if (buf == NULL) break;
		buf += (tsin.psize - 188);

		const int32_t	pid = getPid(buf);

		if ((pid == PID_SIT) || (pid == PID_EIT) || (pid == PID_PAT))
		{
			const bool		bPsiTop = isPsiTop(buf);
			const int32_t	adaplen = getAdapFieldLength(buf);
			const int32_t	pflen = getPointerFieldLength(buf);				// !bPsiTop�ȏꍇ�͖��Ӗ��Ȓl

			int32_t	len;
			bool	bTop = false;
			bool	bFirstSection = true;
			int32_t	i = adaplen + 4;

			while (i < 188)														// 188�o�C�g�p�P�b�g���ł̊e�Z�N�V�����̃��[�v
			{
				if (!bPsiTop) {
					len = 188 - i;
					bTop = false;
				}
				else if (bFirstSection && (pflen != 0)) {
					i++;
					len = pflen;
					bTop = false;
					bFirstSection = false;
				}
				else {
					if (bFirstSection && (pflen == 0)) {
						i++;
						bFirstSection = false;
					}
					if (buf[i] == 0xFF) break;									// TableID�̂���ׂ��ꏊ��0xFF(stuffing byte)�Ȃ炻�̃p�P�b�g�Ɋւ��鏈���͏I��
					len = getSectionLength(buf + i) + 3;						// �Z�N�V�����w�b�_�̓p�P�b�g�ɂ܂������Ĕz�u����邱�Ƃ͖����͂��Ȃ̂Ńp�P�b�g�͈͊O(188�o�C�g�ȍ~)��ǂ݂ɍs�����Ƃ͖����͂��D
					if (i + len > 188) len = 188 - i;
					bTop = true;
				}

				// SIT�Ɋւ��鏈��

				if (pid == PID_SIT) {
					if (bSitStarted && !bTop) {
						memcpy(sitbuf + sitlen, buf + i, len);
						sitlen += len;
					}
					if (bTop) {
						memset(sitbuf, 0, sizeof(sitbuf));
						memcpy(sitbuf, buf + i, len);
						sitlen = len;
						bSitStarted = true;
					}
					if (bSitStarted && (sitlen >= getSectionLength(sitbuf) + 3)) {
						const int32_t	seclen = getSectionLength(sitbuf);
						const uint32_t	crc = calc_crc32(sitbuf, seclen + 3);
						if ((crc == 0) && (sitbuf[0] == 0x7F)) {
							memcpy(psibuf, sitbuf, seclen + 3);
							bSitFound = true;
							break;
						}
						bSitStarted = false;
					}
				}

				// EIT�Ɋւ��鏈��

				if (pid == PID_EIT) {
					if (bEitStarted && !bTop) {
						memcpy(eitbuf + eitlen, buf + i, len);
						eitlen += len;
					}
					if (bTop) {
						memset(eitbuf, 0, sizeof(eitbuf));
						memcpy(eitbuf, buf + i, len);
						eitlen = len;
						bEitStarted = true;
					}
					if (bEitStarted && (eitlen >= getSectionLength(eitbuf) + 3)) {
						const int32_t	seclen = getSectionLength(eitbuf);
						const uint32_t	crc = calc_crc32(eitbuf, seclen + 3);
						const int32_t	sid = eitbuf[0x03] * 256 + eitbuf[0x04];
						if ((crc == 0) && (eitbuf[0] == 0x4E) && (eitbuf[0x06] == 0) && bPatFound && (sid == service_id) && (seclen > 15)) {
							memcpy(psibuf, eitbuf, seclen + 3);
							bEitFound = true;
							break;
						}
						bEitStarted = false;
					}
				}

				// PAT�Ɋւ��鏈��

				if ((pid == PID_PAT) && !bPatFound) {
					if (bPatStarted && !bTop) {
						memcpy(patbuf + patlen, buf + i, len);
						patlen += len;
					}
					if (bTop) {
						memset(patbuf, 0, sizeof(patbuf));
						memcpy(patbuf, buf + i, len);
						patlen = len;
						bPatStarted = true;
					}
					if (bPatStarted && (patlen >= getSectionLength(patbuf) + 3)) {
						const int32_t	seclen = getSectionLength(patbuf);
						const uint32_t	crc = calc_crc32(patbuf, seclen + 3);
						if ((crc == 0) && (patbuf[0] == 0x00)) {
							int32_t		j = 0x08;
							while (j < seclen - 1) {
								service_id = patbuf[j] * 256 + patbuf[j + 1];
								if (service_id != 0) {
									bPatFound = true;
									break;
								}
								j += 4;
							}
						}
						bPatStarted = false;
					}
				}

				if (bSitFound || bEitFound) break;

				i += len;
			}
		}

		if (bSitFound || bEitFound) break;

		if (max_packet_count != 0) {
			packet_count++;
			if (packet_count >= max_packet_count) break;
		}

	}

	if (bSitFound) return PID_SIT;
	if (bEitFound) return PID_EIT;

	return PID_INVALID;
}


bool getSdt(HANDLE hFile, uint8_t *psibuf, const int32_t psize, const int32_t serviceid, const int32_t startpos, const int64_t max_packet_count)
{
	uint8_t		*buf;
	uint8_t		sdtbuf[PSIBUFSIZE] = { 0 };

	bool		bSdtFound = false;
	bool		bSdtStarted = false;
	int32_t		sdtlen = 0;

	TsReadProcess	tsin;
	initTsFileRead(&tsin, hFile, psize);
	setPosTsFileRead(&tsin, startpos);

	int64_t		packet_count = 0;

	while (1)
	{
		buf = getPacketTsFileRead(&tsin);
		if (buf == NULL) break;
		buf += (psize - 188);

		const int32_t	pid = getPid(buf);

		if (pid == PID_SDT)
		{
			const bool		bPsiTop = isPsiTop(buf);
			const int32_t	adaplen = getAdapFieldLength(buf);
			const int32_t	pflen = getPointerFieldLength(buf);				// !bPsiTop�ȏꍇ�͖��Ӗ��Ȓl

			int32_t		len;
			bool		bTop = false;
			bool		bFirstSection = true;
			int32_t		i = adaplen + 4;

			while (i < 188)														// 188�o�C�g�p�P�b�g���ł̊e�Z�N�V�����̃��[�v
			{
				if (!bPsiTop) {
					len = 188 - i;
					bTop = false;
				}
				else if (bFirstSection && (pflen != 0)) {
					i++;
					len = pflen;
					bTop = false;
					bFirstSection = false;
				}
				else {
					if (bFirstSection && (pflen == 0)) {
						i++;
						bFirstSection = false;
					}
					if (buf[i] == 0xFF) break;									// TableID�̂���ׂ��ꏊ��0xFF(stuffing byte)�Ȃ炻�̃p�P�b�g�Ɋւ��鏈���͏I��
					len = getSectionLength(buf + i) + 3;						// �Z�N�V�����w�b�_�̓p�P�b�g�ɂ܂������Ĕz�u����邱�Ƃ͖����͂��Ȃ̂Ńp�P�b�g�͈͊O(188�o�C�g�ȍ~)��ǂ݂ɍs�����Ƃ͖����͂�
					if (i + len > 188) len = 188 - i;
					bTop = true;
				}

				// SDT�Ɋւ��鏈��

				if ((pid == PID_SDT) && !bSdtFound) {
					if (bSdtStarted && !bTop) {
						memcpy(sdtbuf + sdtlen, buf + i, len);
						sdtlen += len;
					}
					if (bTop) {
						memset(sdtbuf, 0, sizeof(sdtbuf));
						memcpy(sdtbuf, buf + i, len);
						sdtlen = len;
						bSdtStarted = true;
					}
					if (bSdtStarted && (sdtlen >= getSectionLength(sdtbuf) + 3)) {
						const int32_t	seclen = getSectionLength(sdtbuf);
						const uint32_t	crc = calc_crc32(sdtbuf, seclen + 3);
						if ((crc == 0) && (sdtbuf[0] == 0x42)) {
							int32_t		j = 0x0B;
							while (j < seclen) {
								const int32_t	serviceID = sdtbuf[j] * 256 + sdtbuf[j + 1];
								const int32_t	slen = getLength(sdtbuf + j + 0x03);
								if (serviceID == serviceid) {
									memcpy(psibuf, sdtbuf, seclen + 3);
									bSdtFound = true;
									break;
								}
								j += (slen + 5);
							}
						}
						bSdtStarted = false;
					}
				}

				i += len;
			}
		}

		if (bSdtFound) break;

		if (max_packet_count != 0) {
			packet_count++;
			if (packet_count >= max_packet_count) break;
		}

	}

	return bSdtFound;
}


void mjd_dec(const int32_t mjd, int32_t *recyear, int32_t *recmonth, int32_t *recday)
{
	const int32_t	mjd2 = (mjd < 15079) ? mjd + 0x10000 : mjd;

	int32_t			year = (int32_t)(((double)mjd2 - 15078.2) / 365.25);
	const int32_t	year2 = (int32_t)((double)year  * 365.25);
	int32_t			month = (int32_t)(((double)mjd2 - 14956.1 - (double)year2) / 30.6001);
	const int32_t	month2 = (int32_t)((double)month * 30.6001);
	const int32_t	day = mjd2 - 14956 - year2 - month2;

	if ((month == 14) || (month == 15)) {
		year += 1901;
		month = month - 13;
	}
	else {
		year += 1900;
		month = month - 1;
	}

	*recyear = year;
	*recmonth = month;
	*recday = day;

	return;
}


int32_t mjd_enc(const int32_t year, const int32_t month, const int32_t day)
{
	const int32_t	year2 = (month > 2) ? year - 1900 : year - 1901;
	const int32_t	month2 = (month > 2) ? month + 1 : month + 13;

	const int32_t	a = (int32_t)(365.25 * (double)year2);
	const int32_t	b = (int32_t)(30.6001 * (double)month2);
	const int32_t	mjd = 14956 + day + a + b;

	return mjd & 0xFFFF;
}


int comparefornidtable(const void *item1, const void *item2)
{
	return *(int32_t*)item1 - *(int32_t*)item2;
}


int32_t getTbChannelNum(const int32_t networkID, const int32_t serviceID, int32_t remoconkey_id)
{
	static int32_t	nIDTable[] =																			// �l�b�g���[�N����, �����R���L�[���ʂ̊֌W�́AARIB TR-B14 ���Q�Ƃ����D
	{
		0x7FE0, 1,	0x7FE1, 2,	0x7FE2, 4,	0x7FE3, 6,	0x7FE4, 8,	0x7FE5, 5,	0x7FE6, 7,	0x7FE8, 12,		// �֓��L��
		0x7FD1, 2,	0x7FD2, 4,	0x7FD3, 6,	0x7FD4, 8,	0x7FD5, 10,											// �ߋE�L��
		0x7FC1, 2,	0x7FC2, 1,	0x7FC3, 5,	0x7FC4, 6,	0x7FC5, 4,											// �����L��			18��

		0x7FB2, 1,	0x7FB3, 5,	0x7FB4, 6,	0x7FB5, 8,	0x7FB6, 7,											// �k�C����
		0x7FA2, 4,	0x7FA3, 5,	0x7FA4, 6,	0x7FA5, 7,	0x7FA6, 8,											// ���R����
		0x7F92, 8,	0x7F93, 6,	0x7F94, 1,																	// ��������
		0x7F50, 3,	0x7F51, 2,	0x7F52, 1,	0x7F53, 5,	0x7F54, 6,	0x7F55, 8,	0x7F56, 7,					// �k�C���i�D�y�j	20��

		0x7F40, 3,	0x7F41, 2,	0x7F42, 1,	0x7F43, 5,	0x7F44, 6,	0x7F45, 8,	0x7F46, 7,					// �k�C���i���فj
		0x7F30, 3,	0x7F31, 2,	0x7F32, 1,	0x7F33, 5,	0x7F34, 6,	0x7F35, 8,	0x7F36, 7,					// �k�C���i����j
		0x7F20, 3,	0x7F21, 2,	0x7F22, 1,	0x7F23, 5,	0x7F24, 6,	0x7F25, 8,	0x7F26, 7,					// �k�C���i�эL�j	21��

		0x7F10, 3,	0x7F11, 2,	0x7F12, 1,	0x7F13, 5,	0x7F14, 6,	0x7F15, 8,	0x7F16, 7,					// �k�C���i���H�j
		0x7F00, 3,	0x7F01, 2,	0x7F02, 1,	0x7F03, 5,	0x7F04, 6,	0x7F05, 8,	0x7F06, 7,					// �k�C���i�k���j
		0x7EF0, 3,	0x7EF1, 2,	0x7EF2, 1,	0x7EF3, 5,	0x7EF4, 6,	0x7EF5, 8,	0x7EF6, 7,					// �k�C���i�����j
		0x7EE0, 3,	0x7EE1, 2,	0x7EE2, 1,	0x7EE3, 8,	0x7EE4, 4,	0x7EE5, 5,								// �{��				27��

		0x7ED0, 1,	0x7ED1, 2,	0x7ED2, 4,	0x7ED3, 8,	0x7ED4, 5,											// �H�c
		0x7EC0, 1,	0x7EC1, 2,	0x7EC2, 4,	0x7EC3, 5,	0x7EC4, 6,	0x7EC5, 8,								// �R�`
		0x7EB0, 1,	0x7EB1, 2,	0x7EB2, 6,	0x7EB3, 4,	0x7EB4, 8,	0x7EB5, 5,								// ���
		0x7EA0, 1,	0x7EA1, 2,	0x7EA2, 8,	0x7EA3, 4,	0x7EA4, 5,	0x7EA5, 6,								// ����				23��

		0x7E90, 3,	0x7E91, 2,	0x7E92, 1,	0x7E93, 6,	0x7E94, 5,											// �X
		0x7E87, 9,																							// ����
		0x7E77, 3,																							// �_�ސ�
		0x7E60, 1, 0x7E67, 3,																				// �Q�n
		0x7E50, 1,																							// ���
		0x7E47, 3,																							// ��t
		0x7E30, 1, 0x7E37, 3,																				// �Ȗ�
		0x7E27, 3,																							// ���
		0x7E10, 1,	0x7E11, 2,	0x7E12, 4,	0x7E13, 5,	0x7E14, 6,	0x7E15, 8,								// ����
		0x7E00, 1,	0x7E01, 2,	0x7E02, 6,	0x7E03, 8,	0x7E04, 4,	0x7E05, 5,								// �V��
		0x7DF0, 1,	0x7DF1, 2,	0x7DF2, 4,	0x7DF3, 6,														// �R��				28 + 2��

		0x7DE0, 3,	0x7DE6, 10,																				// ���m
		0x7DD0, 1,	0x7DD1, 2,	0x7DD2, 4,	0x7DD3, 5,	0x7DD4, 6,	0x7DD5, 8,								// �ΐ�
		0x7DC0, 1,	0x7DC1, 2,	0x7DC2, 6,	0x7DC3, 8,	0x7DC4, 4,	0x7DC5, 5,								// �É�
		0x7DB0, 1,	0x7DB1, 2,	0x7DB2, 7,	0x7DB3, 8,														// ����
		0x7DA0, 3,	0x7DA1, 2,	0x7DA2, 1,	0x7DA3, 8,	0x7DA4, 6,											// �x�R
		0x7D90, 3,	0x7D96, 7,																				// �O�d
		0x7D80, 3,	0x7D86, 8,																				// ��				27��

		0x7D70, 1,	0x7D76, 7,																				// ���
		0x7D60, 1,	0x7D66, 5,																				// ���s
		0x7D50, 1,	0x7D56, 3,																				// ����
		0x7D40, 1,	0x7D46, 5,																				// �a�̎R
		0x7D30, 1,	0x7D36, 9,																				// �ޗ�
		0x7D20, 1,	0x7D26, 3,																				// ����
		0x7D10, 1,	0x7D11, 2,	0x7D12, 3,	0x7D13, 4,	0x7D14, 5,	0x7D15, 8,								// �L��
		0x7D00, 1,	0x7D01, 2,																				// ���R
		0x7CF0, 3,	0x7CF1, 2,																				// ����
		0x7CE0, 3,	0x7CE1, 2,																				// ����				24��

		0x7CD0, 1,	0x7CD1, 2,	0x7CD2, 4,	0x7CD3, 3,	0x7CD4, 5,											// �R��
		0x7CC0, 1,	0x7CC1, 2,	0x7CC2, 4,	0x7CC3, 5,	0x7CC4, 6,	0x7CC5, 8,								// ���Q
		0x7CB0, 1,	0x7CB1, 2,																				// ����
		0x7CA0, 3,	0x7CA1, 2,	0x7CA2, 1,																	// ����
		0x7C90, 1,	0x7C91, 2,	0x7C92, 4,	0x7C93, 6,	0x7C94, 8,											// ���m				21��

		0x7C80, 3,	0x7C81, 2,	0x7C82, 1,	0x7C83, 4,	0x7C84, 5,	0x7C85, 7,	0x7C86, 8,					// ����
		0x7C70, 1,	0x7C71, 2,	0x7C72, 3,	0x7C73, 8,	0x7C74, 4,	0x7C75, 5,								// �F�{
		0x7C60, 1,	0x7C61, 2,	0x7C62, 3,	0x7C63, 8,	0x7C64, 5,	0x7C65, 4,								// ����
		0x7C50, 3,	0x7C51, 2,	0x7C52, 1,	0x7C53, 8,	0x7C54, 5,	0x7C55, 4,								// ������
		0x7C40, 1,	0x7C41, 2,	0x7C42, 6,	0x7C43, 3,														// �{��				29��

		0x7C30, 1,	0x7C31, 2,	0x7C32, 3,	0x7C33, 4,	0x7C34, 5,											// �啪
		0x7C20, 1,	0x7C21, 2,	0x7C22, 3,																	// ����
		0x7C10, 1,	0x7C11, 2,	0x7C12, 3,	0x7C14, 5,	0x7C17, 8,											// ����				13��

		0x7880, 3,	0x7881, 2,																				// �����i����t���O�j	2��			�v255��
	};

	static bool		bTableInit = false;

	if (!bTableInit) {
		qsort(nIDTable, sizeof(nIDTable) / sizeof(int32_t) / 2, sizeof(int32_t) * 2, comparefornidtable);
		bTableInit = true;
	}

	if (remoconkey_id == 0) {
		void *result = bsearch(&networkID, nIDTable, sizeof(nIDTable) / sizeof(int32_t) / 2, sizeof(int32_t) * 2, comparefornidtable);
		if (result == NULL) return 0;
		remoconkey_id = *((int*)result + 1);
	}

	return remoconkey_id * 10 + (serviceID & 0x0007) + 1;
}


size_t my_memcpy_s(void* dst, const size_t bufsize, const void* src, const size_t srclen)
{
	memcpy(dst, src, (srclen < bufsize) ? srclen : bufsize);

	return (srclen < bufsize) ? srclen : bufsize;
}


void parseSit(const uint8_t *sitbuf, ProgInfo *proginfo, const CopyParams* param)
{

	for (int32_t i = 0; i < 3; i++) proginfo->m2tsgenre[i] = -1;			// �ԑg�W�����������N���A���Ă���

	const int32_t	totallen = getSectionLength(sitbuf);
	const int32_t	firstlen = getLength(sitbuf + 0x08);

	// firstloop����

	int32_t			mediaType = MEDIATYPE_UNKNOWN;
	int32_t			networkID = 0;
	int32_t			serviceID = 0;
	int32_t			remoconkey_id = 0;

	int32_t			i = 0x0A;

	while (i < 0x0A + firstlen)
	{
		switch (sitbuf[i])
		{
		case 0xC2:													// �l�b�g���[�N���ʋL�q�q
			mediaType = sitbuf[i + 5] * 256 + sitbuf[i + 6];
			networkID = sitbuf[i + 7] * 256 + sitbuf[i + 8];
			i += (sitbuf[i + 1] + 2);
			break;
		case 0xCD:													// TS���L�q�q
			remoconkey_id = sitbuf[i + 2];
			i += (sitbuf[i + 1] + 2);
			break;
		default:
			i += (sitbuf[i + 1] + 2);
		}
	}


	// secondloop����

	if ((mediaType == MEDIATYPE_CS) && ((networkID == 0x0001) || (networkID == 0x0003)))		// �X�J�p�[ PerfecTV & SKY �T�[�r�X (���݂͎g���Ă��Ȃ��A�Ǝv��)
	{
		while (i < totallen - 1)
		{
			serviceID = sitbuf[i] * 256 + sitbuf[i + 1];
			const int32_t secondlen = getLength(sitbuf + i + 2);

			i += 4;

			proginfo->m2tschnum = serviceID & 0x03FF;												// serviceID����`�����l���ԍ����擾

			const int32_t	j = i;

			uint8_t		tempbuf[CONVBUFSIZE];
			int32_t		pextendlen = 0;

			while (i < secondlen + j)
			{
				switch (sitbuf[i])
				{
					case 0xC3: {															// �p�[�V�����g�����X�|�[�g�X�g���[���^�C���L�q�q
						const int32_t mjd = sitbuf[i + 3] * 256 + sitbuf[i + 4];
						mjd_dec(mjd, &proginfo->m2tsrecyear, &proginfo->m2tsrecmonth, &proginfo->m2tsrecday);
						proginfo->m2tsrechour = (sitbuf[i + 5] >> 4) * 10 + (sitbuf[i + 5] & 0x0f);
						proginfo->m2tsrecmin = (sitbuf[i + 6] >> 4) * 10 + (sitbuf[i + 6] & 0x0f);
						proginfo->m2tsrecsec = (sitbuf[i + 7] >> 4) * 10 + (sitbuf[i + 7] & 0x0f);
						if ((sitbuf[i + 8] != 0xFF) && (sitbuf[i + 9] != 0xFF) && (sitbuf[i + 10] != 0xFF)) {
							proginfo->m2tsdurhour = (sitbuf[i + 8] >> 4) * 10 + (sitbuf[i + 8] & 0x0f);
							proginfo->m2tsdurmin = (sitbuf[i + 9] >> 4) * 10 + (sitbuf[i + 9] & 0x0f);
							proginfo->m2tsdursec = (sitbuf[i + 10] >> 4) * 10 + (sitbuf[i + 10] & 0x0f);
						}
						i += (sitbuf[i + 1] + 2);
						break;
					}
					case 0xB2:																// �ǖ��Ɣԑg���Ɋւ���L�q�q
						if (sitbuf[i + 3] == 0x01) {
							const size_t	chnamelen = sitbuf[i + 2];
							const size_t	pnamelen  = sitbuf[i + 3 + chnamelen];
							proginfo->m2tschnamelen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tschname, CONVBUFSIZE, sitbuf + i + 4, chnamelen - 1, param->bCharSize, param->bIVS);
							proginfo->m2tspnamelen  = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspname, CONVBUFSIZE, sitbuf + i + 5 + chnamelen, pnamelen - 1, param->bCharSize, param->bIVS);
						}
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x83: {															// �ԑg�ڍ׏��Ɋւ���L�q�q
						int32_t len = sitbuf[i + 1] - 1;
						memcpy(tempbuf + pextendlen, sitbuf + i + 3, len);
						pextendlen += len;
						i += (sitbuf[i + 1] + 2);
						break;
					}
					default:
						i += (sitbuf[i + 1] + 2);
				}
			}

			proginfo->m2tspextendlen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend, CONVBUFSIZE, tempbuf, pextendlen, param->bCharSize, param->bIVS);
		}
	}
	else // if( (mediaType == MEDIATYPE_BS) || (mediaType == MEDIATYPE_TB) || ( (mediaType == MEDIATYPE_CS) && (networkID == 0x000A) ) )		// �n��f�W�^��, BS�f�W�^��, (���炭�X�J�p�[HD��)
	{
		uint8_t		tempbuf[CONVBUFSIZE] = { 0 };
		int32_t		templen = 0;

		while (i < totallen - 1)
		{
			serviceID = sitbuf[i] * 256 + sitbuf[i + 1];				// �T�[�r�XID

			const int32_t	secondlen = getLength(sitbuf + i + 2);

			if (mediaType == MEDIATYPE_TB) {
				proginfo->m2tschnum = getTbChannelNum(networkID, serviceID, remoconkey_id);
			}
			else {
				proginfo->m2tschnum = serviceID & 0x03FF;
			}

			proginfo->m2tspextendlen = 0;
			templen = 0;

			i += 4;
			const int32_t	j = i;

			while (i < secondlen + j)
			{
				switch (sitbuf[i])
				{
					case 0xC3: {												// �p�[�V�����g�����X�|�[�g�X�g���[���^�C���L�q�q
						const int32_t mjd = sitbuf[i + 3] * 256 + sitbuf[i + 4];
						mjd_dec(mjd, &proginfo->m2tsrecyear, &proginfo->m2tsrecmonth, &proginfo->m2tsrecday);
						proginfo->m2tsrechour = (sitbuf[i + 5] >> 4) * 10 + (sitbuf[i + 5] & 0x0f);
						proginfo->m2tsrecmin = (sitbuf[i + 6] >> 4) * 10 + (sitbuf[i + 6] & 0x0f);
						proginfo->m2tsrecsec = (sitbuf[i + 7] >> 4) * 10 + (sitbuf[i + 7] & 0x0f);
						if ((sitbuf[i + 8] != 0xFF) && (sitbuf[i + 9] != 0xFF) && (sitbuf[i + 10] != 0xFF)) {
							proginfo->m2tsdurhour = (sitbuf[i + 8] >> 4) * 10 + (sitbuf[i + 8] & 0x0f);
							proginfo->m2tsdurmin = (sitbuf[i + 9] >> 4) * 10 + (sitbuf[i + 9] & 0x0f);
							proginfo->m2tsdursec = (sitbuf[i + 10] >> 4) * 10 + (sitbuf[i + 10] & 0x0f);
						}
						i += (sitbuf[i + 1] + 2);
						break;
					}
					case 0x54: {													// �R���e���g�L�q�q
						int32_t numGenre = 0;
						for (uint32_t n = 0; n < sitbuf[i + 1]; n += 2) {
							if (((sitbuf[i + 2 + n] & 0xF0) != 0xE0) && (numGenre < 3)) {
								proginfo->m2tsgenre[numGenre] = sitbuf[i + 2 + n];
								numGenre++;
							}
						}
						i += (sitbuf[i + 1] + 2);
						break;
					}
					case 0x48: {													// �T�[�r�X�L�q�q
						const int32_t provnamelen = sitbuf[i + 3];
						const int32_t servnamelen = sitbuf[i + 4 + provnamelen];
						proginfo->m2tschnamelen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tschname, CONVBUFSIZE, sitbuf + i + 5 + provnamelen, servnamelen, param->bCharSize, param->bIVS);
						i += (sitbuf[i + 1] + 2);
						break;
					}
					case 0x4D: {													// �Z�`���C�x���g�L�q�q
						const int32_t prognamelen   = sitbuf[i + 5];
						const int32_t progdetaillen = sitbuf[i + 6 + prognamelen];
						proginfo->m2tspnamelen   = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspname, CONVBUFSIZE, sitbuf + i + 6, prognamelen, param->bCharSize, param->bIVS);
						proginfo->m2tspdetaillen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspdetail, CONVBUFSIZE, sitbuf + i + 7 + prognamelen, progdetaillen, param->bCharSize, param->bIVS);
						i += (sitbuf[i + 1] + 2);
						break;
					}
					case 0x4E: {												// �g���`���C�x���g�L�q�q
						const int32_t	itemlooplen = sitbuf[i + 6];
						i += 7;
						const int32_t	k = i;
						while (i < itemlooplen + k)
						{
							const int32_t	idesclen = sitbuf[i];
							const int32_t	itemlen = sitbuf[i + 1 + idesclen];

							if (idesclen != 0) {
								if (proginfo->m2tspextendlen != 0) {
									if (templen != 0) {
										if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, tempbuf, templen, param->bCharSize, param->bIVS);
										templen = 0;
									}
#ifdef __USE_UTF_CODE_CRLF__
									if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
									if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
#ifdef __USE_UTF_CODE_CRLF__
									if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
									if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
								}
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, sitbuf + i + 1, idesclen, param->bCharSize, param->bIVS);
#ifdef __USE_UTF_CODE_CRLF__
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
							}
							templen += (int32_t)my_memcpy_s(tempbuf + templen, CONVBUFSIZE - templen, sitbuf + i + 2 + idesclen, itemlen);
							i += (idesclen + itemlen + 2);
						}
						const int32_t	iextlen = sitbuf[i];
						i += (iextlen + 1);
						break;
					}
					default:
						i += (sitbuf[i + 1] + 2);
				}
			}

			if (templen != 0) {
				if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, tempbuf, templen, param->bCharSize, param->bIVS);
			}
		}
	}

	return;
}


int32_t parseEit(const uint8_t *eitbuf, ProgInfo *proginfo, const CopyParams* param)
{
	for (int32_t i = 0; i < 3; i++) proginfo->m2tsgenre[i] = -1;			// �ԑg�W�����������N���A���Ă���

	const int32_t 	totallen	= getSectionLength(eitbuf);
	const int32_t	serviceID	= eitbuf[0x03] * 256 + eitbuf[0x04];
	const int32_t	networkID	= eitbuf[0x0A] * 256 + eitbuf[0x0B];

	proginfo->m2tschnum = ((networkID >= 0x7880) && (networkID <= 0x7FE8)) ? getTbChannelNum(networkID, serviceID, 0) : serviceID & 0x03FF;	// �`�����l���ԍ��̎擾


	// totalloop����

	int32_t		i = 0x0E;

	uint8_t		tempbuf[CONVBUFSIZE];
	int32_t		templen = 0;

	while (i < totallen - 1)
	{
		const int32_t	mjd = eitbuf[i + 2] * 256 + eitbuf[i + 3];
		mjd_dec(mjd, &proginfo->m2tsrecyear, &proginfo->m2tsrecmonth, &proginfo->m2tsrecday);
		proginfo->m2tsrechour	= (eitbuf[i + 4] >> 4) * 10 + (eitbuf[i + 4] & 0x0f);
		proginfo->m2tsrecmin	= (eitbuf[i + 5] >> 4) * 10 + (eitbuf[i + 5] & 0x0f);
		proginfo->m2tsrecsec	= (eitbuf[i + 6] >> 4) * 10 + (eitbuf[i + 6] & 0x0f);
		if( (eitbuf[i + 7] != 0xFF) && (eitbuf[i + 8] != 0xFF) && (eitbuf[i + 9] != 0xFF) ) {
			proginfo->m2tsdurhour	= (eitbuf[i + 7] >> 4) * 10 + (eitbuf[i + 7] & 0x0f);
			proginfo->m2tsdurmin	= (eitbuf[i + 8] >> 4) * 10 + (eitbuf[i + 8] & 0x0f);
			proginfo->m2tsdursec	= (eitbuf[i + 9] >> 4) * 10 + (eitbuf[i + 9] & 0x0f);
		}

		const int32_t	seclen = getLength(eitbuf + i + 0x0A);

		i += 0x0C;

		// secondloop����

		proginfo->m2tspextendlen = 0;
		templen = 0;

		const int32_t	j = i;

		while (i < seclen + j)
		{
			switch (eitbuf[i])
			{
				case 0x54: {												// �R���e���g�L�q�q
					int32_t	numGenre = 0;
					for (uint32_t n = 0; n < eitbuf[i + 1]; n += 2) {
						if ((eitbuf[i + 2 + n] & 0xF0) != 0xE0) {
							proginfo->m2tsgenre[numGenre] = eitbuf[i + 2 + n];
							numGenre++;
						}
					}
					i += (eitbuf[i + 1] + 2);
					break;
				}
				case 0x4D: {												// �Z�`���C�x���g�L�q�q
					const int32_t	prognamelen   = eitbuf[i + 5];
					const int32_t	progdetaillen = eitbuf[i + 6 + prognamelen];
					proginfo->m2tspnamelen   = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspname, CONVBUFSIZE, eitbuf + i + 6, prognamelen, param->bCharSize, param->bIVS);
					proginfo->m2tspdetaillen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspdetail, CONVBUFSIZE, eitbuf + i + 7 + prognamelen, progdetaillen, param->bCharSize, param->bIVS);
					i += (eitbuf[i + 1] + 2);
					break;
				}
				case 0x4E: {												// �g���`���C�x���g�L�q�q
					const int32_t	itemlooplen = eitbuf[i + 6];
					i += 7;
					const int32_t	k = i;
					while (i < itemlooplen + k)
					{
						const int32_t	idesclen = eitbuf[i];
						const int32_t	itemlen = eitbuf[i + 1 + idesclen];

						if (idesclen != 0) {
							if (proginfo->m2tspextendlen != 0) {
								if (templen != 0) {
									if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, tempbuf, templen, param->bCharSize, param->bIVS);
									templen = 0;
								}
#ifdef __USE_UTF_CODE_CRLF__
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
#ifdef __USE_UTF_CODE_CRLF__
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
								if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
							}
							if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, eitbuf + i + 1, idesclen, param->bCharSize, param->bIVS);
#ifdef __USE_UTF_CODE_CRLF__
							if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000D;
#endif
							if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextend[proginfo->m2tspextendlen++] = 0x000A;
						}
						templen += (int32_t)my_memcpy_s(tempbuf + templen, CONVBUFSIZE - templen, eitbuf + i + 2 + idesclen, itemlen);
						i += (idesclen + itemlen + 2);
					}
					const int32_t	iextlen = eitbuf[i];
					i += (iextlen + 1);
					break;
				}
				default:
					i += (eitbuf[i + 1] + 2);
			}
		}

		if (templen != 0) {
			if (proginfo->m2tspextendlen < CONVBUFSIZE) proginfo->m2tspextendlen += (int32_t)conv_to_unicode((char16_t*)proginfo->m2tspextend + proginfo->m2tspextendlen, CONVBUFSIZE - proginfo->m2tspextendlen, tempbuf, templen, param->bCharSize, param->bIVS);
		}
	}

	return serviceID;
}


void parseSdt(const uint8_t *sdtbuf, ProgInfo *proginfo, const int32_t serviceid, const CopyParams* param)
{
	const int32_t	totallen = getSectionLength(sdtbuf);

	int32_t		i = 0x0B;

	while (i < totallen - 1)
	{
		const int32_t	serviceID = sdtbuf[i] * 256 + sdtbuf[i + 1];
		const int32_t	secondlen = getLength(sdtbuf + i + 3);

		if (serviceID == serviceid) {
			i += 5;
			const int32_t	j = i;
			while (i < secondlen + j)
			{
				switch (sdtbuf[i])
				{
					case 0x48: {													// �T�[�r�X�L�q�q
						const int32_t	provnamelen = sdtbuf[i + 3];
						const int32_t	servnamelen = sdtbuf[i + 4 + provnamelen];
						proginfo->m2tschnamelen = (int32_t)conv_to_unicode((char16_t*)proginfo->m2tschname, CONVBUFSIZE, sdtbuf + i + 5 + provnamelen, servnamelen, param->bCharSize, param->bIVS);
						i += (sdtbuf[i + 1] + 2);
						break;
					}
					default:
						i += (sdtbuf[i + 1] + 2);
				}
			}
			break;
		}
		else {
			i += (secondlen + 5);
		}
	}

	return;
}


size_t putGenreStr(WCHAR *buf, const size_t bufsize, const int32_t* genre)
{
	const WCHAR	*str_genreL[] = {
		L"�j���[�X�^��",			L"�X�|�[�c",	L"���^���C�h�V���[",	L"�h���}",
		L"���y",					L"�o���G�e�B",	L"�f��",				L"�A�j���^���B",
		L"�h�L�������^���[�^���{",	L"����^����",	L"��^����",			L"����",
		L"�\��",					L"�\��",		L"�g��",				L"���̑�"
	};

	const WCHAR	*str_genreM[] = {
		L"�莞�E����", L"�V�C", L"���W�E�h�L�������g", L"�����E����", L"�o�ρE�s��", L"�C�O�E����", L"���", L"���_�E��k",
		L"�񓹓���", L"���[�J���E�n��", L"���", L"-", L"-", L"-", L"-", L"���̑�",

		L"�X�|�[�c�j���[�X", L"�싅", L"�T�b�J�[", L"�S���t", L"���̑��̋��Z", L"���o�E�i���Z", L"�I�����s�b�N�E���ۑ��", L"�}���\���E����E���j",
		L"���[�^�[�X�|�[�c", L"�}�����E�E�B���^�[�X�|�[�c", L"���n�E���c���Z", L"-", L"-", L"-", L"-", L"���̑�",

		L"�|�\�E���C�h�V���[", L"�t�@�b�V����", L"��炵�E�Z�܂�", L"���N�E���", L"�V���b�s���O�E�ʔ�", L"�O�����E����", L"�C�x���g", L"�ԑg�Љ�E���m�点",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"�����h���}", L"�C�O�h���}", L"���㌀", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"�������b�N�E�|�b�v�X", L"�C�O���b�N�E�|�b�v�X", L"�N���V�b�N�E�I�y��", L"�W���Y�E�t���[�W����", L"�̗w�ȁE����", L"���C�u�E�R���T�[�g", L"�����L���O�E���N�G�X�g", L"�J���I�P�E�̂ǎ���",
		L"���w�E�M�y", L"���w�E�L�b�Y", L"�������y�E���[���h�~���[�W�b�N", L"-", L"-", L"-", L"-", L"���̑�",

		L"�N�C�Y", L"�Q�[��", L"�g�[�N�o���G�e�B", L"���΂��E�R���f�B", L"���y�o���G�e�B", L"���o���G�e�B", L"�����o���G�e�B", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"�m��", L"�M��", L"�A�j��", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"�����A�j��", L"�C�O�A�j��", L"���B", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"�Љ�E����", L"���j�E�I�s", L"���R�E�����E��", L"�F���E�Ȋw�E��w", L"�J���`���[�E�`���|�\", L"���w�E���|", L"�X�|�[�c", L"�h�L�������^���[�S��",
		L"�C���^�r���[�E���_", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"���㌀�E�V��", L"�~���[�W�J��", L"�_���X�E�o���G", L"����E���|", L"�̕���E�ÓT", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"���E�ނ�E�A�E�g�h�A", L"���|�E�y�b�g�E��|", L"���y�E���p�E�H�|", L"�͌�E����", L"�����E�p�`���R", L"�ԁE�I�[�g�o�C", L"�R���s���[�^�E�s�u�Q�[��", L"��b�E��w",
		L"�c���E���w��", L"���w���E���Z��", L"��w���E��", L"���U����E���i", L"������", L"-", L"-", L"���̑�",

		L"�����", L"��Q��", L"�Љ��", L"�{�����e�B�A", L"��b", L"�����i�����j", L"�������", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�",

		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",

		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",

		L"BS/�n��f�W�^�������p�ԑg�t�����", L"�L�ш�CS�f�W�^�������p�g��", L"�q���f�W�^�����������p�g��", L"�T�[�o�[�^�ԑg�t�����", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",

		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"-",
		L"-", L"-", L"-", L"-", L"-", L"-", L"-", L"���̑�"
	};

	size_t	len;

	if(genre[2] != -1) {
		len =  swprintf_s(buf, bufsize, L"%s �k%s�l�@%s �k%s�l�@%s �k%s�l", str_genreL[genre[0] >> 4], str_genreM[genre[0]], str_genreL[genre[1] >> 4], str_genreM[genre[1]], str_genreL[genre[2] >> 4], str_genreM[genre[2]]);
	} else if(genre[1] != -1) {
		len =  swprintf_s(buf, bufsize, L"%s �k%s�l�@%s �k%s�l", str_genreL[genre[0] >> 4], str_genreM[genre[0]], str_genreL[genre[1] >> 4], str_genreM[genre[1]]);
	} else if(genre[0] != -1) {
		len =  swprintf_s(buf, bufsize, L"%s �k%s�l", str_genreL[genre[0] >> 4],  str_genreM[genre[0]]);
	} else {
		len =  swprintf_s(buf, bufsize, L"n/a");
	}

	return len;
}