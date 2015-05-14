#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"cbuscore.h"
#include	"board118.h"
#include	"cs4231io.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"s98.h"
#include	"keydisp.h"
#include "gimic/c86boxusb.h"
#include "gimic/gimicusb.h"

/**! G.I.M.I.C インスタンス */
static IC86RealChip* s_gimic = NULL;

// ROMEO対応版 PC-9801-118

static void IOOUTCALL ymf_o188(UINT port, REG8 dat) {

	opn.addr1l = dat;
	opn.addr1h = 0;
	opn.data1 = dat;
	(void)port;
}

static void IOOUTCALL ymf_o18a(UINT port, REG8 dat) {

	UINT	addr;

	opn.data1 = dat;
	if (opn.addr1h != 0) {
		return;
	}

	addr = opn.addr1l;
	S98_put(NORMAL2608, addr, dat);
	if (addr < 0x10) {
		if (addr != 0x0e) {
			psggen_setreg(&psg1, addr, dat);
		}
	}
	else {
		if (addr < 0x20) {
			rhythm_setreg(&rhythm, addr, dat);
		}
		else if (addr < 0x30) {
			if (addr == 0x28) {
				if ((dat & 0x0f) < 3) {
					opngen_keyon(dat & 0x0f, dat);
				}
				else if (((dat & 0x0f) != 3) &&
						((dat & 0x0f) < 7)) {
					opngen_keyon((dat & 0x07) - 1, dat);
				}
			}
			else {
				fmtimer_setreg(addr, dat);
			}
		}
		else if (addr < 0xc0) {
			opngen_setreg(0, addr, dat);
		}
		opn.reg[addr] = dat;
	}
	(void)port;
}

static void IOOUTCALL ymf_o18c(UINT port, REG8 dat) {

	if (opn.extend) {
		opn.addr1l = dat;
		opn.addr1h = 1;
		opn.data1 = dat;
	}
	(void)port;
}

static void IOOUTCALL ymf_o18e(UINT port, REG8 dat) {

	UINT	addr;

	if (!opn.extend) {
		return;
	}
	opn.data1 = dat;

	if (opn.addr1h != 1) {
		return;
	}
	addr = opn.addr1l;
	S98_put(EXTEND2608, addr, dat);
	opn.reg[addr + 0x100] = dat;
	if (addr >= 0x30) {
		opngen_setreg(3, addr, dat);
	}
	else {
		if (addr == 0x10) {
			if (!(dat & 0x80)) {
				opn.adpcmmask = ~(dat & 0x1c);
			}
		}
	}
	(void)port;
}

static REG8 IOINPCALL ymf_i188(UINT port) {

	(void)port;
	return(fmtimer.status);
}

static REG8 IOINPCALL ymf_i18a(UINT port) {

	UINT	addr;

	if (opn.addr1h == 0) {
		addr = opn.addr1l;
		if (addr == 0x0e) {
			return(fmboard_getjoy(&psg1));
		}
		else if (addr < 0x10) {
			return(psggen_getreg(&psg1, addr));
		}
		else if (addr == 0xff) {
			return(1);
		}
	}
	(void)port;
	return(opn.data1);
}

static REG8 IOINPCALL ymf_i18c(UINT port) {

	if (opn.extend) {
		return(fmtimer.status & 3);
	}
	(void)port;
	return(0xff);
}

static void extendchannel(REG8 enable) {

	opn.extend = enable;
	if (enable) {
		opn.channels = 6;
		opngen_setcfg(6, OPN_STEREO | 0x007);
	}
	else {
		opn.channels = 3;
		opngen_setcfg(3, OPN_MONORAL | 0x007);
		rhythm_setreg(&rhythm, 0x10, 0xff);
	}
}

static void IOOUTCALL ymf_oa460(UINT port, REG8 dat) {

	cs4231.extfunc = dat;
	extendchannel((REG8)(dat & 1));
	(void)port;
}

static REG8 IOINPCALL ymf_ia460(UINT port) {

	(void)port;
	return(0x80 | (cs4231.extfunc & 1));
}


// ---- with romeo

static void RestoreRomeo(IC86RealChip* gimic)
{
	const UINT8* data = opn.reg;
	for (UINT i = 0x30; i < 0xa0; i++)
	{
		gimic->Out(i, data[i]);
	}
	for (UINT ch = 0; ch < 3; ch++)
	{
		gimic->Out(ch + 0xa4, data[ch + 0x0a4]);
		gimic->Out(ch + 0xa0, data[ch + 0x0a0]);
		gimic->Out(ch + 0xb0, data[ch + 0x0b0]);
		gimic->Out(ch + 0xb4, data[ch + 0x0b4]);
	}

	for (UINT i = 0x130; i < 0x1a0; i++)
	{
		gimic->Out(i, data[i]);
	}
	for (UINT ch = 0; ch < 3; ch++)
	{
		gimic->Out(ch + 0x1a4, data[ch + 0x1a4]);
		gimic->Out(ch + 0x1a0, data[ch + 0x1a0]);
		gimic->Out(ch + 0x1b0, data[ch + 0x1b0]);
		gimic->Out(ch + 0x1b4, data[ch + 0x1b4]);
	}
	gimic->Out(0x11, data[0x11]);
	gimic->Out(0x18, data[0x18]);
	gimic->Out(0x19, data[0x19]);
	gimic->Out(0x1a, data[0x1a]);
	gimic->Out(0x1b, data[0x1b]);
	gimic->Out(0x1c, data[0x1c]);
	gimic->Out(0x1d, data[0x1d]);

	const UINT8* psg = reinterpret_cast<UINT8*>(&psg1.reg);
	for (UINT i = 0; i < 0x0e; i++)
	{
		gimic->Out(i, psg[i]);
	}
}

static void IOOUTCALL ymfr_o18a(UINT port, REG8 dat)
{
	opn.data1 = dat;
	if (opn.addr1h != 0)
	{
		return;
	}

	const UINT nAddr = opn.addr1l;
	S98_put(NORMAL2608, nAddr, dat);

	if (nAddr < 0x10)
	{
		(reinterpret_cast<UINT8*>(&psg1.reg))[nAddr] = dat;
		if (nAddr < 0x0e)
		{
			s_gimic->Out(nAddr, dat);

			if (nAddr == 0x07)
			{
				keydisp_psgmix(&psg1);
			}
			else if ((nAddr == 0x08) || (nAddr == 0x09) || (nAddr == 0x0a))
			{
				keydisp_psgvol(&psg1, static_cast<UINT8>(nAddr - 8));
			}
		}
	}
	else
	{
		if (nAddr < 0x20)
		{
			s_gimic->Out(nAddr, dat);
		}
		else if (nAddr == 0x28)
		{
			s_gimic->Out(nAddr, dat);
			if ((dat & 0x0f) < 3)
			{
				keydisp_fmkeyon(static_cast<UINT8>(dat & 0x0f), dat);
			}
			else if (((dat & 0x0f) != 3) && ((dat & 0x0f) < 7))
			{
				keydisp_fmkeyon(static_cast<UINT8>((dat & 0x0f) - 1), dat);
			}
		}
		else if (nAddr < 0x30)
		{
			fmtimer_setreg(nAddr, dat);
			if ((nAddr == 0x22) || (nAddr == 0x27))
			{
				s_gimic->Out(nAddr, dat);
			}
		}
		else if (nAddr < 0xc0)
		{
			s_gimic->Out(nAddr, dat);
		}
		opn.reg[nAddr] = dat;
	}
	(void)port;
}

static void IOOUTCALL ymfr_o18e(UINT port, REG8 dat)
{
	if (!opn.extend)
	{
		return;
	}
	opn.data1 = dat;
	if (opn.addr1h != 1) {
		return;
	}

	const UINT nAddr = opn.addr1l;
	S98_put(EXTEND2608, nAddr, dat);
	opn.reg[nAddr] = dat;
	if (nAddr >= 0x30)
	{
		s_gimic->Out(0x100 + nAddr, dat);
	}
	else if (nAddr == 0x10)
	{
		if (!(dat & 0x80))
		{
			opn.adpcmmask = ~(dat & 0x1c);
		}
	}
	(void)port;
}


// ----

static const IOOUT ymf_o[4] = {
			ymf_o188,	ymf_o18a,	ymf_o18c,	ymf_o18e};

static const IOINP ymf_i[4] = {
			ymf_i188,	ymf_i18a,	ymf_i18c,	NULL};

static const IOOUT ymfr_o[4] =
{
	ymf_o188,	ymfr_o18a,	ymf_o18c,	ymfr_o18e
};

void board118_reset(const NP2CFG *pConfig) {

	fmtimer_reset(0xc0);
	opngen_setcfg(3, OPN_STEREO | 0x038);
	cs4231io_reset();
	soundrom_load(0xcc000, OEMTEXT("118"));
	fmboard_extreg(extendchannel);

	IC86RealChip* gimic = s_gimic;
	if (gimic != NULL)
	{
		if (gimic->Reset() != C86CTL_ERR_NONE)
		{
			gimic->Deinitialize();
			delete gimic;
			gimic = NULL;
		}
	}
	s_gimic = gimic;
}

void board118_bind(void)
{
	IC86RealChip* gimic = s_gimic;

	if (gimic == NULL)
	{
		// G.I.M.I.C オープン
		gimic = new CGimicUSB();
		if (gimic->Initialize() == C86CTL_ERR_NONE)
		{
			gimic->Reset();
		}
		else
		{
			delete gimic;
			gimic = NULL;
		}
	}
	if (gimic == NULL)
	{
		// C86BOX オープン
		gimic = new C86BoxUSB();
		if (gimic->Initialize() == C86CTL_ERR_NONE)
		{
			gimic->Reset();
		}
		else
		{
			delete gimic;
			gimic = NULL;
		}
	}
	s_gimic = gimic;

	if (gimic)
	{
		gimic->Out(0x22, 0x00);
		gimic->Out(0x29, 0x80);
		gimic->Out(0x10, 0xbf);
		gimic->Out(0x11, 0x30);

		RestoreRomeo(gimic);

		cbuscore_attachsndex(0x188, ymfr_o, ymf_i);
	}
	else
	{
		fmboard_fmrestore(0, 0);
		fmboard_fmrestore(3, 1);
		psggen_restore(&psg1);
		fmboard_rhyrestore(&rhythm, 0);
		sound_streamregist(&opngen, (SOUNDCB)opngen_getpcm);
		sound_streamregist(&psg1, (SOUNDCB)psggen_getpcm);
		rhythm_bind(&rhythm);
		cbuscore_attachsndex(0x188, ymf_o, ymf_i);
	}
	cs4231io_bind();
	iocore_attachout(0xa460, ymf_oa460);
	iocore_attachinp(0xa460, ymf_ia460);
}

/**
 * Deinitialize
 */
extern "C" void board118_deinitialize(void)
{
	IC86RealChip* gimic = s_gimic;
	s_gimic = NULL;

	if (gimic)
	{
		gimic->Reset();
		gimic->Deinitialize();
		delete gimic;
	}
}
