#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"boardso.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"


/**
 * SNE Sound Orchestra V
 * YM2203(OPN) + Y8950(MSX-Audio) + ADPCM-RAM(32KB)
 *
 */
static void *msxa;
static int samplerate;
static UINT8 adpcmbuf[32*1024]; // 32KB

void * Y8950Init(INT clock, INT rate);
void Y8950ResetChip(void *chip);
void Y8950Shutdown(void *chip);
int  Y8950Write(void *chip, INT a, INT v);
UINT8 Y8950Read(void *chip, INT a);
void Y8950UpdateOne(void *chip, INT16 *buffer, INT length);
void Y8950SetDeltaTMemory(void *chip, void * deltat_mem_ptr, INT deltat_mem_size);

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
	if (addr != 0x0f && addr != 0x0e)  /* Valis2ëŒçÙ */
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
	Y8950Write(msxa, 0, dat);
}

static void IOOUTCALL opn_o18e(UINT port, REG8 dat) {
	(void)port;
	S98_put(NORMAL2608_2, opl.addr, dat);
	opl.reg[opl.addr] = dat;
	Y8950Write(msxa, 1, dat);
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
	return Y8950Read(msxa, 0);
}

static REG8 IOINPCALL opn_i18e(UINT port) {
	(void)port;
	return Y8950Read(msxa, 1);
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

void SOUNDCALL msxagen_getpcm(void* opl, SINT32 *pcm, UINT count) {
	UINT i;
	SINT16 buf = 0;
	for (i=0; i < count; i++) {
		Y8950UpdateOne(msxa, &buf, 1);
		pcm[0] += buf << 1;
		pcm += 2;
	}
}

void boardsov_reset(const NP2CFG *pConfig) {

	opngen_setcfg(3, OPN_STEREO | 0x007);
	fmtimer_reset(pConfig->snd26opt & 0xc0);
	soundrom_loadsne(OEMTEXT("SOV"));
	opn.base = (pConfig->snd26opt & 0x10)?0x000:0x100;
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	ZeroMemory(&opl, sizeof(opl));
	if (msxa) {
		if (samplerate != pConfig->samplingrate) {
			Y8950Shutdown(msxa);
			msxa = Y8950Init(OPNA_CLOCK, np2cfg.samplingrate);
			samplerate = pConfig->samplingrate;
			Y8950SetDeltaTMemory(msxa, adpcmbuf, sizeof(adpcmbuf));
		} else {
			Y8950ResetChip(msxa);
		}
		Y8950Write(msxa, 0, 0x1);
		Y8950Write(msxa, 1, 0x0);
	}
}

void boardsov_bind(void) {
	psgpanset(&psg1);
	fmboard_fmrestore(0, 0);
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	psggen_restore(&psg1);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	cbuscore_attachsndex(0x188 - opn.base, opn_o, opn_i);
	if (!msxa) {
		msxa = Y8950Init(OPNA_CLOCK, np2cfg.samplingrate);
		samplerate = np2cfg.samplingrate;
		Y8950SetDeltaTMemory(msxa, adpcmbuf, sizeof(adpcmbuf));
		Y8950Write(msxa, 0, 0x1);
		Y8950Write(msxa, 1, 0x0);
	}
	sound_streamregist(msxa, (SOUNDCB)msxagen_getpcm);
}

