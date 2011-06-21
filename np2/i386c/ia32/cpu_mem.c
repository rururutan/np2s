/*
 * Copyright (c) 2002-2004 NONAKA Kimihiro
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

#include "compiler.h"
#include "cpu.h"
#include "memory.h"


/*
 * memory access check
 */
static int MEMCALL check_limit_upstairs(descriptor_t *sdp, UINT32 offset, UINT len);
static void MEMCALL cpu_memoryread_check(descriptor_t *sdp, UINT32 offset, UINT len, int e);
static void MEMCALL cpu_memorywrite_check(descriptor_t *sdp, UINT32 offset, UINT len, int e);

static int MEMCALL
check_limit_upstairs(descriptor_t *sdp, UINT32 offset, UINT len)
{
	UINT32 limit;
	UINT32 end;

	__ASSERT(sdp != NULL);
	__ASSERT(len > 0);

	len--;
	end = offset + len;
	limit = SEG_IS_32BIT(sdp) ? 0xffffffff : 0x0000ffff;

	if (SEG_IS_DATA(sdp) && SEG_IS_EXPANDDOWN_DATA(sdp)) {
		/* expand-down data segment */
		if (sdp->u.seg.limit == 0) {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |       |
			 * |       |   +  [1]  + 0000FFFFh
			 * | valid |   |       |
			 * |       |   +-------+ 0000FFFFh - len -1
			 * |       |   | valid |
			 * +-------+   +-------+ 00000000h
			 */
			if (!SEG_IS_32BIT(sdp)) {
				if ((len > limit)		/* len check */
				 || (end > limit)) {		/* [1] */
					return 0;
				}
			} else {
				sdp->flag |= CPU_DESC_FLAG_WHOLEADR;
			}
		} else {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |  [2]  |   |       |
			 * +-------+   +.......+ FFFFFFFFh - len - 1
			 * |       |   |  [2]  |
			 * |       |   +.......+ 0000FFFFh
			 * | valid |   |       |
			 * |       |   +-------+ 0000FFFFh - len - 1
			 * |       |   | valid |
			 * +-------+   +-------+ seg.limit
			 * |  [1]  |   |  [1]  |
			 * +-------+   +-------+ 00000000h
			 */
			if ((len > limit - sdp->u.seg.limit)	/* len check */
			 || (end < offset)			/* wrap check */
			 || (offset < sdp->u.seg.limit) 	/* [1] */
			 || (end > limit)) {			/* [2] */
				return 0;
			}
		}
	} else {
		/* expand-up data or code segment */
		if (sdp->u.seg.limit == limit) {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |       |
			 * |       |   +  [1]  + 0000FFFFh
			 * | valid |   |       |
			 * |       |   +-------+ 0000FFFFh - len - 1
			 * |       |   | valid |
			 * +-------+   +-------+ 00000000h
			 */
			if (!SEG_IS_32BIT(sdp)) {
				if ((len > limit)		/* len check */
				 || (offset + len > limit)) {	/* [1] */
					return 0;
				}
			} else {
				sdp->flag |= CPU_DESC_FLAG_WHOLEADR;
			}
		} else {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |       |
			 * |       |   +.......+ 0000FFFFh
			 * |  [1]  |   |  [1]  |
			 * +.......+   +.......+ seg.limit
			 * |       |   |       |
			 * +-------+   +-------+ seg.limit - len - 1
			 * | valid |   | valid |
			 * +-------+   +-------+ 00000000h
			 */
			if ((len > sdp->u.seg.limit)		/* len check */
			 || (end < offset)			/* wrap check */
			 || (end > sdp->u.seg.limit)) {		/* [1] */
				return 0;
			}
		}
	}
	return 1;	/* Ok! */
}

static void MEMCALL
cpu_memoryread_check(descriptor_t *sdp, UINT32 offset, UINT len, int e)
{

	__ASSERT(sdp != NULL);
	__ASSERT(len > 0);

	if (!SEG_IS_VALID(sdp)) {
		e = GP_EXCEPTION;
		goto exc;
	}
	if (!SEG_IS_PRESENT(sdp)
	 || SEG_IS_SYSTEM(sdp)
	 || (SEG_IS_CODE(sdp) && !SEG_IS_READABLE_CODE(sdp))) {
		goto exc;
	}

	switch (sdp->type) {
	case 0:	 case 1:	/* ro */
	case 2:  case 3:	/* rw */
	case 4:  case 5:	/* ro (expand down) */
	case 6:  case 7:	/* rw (expand down) */
	case 10: case 11:	/* rx */
	case 14: case 15:	/* rxc */
		if (!check_limit_upstairs(sdp, offset, len))
			goto exc;
		break;

	default:
		goto exc;
	}
	sdp->flag |= CPU_DESC_FLAG_READABLE;
	return;

exc:
	VERBOSE(("cpu_memoryread_check: check failure."));
	VERBOSE(("offset = 0x%08x, len = %d", offset, len));
#if defined(DEBUG)
	segdesc_dump(sdp);
#endif
	EXCEPTION(e, 0);
}

static void MEMCALL
cpu_memorywrite_check(descriptor_t *sdp, UINT32 offset, UINT len, int e)
{

	__ASSERT(sdp != NULL);
	__ASSERT(len > 0);

	if (!SEG_IS_VALID(sdp)) {
		e = GP_EXCEPTION;
		goto exc;
	}
	if (!SEG_IS_PRESENT(sdp)
	 || SEG_IS_SYSTEM(sdp)
	 || SEG_IS_CODE(sdp)
	 || (SEG_IS_DATA(sdp) && !SEG_IS_WRITABLE_DATA(sdp))) {
		goto exc;
	}

	switch (sdp->type) {
	case 2: case 3:	/* rw */
	case 6: case 7:	/* rw (expand down) */
		if (!check_limit_upstairs(sdp, offset, len))
			goto exc;
		break;

	default:
		goto exc;
	}
	sdp->flag |= CPU_DESC_FLAG_WRITABLE | CPU_DESC_FLAG_READABLE;
	return;

exc:
	VERBOSE(("cpu_memorywrite_check: check failure."));
	VERBOSE(("offset = 0x%08x, len = %d", offset, len));
#if defined(DEBUG)
	segdesc_dump(sdp);
#endif
	EXCEPTION(e, 0);
}

void
cpu_stack_push_check(UINT16 s, descriptor_t *sdp, UINT32 sp, UINT len)
{
	UINT32 limit;
	UINT32 start;

	__ASSERT(sdp != NULL);
	__ASSERT(len > 0);

	if (!SEG_IS_VALID(sdp)
	 || !SEG_IS_PRESENT(sdp)
	 || SEG_IS_SYSTEM(sdp)
	 || SEG_IS_CODE(sdp)
	 || !SEG_IS_WRITABLE_DATA(sdp)) {
		goto exc;
	}

	len--;
	start = sp - len;
	limit = SEG_IS_32BIT(sdp) ? 0xffffffff : 0x0000ffff;

	if (SEG_IS_EXPANDDOWN_DATA(sdp)) {
		/* expand-down stack */
		if (!SEG_IS_32BIT(sdp)) {
			if (sp > limit) {			/* [*] */
				goto exc;
			}
		}
		if (sdp->u.seg.limit == 0) {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |  [*]  |
			 * |       |   +-------+ 0000FFFFh
			 * | valid |   |       |
			 * |       |   | valid |
			 * |       |   |       |
			 * +-------+   +-------+ 00000000h
			 */
			if (!SEG_IS_32BIT(sdp)) {
				if (sp > limit) {		/* [1] */
					goto exc;
				}
			} else {
				sdp->flag |= CPU_DESC_FLAG_WHOLEADR;
			}
		} else {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |  [*]  |
			 * | valid |   +-------+ 0000FFFFh
			 * |       |   | valid |
			 * +-------+   +-------+ seg.limit + len - 1
			 * |       |   |       |
			 * +..[1]..+   +..[1]..+ seg.limit
			 * |       |   |       |
			 * +-------+   +-------+ 00000000h
			 */
			if ((len > limit - sdp->u.seg.limit)	/* len check */
			 || (start > sp)			/* wrap check */
			 || (start < sdp->u.seg.limit)) {	/* [1] */
				goto exc;
			}
		}
	} else {
		/* expand-up stack */
		if (sdp->u.seg.limit == limit) {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |  [1]  |
			 * |       |   +-------+ 0000FFFFh
			 * | valid |   |       |
			 * |       |   | valid |
			 * |       |   |       |
			 * +-------+   +-------+ 00000000h
			 */
			if (!SEG_IS_32BIT(sdp)) {
				if (sp > limit) {		/* [1] */
					goto exc;
				}
			} else {
				sdp->flag |= CPU_DESC_FLAG_WHOLEADR;
			}
		} else {
			/*
			 *   32bit       16bit
			 * +-------+   +-------+ FFFFFFFFh
			 * |       |   |       |
			 * |  [1]  |   +  [1]  + 0000FFFFh
			 * |       |   |       |
			 * +-------+   +-------+ seg.limit
			 * | valid |   | valid |
			 * +.......+   +.......+ len - 1
			 * |  [+]  |   |  [+]  |
			 * +-------+   +-------+ 00000000h
			 *
			 * [+]: wrap check
			 */
			if ((len > sdp->u.seg.limit)		/* len check */
			 || (start > sp)			/* wrap check */
			 || (sp > sdp->u.seg.limit)) {		/* [1] */
				goto exc;
			}
		}
	}
	return;

exc:
	VERBOSE(("cpu_stack_push_check: check failure."));
	VERBOSE(("s = 0x%04x, sp = 0x%08x, len = %d", s, sp, len));
#if defined(DEBUG)
	segdesc_dump(sdp);
#endif
	EXCEPTION(SS_EXCEPTION, s & 0xfffc);
}

void
cpu_stack_pop_check(UINT16 s, descriptor_t *sdp, UINT32 sp, UINT len)
{

	__ASSERT(sdp != NULL);
	__ASSERT(len > 0);

	if (!SEG_IS_VALID(sdp)
	 || !SEG_IS_PRESENT(sdp)
	 || SEG_IS_SYSTEM(sdp)
	 || SEG_IS_CODE(sdp)
	 || !SEG_IS_WRITABLE_DATA(sdp)) {
		goto exc;
	}

	if (!check_limit_upstairs(sdp, sp, len))
		goto exc;
	return;

exc:
	VERBOSE(("cpu_stack_pop_check: check failure."));
	VERBOSE(("s = 0x%04x, sp = 0x%08x, len = %d", s, sp, len));
#if defined(DEBUG)
	segdesc_dump(sdp);
#endif
	EXCEPTION(SS_EXCEPTION, s & 0xfffc);
}

#if defined(IA32_SUPPORT_DEBUG_REGISTER)
static INLINE void
check_memory_break_point(UINT32 address, UINT length, UINT rw)
{
	int i;

	if (CPU_STAT_BP && !(CPU_EFLAG & RF_FLAG)) {
		for (i = 0; i < CPU_DEBUG_REG_INDEX_NUM; i++) {
			if ((CPU_STAT_BP & (1 << i))
			 && (CPU_DR7_GET_RW(i) & rw)

			 && ((address <= CPU_DR(i) && address + length > CPU_DR(i))
			  || (address > CPU_DR(i) && address < CPU_DR(i) + CPU_DR7_GET_LEN(i)))) {
				CPU_STAT_BP_EVENT |= CPU_STAT_BP_EVENT_B(i);
			}
		}
	}
}
#else
#define	check_memory_break_point(address, length, rw)
#endif


/*
 * code fetch
 */
#define	ucrw	(CPU_PAGE_READ_CODE | CPU_STAT_USER_MODE)

UINT8 MEMCALL
cpu_codefetch(UINT32 offset)
{
	descriptor_t *sdp;
	UINT32 addr;
#if defined(IA32_SUPPORT_TLB)
	TLB_ENTRY_T *ep;
#endif

	sdp = &CPU_CS_DESC;
	if (offset <= sdp->u.seg.limit) {
		addr = sdp->u.seg.segbase + offset;
		if (!CPU_STAT_PAGING)
			return cpu_memoryread(addr);
#if defined(IA32_SUPPORT_TLB)
		ep = tlb_lookup(addr, ucrw);
		if (ep != NULL && ep->memp != NULL) {
			return ep->memp[addr & 0xfff];
		}
#endif
		return cpu_linear_memory_read_b(addr, ucrw);
	}
	EXCEPTION(GP_EXCEPTION, 0);
	return 0;	/* compiler happy */
}

UINT16 MEMCALL
cpu_codefetch_w(UINT32 offset)
{
	descriptor_t *sdp;
	UINT32 addr;
#if defined(IA32_SUPPORT_TLB)
	TLB_ENTRY_T *ep;
	UINT16 value;
#endif

	sdp = &CPU_CS_DESC;
	if (offset <= sdp->u.seg.limit - 1) {
		addr = sdp->u.seg.segbase + offset;
		if (!CPU_STAT_PAGING)
			return cpu_memoryread_w(addr);
#if defined(IA32_SUPPORT_TLB)
		ep = tlb_lookup(addr, ucrw);
		if (ep != NULL && ep->memp != NULL) {
			if ((addr + 1) & 0x00000fff) {
				return LOADINTELWORD(ep->memp + (addr & 0xfff));
			}
			value = ep->memp[0xfff];
			ep = tlb_lookup(addr + 1, ucrw);
			if (ep != NULL && ep->memp != NULL) {
				value += (UINT16)ep->memp[0] << 8;
				return value;
			}
		}
#endif
		return cpu_linear_memory_read_w(addr, ucrw);
	}
	EXCEPTION(GP_EXCEPTION, 0);
	return 0;	/* compiler happy */
}

UINT32 MEMCALL
cpu_codefetch_d(UINT32 offset)
{
	descriptor_t *sdp;
	UINT32 addr;
#if defined(IA32_SUPPORT_TLB)
	TLB_ENTRY_T *ep[2];
	UINT32 value;
	UINT remain;
#endif

	sdp = &CPU_CS_DESC;
	if (offset <= sdp->u.seg.limit - 3) {
		addr = sdp->u.seg.segbase + offset;
		if (!CPU_STAT_PAGING)
			return cpu_memoryread_d(addr);
#if defined(IA32_SUPPORT_TLB)
		ep[0] = tlb_lookup(addr, ucrw);
		if (ep[0] != NULL && ep[0]->memp != NULL) {
			remain = 0x1000 - (addr & 0xfff);
			if (remain >= 4) {
				return LOADINTELDWORD(ep[0]->memp + (addr & 0xfff));
			}
			ep[1] = tlb_lookup(addr + remain, ucrw);
			if (ep[1] != NULL && ep[1]->memp != NULL) {
				switch (remain) {
				case 3:
					value = ep[0]->memp[0xffd];
					value += (UINT32)LOADINTELWORD(ep[0]->memp + 0xffe) << 8;
					value += (UINT32)ep[1]->memp[0] << 24;
					break;

				case 2:
					value = LOADINTELWORD(ep[0]->memp + 0xffe);
					value += (UINT32)LOADINTELWORD(ep[1]->memp + 0) << 16;
					break;

				case 1:
					value = ep[0]->memp[0xfff];
					value += (UINT32)LOADINTELWORD(ep[1]->memp + 0) << 8;
					value += (UINT32)ep[1]->memp[2] << 24;
					break;

				default:
					ia32_panic("cpu_codefetch_d(): out of range. (remain = %d)\n", remain);
					return (UINT32)-1;
				}
				return value;
			}
		}
#endif
		return cpu_linear_memory_read_d(addr, ucrw);
	}
	EXCEPTION(GP_EXCEPTION, 0);
	return 0;	/* compiler happy */
}

#undef	ucrw

/*
 * additional physical address memory access functions
 */
UINT64 MEMCALL
cpu_memoryread_q(UINT32 paddr)
{
	UINT64 value;

	value = cpu_memoryread_d(paddr);
	value += (UINT64)cpu_memoryread_d(paddr + 4) << 32;

	return value;
}

void MEMCALL
cpu_memorywrite_q(UINT32 paddr, UINT64 value)
{

	cpu_memorywrite_d(paddr, (UINT32)value);
	cpu_memorywrite_d(paddr + 4, (UINT32)(value >> 32));
}

REG80 MEMCALL
cpu_memoryread_f(UINT32 paddr)
{
	REG80 value;
	int i;

	for (i = 0; i < (int)sizeof(REG80); ++i) {
		value.b[i] = cpu_memoryread(paddr + i);
	}
	return value;
}

void MEMCALL
cpu_memorywrite_f(UINT32 paddr, const REG80 *value)
{
	int i;

	for (i = 0; i < (int)sizeof(REG80); ++i) {
		cpu_memorywrite(paddr + i, value->b[i]);
	}
}

/*
 * virtual address memory access functions
 */
#include "cpu_mem.mcr"

VIRTUAL_ADDRESS_MEMORY_ACCESS_FUNCTION(b, UINT8, 1)
VIRTUAL_ADDRESS_MEMORY_ACCESS_FUNCTION(w, UINT16, 2)
VIRTUAL_ADDRESS_MEMORY_ACCESS_FUNCTION(d, UINT32, 4)

UINT64 MEMCALL
cpu_vmemoryread_q(int idx, UINT32 offset)
{
	descriptor_t *sdp;
	UINT32 addr;
	int exc;

	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM);

	sdp = &CPU_STAT_SREG(idx);
	if (!SEG_IS_VALID(sdp)) {
		exc = GP_EXCEPTION;
		goto err;
	}

	if (!(sdp->flag & CPU_DESC_FLAG_READABLE)) {
		cpu_memoryread_check(sdp, offset, 8,
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION);
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) {
		if (!check_limit_upstairs(sdp, offset, 8))
			goto range_failure;
	} 
	addr = sdp->u.seg.segbase + offset;
	check_memory_break_point(addr, 8, CPU_DR7_RW_RO);
	if (!CPU_STAT_PAGING)
		return cpu_memoryread_q(addr);
	return cpu_linear_memory_read_q(addr, CPU_PAGE_READ_DATA | CPU_STAT_USER_MODE);

range_failure:
	VERBOSE(("cpu_vmemoryread_q: type = %d, offset = %08x, limit = %08x", sdp->type, offset, sdp->u.seg.limit));
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION;
err:
	EXCEPTION(exc, 0);
	return 0;	/* compiler happy */
}

void MEMCALL
cpu_vmemorywrite_q(int idx, UINT32 offset, UINT64 value)
{
	descriptor_t *sdp;
	UINT32 addr;
	int exc;

	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM);

	sdp = &CPU_STAT_SREG(idx);
	if (!SEG_IS_VALID(sdp)) {
		exc = GP_EXCEPTION;
		goto err;
	}

	if (!(sdp->flag & CPU_DESC_FLAG_WRITABLE)) {
		cpu_memorywrite_check(sdp, offset, 8,
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION);
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) {
		if (!check_limit_upstairs(sdp, offset, 8))
			goto range_failure;
	}
	addr = sdp->u.seg.segbase + offset;
	check_memory_break_point(addr, 8, CPU_DR7_RW_RW);
	if (!CPU_STAT_PAGING) {
		cpu_memorywrite_q(addr, value);
	} else {
		cpu_linear_memory_write_q(addr, value, CPU_PAGE_READ_DATA | CPU_STAT_USER_MODE);
	}
	return;

range_failure:
	VERBOSE(("cpu_vmemorywrite_q: type = %d, offset = %08x, limit = %08x", sdp->type, offset, sdp->u.seg.limit));
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION;
err:
	EXCEPTION(exc, 0);
}

REG80 MEMCALL
cpu_vmemoryread_f(int idx, UINT32 offset)
{
	descriptor_t *sdp;
	UINT32 addr;
	int exc;

	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM);

	sdp = &CPU_STAT_SREG(idx);
	if (!SEG_IS_VALID(sdp)) {
		exc = GP_EXCEPTION;
		goto err;
	}

	if (!(sdp->flag & CPU_DESC_FLAG_READABLE)) {
		cpu_memoryread_check(sdp, offset, 10,
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION);
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) {
		if (!check_limit_upstairs(sdp, offset, 10))
			goto range_failure;
	} 
	addr = sdp->u.seg.segbase + offset;
	check_memory_break_point(addr, 10, CPU_DR7_RW_RO);
	if (!CPU_STAT_PAGING)
		return cpu_memoryread_f(addr);
	return cpu_linear_memory_read_f(addr, CPU_PAGE_READ_DATA | CPU_PAGE_READ_DATA | CPU_STAT_USER_MODE);

range_failure:
	VERBOSE(("cpu_vmemoryread_f: type = %d, offset = %08x, limit = %08x", sdp->type, offset, sdp->u.seg.limit));
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION;
err:
	EXCEPTION(exc, 0);
	{
		REG80 dummy;
		memset(&dummy, 0, sizeof(dummy));
		return dummy;	/* compiler happy */
	}
}

void MEMCALL
cpu_vmemorywrite_f(int idx, UINT32 offset, const REG80 *value)
{
	descriptor_t *sdp;
	UINT32 addr;
	int exc;

	__ASSERT((unsigned int)idx < CPU_SEGREG_NUM);

	sdp = &CPU_STAT_SREG(idx);
	if (!SEG_IS_VALID(sdp)) {
		exc = GP_EXCEPTION;
		goto err;
	}

	if (!(sdp->flag & CPU_DESC_FLAG_WRITABLE)) {
		cpu_memorywrite_check(sdp, offset, 10,
		    (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION);
	} else if (!(sdp->flag & CPU_DESC_FLAG_WHOLEADR)) {
		if (!check_limit_upstairs(sdp, offset, 10))
			goto range_failure;
	}
	addr = sdp->u.seg.segbase + offset;
	check_memory_break_point(addr, 10, CPU_DR7_RW_RW);
	if (!CPU_STAT_PAGING) {
		cpu_memorywrite_f(addr, value);
	} else {
		cpu_linear_memory_write_f(addr, value, CPU_PAGE_WRITE_DATA | CPU_STAT_USER_MODE);
	}
	return;

range_failure:
	VERBOSE(("cpu_vmemorywrite_f: type = %d, offset = %08x, limit = %08x", sdp->type, offset, sdp->u.seg.limit));
	exc = (idx == CPU_SS_INDEX) ? SS_EXCEPTION : GP_EXCEPTION;
err:
	EXCEPTION(exc, 0);
}
