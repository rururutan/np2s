/**
 * @file	oemtext.h
 * @breif	defines converter
 */

#pragma once

#include "codecnv.h"

#define	oemtext_oemtosjis(a, b, c, d)	codecnv_utf8tosjis(a, b, c, d)
#define	oemtext_sjistooem(a, b, c, d)	codecnv_sjistoutf8(a, b, c, d)
