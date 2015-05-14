/**
 * @file	extendmodule.h
 * @brief	外付モジュール インターフェイスの定義を行います
 */

#pragma once

/**
 * @brief 外付モジュール インターフェイス
 */
class IExtendModule
{
public:
	/**
	 * デストラクタ
	 */
	virtual ~IExtendModule() { }

	/**
	 * 初期化
	 * @retval true 成功
	 * @retval false 失敗
	 */
	virtual bool Initialize() = 0;

	/**
	 * 解放
	 */
	virtual void Deinitialize() = 0;

	/**
	 * デバイスは有効?
	 * @retval true 有効
	 * @retval false 無効
	 */
	virtual bool IsEnabled() = 0;

	/**
	 * ビジー?
	 * @retval true ビジー
	 * @retval false レディ
	 */
	virtual bool IsBusy() = 0;

	/**
	 * 音源リセット
	 */
	virtual void Reset() = 0;

	/**
	 * レジスタ書き込み
	 * @param[in] nAddr アドレス
	 * @param[in] cData データ
	 */
	virtual void WriteRegister(UINT nAddr, UINT8 cData) = 0;
};
