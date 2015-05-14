/**
 * @file	juliet.h
 * @brief	ROMEO アクセス クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "..\..\ext\extendmodule.h"

/**
 * @brief ROMEO アクセス クラス
 */
class CJuliet : public IExtendModule
{
public:
	CJuliet();
	virtual ~CJuliet();
	virtual bool Initialize();
	virtual void Deinitialize();
	virtual bool IsEnabled();
	virtual bool IsBusy();
	virtual void Reset();
	virtual void WriteRegister(UINT nAddr, UINT8 cData);

private:
	//! @brief ロード関数
	struct ProcItem
	{
		LPCSTR lpSymbol;		//!< 関数名
		size_t nOffset;			//!< オフセット
	};

	// 定義
	typedef ULONG (WINAPI * FnRead32)(ULONG ulPciAddress, ULONG ulRegAddress);	//!< コンフィグレーション読み取り関数定義
	typedef VOID (WINAPI * FnOut8)(ULONG ulAddress, UCHAR ucParam);				//!< outp 関数定義
	typedef VOID (WINAPI * FnOut32)(ULONG ulAddress, ULONG ulParam);			//!< outpd 関数定義
	typedef UCHAR (WINAPI * FnIn8)(ULONG ulAddress);							//!< inp 関数定義

	HMODULE m_hModule;			//!< モジュール
	FnRead32 m_fnRead32;		//!< コンフィグレーション読み取り関数
	FnOut8 m_fnOut8;			//!< outp 関数
	FnOut32 m_fnOut32;			//!< outpd 関数
	FnIn8 m_fnIn8;				//!< inp 関数
	ULONG m_ulAddress;			//!< ROMEO ベース アドレス
	UCHAR m_ucIrq;				//!< ROMEO IRQ

	ULONG SearchRomeo() const;
};

/**
 * ROMEO は有効?
 * @retval true 有効
 * @retval false 無効
 */
inline bool CJuliet::IsEnabled()
{
	return (m_hModule != NULL);
}
