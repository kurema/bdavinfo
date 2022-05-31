#pragma once

#include <Windows.h>
#include <stdint.h>

#include "bdavinfo2.h"
#include "bdavfileinfo.h"


// �萔

#define		MEDIATYPE_TB		0x5442
#define		MEDIATYPE_BS		0x4253
#define		MEDIATYPE_CS		0x4353
#define		MEDIATYPE_UNKNOWN	0xFFFF

#define		PID_PAT				0x0000
#define		PID_SIT				0x001f
#define		PID_EIT				0x0012
#define		PID_NIT				0x0010
#define		PID_SDT				0x0011
#define		PID_BIT				0x0024
#define		PID_INVALID			0xffff

#define		PSIBUFSIZE			8192

#define		FILE_INVALID		0
#define		FILE_188TS			188
#define		FILE_192TS			192

#define		DEFAULTTSFILEPOS	50
#define		DEFAULTPACKETLIMIT	0



// #define		MAXDESCSIZE			258
// #define		MAXEXTEVENTSIZE		4096

//




// �v���g�^�C�v�錾

bool			readTsFileProgInfo(ProgInfo *, const BdavDiskInfo *, const CopyParams *);
int32_t			tsFileCheck(HANDLE);

int32_t			getSitEit(HANDLE, uint8_t*, const int32_t, const int32_t, const int64_t);
bool			getSdt(HANDLE, uint8_t *, const int32_t, const int32_t, const int32_t, const int64_t);
void			mjd_dec(const int32_t, int32_t*, int32_t*, int32_t*);
int32_t			mjd_enc(const int32_t, const int32_t, const int32_t);
int				comparefornidtable(const void*, const void*);
int32_t			getTbChannelNum(const int32_t, const int32_t, int32_t);

void			parseSit(const uint8_t*, ProgInfo*, const CopyParams*);
int32_t			parseEit(const uint8_t*, ProgInfo*, const CopyParams*);
void			parseSdt(const uint8_t*, ProgInfo*, const int32_t, const CopyParams*);

size_t			putGenreStr(WCHAR *, const size_t, const int32_t *);

