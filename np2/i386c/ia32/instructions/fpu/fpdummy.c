#include "compiler.h"
#include "cpu.h"
#include "ia32.mcr"
#include "fp.h"


void
FWAIT(void)
{
#if !defined(USE_FPU)
	EXCEPTION(NM_EXCEPTION, 0);
#else
//	TRACEOUT(("use FPU - FWAIT"));
#endif
}

void
ESC0(void)
{
#if !defined(USE_FPU)
	EXCEPTION(NM_EXCEPTION, 0);
#else
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU d8 %.2x", op));
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC1(void)
{
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU d9 %.2x", op));
#if !defined(USE_FPU)
	if (op >= 0xc0) {
		EXCEPTION(NM_EXCEPTION, 0);
	} else {
		madr = calc_ea_dst(op);
		if (((op >> 3) & 7) != 7) {
			EXCEPTION(NM_EXCEPTION, 0);
		}
		/* FSTCW */
		TRACEOUT(("FSTCW"));
		cpu_vmemorywrite_w(CPU_INST_SEGREG_INDEX, madr, 0xffff);
	}
#else
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
		if ((op & 0x38) == 0x38) {
			TRACEOUT(("FSTCW"));
			cpu_vmemorywrite_w(CPU_INST_SEGREG_INDEX, madr, 0xffff);
		}
	}
#endif
}

void
ESC2(void)
{
#if !defined(USE_FPU)
	EXCEPTION(NM_EXCEPTION, 0);
#else
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU da %.2x", op));
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC3(void)
{
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU db %.2x", op));
#if !defined(USE_FPU)
	if (op != 0xe3) {
		EXCEPTION(NM_EXCEPTION, 0);
	}
	/* FNINIT */
	(void)madr;
#else
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC4(void)
{
#if !defined(USE_FPU)
	EXCEPTION(NM_EXCEPTION, 0);
#else
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU dc %.2x", op));
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC5(void)
{
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU dd %.2x", op));
#if !defined(USE_FPU)
	if (op >= 0xc0) {
		EXCEPTION(NM_EXCEPTION, 0);
	} else {
		if (((op >> 3) & 7) != 7) {
			EXCEPTION(NM_EXCEPTION, 0);
		}
		/* FSTSW */
		madr = calc_ea_dst(op);
		TRACEOUT(("FSTSW"));
		cpu_vmemorywrite_w(CPU_INST_SEGREG_INDEX, madr, 0);
	}
#else
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC6(void)
{
#if !defined(USE_FPU)
	EXCEPTION(NM_EXCEPTION, 0);
#else
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU de %.2x", op));
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}

void
ESC7(void)
{
	UINT32 op, madr;

	GET_PCBYTE(op);
//	TRACEOUT(("use FPU df %.2x", op));
#if !defined(USE_FPU)
	if (op >= 0xc0) {
		if (op != 0xe0) {
			EXCEPTION(NM_EXCEPTION, 0);
		}
		/* FSTSW AX */
		TRACEOUT(("FSTSW AX"));
		CPU_AX = 0;
	} else {
		madr = calc_ea_dst(op);
		EXCEPTION(NM_EXCEPTION, 0);
	}
#else
	if (op >= 0xc0) {
	} else {
		madr = calc_ea_dst(op);
	}
#endif
}
