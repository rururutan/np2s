/**
 * @file	externalopna.h
 * @brief	外部 OPNA 演奏クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

class IExtendModule;

/**
 * @brief 外部 OPNA 演奏クラス
 */
class CExternalOpna
{
public:
	static CExternalOpna* GetInstance();

	CExternalOpna();
	void Initialize();
	void Deinitialize();
	bool IsEnabled() const;
	bool IsBusy() const;
	void Reset() const;
	void WriteRegister(UINT nAddr, UINT8 cData);
	void Mute(bool bMute) const;
	void Restore(const UINT8* data, bool bOpna);

private:
	static CExternalOpna sm_instance;	//!< 唯一のインスタンスです
	IExtendModule* m_module;			//!< モジュール
	UINT8 m_cPsgMix;					//!< PSG ミキサー
	UINT8 m_cAlgorithm[8];				//!< アルゴリズム テーブル
	UINT8 m_cTtl[8 * 4];				//!< TTL テーブル

	void WriteRegisterInner(UINT nAddr, UINT8 cData) const;
	void SetVolume(UINT nChannel, int nVolume) const;
};

/**
 * インスタンスを得る
 * @return インスタンス
 */
inline CExternalOpna* CExternalOpna::GetInstance()
{
	return &sm_instance;
}

/**
 * インスタンスは有効?
 * @retval true 有効
 * @retval false 無効
 */
inline bool CExternalOpna::IsEnabled() const
{
	return (m_module != NULL);
}
