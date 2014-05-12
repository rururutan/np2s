/**
 * @file	juliet.h
 * @brief	ROMEO アクセス クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

/**
 * @brief ROMEO アクセス クラス
 */
class CJuliet
{
public:
	static CJuliet* GetInstance();
	CJuliet();
	bool Initialize();
	void Deinitialize();
	bool IsEnabled() const;
	bool IsBusy() const;
	void Reset() const;
	void WriteRegister(UINT nAddr, UINT8 cData);
	void Mute(bool bMute) const;
	void Restore(const UINT8* data, bool bOpna);

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


	static CJuliet sm_instance;	//!< 唯一のインスタンスです

	HMODULE m_hModule;			//!< モジュール
	FnRead32 m_fnRead32;		//!< コンフィグレーション読み取り関数
	FnOut8 m_fnOut8;			//!< outp 関数
	FnOut32 m_fnOut32;			//!< outpd 関数
	FnIn8 m_fnIn8;				//!< inp 関数
	bool m_bOpna;				//!< OPNA 有効フラグ
	UCHAR m_ucIrq;				//!< ROMEO IRQ
	UINT8 m_cPsgMix;			//!< PSG ミキサー
	ULONG m_ulAddress;			//!< ROMEO ベース アドレス
	UINT8 m_cAlgorithm[8];		//!< アルゴリズム テーブル
	UINT8 m_cTtl[8 * 4];		//!< TTL テーブル

	void Clear();
	ULONG SearchRomeo() const;
	void WriteRegisterInner(UINT nAddr, UINT8 cData) const;
	void SetVolume(UINT nChannel, int nVolume) const;
};

/**
 * インスタンスを得る
 * @return インスタンス
 */
inline CJuliet* CJuliet::GetInstance()
{
	return &sm_instance;
}

/**
 * ROMEO は有効?
 * @retval true 有効
 * @retval false 無効
 */
inline bool CJuliet::IsEnabled() const
{
	return (m_hModule != NULL);
}
