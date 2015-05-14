/**
 * @file	vc6macros.h
 * @brief	VC6 ópÉ}ÉNÉç
 */

#pragma once

#ifndef _countof
//! countof
#define _countof(x)		(sizeof((x)) / sizeof((x)[0]))
#endif	// _countof

#if (_MSC_VER < 1300)
//! for scope
#define for					if (0 /*NEVER*/) { /* no process */ } else for

#endif	// (_MSC_VER < 1300)
