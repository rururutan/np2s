/**
 * @file	c86boxusb.h
 * @brief	C86BOX USB アクセス クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "c86ctl.h"
#include "usbdev.h"
#include "misc/guard.h"
#include "misc/threadbase.h"

/**
 * @brief C86BOX USB アクセス クラス
 */
class C86BoxUSB : public IC86RealChip, protected CThreadBase
{
public:
	C86BoxUSB();
	virtual ~C86BoxUSB();

	// IRealChipBase
	virtual int Initialize();
	virtual int Deinitialize();

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
	CUsbDev m_usb;			/*!< USB */
	CGuard m_usbGuard;		/*!< USBアクセス */
	CGuard m_queGuard;		/*!< キュー */
	ChipType m_nChipType;	/*!< チップ タイプ */
	int m_nDevId;			/*!< デバイス タイプ */
	size_t m_nQueIndex;		/*!< データ インデックス */
	size_t m_nQueCount;		/*!< データ カウント */
	UINT m_que[0x400];		/*!< キュー */
	UINT8 m_sReg[0x200];	/*!< レジスタ */

	int Transaction(const void* lpOutput, size_t cbOutput, void* lpInput = NULL, size_t cbInput = 0);
};
