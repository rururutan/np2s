/*
 * Copyright (c) 2004 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	VIRTUAL_ADDRESS_MEMORY_ACCESS_FUNCTION(width, valtype, length) \
valtype MEMCALL \
cpu_vmemoryread_##width(int idx, UINT32 offset) \
{ \
	descriptor_t *sdp; \
	UINT32 addr; \
	int exc; \
\
	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM); \
\
	sdp = &CPU_STAT_SREG(idx); \
	if (!SEG_IS_VALID(sdp)) { \
		exc = GP_EXCEPTION; \
		goto err; \
	} \
\
	if (!(sdp->flag & CPU_DESC_FLAG_READABLE)) { \
		cpu_memoryread_check(sdp, offset, (length), \
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION); \
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) { \
		if (!check_limit_upstairs(sdp, offset, (length))) \
			goto range_failure; \
	} \
	addr = sdp->u.seg.segbase + offset; \
	check_memory_break_point(addr, (length), CPU_DR7_RW_RO); \
	if (!CPU_STAT_PAGING) \
		return cpu_memoryread_##width(addr); \
	return cpu_linear_memory_read_##width(addr, CPU_PAGE_READ_DATA | CPU_STAT_USER_MODE); \
\
range_failure: \
	VERBOSE(("cpu_vmemoryread_" #width ": type = %d, offset = %08x, length = %d, limit = %08x", sdp->type, offset, length, sdp->u.seg.limit)); \
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION; \
err: \
	EXCEPTION(exc, 0); \
	return 0;	/* compiler happy */ \
} \
\
void MEMCALL \
cpu_vmemorywrite_##width(int idx, UINT32 offset, valtype value) \
{ \
	descriptor_t *sdp; \
	UINT32 addr; \
	int exc; \
\
	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM); \
\
	sdp = &CPU_STAT_SREG(idx); \
	if (!SEG_IS_VALID(sdp)) { \
		exc = GP_EXCEPTION; \
		goto err; \
	} \
\
	if (!(sdp->flag & CPU_DESC_FLAG_WRITABLE)) { \
		cpu_memorywrite_check(sdp, offset, (length), \
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION); \
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) { \
		if (!check_limit_upstairs(sdp, offset, (length))) \
			goto range_failure; \
	} \
	addr = sdp->u.seg.segbase + offset; \
	check_memory_break_point(addr, (length), CPU_DR7_RW_RW); \
	if (!CPU_STAT_PAGING) { \
		cpu_memorywrite_##width(addr, value); \
	} else { \
		cpu_linear_memory_write_##width(addr, value, CPU_PAGE_WRITE_DATA | CPU_STAT_USER_MODE); \
	} \
	return; \
\
range_failure: \
	VERBOSE(("cpu_vmemorywrite_" #width ": type = %d, offset = %08x, length = %d, limit = %08x", sdp->type, offset, length, sdp->u.seg.limit)); \
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION; \
err: \
	EXCEPTION(exc, 0); \
} \
\
UINT32 MEMCALL \
cpu_memory_access_va_RMW_##width(int idx, UINT32 offset, UINT32 (CPUCALL *func)(UINT32, void *), void *arg) \
{ \
	descriptor_t *sdp; \
	UINT32 addr; \
	UINT32 res, dst; \
	int exc; \
\
	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM); \
\
	sdp = &CPU_STAT_SREG(idx); \
	if (!SEG_IS_VALID(sdp)) { \
		exc = GP_EXCEPTION; \
		goto err; \
	} \
\
	if (!(sdp->flag & CPU_DESC_FLAG_WRITABLE)) { \
		cpu_memorywrite_check(sdp, offset, (length), \
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION); \
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) { \
		if (!check_limit_upstairs(sdp, offset, (length))) \
			goto range_failure; \
	} \
	addr = sdp->u.seg.segbase + offset; \
	check_memory_break_point(addr, (length), CPU_DR7_RW_RW); \
	if (!CPU_STAT_PAGING) { \
		dst = cpu_memoryread_##width(addr); \
		res = (*func)(dst, arg); \
		cpu_memorywrite_##width(addr, res); \
	} else { \
		dst = cpu_memory_access_la_RMW_##width(addr, func, arg); \
	} \
	return dst; \
\
range_failure: \
	VERBOSE(("cpu_memory_access_va_RMW_" #width ": type = %d, offset = %08x, length = %d, limit = %08x", sdp->type, offset, length, sdp->u.seg.limit)); \
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION; \
err: \
	EXCEPTION(exc, 0); \
	return 0;	/* compiler happy */ \
}
