/**
 * @file	gimicusb.h
 * @brief	G.I.M.I.C USB アクセス クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "c86ctl.h"
#include "usbdev.h"
#include "misc/guard.h"
#include "misc/threadbase.h"

/**
 * @brief G.I.M.I.C USB アクセス クラス
 */
class CGimicUSB : public IC86RealChip, protected CThreadBase
{
public:
	CGimicUSB();
	~CGimicUSB();
	virtual int Initialize();
	virtual int Deinitialize();

	// IGimic
	int SetSSGVolume(UINT8 cVolume);
	int GetSSGVolume(UINT8* pcVolume);
	int SetPLLClock(UINT nClock);
	int GetPLLClock(UINT* pnClock);
	int GetFWVer(UINT* pnMajor, UINT* pnMinor, UINT* pnRev, UINT* pnBuild);
	int GetMBInfo(Devinfo* pInfo);
	int GetModuleInfo(Devinfo* pInfo);

	// IGimic2
	int GetModuleType(ChipType* pnType);

	// IRealChip
	virtual int Reset();
	virtual void Out(UINT nAddr, UINT8 cData);
	virtual UINT8 In(UINT nAddr);

	// IRealChip2
	virtual int GetChipStatus(UINT nAddr, UINT8* pcStatus);
	virtual void DirectOut(UINT nAddr, UINT8 cData);

	// IRealChip3
	virtual int GetChipType(ChipType* pnType);

protected:
	virtual bool Task();

private:
	/**
	 * @brief FM データ
	 */
	struct FMDATA
	{
		UINT16 wAddr;		/*!< アドレス */
		UINT8 cData;		/*!< データ */
		UINT8 cPadding;		/*!< パディング */
	};

	CUsbDev m_usb;			/*!< USB */
	CGuard m_usbGuard;		/*!< USBアクセス */
	CGuard m_queGuard;		/*!< キュー */
	ChipType m_nChipType;	/*!< チップ タイプ */
	size_t m_nQueIndex;		/*!< データ インデックス */
	size_t m_nQueCount;		/*!< データ カウント */
	FMDATA m_que[0x400];	/*!< キュー */
	UINT8 m_sReg[0x200];	/*!< レジスタ */

	int Transaction(const void* lpOutput, size_t cbOutput, void* lpInput = NULL, size_t cbInput = 0);
	int GetInfo(UINT8 cParam, Devinfo* pInfo);
	static void TailZeroFill(char* lpBuffer, size_t cbBuffer);
	UINT GetChipAddr(UINT nAddr) const;
};
