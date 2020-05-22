#include	"compiler.h"
#include	<math.h>
#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"
#include	"keydisp.h"


#define	OPM_ARRATE		 399128L
#define	OPM_DRRATE		5514396L

#define	EG_STEP	(96.0 / EVC_ENT)					// dB step
#define	SC(db)	(SINT32)((db) * ((3.0 / EG_STEP) * (1 << ENV_BITS))) + EC_DECAY
#define	D2(v)	(((double)(6 << KF_BITS) * log((double)(v)) / log(2.0)) + 0.5)
#define	FMASMSHIFT	(32 - 6 - (OPM_OUTSB + 1 + FMDIV_BITS) + FMVOL_SFTBIT)
#define	FREQBASE4096	((double)OPNA_CLOCK / calcrate / 64)


	OPNCFG	opncfg;

//OPNCFG構造体のメンバー変数に、移動しました。
//#ifdef OPNGENX86
//	char	envshift[EVC_ENT];
//	char	sinshift[SIN_ENT];
//#endif


static	SINT32	detunetable[8][32];
static	SINT32	attacktable[94];
static	SINT32	decaytable[94];

static const SINT32	decayleveltable[16] = {
		 			SC( 0),SC( 1),SC( 2),SC( 3),SC( 4),SC( 5),SC( 6),SC( 7),
		 			SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)};
static const UINT8 multipletable[] = {
			    	1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
static const SINT32 nulltable[] = {
					0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const UINT8 kftable[16] = {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};
static const UINT8 dttable[] = {
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
					2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
					1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
					5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
					2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
					8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22};
static const int extendslot[4] = {2, 3, 1, 0};
static const int fmslot[4] = {0, 2, 1, 3};

//ハードウェアLFO関連のテーブル
#if (EVC_BITS >= 7)
#define	LA(n)	(SINT32)((n/0.75)*(1<<(EVC_BITS - 7)))/2
#else
#define	LA(n)	(SINT32)((n/0.75)*(1<<(7 - EVC_BITS)))/2
#endif
#define	LP(n)	(SINT32)
       const double lfo_freq[8]			= {3.98, 5.56, 6.02, 6.37, 6.88, 9.63, 48.1, 72.2};	//単位 [Hz]
       		 SINT32 lfo_freq_table[8]	= {0,0,0,0,0,0,0,0};
	   const SINT32 lfo_pms_table[8]	= {
				0x00000000/2,		// 0 [cent]
				0x0080D56F/2,		// 3.4 [cent]	table = (1-(2^(n[cent]/1200))) * 2^32
				0x00FE1ED5/2,		// 6.7 [cent]
				0x017BA56C/2,		//10 [cent]
				0x02141EA7/2,		//14 [cent]
				0x02F97DDC/2,		//20 [cent]
				0x05FBD4D5/2,		//40 [cent]
				0x0C1B77B6/2};		//80 [cent]	MUL freq_inc,この値 を実行して、edxの数値を足す。
       const SINT32 lfo_ams_table[4]	= {LA(0),LA(1.4),LA(5.9),LA(11.8)};	//[dB]


//==============================================================
//			ＯＰＮ処理関連の初期化
//--------------------------------------------------------------
//	◆引数
//		UINT	rate	
//	◆返り値
//		無し
//	◆備考
//			
//==============================================================
void opngen_initialize(UINT rate) {

	//------------------
	//◆Local
	UINT	ratebit;
	int		i;
	char	sft;
	int		j;
	double	pom;
	long	detune;
	double	freq;
	UINT32	calcrate;

	//------------------
	//◆色々設定
	if (rate == 44100) {
		ratebit = 0;
	}
	else if (rate == 22050) {
		ratebit = 1;
	}
	else {
		ratebit = 2;
	}
	calcrate = (OPNA_CLOCK / 72) >> ratebit;
	opncfg.calc1024 = FMDIV_ENT * 44100 / (OPNA_CLOCK / 72);

	//------------------
	//◆LOG Table
	for (i=0; i<EVC_ENT; i++) {
#ifdef OPNGENX86
		sft = ENVTBL_BIT;
		while(sft < (ENVTBL_BIT + 8)) {
			pom = (double)(1 << sft) / pow(10.0, EG_STEP*(EVC_ENT-i)/20.0);
			opncfg.envtable[i] = (SINT32)pom;
			opncfg.envshift[i] = sft - TL_BITS;
			if (opncfg.envtable[i] >= (1 << (ENVTBL_BIT - 1))) {
				break;
			}
			sft++;
		}
#else
		pom = (double)(1 << ENVTBL_BIT) / pow(10.0, EG_STEP*(EVC_ENT-i)/20.0);
		opncfg.envtable[i] = (SINT32)pom;
#endif
	}

	//------------------
	//◆Sin wave Table
	for (i=0; i<SIN_ENT; i++) {
#ifdef OPNGENX86
		char sft;
		sft = SINTBL_BIT;
		while(sft < (SINTBL_BIT + 8)) {
			pom = (double)(1 << sft) * sin(2*PI*i/SIN_ENT);
			opncfg.sintable[i] = (SINT32)pom;
			opncfg.sinshift[i] = sft;
			if (opncfg.sintable[i] >= (1 << (SINTBL_BIT - 1))) {
				break;
			}
			if (opncfg.sintable[i] <= -1 * (1 << (SINTBL_BIT - 1))) {
				break;
			}
			sft++;
		}
#else
		pom = (double)((1 << SINTBL_BIT) - 1) * sin(2*PI*i/SIN_ENT);
		opncfg.sintable[i] = (SINT32)pom;
#endif
	}

	//------------------
	//◆Envelope table
	for (i=0; i<EVC_ENT; i++) {
		pom = pow(((double)(EVC_ENT-1-i)/EVC_ENT), 8) * EVC_ENT;
		opncfg.envcurve[i] = (SINT32)pom;
		opncfg.envcurve[EVC_ENT + i] = i;
	}
	opncfg.envcurve[EVC_ENT*2] = EVC_ENT;

	//------------------
	//◆
//	opmbaserate = (1L << FREQ_BITS) / (rate * x / 44100) * 55466;
//	でも今は x == 55466だから…

//	ここで FREQ_BITS >= 16が条件
	if (rate == 44100) {
		opncfg.ratebit = 0 + (FREQ_BITS - 16);
	}
	else if (rate == 22050) {
		opncfg.ratebit = 1 + (FREQ_BITS - 16);
	}
	else {
		opncfg.ratebit = 2 + (FREQ_BITS - 16);
	}

	//------------------
	//◆Detune table
	for (i=0; i<4; i++) {
		for (j=0; j<32; j++) {
			detune = dttable[i*32 + j];
			sft = ratebit + (FREQ_BITS - 21);
			if (sft >= 0) {
				detune <<= sft;
			}
			else {
				detune >>= (0 - sft);
			}

			detunetable[i][j]   = (SINT32)detune;
			detunetable[i+4][j] = (SINT32)-detune;
		}
	}

	//------------------
	//◆Attack, Decay, Sustain Rate Table
	for (i=0; i<4; i++) {
		attacktable[i] = decaytable[i] = 0;
	}
	for (i=4; i<64; i++) {
		freq = (double)(EVC_ENT << ENV_BITS) * FREQBASE4096;
		if (i < 8) {							// 忘れてます。
			freq *= 1.0 + (i & 2) * 0.25;
		}
		else if (i < 60) {
			freq *= 1.0 + (i & 3) * 0.25;
		}
		freq *= (double)(1 << ((i >> 2) - 1));
#if 0
		attacktable[i] = (SINT32)((freq + OPM_ARRATE - 1) / OPM_ARRATE);
		decaytable[i] = (SINT32)((freq + OPM_DRRATE - 1) / OPM_DRRATE);
#else
		attacktable[i] = (SINT32)(freq / OPM_ARRATE);
		decaytable[i] = (SINT32)(freq / OPM_DRRATE);
#endif
		if (attacktable[i] >= EC_DECAY) {
			TRACEOUT(("attacktable %d %d %ld", i, attacktable[i], EC_DECAY));
		}
		if (decaytable[i] >= EC_DECAY) {
			TRACEOUT(("decaytable %d %d %ld", i, decaytable[i], EC_DECAY));
		}
	}
	attacktable[62] = EC_DECAY - 1;
	attacktable[63] = EC_DECAY - 1;
	for (i=64; i<94; i++) {
		attacktable[i] = attacktable[63];
		decaytable[i] = decaytable[63];
	}

	//------------------
	//◆ハードウェアLFOのfreq用テーブルの作成
	for (i=0; i<8; i++) {
		lfo_freq_table[i] = (SINT32)(((double)SIN_ENT / (((double)OPNA_CLOCK / 72) / lfo_freq[i] )) * (double)(1 << (opncfg.ratebit + 16 - SIN_BITS)));
	}
}

//==============================================================
//			
//==============================================================
void opngen_setvol(UINT vol) {

	opncfg.fmvol = vol * 5 / 4;
#if defined(OPNGENX86)
	opncfg.fmvol <<= FMASMSHIFT;
#endif
}

//==============================================================
//			
//==============================================================
void opngen_setVR(REG8 channel, REG8 value) {

	if ((channel & 3) && (value)) {
		opncfg.vr_en = TRUE;
		opncfg.vr_l = (channel & 1)?value:0;
		opncfg.vr_r = (channel & 2)?value:0;
	}
	else {
		opncfg.vr_en = FALSE;
	}
}

//==============================================================
//			エンベロープ形状の反映
//--------------------------------------------------------------
//	◆引数
//			OPNSLOT *slot
//	◆返り値
//			無し
//	◆備考
//			エンベロープ関連のレジスタライト時は、
//			発音中の音のエンベロープも更新する。
//==============================================================
void	Envlop_update(OPNSLOT *slot)
{
	SINT32	iRate;

	switch(slot->env_mode){
		case(EM_ATTACK):	//AR中に相当
			iRate = slot->env_inc_attack;
			break;
		case(EM_DECAY1):	//DR中に相当？
		case(EM_DECAY2):	//SR中に相当？
			if(slot->env_cnt < slot->decaylevel){
				slot->env_mode	= EM_DECAY1;
				slot->env_end	= slot->decaylevel;
				iRate			= slot->env_inc_decay1;
			} else {
				slot->env_mode	= EM_DECAY2;
				slot->env_end	= EC_OFF;
				iRate			= slot->env_inc_decay2;
			}
			break;
		case(EM_RELEASE):	//RR中に相当
			iRate = slot->env_inc_release;
			break;
		default:			//EM_OFF
			iRate = 0;
			break;
	}
	slot->env_inc = iRate;		//ここで入れた方が、最適化してくれている。
}

//==============================================================
//			OPN	AL
//==============================================================
static void set_algorithm(OPNCH *ch) {

	SINT32	*outd;			//出力先
	SINT32	*cn1;			
	SINT32	*cn2;			
	SINT32	*cn3;			//一旦、変数に入れたほうが、頭の良い機械語を生成するみたい。
	UINT8	outslot;

	if (ch->stereo) {
		switch(ch->pan & 0xc0) {
			case 0x80:
				outd = &opngen.outdl;
				break;

			case 0x40:
				outd = &opngen.outdr;
				break;
			default:
				outd = &opngen.outdc;
				break;
		}
	} else {
		outd = &opngen.outdc;
	}

	switch(ch->algorithm) {
		case 0:		// [1] - [2] - [3] - [4]
			cn1 = &opngen.feedback2;
			cn2 = &opngen.feedback3;
			cn3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 1:		// ([1] + [2]) - [3] - [4]
			cn1 = &opngen.feedback3;
			cn2 = &opngen.feedback3;
			cn3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 2:		//	([1] + ([2] - [3])) - [4]
			cn1 = &opngen.feedback4;
			cn2 = &opngen.feedback3;
			cn3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 3:		//	(([1] - [2]) + (3)] - [4]
			cn1 = &opngen.feedback2;
			cn2 = &opngen.feedback4;
			cn3 = &opngen.feedback4;
			outslot = 0x08;
			break;

		case 4:		//	(([1] - [2]) + ([3] - [4]))
			cn1 = &opngen.feedback2;
			cn2 = outd;
			cn3 = &opngen.feedback4;
			outslot = 0x0a;
			break;

		case 5:		//	[1] - ([2] + [3] + [4])
			cn1 = 0;
			cn2 = outd;
			cn3 = outd;
			outslot = 0x0e;
			break;

		case 6:		// ([1] - [2]) + [3] + [4]
			cn1 = &opngen.feedback2;
			cn2 = outd;
			cn3 = outd;
			outslot = 0x0e;
			break;

		case 7:		//	[1] + [2] + [3] + [4]
		default:
			cn1 = outd;
			cn2 = outd;
			cn3 = outd;
			outslot = 0x0f;
	}

	//ここで、一括で入れる。
	//簡単なオペコードを生成させるする事で、プリフェッチのデコーダーへのスループットを上げる。
	ch->connect1 = cn1;
	ch->connect2 = cn2;
	ch->connect3 = cn3;
	ch->connect4 = outd;
	ch->outslot = outslot;
}

//==============================================================
//			OPN	DT & ML
//==============================================================
static void set_dt1_mul(OPNSLOT *slot, REG8 value) {

	slot->multiple = (SINT32)multipletable[value & 0x0f];
	slot->detune1 = detunetable[(value >> 4) & 7];
}

//==============================================================
//			OPN	TL
//==============================================================
static void set_tl(OPNSLOT *slot, REG8 value) {

#if (EVC_BITS >= 7)
	slot->totallevel = ((~value) & 0x007f) << (EVC_BITS - 7);
#else
	slot->totallevel = ((~value) & 0x007f) >> (7 - EVC_BITS);
#endif
}

//==============================================================
//			OPN KS & AL
//==============================================================
static void set_ks_ar(OPNSLOT *slot, REG8 value) {

	slot->keyscale = ((~value) >> 6) & 3;
	value &= 0x1f;
	slot->attack = (value)?(attacktable + (value << 1)):nulltable;
	slot->env_inc_attack = slot->attack[slot->envratio];
	if (slot->env_mode == EM_ATTACK) {
		slot->env_inc = slot->env_inc_attack;
	}
}

//==============================================================
//			OPN	DR
//==============================================================
static void set_d1r(OPNSLOT *slot, REG8 value) {

	slot->amon = (value & 0x80) >> 7;
	value &= 0x1f;
	slot->decay1 = (value)?(decaytable + (value << 1)):nulltable;
	slot->env_inc_decay1 = slot->decay1[slot->envratio];
	if (slot->env_mode == EM_DECAY1) {
		slot->env_inc = slot->env_inc_decay1;
	}
}

//==============================================================
//			OPN	DT & SR
//==============================================================
static void set_dt2_d2r(OPNSLOT *slot, REG8 value) {

	value &= 0x1f;
	slot->decay2 = (value)?(decaytable + (value << 1)):nulltable;
	slot->env_inc_decay2 = ((slot->ssgeg1==1)? 0 : slot->decay2[slot->envratio] );
	if (slot->env_mode == EM_DECAY2) {
		slot->env_inc = slot->env_inc_decay2;
	}
}
//==============================================================
//			OPN	DL & RR
//==============================================================
static void set_d1l_rr(OPNSLOT *slot, REG8 value) {

	slot->decaylevel = decayleveltable[(value >> 4)];
	slot->release = decaytable + ((value & 0x0f) << 2) + 2;
	slot->env_inc_release = slot->release[slot->envratio];
	Envlop_update(slot);	//SL, RRの変更なので、こっちで同時に更新する。
}

//==============================================================
//			OPN SSGEG
//==============================================================
static void set_ssgeg(OPNSLOT *slot, REG8 value) {

	value &= 0xf;
	if ((value == 0xb) || (value == 0xd)) {
		slot->ssgeg1 = 1;
		slot->env_inc_decay2 = 0;
	}
	else {
		slot->ssgeg1 = 0;
		slot->env_inc_decay2 = slot->decay2[slot->envratio];
	}
	if (slot->env_mode == EM_DECAY2) {
		slot->env_inc = slot->env_inc_decay2;
	}
}

//==============================================================
//			KeyScaleと音程による、エンベロープ形状の更新
//--------------------------------------------------------------
//	◆引数
//			OPNCH *ch
//	◆返り値
//			無し
//	◆備考
//			Key Scale等により、エンベロープ形状を更新する。
//==============================================================
static void channleupdate(OPNCH *ch) {

	int		i;
	UINT8	kc;
	UINT	evr;
	OPNSLOT	*slot = ch->slot;
	int		s;

	for (i=0; i<4; i++, slot++) {
		s = ((ch->extop)? extendslot[i] : 0);
		kc = ch->kcode[s];
		slot->freq_inc = (ch->keynote[s] + slot->detune1[kc]) * slot->multiple;
		evr = kc >> slot->keyscale;
		if (slot->envratio != evr) {
			slot->envratio = evr;
			slot->env_inc_attack = slot->attack[evr];
			slot->env_inc_decay1 = slot->decay1[evr];
			slot->env_inc_decay2 = ((slot->ssgeg1)? 0 : slot->decay2[evr] );
			slot->env_inc_release = slot->release[evr];
			Envlop_update(slot);	//更新があったら、エンベロープを更新
		}
	}
}

//==============================================================
//			
//==============================================================
void opngen_reset(void) {

	OPNCH	*ch;
	UINT	i;
	OPNSLOT	*slot;
	UINT	j;

	ZeroMemory(&opngen, sizeof(opngen));
	ZeroMemory(opnch, sizeof(opnch));
	opngen.playchannels = 3;

	ch = opnch;
	for (i=0; i<OPNCH_MAX; i++) {
		ch->keynote[0] = 0;
		slot = ch->slot;
		for (j=0; j<4; j++) {
			slot->env_mode = EM_OFF;
			slot->env_cnt = EC_OFF;
			slot->env_end = EC_OFF + 1;
			slot->env_inc = 0;
			slot->detune1 = detunetable[0];
			slot->attack = nulltable;
			slot->decay1 = nulltable;
			slot->decay2 = nulltable;
			slot->release = decaytable;
			slot++;
		}
		ch++;
	}
	for (i=0x30; i<0xc0; i++) {
		opngen_setreg(0, i, 0xff);
		opngen_setreg(3, i, 0xff);
		opngen_setreg(6, i, 0xff);
		opngen_setreg(9, i, 0xff);
	}
}

//==============================================================
//			
//==============================================================
void opngen_setcfg(REG8 maxch, UINT32 flag) {

	OPNCH	*ch;
	UINT	i;

	opngen.playchannels = maxch;
	ch = opnch;
	if ((flag & OPN_CHMASK) == OPN_STEREO) {
		for (i=0; i<OPNCH_MAX; i++) {
			if (flag & (1 << i)) {
				ch->stereo = TRUE;
				set_algorithm(ch);
			}
			ch++;
		}
	}
	else {
		for (i=0; i<OPNCH_MAX; i++) {
			if (flag & (1 << i)) {
				ch->stereo = FALSE;
				set_algorithm(ch);
			}
			ch++;
		}
	}
}

//==============================================================
//			
//==============================================================
void opngen_setextch(UINT chnum, REG8 data) {

	OPNCH	*ch;

	ch = opnch;
	ch[chnum].extop = data;
}

//==============================================================
//			レジスタライト
//--------------------------------------------------------------
//	◆引数
//			REG8	chbase	
//			UINT	reg		
//			REG8	value	
//	◆返り値
//			
//	◆備考
//			
//==============================================================
void opngen_setreg(REG8 chbase, UINT reg, REG8 value) {

	UINT	chpos;
	OPNCH	*ch;
	OPNSLOT	*slot;
	UINT	fn;
	UINT8	blk;
	UINT	i;

	chpos = reg & 3;
	if (chpos == 3) {
		return;
	}
	sound_sync();
	ch = opnch + chbase + chpos;
	if (reg == 0x22) {					// LFO ON & Freq
		if (ch->stereo) {
			opngen.lfo_enable	= (value & 0x08) >> 3;
			opngen.lfo_freq_inc	= lfo_freq_table[value & 0x07];
			opngen.lfo_freq_cnt	= 0;
		}
	} else if (reg < 0xa0) {
		slot = ch->slot + fmslot[(reg >> 2) & 3];
		switch(reg & 0xf0) {
			case 0x30:					// DT1 MUL
				set_dt1_mul(slot, value);
				channleupdate(ch);
				break;

			case 0x40:					// TL
				set_tl(slot, value);
				break;

			case 0x50:					// KS AR
				set_ks_ar(slot, value);
				channleupdate(ch);
				break;

			case 0x60:					// D1R
				set_d1r(slot, value);
				break;

			case 0x70:					// DT2 D2R
				set_dt2_d2r(slot, value);
				channleupdate(ch);
				break;

			case 0x80:					// D1L RR
				set_d1l_rr(slot, value);
				break;

			case 0x90:
				set_ssgeg(slot, value);
				channleupdate(ch);
				break;
		}
	}
	else {
		switch(reg & 0xfc) {
			case 0xa0:
				blk = ch->keyfunc[0] >> 3;
				fn = ((ch->keyfunc[0] & 7) << 8) + value;
				ch->kcode[0] = (blk << 2) | kftable[fn >> 7];
//				ch->keynote[0] = fn * opmbaserate / (1L << (22-blk));
				ch->keynote[0] = (fn << (opncfg.ratebit + blk)) >> 6;
				channleupdate(ch);
				break;

			case 0xa4:
				ch->keyfunc[0] = value & 0x3f;
				break;

			case 0xa8:
				ch = opnch + chbase + 2;
				blk = ch->keyfunc[chpos+1] >> 3;
				fn = ((ch->keyfunc[chpos+1] & 7) << 8) + value;
				ch->kcode[chpos+1] = (blk << 2) | kftable[fn >> 7];
//				ch->keynote[chpos+1] = fn * opmbaserate / (1L << (22-blk));
				ch->keynote[chpos+1] = (fn << (opncfg.ratebit + blk)) >> 6;
				channleupdate(ch);
				break;

			case 0xac:
				ch = opnch + chbase + 2;
				ch->keyfunc[chpos+1] = value & 0x3f;
				break;

			case 0xb0:
				//エンベロープ初期化
				slot = ch->slot;
				for (i=0; i<4; i++) {
					//発音中だったら、アタックからやり直す。
					if(slot->env_mode > EM_RELEASE){
						slot->env_mode = EM_ATTACK;
						slot->env_cnt = EC_ATTACK;
						slot->env_end = EC_DECAY;
						slot->env_inc = slot->env_inc_attack;
					//note off中だったら、発音なっしんぐ。
					} else {
						slot->env_mode = EM_OFF;
						slot->env_cnt = EC_ATTACK;
						slot->env_end = EC_DECAY;
						slot->env_inc = 0;
					}
					slot++;
				}
				ch->algorithm = (UINT8)(value & 7);
				value = (value >> 3) & 7;
				if (value) {
					ch->feedback = 8 - value;
				}
				else {
					ch->feedback = 0;
				}
				set_algorithm(ch);
				break;

			case 0xb4:
				if (ch->stereo) {
					ch->pms = lfo_pms_table[ value & 0x07];	
					ch->ams = lfo_ams_table[(value & 0x30) >> 4];
					ch->pan = (UINT8)(value & 0xc0);
				}
				set_algorithm(ch);
				break;
		}
	}
}

//==============================================================
//			Key On
//==============================================================
void opngen_keyon(UINT chnum, REG8 value) {

	OPNCH	*ch;
	OPNSLOT	*slot;
	REG8	bit;
	UINT	i;
	SINT32	iEnv;				//現在のエンベロープ値

	sound_sync();
	opngen.keyreg[chnum] = value;
	opngen.playing++;
	ch = opnch + chnum;
	ch->playing |= value >> 4;
	slot = ch->slot;
	bit = 0x10;
	for (i=0; i<4; i++) {
		if (value & bit) {							// keyon
			if (slot->env_mode <= EM_RELEASE) {
				slot->freq_cnt = 0;
				if (i == OPNSLOT1) {
					ch->op1fb = 0;
				}
				iEnv = opncfg.envcurve[slot->env_cnt >> ENV_BITS];	//現在の音量
				slot->env_mode = EM_ATTACK;
				slot->env_inc = slot->env_inc_attack;
				slot->env_end = EC_DECAY;
				if (iEnv<=0) {
					//アタックしきっている場合
					slot->env_cnt  = EC_DECAY;
				} else if (iEnv>=EVC_ENT) {
					//減衰しきっている場合
					slot->env_cnt  = EC_ATTACK;
				} else {
					//アタックは、現在のエンベロープの音量から始める。
					slot->env_cnt = (long)((EVC_ENT-1 -sqrt(EVC_ENT*sqrt(EVC_ENT*sqrt(EVC_ENT*iEnv)))) * (1 << ENV_BITS));
					if((slot->env_cnt)< EC_ATTACK ){
						slot->env_cnt = EC_ATTACK;
					}
				}
			}
		}
		else {										// keyoff
			if (slot->env_mode > EM_RELEASE) {
				slot->env_mode = EM_RELEASE;
				if (!(slot->env_cnt & EC_DECAY)) {
					//まだ、Decayに達していない場合。
					slot->env_cnt = (opncfg.envcurve[slot->env_cnt
										>> ENV_BITS] << ENV_BITS) + EC_DECAY;
				}
				slot->env_end = EC_OFF;
				slot->env_inc = slot->env_inc_release;
			}
		}
		slot++;
		bit <<= 1;
	}
	keydisp_fmkeyon((UINT8)chnum, value);
}

//==============================================================
//			
//==============================================================
void opngen_csm(void) {
	opngen_keyon(2, 0x02);
	opngen_keyon(2, 0xf2);
}
