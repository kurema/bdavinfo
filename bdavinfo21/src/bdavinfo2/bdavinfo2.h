#pragma once

#include <windows.h>
#include <stdint.h>

// #include "bdavfileinfo.h"


// 定数など

#define		F_FileName			1
#define		F_MtsName			2
#define		F_MtsSize			3
#define		F_RecDate			4
#define		F_RecTime			5
#define		F_RecDuration		6
#define		F_RecTimeZone		7
#define		F_MakerID			8
#define		F_ModelCode			9
#define		F_ChannelName		10
#define		F_ChannelNum		11
#define		F_RecSrc			12
#define		F_ProgName			13
#define		F_ProgDetail		14
#define		F_ProgExtend		15
#define		F_ProgGenre			16
#define		F_MtsRecDate		17
#define		F_MtsRecTime		18
#define		F_MtsRecDuration	19
#define		F_MtsChannelName	21
#define		F_MtsChannelNum		22
#define		F_MtsProgName		23
#define		F_MtsProgDetail		24
#define		F_MtsProgExtend		25
#define		F_MtsProgGenre		26

#define		S_NORMAL			0
#define		S_TAB				1
#define		S_SPACE				2
#define		S_CSV				3

#define		MAXFLAGNUM			256
#define		CONVBUFSIZE			16384


// 番組情報構造体

typedef struct {
	WCHAR		rplsname[_MAX_PATH + 1];
	WCHAR		rplsfullpath[_MAX_PATH + 1];
	int32_t		recyear;
	int32_t		recmonth;
	int32_t		recday;
	int32_t		rechour;
	int32_t		recmin;
	int32_t		recsec;
	int32_t		durhour;
	int32_t		durmin;
	int32_t		dursec;
	int32_t		rectimezone;
	int32_t		makerid;
	int32_t		modelcode;
	int32_t		chnum;
	WCHAR		chname[CONVBUFSIZE];
	int32_t		chnamelen;
	WCHAR		pname[CONVBUFSIZE];
	int32_t		pnamelen;
	WCHAR		pdetail[CONVBUFSIZE];
	int32_t		pdetaillen;
	WCHAR		pextend[CONVBUFSIZE];
	int32_t		pextendlen;
	int32_t		genre[3];
	int32_t		recsrc;
	WCHAR		m2tsname[_MAX_PATH + 1];
	WCHAR		m2tsfullpath[_MAX_PATH + 1];
	int64_t		m2tssize;
	int32_t		m2tsrecyear;
	int32_t		m2tsrecmonth;
	int32_t		m2tsrecday;
	int32_t		m2tsrechour;
	int32_t		m2tsrecmin;
	int32_t		m2tsrecsec;
	int32_t		m2tsdurhour;
	int32_t		m2tsdurmin;
	int32_t		m2tsdursec;
	int32_t		m2tschnum;
	WCHAR		m2tschname[CONVBUFSIZE];
	int32_t		m2tschnamelen;
	WCHAR		m2tspname[CONVBUFSIZE];
	int32_t		m2tspnamelen;
	WCHAR		m2tspdetail[CONVBUFSIZE];
	int32_t		m2tspdetaillen;
	WCHAR		m2tspextend[CONVBUFSIZE];
	int32_t		m2tspextendlen;
	int32_t		m2tsgenre[3];
} ProgInfo;


// パラメータ構造体

typedef struct {
	int32_t		argSrc;
	int32_t		argDest;
	int32_t		separator;
	int32_t		flags[MAXFLAGNUM + 1];
	bool		bShowDiskName;
	bool		bNoControl;
	bool		bNoComma;
	bool		bDQuot;
	bool		bDisplay;
	bool		bCharSize;
	bool		bIVS;
	bool		bM2tsInfo;
	bool		bUseBOM;
} CopyParams;