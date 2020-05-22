#include	"compiler.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"sound.h"
#include	"fmboard.h"

extern "C"	OPNCFG	opncfg;

//==============================================================
//			スロット
//--------------------------------------------------------------
//	◆引数
//		SINT32 envout	エンベロープジェネレータからの入力
//		SINT32 freq_cnt	現在の位相
//		SINT32 feedback	フィードバック入力
//	◆返り値
//		SINT32			振幅レベル
//==============================================================
SINT32 slot_out(SINT32 envout, SINT32 freq_cnt, SINT32 feedback ){

	freq_cnt += feedback;
	freq_cnt >>= (FREQ_BITS - SIN_BITS);
	freq_cnt &= (SIN_ENT - 1);
	
	return((opncfg.sintable[freq_cnt] * opncfg.envtable[envout]) >> (opncfg.sinshift[freq_cnt] + opncfg.envshift[envout]));

}

//==============================================================
//			オペレータ
//--------------------------------------------------------------
//	◆引数
//		OPNCH	*ch		
//		UINT32	lfo_level;		//ハードウェアLFO
//		UINT8	lfo_shift;		//ハードウェアLFO
//	◆返り値
//		無し
//==============================================================
static void calcratechannel(OPNCH *ch, SINT32 lfo_level, char lfo_shift) {

	SINT32	i;
	SINT64	i64;			//計算用
	UINT	cnt_slot;		//Slot用	カウンタ

	OPNSLOT	*slot;			//各Slot

	SINT32	envout;
	SINT32	opout;


	opngen.feedback2 = 0;
	opngen.feedback3 = 0;
	opngen.feedback4 = 0;

	//Each Slot process
	for (cnt_slot=0; cnt_slot<4; cnt_slot++){
		slot = &ch->slot[cnt_slot];

		//音程
		i = slot->freq_inc;
		i64 = i * lfo_level;
		i64 *= ch->pms;
		i += (SINT32)(i64 >> (32 + lfo_shift));		//音程LFO処理
		slot->freq_cnt += i;

		//Envelope Generator
		slot->env_cnt += slot->env_inc;
		if (slot->env_cnt >= slot->env_end){
			switch(slot->env_mode){
				case EM_ATTACK:
					slot->env_mode = EM_DECAY1;
					slot->env_cnt = EC_DECAY;
					slot->env_end = slot->decaylevel;
					slot->env_inc = slot->env_inc_decay1;
					break;
				case EM_DECAY1:	
					slot->env_mode = EM_DECAY2;
					slot->env_cnt = slot->decaylevel;
					slot->env_end = EC_OFF;
					slot->env_inc = slot->env_inc_decay2;
					break;
				case EM_RELEASE:
					slot->env_mode = EM_OFF;
				case EM_DECAY2:
					slot->env_cnt = EC_OFF;
					slot->env_end = EC_OFF + 1;
					slot->env_inc = 0;
					ch->playing &= ~(1 << cnt_slot);
			}
		}
		envout = slot->totallevel;
		if ((slot->amon) & 0x01) {
			envout += (lfo_level * ch->ams) >> lfo_shift;	//音量LFO
		}
		envout -= opncfg.envcurve[slot->env_cnt >> ENV_BITS];

		//Osc
		if (envout>0) {
			switch(cnt_slot){
				case 0:
					opout = ch->op1fb;
					if (ch->feedback > 0){
						i = ch->op1fb >> ch->feedback;
					} else {
						i = 0;
					}
					ch->op1fb = slot_out(envout, slot->freq_cnt, i);
					opout += ch->op1fb;
					opout /= 2;
					if (ch->algorithm == 5) {
						opngen.feedback2 = opout;
						opngen.feedback3 = opout;
						opngen.feedback4 = opout;
					} else {
						*ch->connect1 += opout;
					}
					break;
				case 1:
					*ch->connect2 += slot_out(envout, slot->freq_cnt, opngen.feedback2);
					break;
				case 2:
					*ch->connect3 += slot_out(envout, slot->freq_cnt, opngen.feedback3);
					break;
				case 3:
					*ch->connect4 += slot_out(envout, slot->freq_cnt, opngen.feedback4);
					break;
			}
		}
	}
}

//==============================================================
//			
//--------------------------------------------------------------
//	◆引数
//		void	*hdl	
//		SINT32	*pcm	波形バッファ
//		UINT	count	必要サンプル数
//	◆返り値
//		無し
//==============================================================
void SOUNDCALL opngen_getpcm(void *hdl, SINT32 *pcm, UINT count) {

	SINT32	i;				//計算用
	SINT64	i64;			//計算用
	UINT	cnt_ch;			//Slot用	カウンタ

	OPNCH	*fm = opnch;	//各Ch

	SINT32	samp_l;
	SINT32	samp_r;

	SINT32	lfo_level;		//ハードウェアLFO
	char	lfo_shift;		//ハードウェアLFO

	if (!opngen.playing) {
		return;
	}

	while(count>0){

		samp_l = opngen.outdl * opngen.calcremain;
		samp_r = opngen.outdr * opngen.calcremain;
		opngen.calcremain = FMDIV_ENT - opngen.calcremain;

		while(1){
			opngen.playing = 0;
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;

			//Hardware LFO Frequency
			if	(opngen.lfo_enable & 0x01){
				i = opngen.lfo_freq_cnt + opngen.lfo_freq_inc;
				opngen.lfo_freq_cnt = i;
				i >>= (FREQ_BITS - SIN_BITS);
				i &= (SIN_ENT - 1);
				lfo_level = opncfg.sintable[i];
				lfo_shift = opncfg.sinshift[i];
			} else {
				lfo_level = 0;
				lfo_shift = 0;
			}

			//Each Channel process
			for (cnt_ch=0; cnt_ch<opngen.playchannels; cnt_ch++) {
				fm = &opnch[cnt_ch];
				if (fm->playing & fm->outslot) {
					calcratechannel(fm, lfo_level, lfo_shift);
					opngen.playing++;
				}
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				opngen.calcremain -= opncfg.calc1024;
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
			} else {
				break;
			}
		}

		i64 = opngen.calcremain * opngen.outdl + samp_l;
		i64 *= opncfg.fmvol;
		pcm[0] += (SINT32)(i64 >> 32);

		i64 = opngen.calcremain * opngen.outdr + samp_r;
		i64 *= opncfg.fmvol;
		pcm[1] += (SINT32)(i64 >> 32);

		pcm += 2;

		opngen.calcremain = opncfg.calc1024 - opngen.calcremain;
		count--;
	}

}

//==============================================================
//			
//--------------------------------------------------------------
//	◆引数
//		void	*hdl	
//		SINT32	*pcm	波形バッファ
//		UINT	count	必要サンプル数
//	◆返り値
//		無し
//==============================================================
void SOUNDCALL opngen_getpcmvr(void *hdl, SINT32 *pcm, UINT count) {

	SINT32	i;				//計算用
	SINT64	i64;
	UINT	cnt_ch;			//Slot用	カウンタ

	SINT32	samp_l;
	SINT32	samp_r;

	UINT32	lfo_level;		//ハードウェアLFO
	UINT8	lfo_shift;		//ハードウェアLFO

	while(count>0){

		samp_l = opngen.outdl * opngen.calcremain;
		samp_r = opngen.outdr * opngen.calcremain;
		opngen.calcremain = FMDIV_ENT - opngen.calcremain;

		while(1){
			opngen.playing = 0;
			opngen.outdc = 0;
			opngen.outdl = 0;
			opngen.outdr = 0;

			//Hardware LFO Frequency
			if	(opngen.lfo_enable & 0x01){
				i = opngen.lfo_freq_cnt + opngen.lfo_freq_inc;
				opngen.lfo_freq_cnt = i;
				i >>= (FREQ_BITS - SIN_BITS);
				i &= (SIN_ENT - 1);
				lfo_level = opncfg.sintable[i];
				lfo_shift = opncfg.sinshift[i];
			} else {
				lfo_level = 0;
				lfo_shift = 0;
			}

			//Each Channel process
			for (cnt_ch=0; cnt_ch<opngen.playchannels; cnt_ch++) {
				calcratechannel(&opnch[cnt_ch], lfo_level, lfo_shift);
			}
			if (opncfg.vr_en) {
				SINT32 tmpl;
				SINT32 tmpr;
				tmpl = (opngen.outdl >> 5) * opncfg.vr_l;
				tmpr = (opngen.outdr >> 5) * opncfg.vr_r;
				opngen.outdl += tmpr;
				opngen.outdr += tmpl;
			}
			opngen.outdl += opngen.outdc;
			opngen.outdr += opngen.outdc;
			opngen.outdl >>= FMVOL_SFTBIT;
			opngen.outdr >>= FMVOL_SFTBIT;
			if (opngen.calcremain > opncfg.calc1024) {
				opngen.calcremain -= opncfg.calc1024;
				samp_l += opngen.outdl * opncfg.calc1024;
				samp_r += opngen.outdr * opncfg.calc1024;
			} else {
				break;
			}
		}
		i64 = opngen.calcremain * opngen.outdl + samp_l;
		i64 *= opncfg.fmvol;
		pcm[0] += (SINT32)(i64 >> 32);

		i64 = opngen.calcremain * opngen.outdr + samp_r;
		i64 *= opncfg.fmvol;
		pcm[1] += (SINT32)(i64 >> 32);

		pcm += 2;

		opngen.calcremain -= opncfg.calc1024;
		count--;
	}

}

