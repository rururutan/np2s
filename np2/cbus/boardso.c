#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"boardso.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"

/**
 * SNE Sound Orchestra
 * YM2203(OPN) with panning + YM3812(OPL2)
 *
 */
static void *opl2;
static int samplerate;

void *YM3812Init(INT clock, INT rate);
void YM3812ResetChip(void *chip);
void YM3812Shutdown(void *chip);
INT YM3812Write(void *chip, INT a, INT v);
UINT8 YM3812Read(void *chip, INT a);
void YM3812UpdateOne(void *chip, INT16 *buffer, INT length);


static void IOOUTCALL opn_o188(UINT port, REG8 dat) {

	opn.addr = dat;
	opn.data = dat;
	(void)port;
}

static void IOOUTCALL opn_o18a(UINT port, REG8 dat) {

	UINT	addr;

	if ((opn.addr & 0xb4) == 0xb4)
		return;

	opn.data = dat;
	addr = opn.addr;
	if (addr != 0xf)
		S98_put(NORMAL2608, addr, dat);
	if (addr < 0x10) {
		if (addr != 0x0e) {
			psggen_setreg(&psg1, addr, dat);
		}
	}
	else if (addr < 0x100) {
		if (addr < 0x30) {
			if (addr == 0x28) {
				if ((dat & 0x0f) < 3) {
					opngen_keyon(dat & 0x0f, dat);
				}
			}
			else {
				fmtimer_setreg(addr, dat);
				if (addr == 0x27) {
					opnch[2].extop = dat & 0xc0;
				}
			}
		}
		else if (addr < 0xc0) {
			opngen_setreg(0, addr, dat);
		}
		opn.reg[addr] = dat;
	}
	(void)port;
}

static void IOOUTCALL opn_o18c(UINT port, REG8 dat) {
	(void)port;
	opl.addr = dat;
	YM3812Write(opl2, 0, dat);
}

static void IOOUTCALL opn_o18e(UINT port, REG8 dat) {
	(void)port;
	S98_put(NORMAL2608_2, opl.addr, dat);
	opl.reg[opl.addr] = dat;
	YM3812Write(opl2, 1, dat);
}

static REG8 IOINPCALL opn_i188(UINT port) {

	(void)port;
	return(fmtimer.status);
}

static REG8 IOINPCALL opn_i18a(UINT port) {

	UINT	addr;

	addr = opn.addr;
	if (addr == 0x0e) {
		return(fmboard_getjoy(&psg1));
	}
	else if (addr < 0x10) {
		return(psggen_getreg(&psg1, addr));
	}
	(void)port;
	return(opn.data);
}

static REG8 IOINPCALL opn_i18c(UINT port) {
	(void)port;
	return YM3812Read(opl2, 0);
}

static REG8 IOINPCALL opn_i18e(UINT port) {
	(void)port;
	return YM3812Read(opl2, 1);
}
// ----

static const IOOUT opn_o[4] = {
			opn_o188,	opn_o18a,	opn_o18c,		opn_o18e};

static const IOINP opn_i[4] = {
			opn_i188,	opn_i18a,	opn_i18c,		opn_i18e};

static void psgpanset(PSGGEN psg) {
	psggen_setpan(psg, 0, 0);
	psggen_setpan(psg, 1, 0);
	psggen_setpan(psg, 2, 0);
}

void SOUNDCALL opl2gen_getpcm(void* opl, SINT32 *pcm, UINT count) {
	UINT i;
	SINT16 buf = 0;
	for (i=0; i < count; i++) {
		YM3812UpdateOne(opl2, &buf, 1);
		pcm[0] += buf << 1;
		pcm += 2;
	}
}

void boardso_reset(const NP2CFG *pConfig) {

	opngen_setcfg(3, OPN_STEREO | 0x007);
	fmtimer_reset(pConfig->snd26opt & 0xc0);
	soundrom_loadsne(OEMTEXT("SO"));
	opn.base = (pConfig->snd26opt & 0x10)?0x000:0x100;
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	ZeroMemory(&opl, sizeof(opl));
	if (opl2) {
		if (samplerate != pConfig->samplingrate) {
			YM3812Shutdown(opl2);
			opl2 = YM3812Init(OPNA_CLOCK, np2cfg.samplingrate);
			samplerate = np2cfg.samplingrate;
		} else {
			YM3812ResetChip(opl2);
		}
		YM3812Write(opl2, 0, 0x1);
		YM3812Write(opl2, 1, 0x0);
	}
}

void boardso_bind(void) {
	psgpanset(&psg1);
	fmboard_fmrestore(0, 0);
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	psggen_restore(&psg1);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	cbuscore_attachsndex(0x188 - opn.base, opn_o, opn_i);
	if (!opl2) {
		opl2 = YM3812Init(OPNA_CLOCK, np2cfg.samplingrate);
		samplerate = np2cfg.samplingrate;
		YM3812Write(opl2, 0, 0x1);
		YM3812Write(opl2, 1, 0x0);
	}
	sound_streamregist(opl2, (SOUNDCB)opl2gen_getpcm);
}

