/**
 * @file	oemtext.h
 * @breif	Interface of the text converter
 */

#pragma once

#include "codecnv.h"

#ifdef __cplusplus
extern "C"
{
#endif

UINT oemtext_mbtoucs2(UINT cp, wchar_t *dst, UINT dcnt, const char *src, UINT scnt);
UINT oemtext_ucs2tomb(UINT cp, char *dst, UINT dcnt, const wchar_t *src, UINT scnt);
UINT oemtext_mbtoutf8(UINT cp, char *dst, UINT dcnt, const char *src, UINT scnt);
UINT oemtext_utf8tomb(UINT cp, char *dst, UINT dcnt, const char *src, UINT scnt);

UINT oemtext_chartoucs2(wchar_t *dst, UINT dcnt, const char *src, UINT scnt);
UINT oemtext_ucs2tochar(char *dst, UINT dcnt, const wchar_t *src, UINT scnt);
UINT oemtext_chartoutf8(char *dst, UINT dcnt, const char *src, UINT scnt);
UINT oemtext_utf8tochar(char *dst, UINT dcnt, const char *src, UINT scnt);

#ifdef __cplusplus
}
#endif

#if defined(OSLANG_UCS2)
#define oemtext_sjistooem(a, b, c, d)	oemtext_mbtoucs2(932, a, b, c, d)
#define	oemtext_oemtosjis(a, b, c, d)	oemtext_ucs2tomb(932, a, b, c, d)
#endif



// ---- strres

#define	tchar_null		str_null
#define	tchar_d			str_d
#define	tchar_u			str_u
#define	tchar_2x		str_2x
#define	tchar_2d		str_2d
#define	tchar_4X		str_4X
#define	tchar_bmp		str_bmp
#define	tchar_d88		str_d88
#define	tchar_thd		str_thd
#define	tchar_hdd		str_hdd



// ---- milstr

#define	miltchar_solveHEX(s)	milstr_solveHEX(s)
#define	miltchar_solveINT(s)	milstr_solveINT(s)
