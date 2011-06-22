#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"ct1741io.h"
#include	"ct1745io.h"
#include	"sound.h"
#include	"fmboard.h"

/**
 * Creative SoundBlaster16 mixer CT1745
 */

enum {
	MIXER_RESET = 0x00,
	MIXER_VOL_START = 0x30,
	MIXER_VOL_END = 0x47,
	MIXER_MASTER_LEFT = 0x0,
	MIXER_MASTER_RIGHT,
	MIXER_VOC_LEFT,
	MIXER_VOC_RIGHT,
	MIXER_MIDI_LEFT,
	MIXER_MIDI_RIGHT,
	MIXER_CD_LEFT,
	MIXER_CD_RIGHT,
	MIXER_LINE_LEFT,
	MIXER_LINE_RIGHT,
	MIXER_MIC,
	MIXER_PCSPEAKER,
	MIXER_OUT_SW,
	MIXER_IN_SW_LEFT,
	MIXER_IN_SW_RIGHT,
	MIXER_IN_GAIN_LEFT,
	MIXER_IN_GAIN_RIGHT,
	MIXER_OUT_GAIN_LEFT,
	MIXER_OUT_GAIN_RIGHT,
	MIXER_AGC,
	MIXER_TREBLE_LEFT,
	MIXER_TREBLE_RIGHT,
	MIXER_BASS_LEFT,
	MIXER_BASS_RIGHT
};

static void ct1745_mixer_reset() {
	ZeroMemory(sb16.mixreg, sizeof(sb16.mixreg));

	sb16.mixreg[MIXER_MASTER_LEFT] =
	sb16.mixreg[MIXER_MASTER_RIGHT] =
	sb16.mixreg[MIXER_VOC_LEFT] =
	sb16.mixreg[MIXER_VOC_RIGHT] =
	sb16.mixreg[MIXER_MIDI_LEFT] =
	sb16.mixreg[MIXER_MIDI_RIGHT] = 24;
	sb16.mixreg[MIXER_OUT_SW] = 0x1f;
	sb16.mixreg[MIXER_IN_SW_LEFT] = 0x15;
	sb16.mixreg[MIXER_IN_SW_RIGHT] = 0x0b;

	sb16.mixreg[MIXER_TREBLE_LEFT] =
	sb16.mixreg[MIXER_TREBLE_RIGHT] =
	sb16.mixreg[MIXER_BASS_LEFT] =
	sb16.mixreg[MIXER_BASS_RIGHT] = 8;
}

static void IOOUTCALL sb16_o2400(UINT port, REG8 dat) {
	(void)port;
	sb16.mixsel = dat;
}
static void IOOUTCALL sb16_o2500(UINT port, REG8 dat) {

	if (sb16.mixsel >= MIXER_VOL_START &&
		sb16.mixsel <= MIXER_VOL_END) {
		sb16.mixreg[sb16.mixsel - MIXER_VOL_START] = dat;
		return;
	}

	switch (sb16.mixsel) {
		case MIXER_RESET:	// Mixer reset
			ct1745_mixer_reset();
			break;
		case 0x04:			// Voice volume(old)
			sb16.mixreg[MIXER_VOC_LEFT]  = (dat & 0x0f);
			sb16.mixreg[MIXER_VOC_RIGHT] = (dat & 0xf0) >> 3;
			break;
		case 0x0a:			// Mic volume(old)
			sb16.mixreg[MIXER_MIC] = dat & 0x7;
			break;
		case 0x22:			// Master volume(old)
			sb16.mixreg[MIXER_MASTER_LEFT]  = (dat & 0x0f);
			sb16.mixreg[MIXER_MASTER_RIGHT] = (dat & 0xf0) >> 3;
			break;
		case 0x26:			// MIDI volume(old)
			sb16.mixreg[MIXER_MIDI_LEFT]  = (dat & 0x0f);
			sb16.mixreg[MIXER_MIDI_RIGHT] = (dat & 0xf0) >> 3;
			break;
		case 0x28:			// CD volume(old)
			sb16.mixreg[MIXER_CD_LEFT]  = (dat & 0x0f);
			sb16.mixreg[MIXER_CD_RIGHT] = (dat & 0xf0) >> 3;
			break;
		case 0x2e:			// Line volume(old)
			sb16.mixreg[MIXER_LINE_LEFT]  = (dat & 0x0f);
			sb16.mixreg[MIXER_LINE_RIGHT] = (dat & 0xff) >> 3;
		case 0x80:			// Write irq num
			ct1741_set_dma_irq(dat);
			break;
		case 0x81:			// Write dma num
			ct1741_set_dma_ch(dat);
			break;
		default:
			break;
	}

}

static REG8 IOINPCALL sb16_i2400(UINT port) {
	return sb16.mixsel;
}
static REG8 IOINPCALL sb16_i2500(UINT port) {

	if (sb16.mixsel >= MIXER_VOL_START && sb16.mixsel <= MIXER_VOL_END) {
		return 	sb16.mixreg[sb16.mixsel - MIXER_VOL_START];
	}

	switch (sb16.mixsel) {
		case 0x04:
			return  (sb16.mixreg[MIXER_VOC_LEFT] + sb16.mixreg[MIXER_VOC_RIGHT]) << 1;
		case 0x22:
			return  (sb16.mixreg[MIXER_MASTER_LEFT] + sb16.mixreg[MIXER_MASTER_RIGHT]) << 1;
		case 0x26:
			return  (sb16.mixreg[MIXER_MIDI_LEFT] + sb16.mixreg[MIXER_MIDI_RIGHT]) << 1;
		case 0x28:			// CD volume(old)
			return  (sb16.mixreg[MIXER_CD_LEFT] + sb16.mixreg[MIXER_CD_RIGHT]) << 1;
		case 0x2e:			// Line volume(old)
			return  (sb16.mixreg[MIXER_LINE_LEFT] + sb16.mixreg[MIXER_LINE_RIGHT]) << 1;
		case 0x0a:			// Mic volume(old)
			return sb16.mixreg[MIXER_MIC];
		case 0x80:			// Read irq num
			return ct1741_get_dma_irq();
		case 0x81:			// Read dma num
			return ct1741_get_dma_ch();
		case 0x82:			// Irq pending(98‚É‚Í•s—v)
		default:
			break;
	}
	return 0;
}

void ct1745io_reset(void)
{
	ct1745_mixer_reset();
}

void ct1745io_bind(void)
{
	iocore_attachout(0x2400 + sb16.base, sb16_o2400);	/* Mixer Chip Register Address Port */
	iocore_attachout(0x2500 + sb16.base, sb16_o2500);	/* Mixer Chip Data Port */
	iocore_attachinp(0x2400 + sb16.base, sb16_i2400);	/* Mixer Chip Register Address Port */
	iocore_attachinp(0x2500 + sb16.base, sb16_i2500);	/* Mixer Chip Data Port */
}
