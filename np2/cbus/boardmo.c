#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"boardso.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"

/**
 * SNE Multimedia Orchestra
 * YM2203C(OPN) + YMF262-M(OPL3) + YMZ263B(MMA)
 *
 * 対応ソフト/マニュアルが無い為に現状MMAのポートが不明。
 *
 */
static void *opl3;
static int samplerate;

void *YMF262Init(INT clock, INT rate);
void YMF262ResetChip(void *chip);
void YMF262Shutdown(void *chip);
INT YMF262Write(void *chip, INT a, INT v);
UINT8 YMF262Read(void *chip, INT a);
void YMF262UpdateOne(void *chip, INT16 **buffer, INT length);

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

static void IOOUTCALL opl_o288(UINT port, REG8 dat) {
	(void)port;
	opl.addr = dat;
	YMF262Write(opl3, 0, dat);
}

static void IOOUTCALL opl_o28a(UINT port, REG8 dat) {
	(void)port;
	S98_put(NORMAL2608_2, opl.addr, dat);
	opl.reg[opl.addr] = dat;
	YMF262Write(opl3, 1, dat);
}

static void IOOUTCALL opl_o28c(UINT port, REG8 dat) {
	(void)port;
	opl.addr2 = dat;
	YMF262Write(opl3, 2, dat);
}

static void IOOUTCALL opl_o28e(UINT port, REG8 dat) {
	(void)port;
	S98_put(EXTEND2608_2, opl.addr2, dat);
	YMF262Write(opl3, 3, dat);
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

static REG8 IOINPCALL opl_i288(UINT port) {
	(void)port;
	return YMF262Read(opl3, 0);
}

static REG8 IOINPCALL opl_i28a(UINT port) {
	(void)port;
	return YMF262Read(opl3, 1);
}
static REG8 IOINPCALL opl_i28c(UINT port) {
	(void)port;
	return YMF262Read(opl3, 2);
}

static REG8 IOINPCALL opl_i28e(UINT port) {
	(void)port;
	return YMF262Read(opl3, 3);
}
// ----

static const IOOUT opn_o[4] = {
			opn_o188,	opn_o18a,	opl_o288,		opl_o28a};

static const IOINP opn_i[4] = {
			opn_i188,	opn_i18a,	opl_i288,		opl_i28a};

static const IOOUT opl_o[4] = {
			opl_o288,	opl_o28a,	opl_o28c,		opl_o28e};

static const IOINP opl_i[4] = {
			opl_i288,	opl_i28a,	opl_i28c,		opl_i28e};

static void psgpanset(PSGGEN psg) {
	// SSGはL固定
	psggen_setpan(psg, 0, 2);
	psggen_setpan(psg, 1, 2);
	psggen_setpan(psg, 2, 2);
}

extern void SOUNDCALL opl3gen_getpcm(void* opl3, SINT32 *pcm, UINT count);

void boardmo_reset(const NP2CFG *pConfig) {

	opngen_setcfg(3, OPN_STEREO | 0x007);
	fmtimer_reset(pConfig->snd26opt & 0xc0);
	soundrom_loadex(pConfig->snd26opt & 7, OEMTEXT("MO"));
	opn.base = (pConfig->snd26opt & 0x10)?0x000:0x100;
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	ZeroMemory(&opl, sizeof(opl));
	if (opl3) {
		if (samplerate != pConfig->samplingrate) {
			YMF262Shutdown(opl3);
			opl3 = YMF262Init(14400000, pConfig->samplingrate);
			samplerate = pConfig->samplingrate;
		} else {
			YMF262ResetChip(opl3);
		}
	}
}

void boardmo_bind(void) {
	psgpanset(&psg1);
	fmboard_fmrestore(0, 0);
	opngen_setreg(0, 0xb4, 1 << 6);
	opngen_setreg(0, 0xb5, 1 << 6);
	opngen_setreg(0, 0xb6, 1 << 6);
	psggen_restore(&psg1);
	sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
	sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
	cbuscore_attachsndex(0x188 - opn.base, opn_o, opn_i);
	cbuscore_attachsndex(0x288, opl_o, opl_i);
	if (!opl3) {
		opl3 = YMF262Init(15974400, np2cfg.samplingrate);
		samplerate = np2cfg.samplingrate;
	}
	sound_streamregist(opl3, (SOUNDCB)opl3gen_getpcm);
}

